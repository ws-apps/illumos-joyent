/*
 * COPYRIGHT 2013 Pluribus Networks Inc.
 *
 * All rights reserved. This copyright notice is Copyright Management
 * Information under 17 USC 1202 and is included to protect this work and
 * deter copyright infringement.  Removal or alteration of this Copyright
 * Management Information without the express written permission from
 * Pluribus Networks Inc is prohibited, and any such unauthorized removal
 * or alteration will be a violation of federal law.
 */
#ifndef _COMPAT_FREEBSD_AMD64_MACHINE_PARAM_H_
#define	_COMPAT_FREEBSD_AMD64_MACHINE_PARAM_H_

#ifdef	_KERNEL
#define	MAXCPU		NCPU
#endif	/* _KERNEL */

#define	PAGE_SHIFT	12		/* LOG2(PAGE_SIZE) */
#define	PAGE_SIZE	(1<<PAGE_SHIFT)	/* bytes/page */
#define	PAGE_MASK	(PAGE_SIZE-1)

/* Size of the level 1 page table units */
#define	NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

/* Size of the level 2 page directory units */
#define	NPDEPG		(PAGE_SIZE/(sizeof (pd_entry_t)))

/* Size of the level 3 page directory pointer table units */
#define	NPDPEPG		(PAGE_SIZE/(sizeof (pdp_entry_t)))

/* Size of the level 4 page-map level-4 table units */
#define	NPML4EPG	(PAGE_SIZE/(sizeof (pml4_entry_t)))

#endif	/* _COMPAT_FREEBSD_AMD64_MACHINE_PARAM_H_ */
