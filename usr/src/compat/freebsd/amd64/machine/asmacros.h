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
#ifndef _COMPAT_FREEBSD_AMD64_MACHINE_ASMACROS_H_
#define	_COMPAT_FREEBSD_AMD64_MACHINE_ASMACROS_H_

#define	ENTRY(x) \
	.text; .p2align 4,0x90; \
	.globl  x; \
	.type   x, @function; \
x:

#define	END(x) \
	.size x, [.-x]

#define	ALIGN_TEXT \
	.p2align 4,0x90; /* 16-byte alignment, nop filled */

#endif	/* _COMPAT_FREEBSD_AMD64_MACHINE_ASMACROS_H_ */
