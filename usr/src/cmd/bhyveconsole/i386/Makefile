#
# COPYRIGHT 2013 Pluribus Networks Inc.
#
# All rights reserved. This copyright notice is Copyright Management
# Information under 17 USC 1202 and is included to protect this work and
# deter copyright infringement.  Removal or alteration of this Copyright
# Management Information without the express written permission from
# Pluribus Networks Inc is prohibited, and any such unauthorized removal
# or alteration will be a violation of federal law.

PROG=	bhyveconsole

OBJS=	bhyveconsole.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS	+= $(CCVERBOSE)
LDLIBS += -lsocket

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG32)

clean:
	$(RM) $(OBJS)

include ../../Makefile.targ
