/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2017 Joyent, Inc.
 */

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/thread.h>
#include <sys/list.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/machsystm.h>
#include <sys/vmsystm.h>
#include <vm/as.h>
#include <vm/seg_vn.h>
#include <vm/seg_kmem.h>

#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include "vm/vm_glue.h"

#define	PMAP_TO_VMMAP(pm)	((vm_map_t)		\
	((caddr_t)(pm) - offsetof(struct vmspace, vms_pmap)))
#define	VMMAP_TO_VMSPACE(vmmap)	((struct vmspace *)		\
	((caddr_t)(vmmap) - offsetof(struct vmspace, vm_map)))

/* Similar to htable, but without the bells and whistles */
struct eptable {
	struct eptable	*ept_next;
	uintptr_t	ept_vaddr;
	pfn_t		ept_pfn;
	int16_t		ept_level;
	int16_t		ept_valid_cnt;
	uint32_t	_ept_pad2;
	struct eptable	*ept_prev;
	struct eptable	*ept_parent;
};
typedef struct eptable eptable_t;

struct eptable_map {
	kmutex_t		em_lock;
	eptable_t		*em_root;
	eptable_t		**em_hash;
	size_t			em_table_cnt;
	size_t			em_wired;

	/* Protected by eptable_map_lock */
	struct eptable_map	*em_next;
	struct eptable_map	*em_prev;
};
typedef struct eptable_map eptable_map_t;

#define	EPTABLE_HASH(va, lvl, sz)				\
	((((va) >> LEVEL_SHIFT(1)) + ((va) >> 28) + (lvl))	\
	& ((sz) - 1))

#define	EPTABLE_VA2IDX(tbl, va)					\
	(((va) - (tbl)->ept_vaddr) >>				\
	LEVEL_SHIFT((tbl)->ept_level))

#define	EPTABLE_IDX2VA(tbl, idx)				\
	((tbl)->ept_vaddr +					\
	((idx) << LEVEL_SHIFT((tbl)->ept_level)))

#define	EPT_R	(0x1 << 0)
#define	EPT_W	(0x1 << 1)
#define	EPT_X	(0x1 << 2)
#define	EPT_RWX	(EPT_R|EPT_W|EPT_X)
#define	EPT_MAP	(0x1 << 7)

#define	EPT_CACHE_WB	(0x6 << 3)

#define	EPT_PADDR	(0x000ffffffffff000ull)

#define	EPT_IS_ABSENT(pte)	(((pte) & EPT_RWX) == 0)
#define	EPT_IS_TABLE(pte)	(((pte) & EPT_MAP) == 0)
#define	EPT_PTE_PFN(pte)	mmu_btop((pte) & EPT_PADDR)
#define	EPT_PTE_PROT(pte)	((pte) & EPT_RWX)

#define	EPT_PTE_ASSIGN_TABLE(pfn)			\
	((pfn_to_pa(pfn) & EPT_PADDR) | EPT_RWX)
#define	EPT_PTE_ASSIGN_PAGE(pfn, prot)			\
	((pfn_to_pa(pfn) & EPT_PADDR) | EPT_MAP |	\
	((prot) & EPT_RWX) |				\
	EPT_CACHE_WB)

struct vmspace_mapping {
	list_node_t	vmsm_node;
	vm_object_t	vmsm_object;
	caddr_t		vmsm_addr;
	size_t		vmsm_len;
	off_t		vmsm_offset;
};
typedef struct vmspace_mapping vmspace_mapping_t;

/* Private glue interfaces */
static void pmap_free(pmap_t);
static eptable_t *eptable_alloc(void);
static void eptable_free(eptable_t *);
static void eptable_init(eptable_map_t *);
static void eptable_fini(eptable_map_t *);
static eptable_t *eptable_hash_lookup(eptable_map_t *, uintptr_t, level_t);
static void eptable_hash_insert(eptable_map_t *, eptable_t *);
static void eptable_hash_remove(eptable_map_t *, eptable_t *);
static eptable_t *eptable_walk(eptable_map_t *, uintptr_t, level_t, uint_t *,
    boolean_t);
static page_t *eptable_mapin(eptable_map_t *, uintptr_t, page_t *, uint_t);
static page_t *eptable_mapout(eptable_map_t *, uintptr_t);
static int eptable_find(eptable_map_t *, uintptr_t, pfn_t *, uint_t *);
static void vm_object_release(vm_object_t);
static vmspace_mapping_t *vm_mapping_find(struct vmspace *, caddr_t, size_t);
static void vm_mapping_remove(struct vmspace *, vmspace_mapping_t *);


static kmutex_t eptable_map_lock;
static struct eptable_map *eptable_map_head = NULL;

