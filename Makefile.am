SUBDIRS = src tests 
bin_PROGRAMS= webpavideo
webpavideo_SOURCES = src/main.c
webpavideo_LDADD = $(top_builddir)/src/wal/libwal.la
AM_LDFLAGS=-lrdkloggers
AM_CFLAGS = -I$(top_srcdir)/include/wal
if WITH_WEBPA_RFC
AM_CFLAGS+= -DWEBPA_RFC_ENABLED
endif
