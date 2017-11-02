#
# COPYRIGHT 2013 Pluribus Networks Inc.
#
# All rights reserved. This copyright notice is Copyright Management
# Information under 17 USC 1202 and is included to protect this work and
# deter copyright infringement.  Removal or alteration of this Copyright
# Management Information without the express written permission from
# Pluribus Networks Inc is prohibited, and any such unauthorized removal
# or alteration will be a violation of federal law.

PROG= bhyvectl

SRCS = bhyvectl.c
OBJS = $(SRCS:.c=.o) humanize_number.o

include ../../Makefile.cmd

.KEEP_STATE:

CFLAGS +=	$(CCVERBOSE)
CPPFLAGS =	-I$(COMPAT)/freebsd -I$(CONTRIB)/freebsd $(CPPFLAGS.master) \
	-I$(ROOT)/usr/platform/i86pc/include \
	-I$(SRC)/uts/i86pc/io/vmm
LDLIBS +=	-lvmmapi

CERRWARN +=	-_gcc=-Wno-uninitialized

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../Makefile.targ

%.o: $(CONTRIB)/freebsd/lib/libutil/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

%.o: ../%.c
	$(COMPILE.c) -I$(SRC)/common $<
	$(POST_PROCESS_O)