struct vmspace *
vmspace_alloc(vm_offset_t start, vm_offset_t end, pmap_pinit_t pinit)
{
	struct vmspace *vms;
	struct as *as;
	const uintptr_t size = end + 1;

	/*
	 * This whole mess is built on the assumption that a 64-bit address
	 * space is available to work with for the various pagetable tricks.
	 */
	VERIFY(ttoproc(curthread)->p_model == DATAMODEL_LP64);
	VERIFY(start == 0 && size > 0 && (size & PAGEOFFSET) == 0 &&
	    size <= (uintptr_t)USERLIMIT);

	vms = kmem_zalloc(sizeof (*vms), KM_SLEEP);
	vms->vms_size = size;
	list_create(&vms->vms_maplist, sizeof (vmspace_mapping_t),
	    offsetof(vmspace_mapping_t, vmsm_node));

	if (pinit(&vms->vms_pmap) == 0) {
		kmem_free(vms, sizeof (*vms));
		return (NULL);
	}

	as = as_alloc();
	as->a_userlimit = (caddr_t)end;

	/* For now, lock all new mappings into the AS automatically */
	VERIFY0(as_ctl(as, NULL, 0, MC_LOCKAS, 0, MCL_FUTURE, NULL, 0));

	vms->vms_as = as;
	return (vms);
}

void
vmspace_free(struct vmspace *vms)
{
	struct as *as = vms->vms_as;

	VERIFY(list_is_empty(&vms->vms_maplist));
	VERIFY0(avl_numnodes(&as->a_segtree));

	as_free(as);
	pmap_free(&vms->vms_pmap);
	kmem_free(vms, sizeof (*vms));
}

pmap_t
vmspace_pmap(struct vmspace *vms)
{
	return (&vms->vms_pmap);
}

long
vmspace_resident_count(struct vmspace *vms)
{
	/* XXXJOY: finish */
	return (0);
}

static int
vmspace_pmap_wire(struct vmspace *vms, caddr_t addr, size_t size, page_t **pp,
    uint_t prot)
{
	enum pmap_type type = vms->vms_pmap.pm_type;

	ASSERT(MUTEX_HELD(&vms->vms_lock));

	switch (type) {
	case PT_EPT: {
		eptable_map_t *map = (eptable_map_t *)vms->vms_pmap.pm_map;
		uintptr_t maddr = (uintptr_t)addr;
		const ulong_t npages = btop(size);
		ulong_t idx;

		for (idx = 0; idx < npages; idx++, maddr += PAGESIZE) {
			page_t *oldpp;

			oldpp = eptable_mapin(map, maddr, pp[idx], prot);
			ASSERT(oldpp == NULL);
		}
		vms->vms_pmap.pm_eptgen++;
		return (0);
	}
	case PT_RVI:
		/* RVI support not yet implemented */
	default:
		panic("unsupported pmap type: %x", type);
		/* NOTREACHED */
		break;
	}
	return (0);
}

static int
vmspace_pmap_iswired(struct vmspace *vms, caddr_t addr, uint_t *prot)
{
	enum pmap_type type = vms->vms_pmap.pm_type;

	ASSERT(MUTEX_HELD(&vms->vms_lock));

	switch (type) {
	case PT_EPT: {
		eptable_map_t *map = (eptable_map_t *)vms->vms_pmap.pm_map;
		uintptr_t maddr = (uintptr_t)addr;
		pfn_t pfn;

		return (eptable_find(map, maddr, &pfn, prot));
	}
	case PT_RVI:
		/* RVI support not yet implemented */
	default:
		panic("unsupported pmap type: %x", type);
		/* NOTREACHED */
		break;
	}
	return (-1);
}

static int
vmspace_pmap_unmap(struct vmspace *vms, caddr_t addr, size_t size)
{
	enum pmap_type type = vms->vms_pmap.pm_type;

	ASSERT(MUTEX_HELD(&vms->vms_lock));

	switch (type) {
	case PT_EPT: {
		eptable_map_t *map = (eptable_map_t *)vms->vms_pmap.pm_map;
		uintptr_t maddr = (uintptr_t)addr;
		const ulong_t npages = btop(size);
		ulong_t idx;

		for (idx = 0; idx < npages; idx++, maddr += PAGESIZE) {
			page_t *pp;

			pp = eptable_mapout(map, maddr);
			if (pp != NULL) {
				page_unlock(pp);
			}
		}
		vms->vms_pmap.pm_eptgen++;
		return (0);
	}
		break;
	case PT_RVI:
		/* RVI support not yet implemented */
	default:
		panic("unsupported pmap type: %x", type);
		/* NOTREACHED */
		break;
	}
	return (0);
}

static void
pmap_free(pmap_t pmap)
{
	switch (pmap->pm_type) {
	case PT_EPT: {
		eptable_map_t *map = (eptable_map_t *)pmap->pm_map;

		pmap->pm_pml4 = NULL;
		pmap->pm_map = NULL;

		eptable_fini(map);
		kmem_free(map, sizeof (*map));
		return;
	}
	case PT_RVI:
		/* RVI support not yet implemented */
	default:
		panic("unsupported pmap type: %x", pmap->pm_type);
		/* NOTREACHED */
		break;
	}
}

int
pmap_pinit_type(pmap_t pmap, enum pmap_type type, int flags)
{
	/* For use in vmm only */
	pmap->pm_type = type;
	switch (type) {
	case PT_EPT: {
		eptable_map_t *map;
		caddr_t pml4_va;

		map = kmem_zalloc(sizeof (*map), KM_SLEEP);
		eptable_init(map);
		pml4_va = hat_kpm_mapin_pfn(map->em_root->ept_pfn);

		pmap->pm_map = map;
		pmap->pm_pml4 = pml4_va;
		return (1);
	}
	case PT_RVI:
		/* RVI support not yet implemented */
		return (0);
	default:
		panic("unsupported pmap type: %x", type);
		/* NOTREACHED */
		break;
	}

	/* XXXJOY: finish */
	return (1);
}

