# $Id: Makefile $date aon $
#
# Emergency CSCF
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=ecscf.so

DEFS+=-DSER
DEFS+=-DSER_MOD_INTERFACE
DEFS+=-DCDP_FOR_SER

INCLUDES += -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
			-I$(LOCALBASE)/include -I../../lib -I../..

LIBS += -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/lost/lost_client \

#MOD_INSTALL_CFGS=ecscf.cfg

include ../../Makefile.modules
