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
#ifndef _COMPAT_FREEBSD_LIBUTIL_H_
#define	_COMPAT_FREEBSD_LIBUTIL_H_

int	expand_number(const char *_buf, uint64_t *_num);
int	humanize_number(char *_buf, size_t _len, int64_t _number,
    const char *_suffix, int _scale, int _flags);

/* Values for humanize_number(3)'s flags parameter. */
#define HN_DECIMAL      0x01
#define HN_NOSPACE      0x02
#define HN_B            0x04
#define HN_DIVISOR_1000     0x08
#define HN_IEC_PREFIXES     0x10

/* Values for humanize_number(3)'s scale parameter. */
#define HN_GETSCALE     0x10
#define HN_AUTOSCALE        0x20


#endif	/* _COMPAT_FREEBSD_LIBUTIL_H_ */