long
pmap_wired_count(pmap_t pmap)
{
	enum pmap_type type = pmap->pm_type;
	long val = 0L;

	switch (type) {
	case PT_EPT:
		val = ((eptable_map_t *)pmap->pm_map)->em_wired;
		break;
	case PT_RVI:
		/* RVI support not yet implemented */
	default:
		panic("unsupported pmap type: %x", type);
		/* NOTREACHED */
		break;
	}
	return (val);
}

int
pmap_emulate_accessed_dirty(pmap_t pmap, vm_offset_t va, int ftype)
{
	vm_map_t map = PMAP_TO_VMMAP(pmap);

	/*
	 * Faults not related to execution come in as accessed/dirty calls.
	 * For now, just handle them as faults.
	 */
	return (vm_fault(map, va, ftype, 0));
}


static eptable_t *
eptable_alloc(void)
{
	eptable_t *ept;
	page_t *pp;
	caddr_t pva;

	ept = kmem_zalloc(sizeof (*ept), KM_SLEEP);

	do {
		pp = page_get_physical((uintptr_t)ept);
		if (pp == NULL) {
			kmem_reap();
			delay(1);
		}
	} while (pp == NULL);

	ept->ept_pfn = pp->p_pagenum;
	pva = hat_kpm_mapin_pfn(pp->p_pagenum);
	bzero(pva, PAGESIZE);

	return (ept);
}

static void
eptable_free(eptable_t *ept)
{
	page_t *pp;

	ASSERT(ept->ept_pfn != PFN_INVALID);

	pp = page_numtopp_nolock(ept->ept_pfn);
	if (!page_tryupgrade(pp)) {
		u_offset_t off = pp->p_offset;

		page_unlock(pp);
		pp = page_lookup(&kvp, off, SE_EXCL);
		if (pp == NULL)
			panic("page not found");
	}
	page_hashout(pp, NULL);
	page_free(pp, 1);
	page_unresv(1);

	ept->ept_pfn = PFN_INVALID;
	kmem_free(ept, sizeof (*ept));
}

static void
eptable_init(eptable_map_t *map)
{
	eptable_t *root;

	VERIFY0(mmu.hash_cnt & (mmu.hash_cnt - 1));

	map->em_table_cnt = mmu.hash_cnt;
	map->em_hash = kmem_zalloc(sizeof (eptable_t *) * map->em_table_cnt,
	    KM_SLEEP);

	root = eptable_alloc();
	root->ept_level = mmu.max_level;
	map->em_root = root;

	/* Insert into global tracking list of eptable maps */
	mutex_enter(&eptable_map_lock);
	map->em_next = eptable_map_head;
	map->em_prev = NULL;
	if (eptable_map_head != NULL) {
		eptable_map_head->em_prev = map;
	}
	eptable_map_head = map;
	mutex_exit(&eptable_map_lock);
}

static void
eptable_fini(eptable_map_t *map)
{
	const uint_t cnt = map->em_table_cnt;

	/* Remove from global tracking list of eptable maps */
	mutex_enter(&eptable_map_lock);
	if (map->em_next != NULL) {
		map->em_next->em_prev = map->em_prev;
	}
	if (map->em_prev != NULL) {
		map->em_prev->em_next = map->em_next;
	} else {
		eptable_map_head = map->em_next;
	}
	mutex_exit(&eptable_map_lock);

	mutex_enter(&map->em_lock);
	/* XXJOY: Should we expect to need this clean-up? */
	for (uint_t i = 0; i < cnt; i++) {
		eptable_t *ept = map->em_hash[i];

		while (ept != NULL) {
			eptable_t *next = ept->ept_next;

			eptable_hash_remove(map, ept);
			eptable_free(ept);
			ept = next;
		}
	}

	kmem_free(map->em_hash, sizeof (eptable_t *) * cnt);
	eptable_free(map->em_root);

	mutex_exit(&map->em_lock);
	mutex_destroy(&map->em_lock);
}

static eptable_t *
eptable_hash_lookup(eptable_map_t *map, uintptr_t va, level_t lvl)
{
	const uint_t hash = EPTABLE_HASH(va, lvl, map->em_table_cnt);
	eptable_t *ept;

	ASSERT(MUTEX_HELD(&map->em_lock));

	for (ept = map->em_hash[hash]; ept != NULL; ept = ept->ept_next) {
		if (ept->ept_vaddr == va && ept->ept_level == lvl)
			break;
	}
	return (ept);
}

static void
eptable_hash_insert(eptable_map_t *map, eptable_t *ept)
{
	const uintptr_t va = ept->ept_vaddr;
	const uint_t lvl = ept->ept_level;
	const uint_t hash = EPTABLE_HASH(va, lvl, map->em_table_cnt);

	ASSERT(MUTEX_HELD(&map->em_lock));
	ASSERT(ept_hash_table_lookup(map, va, lvl) == NULL);

	ept->ept_prev = NULL;
	if (map->em_hash[hash] == NULL) {
		ept->ept_next = NULL;
	} else {
		eptable_t *chain = map->em_hash[hash];

		ept->ept_next = chain;
		chain->ept_prev = ept;
	}
	map->em_hash[hash] = ept;
}

