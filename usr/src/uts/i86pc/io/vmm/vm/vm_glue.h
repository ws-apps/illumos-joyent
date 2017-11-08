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

#ifndef	_VM_GLUE_
#define	_VM_GLUE_

#include <vm/pmap.h>
#include <vm/vm.h>
#include <sys/cpuvar.h>

struct vmspace;
struct vm_map;
struct pmap;

struct vm_map {
	struct vmspace *vmm_space;
};

struct pmap {
	void		*pm_pml4;
	cpuset_t	pm_active;
	long		pm_eptgen;

	/* Implementation private */
	enum pmap_type	pm_type;
	void		*pm_map;
};

struct vmspace {
	struct vm_map vm_map;

	/* Implementation private */
	kmutex_t	vms_lock;
	struct pmap	vms_pmap;
	struct as	*vms_as;
	uintptr_t	vms_size;	/* fixed after creation */

	list_t		vms_maplist;
};

struct vm_object {
	kmutex_t	vmo_lock;
	uint_t		vmo_refcnt;
	objtype_t	vmo_type;
	size_t		vmo_size;
	struct anon_map	*vmo_amp;
};

int vm_map_user(struct vmspace *, off_t, struct as *, caddr_t *, off_t, uint_t,
    uint_t, uint_t, cred_t *);
int vm_map_user_obj(struct vmspace *, vm_object_t, struct as *, caddr_t *,
    uint_t, uint_t, uint_t, cred_t *);

#endif /* _VM_GLUE_ */
