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
#ifndef _COMPAT_FREEBSD_PTHREAD_NP_H_
#define	_COMPAT_FREEBSD_PTHREAD_NP_H_

#include <sys/param.h>
#include <sys/cpuset.h>

#include <synch.h>

#define	pthread_set_name_np(thread, name)

#define	pthread_mutex_isowned_np(x)	_mutex_held(x)

#endif	/* _COMPAT_FREEBSD_PTHREAD_NP_H_ */
