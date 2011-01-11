# $Id: Makefile $date aon $
#
# Emergency CSCF
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=ecscf.so

DEFS+=-DSER_MOD_INTERFACE
SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1

DEFS += -DCDP_FOR_SER
DEFS += -I/usr/include/libxml2 -I$(SERLIBPATH) -I../..
LIBS += -L$(LOCALBASE)/lib -lxml2

#MOD_INSTALL_CFGS=ecscf.cfg ecscf.xml

include ../../Makefile.modules
