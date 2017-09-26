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
 * Copyright (c) 2011 NetApp, Inc.
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
 * $FreeBSD: head/sys/amd64/vmm/vmm_mem.c 245678 2013-01-20 03:42:49Z neel $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/amd64/vmm/vmm_mem.c 245678 2013-01-20 03:42:49Z neel $");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <machine/pmap.h>

#include <sys/ddi.h>

#include "vmm_util.h"
#include "vmm_mem.h"

int
vmm_mem_init(void)
{
	return (0);
}

vm_paddr_t
vmm_mem_alloc(size_t size)
{
	clock_t usec = 2 * 1000000;
	vm_paddr_t pa;
	caddr_t addr;

	if (size != PAGE_SIZE)
		panic("vmm_mem_alloc: invalid allocation size %lu", size);

	while (usec > 0) {
		if ((addr = kmem_zalloc(PAGE_SIZE, KM_NOSLEEP)) != NULL) {
			ASSERT(((uintptr_t)addr & PAGE_MASK) == 0);
			pa = vtophys((vm_offset_t)addr);
			return (pa);
		}
		delay(drv_usectohz((clock_t)500000));
		usec -= 500000;
	}

	return (NULL);
}

void
vmm_mem_free(vm_paddr_t base, size_t length)
{
	page_t	*pp;

	if (base & PAGE_MASK) {
		panic("vmm_mem_free: base 0x%0lx must be aligned on a "
		      "0x%0x boundary\n", base, PAGE_SIZE);
	}

	if (length != PAGE_SIZE) {
		panic("vmm_mem_free: invalid length %lu", length);
	}

	pp = page_numtopp_nolock(btop(base));
	kmem_free((void *)pp->p_offset, PAGE_SIZE);
}

vm_paddr_t
vmm_mem_maxaddr(void)
{

	return (ptob(physmax + 1));
}
