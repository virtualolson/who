# $Id: Makefile 719 2009-08-12 15:51:50Z vingarzan $ Makefile  $Date: 2009-08-12 17:51:50 +0200 (Mi, 12. Aug 2009) $ $Author: vingarzan $ dvi
#
# Makefile for the Proxy-CSCF module
# \author Dragos Vingarzan vingarzan -at- fokus dot fraunhofer dot de
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=pcscf.so

DEFS+=-DSER
INCLUDES = -I../../lib -I../..
DEFS+=-DSER_MOD_INTERFACE
SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/cds/ser_cds

DEFS += -DCDP_FOR_SER
DEFS += -I/usr/include/libxml2 
LIBS += -L$(LOCALBASE)/lib -lxml2

MOD_INSTALL_CFGS=emerg_info.xml reginfo.dtd

#DEFS += -DWITH_IMS_PM

include ../../Makefile.modules
