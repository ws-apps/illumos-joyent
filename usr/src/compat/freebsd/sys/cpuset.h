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

#include <sys/_cpuset.h>

#define	CPU_SET(cpu, set)		cpuset_add((set), (cpu))
#define	CPU_SETOF(cpu, set)		cpuset_only((set), (cpu))
#define	CPU_ZERO(set)			cpuset_zero((cpuset_t *)(set))
#define	CPU_CLR(cpu, set)		cpuset_del((set), (cpu))
#define	CPU_FFS(set)			cpusetobj_ffs(set)
#define	CPU_ISSET(cpu, set)		cpu_in_set((cpuset_t *)(set), (cpu))
#define	CPU_AND(dst, src)		cpuset_and(			\
						(cpuset_t *)(dst),	\
						(cpuset_t *)(src))
#define	CPU_CMP(set1, set2)		(cpuset_isequal(		\
						(cpuset_t *)(set1),	\
						(cpuset_t *)(set2)) == 0)
#define	CPU_SET_ATOMIC(cpu, set)	cpuset_atomic_add(		\
						(cpuset_t *)(set),	\
						(cpu))
#define	CPU_CLR_ATOMIC(cpu, set)	cpuset_atomic_del(		\
						(cpuset_t *)(set),	\
						(cpu))

/* XXXJOY: The _ACQ variants appear to imply a membar too. Is that an issue? */
#define	CPU_SET_ATOMIC_ACQ(cpu, set)	cpuset_atomic_add((set), (cpu))


int	cpusetobj_ffs(const cpuset_t *set);

#else
#include <machine/atomic.h>

typedef int cpuset_t;

#define	CPUSET(cpu)			(1UL << (cpu))

#define	CPU_SETSIZE			(sizeof (cpuset_t))
#define	CPU_SET_ATOMIC(cpu, set)	atomic_set_int((u_int *)(set), CPUSET(cpu))
#define	CPU_CLR_ATOMIC(cpu, set)	atomic_clear_int((u_int *)(set), CPUSET(cpu))
#define	CPU_ISSET(cpu, set)		((*(set) & CPUSET(cpu)) != 0)
#define	CPU_EMPTY(set)			(*(set) == 0)
#endif

#endif	/* _COMPAT_FREEBSD_SYS_CPUSET_H_ */
