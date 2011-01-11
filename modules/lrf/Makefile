# $Id: Makefile $date aon $
#
# Location Retrieval Function (LRF) including a Routing Determination Function based on a LoST (RFC 5222) client
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=lrf.so

DEFS+=-DSER
DEFS+=-DSER_MOD_INTERFACE
DEFS+=-DCDP_FOR_SER

INCLUDES += -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
			-I$(LOCALBASE)/include -I../../lib -I../..

LIBS += -L$(LOCALBASE)/lib -L/usr/pkg/lib -lxml2 

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/cds/ser_cds $(SERLIBPATH)/lost/lost_client

#MOD_INSTALL_CFGS=lrf.cfg

include ../../Makefile.modules
