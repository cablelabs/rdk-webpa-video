#Copyright (c) 2013 Comcast. All rights reserved.

AM_CXXFLAGS = $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) \
        -I$(top_srcdir)/include 

AM_LDFLAGS = $(GLIB_LDFLAGS) $(GLIB_LIBS) \
        $(G_THREAD_LIBS) -lrdkloggers $(LOG4C_LIBS) ${DIRECTFB_LIBS} \
        -lIARMBus -ldirect -lfusion -ldbus-1 -lIARMBus -lpthread


AM_CFLAGS =  $(GLIB_CFLAGS) $(GTHREAD_CFLAGS)
AM_CPPFLAGS =  $(GLIB_CFLAGS) $(GTHREAD_CFLAGS)

# the list of header files that belong to the library (to be installed later)
libwebpa_la_includedir = $(includedir)
libwebpa_la_include_HEADERS =    $(top_srcdir)/include/wal.h

lib_LTLIBRARIES = libwebpa.la
libwebpa_la_SOURCES = $(top_srcdir)/src/wal.c

