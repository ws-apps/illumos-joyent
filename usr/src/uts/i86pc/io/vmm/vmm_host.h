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
/*-
 * Copyright (c) 2012 NetApp, Inc.
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
 * $FreeBSD: head/sys/amd64/vmm/vmm_host.h 242275 2012-10-29 01:51:24Z neel $
 */

#ifndef	_VMM_HOST_H_
#define	_VMM_HOST_H_

#ifndef	__FreeBSD__
#include <sys/cpuvar.h>
#endif

#ifndef	_KERNEL
#error "no user-servicable parts inside"
#endif

void vmm_host_state_init(void);

uint64_t vmm_get_host_pat(void);
uint64_t vmm_get_host_efer(void);
uint64_t vmm_get_host_cr0(void);
uint64_t vmm_get_host_cr4(void);
uint64_t vmm_get_host_datasel(void);
uint64_t vmm_get_host_codesel(void);
uint64_t vmm_get_host_tsssel(void);
uint64_t vmm_get_host_fsbase(void);
uint64_t vmm_get_host_idtrbase(void);

/*
 * Inline access to host state that is used on every VM entry
 */
static __inline uint64_t
vmm_get_host_trbase(void)
{

#ifdef	__FreeBSD__
	return ((uint64_t)PCPU_GET(tssp));
#else
	return ((u_long)CPU->cpu_tss);
#endif
}

static __inline uint64_t
vmm_get_host_gdtrbase(void)
{

#ifdef	__FreeBSD__
	return ((uint64_t)&gdt[NGDT * curcpu]);
#else
	desctbr_t gdtr;

	rd_gdtr(&gdtr);
	return (gdtr.dtr_base);
#endif
}

struct pcpu;
extern struct pcpu __pcpu[];

static __inline uint64_t
vmm_get_host_gsbase(void)
{

#ifdef	__FreeBSD__
	return ((uint64_t)&__pcpu[curcpu]);
#else
	return (rdmsr(MSR_GSBASE));
#endif
}

#ifndef	__FreeBSD__
static __inline uint64_t
vmm_get_host_fssel(void)
{
	return (KFS_SEL);
}

static __inline uint64_t
vmm_get_host_gssel(void)
{
	return (KGS_SEL);
}
#endif
#endif
