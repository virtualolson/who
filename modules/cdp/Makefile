# $Id: Makefile 920 2010-03-05 19:07:50Z vingarzan $
#
# Makefile for the CDP module
# \author Dragos Vingarzan vingarzan -at- fokus dot fraunhofer dot de
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=cdp.so

ifeq ($(CFG_NAME),sip-router)
	DEFS+=-DSER_MOD_INTERFACE
endif	

ifeq ($(CFG_NAME),kamailio)
	DEFS+=-DSER_MOD_INTERFACE
endif	

ifeq ($(CFG_NAME),Wharf)
	DEFS += -DWHARF
else
	DEFS += -DCDP_FOR_SER
endif	

DEFS += -I/usr/include/libxml2 
LIBS += -L$(LOCALBASE)/lib -lxml2 -lrt

include ../../Makefile.modules

