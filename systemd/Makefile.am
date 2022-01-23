#
# Copyright (c) 2016 Thorsten Kukuk <kukuk@suse.de>
#

systemddir = @SYSTEMDDIR@

systemd_DATA = transactional-update.timer transactional-update.service \
	transactional-update-cleanup.timer transactional-update-cleanup.service \
	create-dirs-from-rpmdb.service
EXTRA_DIST = transactional-update.timer transactional-update-cleanup.timer \
	transactional-update.service.in transactional-update-cleanup.service \
	create-dirs-from-rpmdb.service
CLEANFILES = transactional-update.service

do_subst = sed -e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	-e 's,[@]sbindir[@],$(sbindir),g' \
	-e 's,[@]prefix[@],$(prefix),g'

transactional-update.service: transactional-update.service.in Makefile
	$(do_subst) < transactional-update.service.in > transactional-update.service
