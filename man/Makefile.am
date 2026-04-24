# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

CLEANFILES = $(MANS) $(DATA)

EXTRA_DIST = $(MANS) $(XMLS) $(DATA) custom-man.xsl

XMLS = transactional-update.8.xml transactional-update.conf.5.xml tukit.conf.5.xml

if ENABLE_REGENERATE_DOC
man_MANS = transactional-update.8 transactional-update.service.8 \
	transactional-update.timer.8 transactional-update.conf.5 tukit.conf.5

XSLTPROC_FLAGS_MAN = \
	--nonet \
	--xinclude \
	--stringparam sysconfdir $(sysconfdir) \
	--stringparam prefix $(prefix) \
	$(srcdir)/custom-man.xsl

transactional-update.8: transactional-update.8.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_MAN) $<

transactional-update.service.8: transactional-update.8

transactional-update.timer.8: transactional-update.8

transactional-update.conf.5: transactional-update.conf.5.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_MAN) $<

tukit.conf.5: tukit.conf.5.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_MAN) $<

endif
