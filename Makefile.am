SUBDIRS = src tests 
bin_PROGRAMS= webpavideo
webpavideo_SOURCES = src/main.c
webpavideo_LDADD = $(top_builddir)/src/wal/libwal.la
AM_LDFLAGS=-lrdkloggers

if WITH_WEBPA_RFC
AM_CFLAGS = -DWEBPA_RFC_ENABLED
endif
