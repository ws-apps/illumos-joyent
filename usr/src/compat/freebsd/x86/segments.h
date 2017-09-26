/*
 * COPYRIGHT 2015 Pluribus Networks Inc.
 *
 * All rights reserved. This copyright notice is Copyright Management
 * Information under 17 USC 1202 and is included to protect this work and
 * deter copyright infringement.  Removal or alteration of this Copyright
 * Management Information without the express written permission from
 * Pluribus Networks Inc is prohibited, and any such unauthorized removal
 * or alteration will be a violation of federal law.
 */
#ifndef _COMPAT_FREEBSD_X86_SEGMENTS_H_
#define	_COMPAT_FREEBSD_X86_SEGMENTS_H_

/*
 * Entries in the Interrupt Descriptor Table (IDT)
 */
#define	IDT_BP		3	/* #BP: Breakpoint */
#define	IDT_UD		6	/* #UD: Undefined/Invalid Opcode */
#define	IDT_SS		12	/* #SS: Stack Segment Fault */
#define	IDT_GP		13	/* #GP: General Protection Fault */
#define	IDT_AC		17	/* #AC: Alignment Check */

#endif	/* _COMPAT_FREEBSD_AMD64_MACHINE_SEGMENTS_H_ */
