#
# Copyright (c) 2016, 2017 Thorsten Kukuk <kukuk@suse.de>
#

sbin_SCRIPTS = transactional-update
sbin_PROGRAMS = create_dirs_from_rpmdb
create_dirs_from_rpmdb_SOURCES = create_dirs_from_rpmdb.c
create_dirs_from_rpmdb_CFLAGS = $(LIBRPM_CFLAGS) $(SELINUX_CFLAGS)
create_dirs_from_rpmdb_LDFLAGS = $(LIBRPM_LIBS) $(SELINUX_LIBS)
CLEANFILES = transactional-update
EXTRA_DIST = transactional-update.in

do_subst = sed -e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	-e 's,[@]prefix[@],$(prefix),g' \
	-e 's,[@]VERSION[@],$(VERSION),g'

transactional-update: transactional-update.in Makefile
	$(do_subst) < transactional-update.in > transactional-update
	chmod +x transactional-update
