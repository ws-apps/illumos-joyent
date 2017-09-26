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
#ifndef _COMPAT_FREEBSD_AMD64_MACHINE_MD_VAR_H_
#define	_COMPAT_FREEBSD_AMD64_MACHINE_MD_VAR_H_

extern  u_int   cpu_high;		/* Highest arg to CPUID */
extern	u_int	cpu_exthigh;		/* Highest arg to extended CPUID */
extern	u_int	cpu_id;			/* Stepping ID */
extern	char	cpu_vendor[];		/* CPU Origin code */

#endif	/* _COMPAT_FREEBSD_AMD64_MACHINE_MD_VAR_H_ */
