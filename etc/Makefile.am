#
# Copyright (c) 2018 Ignaz Forster <iforster@suse.com>
#

sysconf_DATA = transactional-update.conf

EXTRA_DIST = $(DATA)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(prefix)/etc
	$(INSTALL_DATA) $(srcdir)/$(DATA) $(DESTDIR)$(prefix)/etc
