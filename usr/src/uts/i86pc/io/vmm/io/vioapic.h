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
/*-
 * Copyright (c) 2013 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VIOAPIC_H_
#define	_VIOAPIC_H_

#define	VIOAPIC_BASE	0xFEC00000
#define	VIOAPIC_SIZE	4096

struct vioapic *vioapic_init(struct vm *vm);
void	vioapic_cleanup(struct vioapic *vioapic);

int	vioapic_assert_irq(struct vm *vm, int irq);
int	vioapic_deassert_irq(struct vm *vm, int irq);
int	vioapic_pulse_irq(struct vm *vm, int irq);

int	vioapic_mmio_write(void *vm, int vcpuid, uint64_t gpa,
	    uint64_t wval, int size, void *arg);
int	vioapic_mmio_read(void *vm, int vcpuid, uint64_t gpa,
	    uint64_t *rval, int size, void *arg);

int	vioapic_pincount(struct vm *vm);
void	vioapic_process_eoi(struct vm *vm, int vcpuid, int vector);
#endif
