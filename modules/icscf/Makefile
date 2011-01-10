# $Id: Makefile 719 2009-08-12 15:51:50Z vingarzan $
#
# Makefile for the I-CSCF module
# \author Dragos Vingarzan vingarzan -at- fokus dot fraunhofer dot de
#
# WARNING: do not run this directly, it should be run by the master Makefile



include ../../Makefile.defs
auto_gen=
NAME=icscf.so

DEFS+=-DSER_MOD_INTERFACE
SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1

DEFS += -DCDP_FOR_SER
DEFS += -I/usr/include/libxml2 
LIBS += -L$(LOCALBASE)/lib -lxml2

MOD_INSTALL_CFGS=icscf.cfg icscf.xml

#DEFS+= -DWITH_IMS_PM


include ../../Makefile.modules

