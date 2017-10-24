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
#ifndef _COMPAT_FREEBSD_SYS_NET_ETHERNET_H_
#define	_COMPAT_FREEBSD_SYS_NET_ETHERNET_H_

#define	ether_addr_octet	octet

#include <sys/ethernet.h>

/*
 * Some basic Ethernet constants.
 */
#define	ETHER_ADDR_LEN		6	/* length of an Ethernet address */
#define	ETHER_CRC_LEN		4	/* length of the Ethernet CRC */
#define	ETHER_MIN_LEN		64	/* minimum frame len, including CRC */

#define	ETHER_VLAN_ENCAP_LEN	4	/* len of 802.1Q VLAN encapsulation */

#define	ETHER_IS_MULTICAST(addr) (*(addr) & 0x01) /* is address mcast/bcast? */

#endif	/* _COMPAT_FREEBSD_SYS_NET_ETHERNET_H_ */
