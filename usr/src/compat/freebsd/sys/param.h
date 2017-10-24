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
#ifndef _COMPAT_FREEBSD_SYS_PARAM_H_
#define	_COMPAT_FREEBSD_SYS_PARAM_H_

#ifndef	_KERNEL
#define	MAXCOMLEN	16
#endif
#define	MAXHOSTNAMELEN	256
#define	SPECNAMELEN	63

#ifdef	_KERNEL
#include <sys/time.h>

#ifndef	FALSE
#define	FALSE	0
#endif
#ifndef	TRUE
#define	TRUE	1
#endif
#endif

#include <machine/param.h>

#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#define	rounddown(x,y)	(((x)/(y))*(y))
#define	rounddown2(x, y) ((x)&(~((y)-1)))   /* if y is power of two */
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))  /* to any y */
#define	roundup2(x,y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#define	powerof2(x)	((((x)-1)&(x))==0)

/* Macros for min/max. */
#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))

#define	trunc_page(x)	((unsigned long)(x) & ~(PAGE_MASK))
#define	ptoa(x)		((unsigned long)(x) << PAGE_SHIFT)

#include_next <sys/param.h>

#endif	/* _COMPAT_FREEBSD_SYS_PARAM_H_ */