static void
eptable_hash_remove(eptable_map_t *map, eptable_t *ept)
{
	const uintptr_t va = ept->ept_vaddr;
	const uint_t lvl = ept->ept_level;
	const uint_t hash = EPTABLE_HASH(va, lvl, map->em_table_cnt);

	ASSERT(MUTEX_HELD(&map->em_lock));

	if (ept->ept_prev == NULL) {
		ASSERT(map->em_hash[hash] == ept);

		map->em_hash[hash] = ept->ept_next;
	} else {
		ept->ept_prev->ept_next = ept->ept_next;
	}
	if (ept->ept_next != NULL) {
		ept->ept_next->ept_prev = ept->ept_prev;
	}
	ept->ept_next = NULL;
	ept->ept_prev = NULL;
}

static eptable_t *
eptable_walk(eptable_map_t *map, uintptr_t va, level_t tgtlvl, uint_t *idxp,
    boolean_t do_create)
{
	eptable_t *ept = map->em_root;
	level_t lvl = ept->ept_level;
	uint_t idx = UINT_MAX;

	ASSERT(MUTEX_HELD(&map->em_lock));

	while (lvl >= tgtlvl) {
		x86pte_t *ptes, entry;
		const uintptr_t masked_va = va & LEVEL_MASK((uint_t)lvl);
		eptable_t *newept = NULL;

		idx = EPTABLE_VA2IDX(ept, va);
		if (lvl == tgtlvl || lvl == 0) {
			break;
		}

		ptes = (x86pte_t *)hat_kpm_mapin_pfn(ept->ept_pfn);
		entry = ptes[idx];
		if (EPT_IS_ABSENT(entry)) {
			if (!do_create) {
				break;
			}

			newept = eptable_alloc();
			newept->ept_level = lvl - 1;
			newept->ept_vaddr = masked_va;
			newept->ept_parent = ept;

			eptable_hash_insert(map, newept);
			entry = EPT_PTE_ASSIGN_TABLE(newept->ept_pfn);
			ptes[idx] = entry;
			ept->ept_valid_cnt++;
		} else if (EPT_IS_TABLE(entry)) {
			/* Do lookup in next level of page table */
			newept = eptable_hash_lookup(map, masked_va, lvl - 1);

			VERIFY(newept);
			VERIFY3P(pfn_to_pa(newept->ept_pfn), ==,
			    (entry & EPT_PADDR));
		} else {
			/*
			 * There is a (large) page mapped here.  Since support
			 * for non-PAGESIZE pages is not yet present, this is a
			 * surprise.
			 */
			panic("unexpected parge page in pte %p", &ptes[idx]);
		}
		ept = newept;
		lvl--;
	}

	VERIFY(lvl >= 0 && idx != UINT_MAX);
	*idxp = idx;
	return (ept);
}

static page_t *
eptable_mapin(eptable_map_t *map, uintptr_t va, page_t *pp, uint_t prot)
{
	level_t tgtlvl;
	uint_t idx;
	eptable_t *ept;
	x86pte_t *ptes, entry;
	size_t pgsize;
	page_t *oldpp = NULL;

	CTASSERT(EPT_R == PROT_READ);
	CTASSERT(EPT_W == PROT_WRITE);
	CTASSERT(EPT_X == PROT_EXEC);
	ASSERT((prot & EPT_RWX) != 0 && (prot & ~EPT_RWX) == 0);

	/* XXXJOY: punt on large pages for now */
	VERIFY(pp->p_szc == 0);
	tgtlvl = 0;
	pgsize = (size_t)LEVEL_SIZE((uint_t)tgtlvl);

	mutex_enter(&map->em_lock);
	ept = eptable_walk(map, va, tgtlvl, &idx, B_TRUE);
	ptes = (x86pte_t *)hat_kpm_mapin_pfn(ept->ept_pfn);
	entry = ptes[idx];

	if (!EPT_IS_ABSENT(entry)) {
		pfn_t oldpfn;

		if (EPT_IS_TABLE(entry)) {
			panic("unexpected table entry %lx in %p[%d]",
			    entry, ept, idx);
		}
		/*
		 * XXXJOY: Just clean the entry for now. Assume(!) that
		 * invalidation is going to occur anyways.
		 */
		oldpfn = EPT_PTE_PFN(ptes[idx]);
		ept->ept_valid_cnt--;
		ptes[idx] = (x86pte_t)0;
		map->em_wired -= (pgsize >> PAGESHIFT);

		/* The page should have been locked when it was wired in */
		oldpp = page_numtopp_nolock(oldpfn);
	}

	entry = EPT_PTE_ASSIGN_PAGE(pp->p_pagenum, prot);
	ptes[idx] = entry;
	ept->ept_valid_cnt++;
	map->em_wired += (pgsize >> PAGESHIFT);
	mutex_exit(&map->em_lock);

	return (oldpp);
}

