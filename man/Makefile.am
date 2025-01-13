# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

CLEANFILES = $(MANS) $(DATA) transactional-update.index.xml

EXTRA_DIST = $(MANS) $(XMLS) $(DATA) custom-html.xsl make-man-index.py \
	xml_helper.py transactional-update.index.xml

XMLS = transactional-update.8.xml transactional-update.conf.5.xml tukit.conf.5.xml

if ENABLE_REGENERATE_MAN
man_MANS = transactional-update.8 transactional-update.service.8 \
	transactional-update.timer.8 transactional-update.conf.5 tukit.conf.5

noinst_DATA = transactional-update.8.html transactional-update.service.8.html \
	transactional-update.timer.8.html transactional-update.conf.5.html \
	transactional-update.index.html tukit.conf.5.html

XSLTPROC_FLAGS_HTML = \
        --nonet \
        --xinclude \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0 \
	--stringparam package.name $(PACKAGE) \
        --stringparam package.version $(VERSION) \
        --stringparam chunker.output.encoding UTF-8 \
	--stringparam sysconfdir $(sysconfdir) \
	--stringparam prefix $(prefix) \
        $(srcdir)/custom-html.xsl

XSLTPROC_FLAGS_MAN = \
	--nonet \
	--xinclude \
	--stringparam sysconfdir $(sysconfdir) \
	--stringparam prefix $(prefix) \
	$(srcdir)/custom-man.xsl

define html_alias
	$(LN_S) -f $(notdir $<) $@
endef

transactional-update.index.xml: make-man-index.py $(XMLS)
	python3 $(srcdir)/make-man-index.py $(srcdir)/$@ $(XMLS)

transactional-update.8: transactional-update.8.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_MAN) $<

transactional-update.service.8: transactional-update.8

transactional-update.timer.8: transactional-update.8

transactional-update.8.html: transactional-update.8.xml custom-html.xsl
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_HTML) $<

transactional-update.service.8.html: transactional-update.8.html
	$(html_alias)

transactional-update.timer.8.html: transactional-update.8.html
	$(html_alias)

transactional-update.index.html: transactional-update.index.xml custom-html.xsl
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_HTML) $<

transactional-update.conf.5: transactional-update.conf.5.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_MAN) $<

transactional-update.conf.5.html: transactional-update.conf.5.xml custom-html.xsl
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_HTML) $<

tukit.conf.5: tukit.conf.5.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_MAN) $<

tukit.conf.5.html: tukit.conf.5.xml custom-html.xsl
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) $(XSLTPROC_FLAGS_HTML) $<

endif
