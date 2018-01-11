#
# Copyright (c) 2016, 2017 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

CLEANFILES = $(MANS) $(DATA) transactional-update.index.xml

EXTRA_DIST = $(MANS) $(XMLS) $(DATA) custom-html.xsl make-man-index.py \
	xml_helper.py transactional-update.index.xml

man_MANS = transactional-update.8 transactional-update.service.8 \
	transactional-update.timer.8

noinst_DATA = transactional-update.8.html transactional-update.service.8.html \
	transactional-update.timer.8.html transactional-update.index.html

XMLS = transactional-update.8.xml

XSLTPROC_FLAGS_HTML = \
        --nonet \
        --xinclude \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0 \
	--stringparam package.name $(PACKAGE) \
        --stringparam package.version $(VERSION) \
        --stringparam chunker.output.encoding UTF-8 \
        $(srcdir)/custom-html.xsl


if ENABLE_REGENERATE_MAN
define html_alias
	$(LN_S) -f $(notdir $<) $@
endef

transactional-update.index.xml: make-man-index.py $(XMLS)
	python $(srcdir)/make-man-index.py $(srcdir)/$@ $(XMLS)

transactional-update.8: transactional-update.8.xml
	$(XSLTPROC) -o $(srcdir)/$@ --path $(srcdir) --xinclude --nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

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

endif
