#
# Copyright (c) 2020 Ignaz Forster <iforster@suse.com>
#

sbin_PROGRAMS = tukitd
tukitd_SOURCES = tukitd.c
tukitd_CPPFLAGS = -I $(top_srcdir)/lib $(LIBSYSTEMD_CFLAGS) -pthread
tukitd_LDFLAGS = $(top_builddir)/lib/libtukit.la $(LIBSYSTEMD_LIBS) -lpthread
dbusconfdir = @DBUSCONFDIR@
dbusconf_DATA = org.opensuse.tukit.conf

EXTRA_DIST = $(DATA)
