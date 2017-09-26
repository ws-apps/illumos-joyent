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
#ifndef _COMPAT_FREEBSD_STRING_H_
#define	_COMPAT_FREEBSD_STRING_H_

/*
 * This is quite a hack; blame bcopy/bcmp/bzero and memcpy/memcmp/memset.
 */
#include <strings.h>

#include_next <string.h>

#endif	/* _COMPAT_FREEBSD_STRING_H_ */
