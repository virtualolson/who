include ../../Makefile.defs
auto_gen=
NAME=Client_Ro.so
LIBS=

ifeq ($(CFG_NAME),sip-router)
	DEFS += -DSER_MOD_INTERFACE
	SERLIBPATH=/../../lib
	DEFS += -I/usr/include/libxml2 -I../lib -DSER -I../..
	DEFS += -I../../lib/cds
endif

	DEFS += -DCDP_FOR_SER
	DEFS += -I/usr/include/libxml2 -I../../lib -DSER -I../..

DEFS += -I/usr/include/libxml2
LIBS += -L$(LOCALBASE)/lib -lxml2 -lrt

include ../../Makefile.modules
