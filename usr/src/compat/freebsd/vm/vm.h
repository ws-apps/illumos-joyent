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
#ifndef _FREEBSD_VM_VM_H_
#define	_FREEBSD_VM_VM_H_

#include <machine/vm.h>

typedef u_char vm_prot_t;

#define	VM_PROT_NONE		((vm_prot_t) 0x00)
#define	VM_PROT_READ		((vm_prot_t) 0x01)
#define	VM_PROT_WRITE		((vm_prot_t) 0x02)
#define	VM_PROT_EXECUTE		((vm_prot_t) 0x04)

#define	VM_PROT_ALL		(VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE)
#define	VM_PROT_RW		(VM_PROT_READ|VM_PROT_WRITE)

/*
 * <sys/promif.h> contains a troublesome preprocessor define for BYTE.
 * Do this ugly workaround to avoid it.
 */
#define	_SYS_PROMIF_H
#include <vm/hat_i86.h>
#undef	_SYS_PROMIF_H

#endif	/* _FREEBSD_VM_VM_H_ */
