#
# Copyright (c) 2016, 2017 Thorsten Kukuk <kukuk@suse.de>
#

sbin_SCRIPTS = transactional-update tu-rebuild-kdump-initrd
CLEANFILES = transactional-update
EXTRA_DIST = transactional-update.in tu-rebuild-kdump-initrd

do_subst = sed -e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	-e 's,[@]VERSION[@],$(VERSION),g'

transactional-update: transactional-update.in Makefile
	$(do_subst) < transactional-update.in > transactional-update
	chmod +x transactional-update
