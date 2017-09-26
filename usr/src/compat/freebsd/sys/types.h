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
#ifndef _COMPAT_FREEBSD_SYS_TYPES_H_
#define	_COMPAT_FREEBSD_SYS_TYPES_H_

#include <sys/_types.h>

typedef __uint8_t	u_int8_t;	/* unsigned integrals (deprecated) */
typedef __uint16_t	u_int16_t;
typedef __uint32_t	u_int32_t;
typedef __uint64_t	u_int64_t;

#ifndef	__REGISTER_T_DEFINED
#define	__REGISTER_T_DEFINED
typedef __register_t	register_t;
#endif

#ifndef	__SBINTIME_T_DEFINED
#define	__SBINTIME_T_DEFINED
typedef __int64_t	sbintime_t;
#endif

#ifndef	__VM_MEMATTR_T_DEFINED
#define	__VM_MEMATTR_T_DEFINED
typedef char	vm_memattr_t;
#endif

#ifndef	__VM_OFFSET_T_DEFINED
#define	__VM_OFFSET_T_DEFINED
typedef __vm_offset_t	vm_offset_t;
#endif

#ifndef	__VM_OOFFSET_T_DEFINED
#define	__VM_OOFFSET_T_DEFINED
typedef __vm_ooffset_t	vm_ooffset_t;
#endif

#ifndef	__VM_PADDR_T_DEFINED
#define	__VM_PADDR_T_DEFINED
typedef __vm_paddr_t	vm_paddr_t;
#endif

#ifndef	__VM_MEMATTR_T_DEFINED
#define	__VM_MEMATTR_T_DEFINED
typedef char		vm_memattr_t;
#endif

#ifndef	__bool_true_false_are_defined
#define	__bool_true_false_are_defined	1
#define	false	0
#define	true	1
typedef _Bool bool;
#endif

#if defined(_KERNEL) && !defined(offsetof)
#define	offsetof(s, m)	((size_t)(&(((s *)0)->m)))
#endif

#include_next <sys/types.h>

#endif	/* _COMPAT_FREEBSD_SYS_TYPES_H_ */
