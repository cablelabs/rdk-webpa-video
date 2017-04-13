lib_LTLIBRARIES = libwal.la
libwal_la_SOURCES = wal.c
libwal_la_LDFLAGS = -lwebpacore -lIARMBus
AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/wal -I${PKG_CONFIG_SYSROOT_DIR}/$(includedir)/rdk/iarmbus  $(GLIB_CFLAGS)
libwal_la_LIBADD = $(top_builddir)/src/waldb/libwaldb.la
