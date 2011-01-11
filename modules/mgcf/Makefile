# $Id: Makefile 671 2009-06-12 07:51:06Z vingarzan $
#
# Makefile for the MGCF module
# \author Dragos Vingarzan dragos dot vingarzan -at- fokus dot fraunhofer dot de
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=mgcf.so

DEFS+=-DSER_MOD_INTERFACE
DEFS+=-DSER

SERLIBPATH=../../lib
INCLUDES = -I../../lib -I../..
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/cds/ser_cds

MOD_INSTALL_CFGS=mgcf.cfg

include ../../Makefile.modules

#DEFS += -DWITH_IMS_PM
