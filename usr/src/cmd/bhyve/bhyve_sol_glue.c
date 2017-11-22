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
#include <sys/uio.h>

#include <termios.h>
#include <unistd.h>

/*
 * Make a pre-existing termios structure into "raw" mode: character-at-a-time
 * mode with no characters interpreted, 8-bit data path.
 */
void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL|IXOFF|INPCK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|
	    ICRNL|IXON|IGNPAR);
	t->c_iflag |= IGNBRK;
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|ISIG|IEXTEN|NOFLSH|
	    TOSTOP|PENDIN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8|CREAD;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
}
