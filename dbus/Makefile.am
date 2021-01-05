#
# Copyright (c) 2020 Ignaz Forster <iforster@suse.com>
#

sbin_PROGRAMS = tukitd
tukitd_SOURCES = tukitd.c
tukitd_CPPFLAGS = $(LIBSYSTEMD_CFLAGS)
tukitd_LDFLAGS = $(LIBSYSTEMD_LIBS)

dbusconfdir = @DBUSCONFDIR@
dbusconf_DATA = org.opensuse.tukit.conf

EXTRA_DIST = $(DATA)
