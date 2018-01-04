#
# Copyright (c) 2017 Thorsten Kukuk <kukuk@thkukuk.de>
#

CLEANFILES = transactional-update.fo *~

EXTRA_DIST = $(XMLS)

XMLS = transactional-update.xml
#DEP_XMLS = $(shell ls $(top_srcdir)/doc/man/pam_*.xml)

if ENABLE_REGENERATE_MAN
MAINTAINERCLEANFILES = transactional-update.txt html/*.html

all: transactional-update.txt html/transactional-update.html

transactional-update.txt: $(XMLS) $(DEP_XMLS)
	$(XMLLINT) --nonet --xinclude --postvalid --noent --noout $<
	$(XSLTPROC) --stringparam generate.toc "book toc" \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 3 --xinclude --nonet \
	  http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $< | $(BROWSER) > $@

html/transactional-update.html: $(XMLS) $(DEP_XMLS)
	@test -d html || mkdir -p html
	$(XMLLINT) --nonet --xinclude --postvalid --noent --noout $<
	$(XSLTPROC) --stringparam base.dir html/ \
	  --stringparam img.src.path ../ \
	  --stringparam root.filename transactional-update \
	  --stringparam use.id.as.filename 1 \
	  --stringparam chunk.section.depth 0 \
	  --stringparam chunk.first.sections 0 \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 3 --xinclude --nonet \
	  --stringparam chunker.output.encoding UTF-8 \
	  http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl $<

transactional-update.md: $(XMLS) $(DEP_XMLS)
	pandoc -f docbook -t markdown -s $< -o $@

distclean-local:
	-rm -rf html transactional-update.txt

endif

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(docdir)
	$(mkinstalldirs) $(DESTDIR)$(pdfdir)
	$(mkinstalldirs) $(DESTDIR)$(htmldir)
	if test -f html/transactional-update.html; then \
	    $(install_sh_DATA) html/transactional-update.html html/tu-*.html \
		$(DESTDIR)$(htmldir)/; \
	elif test -f $(srcdir)/html/transactional-update.html; then \
	    $(install_sh_DATA) $(srcdir)/html/transactinal-update.html \
		$(srcdir)/html/tu-*.html \
		$(DESTDIR)$(htmldir)/; \
	fi
	if test -f transactional-update.txt; then \
	    $(install_sh_DATA) transactional-update.txt $(DESTDIR)$(docdir)/; \
	elif test -f $(srcdir)/transactional-update.txt; then \
	    $(install_sh_DATA) $(srcdir)/transactional-update.txt \
		$(DESTDIR)$(docdir)/; \
	fi

uninstall-local:
	-rm $(DESTDIR)$(htmldir)/transactional-update.html
	-rm $(DESTDIR)$(htmldir)/tu-*.html
	-rm $(DESTDIR)$(docdir)/transactional-update.txt

releasedocs: all
	$(mkinstalldirs) $(top_builddir)/transactional-update-$(VERSION)/doc/adg/html
	if test -f html/Linux-PAM_ADG.html; then \
	    cp -ap html/Linux-PAM_ADG.html html/adg-*.html \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/adg/html/; \
	elif test -f $(srcdir)/html/Linux-PAM_ADG.html; then \
	    cp -ap $(srcdir)/html/Linux-PAM_ADG.html \
		$(srcdir)/html/adg-*.html \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/adg/html/; \
	else exit 1; \
	fi
	if test -f Linux-PAM_ADG.txt; then \
	    cp -p Linux-PAM_ADG.txt \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/adg/; \
	elif test -f $(srcdir)/Linux-PAM_ADG.txt; then \
	    cp -p $(srcdir)/Linux-PAM_ADG.txt \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/adg/; \
	else exit 1; \
	fi
