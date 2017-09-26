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
#ifndef _COMPAT_FREEBSD_SYS_CALLOUT_H_
#define	_COMPAT_FREEBSD_SYS_CALLOUT_H_

#include <sys/cyclic.h>

struct callout {
	cyclic_id_t	c_cyc_id;
	int		c_flags;
	void		(*c_func)(void *);
	void		*c_arg;

};

#define	CALLOUT_ACTIVE		0x0002	/* callout is currently active */
#define	CALLOUT_PENDING		0x0004	/* callout is waiting for timeout */

#define	C_ABSOLUTE		0x0200	/* event time is absolute. */

#define	callout_active(c)	((c)->c_flags & CALLOUT_ACTIVE)
#define	callout_deactivate(c)	((c)->c_flags &= ~CALLOUT_ACTIVE)
#define	callout_pending(c)	((c)->c_flags & CALLOUT_PENDING)

void	vmm_glue_callout_init(struct callout *c, int mpsafe);
int	vmm_glue_callout_reset_sbt(struct callout *c, sbintime_t sbt,
    sbintime_t pr, void (*func)(void *), void *arg, int flags);
int	vmm_glue_callout_stop(struct callout *c);
int	vmm_glue_callout_drain(struct callout *c);

static __inline void
callout_init(struct callout *c, int mpsafe)
{
	vmm_glue_callout_init(c, mpsafe);
}

static __inline int
callout_stop(struct callout *c)
{
	return (vmm_glue_callout_stop(c));
}

static __inline int
callout_drain(struct callout *c)
{
	return (vmm_glue_callout_drain(c));
}

static __inline int
callout_reset_sbt(struct callout *c, sbintime_t sbt, sbintime_t pr,
    void (*func)(void *), void *arg, int flags)
{
	return (vmm_glue_callout_reset_sbt(c, sbt, pr, func, arg, flags));
}


#endif	/* _COMPAT_FREEBSD_SYS_CALLOUT_H_ */
