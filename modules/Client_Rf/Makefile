# $Id: Makefile $date aon $
#
# Location Retrieval Function (LRF) including a Routing Determination Function based on a LoST (RFC 5222) client
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=Client_Rf.so
LIBS=

ifeq ($(CFG_NAME),Wharf)
	DEFS += -DWHARF
else
	DEFS += -DSER_MOD_INTERFACE
	DEFS += -DCDP_FOR_SER
	SERLIBPATH=../../lib
	LIBS += -L/usr/pkg/lib
	DEFS += -I../../lib -DOPEN_IMS_CORE -DSER -I../..
	SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/cds/ser_cds
endif	

DEFS += -I/usr/include/libxml2
LIBS += -L$(LOCALBASE)/lib -lxml2 -lrt

include ../../Makefile.modules
