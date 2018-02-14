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
#ifndef _COMPAT_FREEBSD_SYS_SYSTM_H_
#define	_COMPAT_FREEBSD_SYS_SYSTM_H_

#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <sys/callout.h>
#include <sys/queue.h>

struct mtx;

#define	KASSERT(exp,msg) do {						\
	if (!(exp))							\
		panic msg;						\
} while (0)

void	critical_enter(void);
void	critical_exit(void);

struct unrhdr *new_unrhdr(int low, int high, struct mtx *mutex);
void delete_unrhdr(struct unrhdr *uh);
int alloc_unr(struct unrhdr *uh);
void free_unr(struct unrhdr *uh, u_int item);

#include <sys/libkern.h>

#include_next <sys/systm.h>
#include <sys/cmn_err.h>

#endif	/* _COMPAT_FREEBSD_SYS_SYSTM_H_ */
