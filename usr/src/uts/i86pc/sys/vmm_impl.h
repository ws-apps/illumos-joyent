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
#ifndef _VMM_IMPL_H_
#define _VMM_IMPL_H_

#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/varargs.h>

/*
 * /dev names:
 *      /dev/vmmctl         - control device
 *      /dev/vmm/<name>     - vm devices
 */
#define	VMM_DRIVER_NAME		"vmm"

#define	VMM_CTL_MINOR_NODE	"ctl"
#define	VMM_CTL_MINOR_NAME	VMM_DRIVER_NAME VMM_CTL_NODE
#define	VMM_CTL_MINOR		0

#define	VMM_IOC_BASE		(('V' << 16) | ('M' << 8))

#define	VMM_CREATE_VM		(VMM_IOC_BASE | 0x01)
#define	VMM_DESTROY_VM		(VMM_IOC_BASE | 0x02)

struct vmm_ioctl {
	char vmm_name[VM_MAX_NAMELEN];
};

#ifdef	_KERNEL
struct vmm_softc {
	boolean_t			open;
	minor_t				minor;
	struct vm			*vm;
	char				name[VM_MAX_NAMELEN];
	SLIST_ENTRY(vmm_softc)		link;
};
#endif

/*
 * VMM trace ring buffer constants
 */
#define	VMM_DMSG_RING_SIZE		0x100000	/* 1MB */
#define	VMM_DMSG_BUF_SIZE		256

/*
 * VMM trace ring buffer content
 */
typedef struct vmm_trace_dmsg {
	timespec_t		timestamp;
	char			buf[VMM_DMSG_BUF_SIZE];
	struct vmm_trace_dmsg	*next;
} vmm_trace_dmsg_t;

/*
 * VMM trace ring buffer header
 */
typedef struct vmm_trace_rbuf {
	kmutex_t		lock;		/* lock to avoid clutter */
	int			looped;		/* completed ring */
	int			allocfailed;	/* dmsg mem alloc failed */
	size_t			size;		/* current size */
	size_t			maxsize;	/* max size */
	vmm_trace_dmsg_t	*dmsgh;		/* messages head */
	vmm_trace_dmsg_t	*dmsgp;		/* ptr to last message */
} vmm_trace_rbuf_t;

/*
 * VMM trace ring buffer interfaces
 */
void vmm_trace_log(const char *fmt, ...);

#endif	/* _VMM_IMPL_H_ */
