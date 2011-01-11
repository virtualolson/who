# example library makefile
#

include ../../Makefile.defs
auto_gen=
NAME:=lost_client
DEFS     += -fPIC
MAJOR_VER=0
MINOR_VER=1
BUGFIX_VER=0
DEFS+=-DSER
libxml2_includes=-I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
					-I$(LOCALBASE)/include
libcurl_includes=-I/usr/include/curl -I$(LOCALBASE)/include/curl \
					-I$(LOCALBASE)/include
libxml2_libs=-L$(LOCALBASE)/lib -lxml2
libcurl_libs=-L$(LOCALBASE)/lib -lcurl
INCLUDES= -I$(CURDIR)/.. -I$(CURDIR)/../.. $(libxml2_includes) $(libcurl_includes)  
LIBS=$(libxml2_libs) $(libcurl_libs)
SERLIBPATH=..
SER_LIBS=$(SERLIBPATH)/cds/ser_cds

# TYPE=lib => shared or static library, executable otherwise
TYPE = lib

include ../Makefile.defs

