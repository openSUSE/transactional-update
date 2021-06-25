#
# Copyright (c) 2020 Ignaz Forster <iforster@suse.com>
#

sbin_PROGRAMS = tukitd
tukitd_SOURCES = tukitd.c
tukitd_CPPFLAGS = -I $(top_srcdir)/lib $(LIBSYSTEMD_CFLAGS)
tukitd_LDFLAGS = $(top_builddir)/lib/libtukit.la $(LIBSYSTEMD_LIBS)
dbusconfdir = @DBUSCONFDIR@
dbusconf_DATA = org.opensuse.tukit.conf
dbussystembusservicedir = @DBUSSYSTEMBUSSERVICEDIR@
dbussystembusservice_DATA = tukitd.d-bus.service
systemdsystemconfdir = @SYSTEMDSYSTEMCONFDIR@
systemdsystemconf_DATA = tukitd.service

EXTRA_DIST = $(DATA)
