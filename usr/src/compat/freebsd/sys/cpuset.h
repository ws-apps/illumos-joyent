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
#ifndef _COMPAT_FREEBSD_SYS_CPUSET_H_
#define	_COMPAT_FREEBSD_SYS_CPUSET_H_

#define	NOCPU			-1

#ifdef	_KERNEL
#define	CPU_SET(cpu, set)		CPUSET_ADD(*(set), cpu)
#define	CPU_SETOF(cpu, set)		CPUSET_ONLY(*(set), cpu)
#define	CPU_ZERO(set)			CPUSET_ZERO(*(set))
#define	CPU_CLR(cpu, set)		CPUSET_DEL(*(set), cpu)
#define	CPU_FFS(set)			cpusetobj_ffs(set)
#define	CPU_ISSET(cpu, set)		CPU_IN_SET(*(set), cpu)
#define	CPU_CMP(set1, set2)		CPUSET_ISEQUAL(*(set1), *(set2))
#define	CPU_SET_ATOMIC(cpu, set)	CPUSET_ATOMIC_ADD(*(set), cpu)

#include <sys/cpuvar.h>

int	cpusetobj_ffs(const cpuset_t *set);
#else
#include <machine/atomic.h>

typedef int cpuset_t;

#define	CPUSET(cpu)			(1UL << (cpu))

#define	CPU_SET_ATOMIC(cpu, set)	atomic_set_int((set), CPUSET(cpu))
#endif

#endif	/* _COMPAT_FREEBSD_SYS_CPUSET_H_ */