static page_t *
eptable_mapout(eptable_map_t *map, uintptr_t va)
{
	eptable_t *ept;
	uint_t idx;
	x86pte_t *ptes, entry;
	page_t *oldpp = NULL;

	mutex_enter(&map->em_lock);
	/* Find the lowest level entry at this VA */
	ept = eptable_walk(map, va, -1, &idx, B_FALSE);

	ptes = (x86pte_t *)hat_kpm_mapin_pfn(ept->ept_pfn);
	entry = ptes[idx];

	if (EPT_IS_ABSENT(entry)) {
		/*
		 * There is nothing here to free up.  If this was a sparsely
		 * wired mapping, the absence is no concern.
		 */
		mutex_exit(&map->em_lock);
		return (NULL);
	} else {
		pfn_t oldpfn;
		const size_t pagesize = LEVEL_SIZE((uint_t)ept->ept_level);

		if (EPT_IS_TABLE(entry)) {
			panic("unexpected table entry %lx in %p[%d]",
			    entry, ept, idx);
		}
		/*
		 * XXXJOY: Just clean the entry for now. Assume(!) that
		 * invalidation is going to occur anyways.
		 */
		oldpfn = EPT_PTE_PFN(ptes[idx]);
		ept->ept_valid_cnt--;
		ptes[idx] = (x86pte_t)0;
		map->em_wired -= (pagesize >> PAGESHIFT);

		/* The page should have been locked when it was wired in */
		oldpp = page_numtopp_nolock(oldpfn);
	}

	while (ept->ept_valid_cnt == 0 && ept->ept_parent != NULL) {
		eptable_t *next = ept->ept_parent;

		idx = EPTABLE_VA2IDX(next, va);
		ptes = (x86pte_t *)hat_kpm_mapin_pfn(next->ept_pfn);

		entry = ptes[idx];
		ASSERT(EPT_IS_TABLE(entry));
		ASSERT(EPT_PTE_PFN(entry) == ept->ept_pfn);

		ptes[idx] = (x86pte_t)0;
		next->ept_valid_cnt--;
		eptable_hash_remove(map, ept);
		ept->ept_parent = NULL;
		eptable_free(ept);

		ept = next;
	}
	mutex_exit(&map->em_lock);

	return (oldpp);
}

static int
eptable_find(eptable_map_t *map, uintptr_t va, pfn_t *pfn, uint_t *prot)
{
	eptable_t *ept;
	uint_t idx;
	x86pte_t *ptes, entry;
	int err = -1;

	mutex_enter(&map->em_lock);
	/* Find the lowest level entry at this VA */
	ept = eptable_walk(map, va, -1, &idx, B_FALSE);

	/* XXXJOY: Until large pages are supported, this check is easy */
	if (ept->ept_level != 0) {
		mutex_exit(&map->em_lock);
		return (-1);
	}

	ptes = (x86pte_t *)hat_kpm_mapin_pfn(ept->ept_pfn);
	entry = ptes[idx];

	if (!EPT_IS_ABSENT(entry)) {
		if (EPT_IS_TABLE(entry)) {
			panic("unexpected table entry %lx in %p[%d]",
			    entry, ept, idx);
		}

		*pfn = EPT_PTE_PFN(entry);
		*prot = EPT_PTE_PROT(entry);
		err = 0;
	}

	mutex_exit(&map->em_lock);
	return (err);
}


vm_object_t
vm_object_allocate(objtype_t type, vm_pindex_t psize)
{
	vm_object_t vmo;
	const size_t size = ptob((size_t)psize);

	/* only support anon for now */
	VERIFY(type == OBJT_DEFAULT);

	vmo = kmem_alloc(sizeof (*vmo), KM_SLEEP);
	mutex_init(&vmo->vmo_lock, NULL, MUTEX_DEFAULT, NULL);

	/* For now, these are to stay fixed after allocation */
	vmo->vmo_type = type;
	vmo->vmo_size = size;

	vmo->vmo_refcnt = 0;
	vmo->vmo_amp = anonmap_alloc(size, 0, ANON_SLEEP);
	/* XXXJOY: opt-in to larger pages? */

	return (vmo);
}

void
vm_object_deallocate(vm_object_t vmo)
{
	struct anon_map *amp = vmo->vmo_amp;

	/* only support anon for now */
	VERIFY(vmo->vmo_type == OBJT_DEFAULT);

	/* By the time this is deallocated, its mappings should be gone. */
	VERIFY(vmo->vmo_refcnt == 0);

	ANON_LOCK_ENTER(&amp->a_rwlock, RW_WRITER);
	amp->refcnt--;
	if (amp->refcnt != 0) {
		/*
		 * It is possible that userspace still has lingering mappings
		 * to the guest memory.  In such cases, clean-up of the anon
		 * resources is left to the consuming segment(s).
		 */
		ANON_LOCK_EXIT(&amp->a_rwlock);
	} else {
		/* Release anon memory */
		anonmap_purge(amp);
		if (amp->a_szc != 0) {
			anon_shmap_free_pages(amp, 0, amp->size);
		} else {
			anon_free(amp->ahp, 0, amp->size);
		}
		ANON_LOCK_EXIT(&amp->a_rwlock);

		anonmap_free(amp);
	}
	vmo->vmo_amp = NULL;
	kmem_free(vmo, sizeof (*vmo));
}

