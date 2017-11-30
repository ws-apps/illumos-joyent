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
#ifndef _COMPAT_FREEBSD_AMD64_MACHINE_SMP_H_
#define	_COMPAT_FREEBSD_AMD64_MACHINE_SMP_H_

#ifdef _KERNEL

/*
 * APIC-related definitions would normally be stored in x86/include/apicvar.h,
 * accessed here via x86/include/x86_smp.h.  Until it becomes necessary to
 * implment that whole chain of includes, those definitions are short-circuited
 * into this file.
 */

#define	IDTVEC(name)	idtvec_ ## name

extern int idtvec_justreturn;

extern int lapic_ipi_alloc(int *);
extern void lapic_ipi_free(int vec);


#endif /* _KERNEL */

#endif	/* _COMPAT_FREEBSD_AMD64_MACHINE_SMP_H_ */
