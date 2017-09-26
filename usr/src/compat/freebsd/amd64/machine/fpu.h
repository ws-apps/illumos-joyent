/*
 * COPYRIGHT 2014 Pluribus Networks Inc.
 *
 * All rights reserved. This copyright notice is Copyright Management
 * Information under 17 USC 1202 and is included to protect this work and
 * deter copyright infringement.  Removal or alteration of this Copyright
 * Management Information without the express written permission from
 * Pluribus Networks Inc is prohibited, and any such unauthorized removal
 * or alteration will be a violation of federal law.
 */
#ifndef _COMPAT_FREEBSD_AMD64_MACHINE_FPU_H_
#define	_COMPAT_FREEBSD_AMD64_MACHINE_FPU_H_

#define	XSAVE_AREA_ALIGN	64

void	fpuexit(kthread_t *td);
void	fpurestore(void *);
void	fpusave(void *);

struct savefpu	*fpu_save_area_alloc(void);
void	fpu_save_area_free(struct savefpu *fsa);
void	fpu_save_area_reset(struct savefpu *fsa);

#endif	/* _COMPAT_FREEBSD_AMD64_MACHINE_FPU_H_ */