void
vm_object_reference(vm_object_t vmo)
{
	ASSERT(vmo != NULL);

	mutex_enter(&vmo->vmo_lock);
	vmo->vmo_refcnt++;
	mutex_exit(&vmo->vmo_lock);
}

static void
vm_object_release(vm_object_t vmo)
{
	ASSERT(vmo != NULL);

	mutex_enter(&vmo->vmo_lock);
	VERIFY(vmo->vmo_refcnt);
	vmo->vmo_refcnt--;
	mutex_exit(&vmo->vmo_lock);
}


int
vm_fault(vm_map_t map, vm_offset_t off, vm_prot_t type, int flag)
{
	struct vmspace *vms = VMMAP_TO_VMSPACE(map);
	const caddr_t addr = (caddr_t)off;
	vmspace_mapping_t *vmsm;
	struct as *as = vms->vms_as;
	struct seg *seg;
	uint_t prot;
	int err = 0;

	mutex_enter(&vms->vms_lock);

	if (vmspace_pmap_iswired(vms, addr, &prot) == 0) {
		/* The page is wired up, perhaps the prot is wrong? */
		if ((prot & type) != type) {
			mutex_exit(&vms->vms_lock);
			return (EFAULT);
		}
		panic("unexpected vm_fault %p %x %x", addr, type, prot);
		/*NOTREACHED*/
	}

	/* Try to wire up the address */
	if ((vmsm = vm_mapping_find(vms, addr, 0)) == NULL) {
		mutex_exit(&vms->vms_lock);
		return (ENOENT);
	}
	type = vmsm->vmsm_object->vmo_type;
	as = vms->vms_as;

	AS_LOCK_ENTER(as, RW_READER);
	VERIFY(seg = as_segat(as, addr));

	switch (type) {
	case OBJT_DEFAULT: {
		struct anon_map *amp = vmsm->vmsm_object->vmo_amp;
		struct segvn_data *svd;
		/* XXXJOY: punt on large pages for now */
		const size_t align_size = PAGESIZE;
		page_t *pp[1];
		const caddr_t align_addr = (caddr_t)P2ALIGN((uintptr_t)addr,
		    align_size);
		const ulong_t idx = btop(vmsm->vmsm_offset +
		    (align_addr - vmsm->vmsm_addr));
		cred_t *cr = CRED();

		VERIFY(seg->s_ops == &segvn_ops);
		svd = (struct segvn_data *)seg->s_data;
		VERIFY(svd->amp == amp);
		prot = svd->prot;

		/*
		 * This conveniently grabs an SE_SHARED lock on all the pages
		 * in question.
		 */
		err = anon_map_createpages(amp, idx, align_size, pp, seg,
		    align_addr, S_CREATE, cr);
		if (err == 0) {
			/*
			 * If pmap failure is to be handled, the previously
			 * acquired page locks would need to be released.
			 */
			VERIFY0(vmspace_pmap_wire(vms, align_addr, align_size,
			    pp, prot));
		}
	}
		break;
	default:
		panic("unsupported object type: %x", type);
		/* NOTREACHED */
		break;
	}

	AS_LOCK_EXIT(as);
	mutex_exit(&vms->vms_lock);
	return (0);
}

int
vm_fault_quick_hold_pages(vm_map_t map, vm_offset_t addr, vm_size_t len,
    vm_prot_t prot, vm_page_t *ma, int max_count)
{
	struct vmspace *vms = VMMAP_TO_VMSPACE(map);
	struct as *as = vms->vms_as;
	const caddr_t vaddr = (caddr_t)addr;
	pfn_t pfn;
	uint_t pprot;
	page_t *pp = NULL;

	ASSERT(len == PAGESIZE);
	ASSERT(max_count == 1);

	mutex_enter(&vms->vms_lock);
	/* Must be within the bounds of the vmspace */
	if (addr >= vms->vms_size) {
		mutex_exit(&vms->vms_lock);
		return (-1);
	}

	AS_LOCK_ENTER(as, RW_READER);
	pfn = hat_getpfnum(as->a_hat, vaddr);
	if (pf_is_memory(pfn) && hat_getattr(as->a_hat, vaddr, &pprot) == 0) {
		/* The page is expected to be share-locked already */
		pp = page_numtopp_nolock(pfn);
		if (pp == NULL || (prot & ~pprot) == 0 ||
		    page_lock(pp, SE_SHARED, (kmutex_t *)NULL,
		    P_RECLAIM) == 0) {
			pp = NULL;
		}
	}
	AS_LOCK_EXIT(as);
	mutex_exit(&vms->vms_lock);

	if (pp != NULL) {
		*ma = (vm_page_t)pp;
		return (0);
	}
	return (-1);
}

/*
 * Find a suitable location for a mapping (and install it).
 */
