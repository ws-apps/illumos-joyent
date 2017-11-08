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
#ifndef _COMPAT_FREEBSD_AMD64_MACHINE_VMPARAM_H_
#define	_COMPAT_FREEBSD_AMD64_MACHINE_VMPARAM_H_

extern caddr_t kpm_vbase;
extern size_t kpm_size;

#define	PHYS_TO_DMAP(x)	({ 			\
	ASSERT((uintptr_t)(x) < kpm_size);	\
	(uintptr_t)(x) | (uintptr_t)kpm_vbase; })

#define	DMAP_TO_PHYS(x)	({				\
	ASSERT((uintptr_t)(x) >= (uintptr_t)kpm_vbase);		\
	ASSERT((uintptr_t)(x) < ((uintptr_t)kpm_vbase + kpm_size));	\
	(uintptr_t)(x) & ~(uintptr_t)kpm_vbase; })	\


#endif	/* _COMPAT_FREEBSD_AMD64_MACHINE_VMPARAM_H_ */
