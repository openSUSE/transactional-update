#
# Copyright (c) 2016 Thorsten Kukuk <kukuk@suse.de>
#

systemddir = @SYSTEMDDIR@

systemd_DATA = transactional-update.timer transactional-update.service \
	transactional-update-cleanup.timer transactional-update-cleanup.service \
	create-dirs-from-rpmdb.service prepare-nextroot-for-softreboot.service
EXTRA_DIST = transactional-update.timer transactional-update-cleanup.timer \
	transactional-update.service.in transactional-update-cleanup.service.in \
	create-dirs-from-rpmdb.service prepare-nextroot-for-softreboot.service.in \
	prepare-nextroot-for-softreboot
CLEANFILES = transactional-update.service transactional-update-cleanup.service \
	prepare-nextroot-for-softreboot.service
dist_libexec_SCRIPTS = prepare-nextroot-for-softreboot

do_subst = sed -e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	-e 's,[@]sbindir[@],$(sbindir),g' \
	-e 's,[@]libexecdir[@],$(libexecdir),g' \
	-e 's,[@]prefix[@],$(prefix),g'

transactional-update.service: transactional-update.service.in Makefile
	$(do_subst) < transactional-update.service.in > transactional-update.service
transactional-update-cleanup.service: transactional-update-cleanup.service.in Makefile
	$(do_subst) < transactional-update-cleanup.service.in > transactional-update-cleanup.service
prepare-nextroot-for-softreboot.service: prepare-nextroot-for-softreboot.service.in Makefile
	$(do_subst) < prepare-nextroot-for-softreboot.service.in > prepare-nextroot-for-softreboot.service
