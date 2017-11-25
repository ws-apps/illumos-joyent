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
#ifndef	_VIONA_IO_H_
#define	_VIONA_IO_H_

#define	VNA_IOC			(('V' << 16)|('C' << 8))
#define	VNA_IOC_CREATE		(VNA_IOC | 1)
#define	VNA_IOC_DELETE		(VNA_IOC | 2)
#define	VNA_IOC_RX_RING_INIT	(VNA_IOC | 3)
#define	VNA_IOC_TX_RING_INIT	(VNA_IOC | 4)
#define	VNA_IOC_RX_RING_RESET	(VNA_IOC | 5)
#define	VNA_IOC_TX_RING_RESET	(VNA_IOC | 6)
#define	VNA_IOC_RX_RING_KICK	(VNA_IOC | 7)
#define	VNA_IOC_TX_RING_KICK	(VNA_IOC | 8)
#define	VNA_IOC_RX_INTR_CLR	(VNA_IOC | 9)
#define	VNA_IOC_TX_INTR_CLR	(VNA_IOC | 10)
#define	VNA_IOC_SET_FEATURES	(VNA_IOC | 11)
#define	VNA_IOC_GET_FEATURES	(VNA_IOC | 12)

typedef struct vioc_create {
	datalink_id_t	c_linkid;
	int		c_vmfd;
} vioc_create_t;

typedef struct vioc_ring_init {
	uint16_t	ri_qsize;
	uint64_t	ri_qaddr;
} vioc_ring_init_t;

#endif	/* _VIONA_IO_H_ */