int
vm_map_find(vm_map_t map, vm_object_t vmo, vm_ooffset_t off, vm_offset_t *addr,
    vm_size_t len, vm_offset_t max_addr, int find_flags, vm_prot_t prot,
    vm_prot_t prot_max, int cow)
{
	struct vmspace *vms = VMMAP_TO_VMSPACE(map);
	struct as *as = vms->vms_as;
#if notyet
	const size_t slen = (size_t)len;
#endif
	caddr_t base = (caddr_t)*addr;
	size_t maxlen;
	struct segvn_crargs svna;
	vmspace_mapping_t *vmsm;
	int res;

	/* For use in vmm only */
	VERIFY(find_flags == VMFS_NO_SPACE); /* essentially MAP_FIXED */
	VERIFY(vmo->vmo_type == OBJT_DEFAULT); /* only anon for now */

	if (len == 0 || off >= (off + len) || vmo->vmo_size < (off + len)) {
		return (EINVAL);
	}

	maxlen = (max_addr != 0) ? max_addr : vms->vms_size;
	if (*addr >= maxlen) {
		return (ENOMEM);
	} else {
		maxlen -= *addr;
	}

	vmsm = kmem_alloc(sizeof (*vmsm), KM_SLEEP);

	mutex_enter(&vms->vms_lock);
	as_rangelock(as);
	if (as_gap(as, len, &base, &maxlen, AH_LO, NULL) != 0) {
		res = ENOMEM;
		goto out;
	}

	/* XXX: verify offset/len fits in object */

	svna.vp = NULL;
	svna.offset = (off_t)off;
	svna.type = MAP_SHARED;
	svna.prot = (prot & VM_PROT_ALL) | PROT_USER;
	svna.maxprot = (prot_max & VM_PROT_ALL) | PROT_USER;
	svna.flags = MAP_ANON;
	svna.cred = CRED(); /* XXXJOY: really? */
	svna.amp = vmo->vmo_amp;
	svna.szc = 0;
	svna.lgrp_mem_policy_flags = 0;

	res = as_map(as, base, len, segvn_create, &svna);

	if (res == 0) {
		vmsm->vmsm_object = vmo;
		vmsm->vmsm_addr = base;
		vmsm->vmsm_len = len;
		vmsm->vmsm_offset = (off_t)off;
		list_insert_tail(&vms->vms_maplist, vmsm);

		/* Communicate out the chosen address. */
		*addr = (vm_offset_t)base;
	}
out:
	as_rangeunlock(as);
	mutex_exit(&vms->vms_lock);
	if (res != 0) {
		kmem_free(vmsm, sizeof (*vmsm));
	}
	return (res);
}

static vmspace_mapping_t *
vm_mapping_find(struct vmspace *vms, caddr_t addr, size_t size)
{
	vmspace_mapping_t *vmsm;
	list_t *ml = &vms->vms_maplist;

	ASSERT(MUTEX_HELD(&vms->vms_lock));

	for (vmsm = list_head(ml); vmsm != NULL; vmsm = list_next(ml, vmsm)) {
		if (addr >= vmsm->vmsm_addr &&
		    addr < (vmsm->vmsm_addr + vmsm->vmsm_len)) {
			break;
		}
	}

	/* If size is specified, both length and address must match exactly */
	if (size != 0) {
		if (vmsm->vmsm_len != size || vmsm->vmsm_addr != addr) {
			vmsm = NULL;
		}
	}

	return (vmsm);
}

static void
vm_mapping_remove(struct vmspace *vms, vmspace_mapping_t *vmsm)
{
	list_t *ml = &vms->vms_maplist;

	ASSERT(MUTEX_HELD(&vms->vms_lock));

	list_remove(ml, vmsm);
	vm_object_release(vmsm->vmsm_object);
	kmem_free(vmsm, sizeof (*vmsm));
}

int
vm_map_remove(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	struct vmspace *vms = VMMAP_TO_VMSPACE(map);
	const caddr_t addr = (caddr_t)start;
	const size_t size = (size_t)(end - start);
	vmspace_mapping_t *vmsm;
	objtype_t type;
	int err = 0;

	ASSERT(start < end);

	mutex_enter(&vms->vms_lock);
	if ((vmsm = vm_mapping_find(vms, addr, size)) == NULL) {
		mutex_exit(&vms->vms_lock);
		return (ENOENT);
	}

	err = as_unmap(vms->vms_as, addr, size);
	if (err != 0) {
		mutex_exit(&vms->vms_lock);
		return (err);
	}

	type = vmsm->vmsm_object->vmo_type;
	switch (type) {
	case OBJT_DEFAULT: {
		VERIFY0(vmspace_pmap_unmap(vms, addr, size));
	}
		break;
	default:
		panic("unsupported object type: %x", type);
		/* NOTREACHED */
		break;
	}

	vm_mapping_remove(vms, vmsm);
	mutex_exit(&vms->vms_lock);
	return (0);
}

