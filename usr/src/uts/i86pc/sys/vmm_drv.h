/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2017 Joyent, Inc.
 */

#ifndef _VMM_DRV_H_
#define	_VMM_DRV_H_

#ifdef	_KERNEL
struct vmm_hold;
typedef struct vmm_hold vmm_hold_t;

extern int vmm_drv_hold(file_t *, cred_t *, vmm_hold_t **);
extern void vmm_drv_rele(vmm_hold_t *);
extern boolean_t vmm_drv_expired(vmm_hold_t *);
extern void *vmm_drv_gpa2kva(vmm_hold_t *, uintptr_t, size_t);
#endif /* _KERNEL */

#endif /* _VMM_DRV_H_ */
