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
#ifndef _COMPAT_FREEBSD_SYS_MALLOC_H_
#define	_COMPAT_FREEBSD_SYS_MALLOC_H_

/*
 * flags to malloc.
 */
#define	M_NOWAIT	0x0001		/* do not block */
#define	M_WAITOK	0x0002		/* ok to block */
#define	M_ZERO		0x0100		/* bzero the allocation */

struct malloc_type {
	const char	*ks_shortdesc;	/* Printable type name. */
};

#ifdef	_KERNEL
#define	MALLOC_DEFINE(type, shortdesc, longdesc)			\
	struct malloc_type type[1] = {					\
		{ shortdesc }						\
	}

#define	MALLOC_DECLARE(type)						\
	extern struct malloc_type type[1]

void	free(void *addr, struct malloc_type *type);
void	*malloc(unsigned long size, struct malloc_type *type, int flags);
void	*old_malloc(unsigned long size, struct malloc_type *type , int flags);
void	*contigmalloc(unsigned long, struct malloc_type *, int, vm_paddr_t,
    vm_paddr_t, unsigned long, vm_paddr_t);
void	contigfree(void *, unsigned long, struct malloc_type *);


#endif	/* _KERNEL */

#endif	/* _COMPAT_FREEBSD_SYS_MALLOC_H_ */