int
vm_map_wire(vm_map_t map, vm_offset_t start, vm_offset_t end, int flags)
{
	struct vmspace *vms = VMMAP_TO_VMSPACE(map);
	const caddr_t addr = (caddr_t)start;
	const size_t size = (size_t)(end - start);
	vmspace_mapping_t *vmsm;
	struct as *as;
	struct seg *seg;
	cred_t *cr = CRED();
	objtype_t type;
	int err;

	if (((uintptr_t)addr & PAGEOFFSET) != 0 ||
	    (size & PAGEOFFSET) != 0 ||
	    size < PAGESIZE) {
		return (EINVAL);
	}

	mutex_enter(&vms->vms_lock);
	if ((vmsm = vm_mapping_find(vms, addr, size)) == NULL) {
		mutex_exit(&vms->vms_lock);
		return (ENOENT);
	}
	type = vmsm->vmsm_object->vmo_type;
	as = vms->vms_as;

	AS_LOCK_ENTER(as, RW_READER);
	VERIFY(seg = as_segat(as, addr));

	switch (type) {
	case OBJT_DEFAULT: {
		struct anon_map *amp = vmsm->vmsm_object->vmo_amp;
		struct segvn_data *svd;
		const ulong_t idx = btop(vmsm->vmsm_offset);
		const size_t arr_sz = sizeof (page_t *) * btop(size);
		page_t **pp;
		uint_t prot;

		VERIFY(seg->s_ops == &segvn_ops);
		svd = (struct segvn_data *)seg->s_data;
		VERIFY(svd->amp == amp);
		prot = svd->prot;

		pp = kmem_alloc(arr_sz, KM_SLEEP);
		/*
		 * This conveniently grabs an SE_SHARED lock on all the pages
		 * in question.
		 */
		err = anon_map_createpages(amp, idx, size, pp, seg, addr,
		    S_CREATE, cr);
		if (err == 0) {
			/*
			 * If pmap failure is to be handled, the previously
			 * acquired page locks would need to be released.
			 */
			VERIFY0(vmspace_pmap_wire(vms, addr, size, pp, prot));
		}
		kmem_free(pp, arr_sz);
	}
		break;
	default:
		panic("unsupported object type: %x", type);
		/* NOTREACHED */
		break;
	}

	AS_LOCK_EXIT(as);
	mutex_exit(&vms->vms_lock);

	return (err);
}

int
vm_map_user(struct vmspace *vms, off_t off, struct as *as, caddr_t *addrp,
    off_t len, uint_t prot, uint_t maxprot, uint_t flags, cred_t *cr)
{
	int err = 0;
	const caddr_t poff = (caddr_t)(uintptr_t)off;
	const size_t size = (size_t)len;
	vmspace_mapping_t *vmsm;
	vm_object_t vmo;

	if (off < 0 || len <= 0) {
		return (EINVAL);
	}

	mutex_enter(&vms->vms_lock);

	if ((vmsm = vm_mapping_find(vms, poff, size)) == NULL) {
		mutex_exit(&vms->vms_lock);
		return (ENOENT);
	}
	vmo = vmsm->vmsm_object;
	if (vmo->vmo_type != OBJT_DEFAULT) {
		/* Only support sysmem for now */
		mutex_exit(&vms->vms_lock);
		return (ENODEV);
	}

	ASSERT(as != vms->vms_as);
	as_rangelock(as);

	err = choose_addr(as, addrp, len, off, ADDR_VACALIGN, flags);
	if (err == 0) {
		struct segvn_crargs svna;

		ASSERT(off >= vmsm->vmsm_offset);

		svna.vp = NULL;
		svna.offset = (off - vmsm->vmsm_offset);
		svna.type = MAP_SHARED;
		svna.prot = prot;
		svna.maxprot = maxprot;
		svna.flags = MAP_ANON;
		svna.cred = cr;
		svna.amp = vmo->vmo_amp;
		svna.szc = 0;
		svna.lgrp_mem_policy_flags = 0;

		err = as_map(as, *addrp, size, segvn_create, &svna);
	}

	as_rangeunlock(as);
	mutex_exit(&vms->vms_lock);
	return (err);
}

/* Provided custom for 'devmem' segment mapping */
int
vm_map_user_obj(struct vmspace *vms, vm_object_t vmo, struct as *as,
    caddr_t *addrp, uint_t prot, uint_t maxprot, uint_t flags, cred_t *cr)
{
	size_t len = vmo->vmo_size;
	off_t off = 0;
	int err;

	if (vmo->vmo_type != OBJT_DEFAULT) {
		return (ENODEV);
	}

	mutex_enter(&vms->vms_lock);
	as_rangelock(as);

	err = choose_addr(as, addrp, len, off, ADDR_VACALIGN, flags);
	if (err == 0) {
		struct segvn_crargs svna;

		svna.vp = NULL;
		svna.offset = off;
		svna.type = MAP_SHARED;
		svna.prot = prot;
		svna.maxprot = maxprot;
		svna.flags = MAP_ANON;
		svna.cred = cr;
		svna.amp = vmo->vmo_amp;
		svna.szc = 0;
		svna.lgrp_mem_policy_flags = 0;

		err = as_map(as, *addrp, len, segvn_create, &svna);
	}

	as_rangeunlock(as);
	mutex_exit(&vms->vms_lock);
	return (err);
}

void
vm_page_lock(vm_page_t vmp)
{
	/*EMPTY*/
}

void
vm_page_unlock(vm_page_t vmp)
{
	/*EMPTY*/
}

void
vm_page_unhold(vm_page_t vmp)
{
	page_t *pp = (page_t *)vmp;

	ASSERT(PAGE_SHARED(pp));

	page_unlock(pp);
}
