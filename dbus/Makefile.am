#
# Copyright (c) 2020 Ignaz Forster <iforster@suse.com>
#

sbin_PROGRAMS = tukitd
tukitd_SOURCES = tukitd.c
tukitd_CPPFLAGS = -I $(top_srcdir)/lib $(LIBSYSTEMD_CFLAGS) -pthread
tukitd_LDFLAGS = $(top_builddir)/lib/libtukit.la $(LIBSYSTEMD_LIBS) -lpthread
dbusconfdir = @DBUSCONFDIR@
dbusservicesdir = @DBUSSERVICESDIR@
dbusconf_DATA = org.opensuse.tukit.conf
dbusservices_DATA = org.opensuse.tukit.service
EXTRA_DIST = org.opensuse.tukit.conf org.opensuse.tukit.service.in
CLEANFILES = org.opensuse.tukit.service

do_subst = sed -e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	-e 's,[@]sbindir[@],$(sbindir),g' \
	-e 's,[@]prefix[@],$(prefix),g'

org.opensuse.tukit.service: org.opensuse.tukit.service.in Makefile
	$(do_subst) < org.opensuse.tukit.service.in > org.opensuse.tukit.service
