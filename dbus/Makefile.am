# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

sbin_PROGRAMS = tukitd
tukitd_SOURCES = tukitd.c
tukitd_CPPFLAGS = -I $(top_srcdir)/lib $(LIBSYSTEMD_CFLAGS) $(PTHREAD_CFLAGS)
tukitd_LDFLAGS = $(top_builddir)/lib/libtukit.la $(LIBSYSTEMD_LIBS) $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
dbusconfdir = @DBUSCONFDIR@
dbusconf_DATA = org.opensuse.tukit.conf
dbussystembusservicedir = @DBUSSYSTEMBUSSERVICEDIR@
dbussystembusservice_DATA = org.opensuse.tukit.service
dbusinterfacesdir = @DBUSINTERFACESDIR@
dbusinterfaces_DATA = org.opensuse.tukit.Transaction.xml org.opensuse.tukit.Snapshot.xml
systemdsystemunitdir = @SYSTEMDDIR@
systemdsystemunit_DATA = tukitd.service

EXTRA_DIST = $(DATA)
