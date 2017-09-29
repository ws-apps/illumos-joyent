#
# COPYRIGHT 2013 Pluribus Networks Inc.
#
# All rights reserved. This copyright notice is Copyright Management
# Information under 17 USC 1202 and is included to protect this work and
# deter copyright infringement.  Removal or alteration of this Copyright
# Management Information without the express written permission from
# Pluribus Networks Inc is prohibited, and any such unauthorized removal
# or alteration will be a violation of federal law.

include ../Makefile.cmd

PROG=	bhyveconsole

SUBDIRS= $(MACH)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all:	$(SUBDIRS)

clean clobber lint:	$(SUBDIRS)

install:	$(SUBDIRS)
	-$(RM) $(ROOTUSRSBINPROG)
	-$(LN) $(ISAEXEC) $(ROOTUSRSBINPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) CW_NO_SHADOW=true __GNUC= $(TARGET)

FRC:

include ../Makefile.targ
