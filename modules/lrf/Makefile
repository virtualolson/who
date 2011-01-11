# $Id: Makefile $date aon $
#
# Location Retrieval Function (LRF) including a Routing Determination Function based on a LoST (RFC 5222) client
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=lrf.so
LIBS=

DEFS+=-DSER
INCLUDES = -I../../lib -I../..
DEFS+=-DSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/cds/ser_cds $(SERLIBPATH)/lost/lost_client

DEFS += -DCDP_FOR_SER
DEFS += -I/usr/include/libxml2 
LIBS += -L$(LOCALBASE)/lib -lxml2

#MOD_INSTALL_CFGS=lrf.cfg

include ../../Makefile.modules
