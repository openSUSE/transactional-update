#
# Copyright (c) 2018 Ignaz Forster <iforster@suse.com>
#

EXTRA_DIST = transactional-update.conf

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(prefix)$(sysconfdir)
	$(INSTALL_DATA) $(srcdir)/$(EXTRA_DIST) $(DESTDIR)$(prefix)$(sysconfdir)

uninstall-local:
	-rm $(DESTDIR)$(prefix)$(sysconfdir)/$(EXTRA_DIST)
