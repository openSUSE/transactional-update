# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

CLEANFILES = transactional-update.txt html/*.html html/*.png html/*.css html/tukit.devhelp2 html.stamp tukit-*.xml *~

EXTRA_DIST = $(XMLS) transactional-update.txt

XMLS = transactional-update.xml
API_XMLS = tukit-org.opensuse.tukit.Snapshot.xml tukit-org.opensuse.tukit.Transaction.xml
MAN_XMLS = ../man/transactional-update.8.xml ../man/transactional-update.conf.5.xml ../man/tukit.conf.5.xml

if ENABLE_REGENERATE_DOC
all: transactional-update.txt html/index.html

transactional-update.txt: $(XMLS) $(DEP_XMLS)
	$(XMLLINT) --nonet --xinclude --postvalid --noent --noout $<
	$(XSLTPROC) --stringparam generate.toc "book toc" \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 3 --xinclude --nonet \
	  http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $< | $(BROWSER) > $@

tukit-org.opensuse.tukit.%.xml: ../dbus/org.opensuse.tukit.%.xml
	xsltproc spec-to-docbook.xsl $< > $@

html/index.html: $(XMLS) $(API_XMLS) $(MAN_XMLS) transactional-update-docs.xml
	cd html && gtkdoc-mkhtml tukit ../transactional-update-docs.xml

transactional-update.md: $(XMLS) $(DEP_XMLS)
	pandoc -f docbook -t markdown -s $< -o $@

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(docdir)
	$(mkinstalldirs) $(DESTDIR)$(htmldir)
	$(mkinstalldirs) $(DESTDIR)$(htmldir)/png
	if test -f html/index.html; then \
	    $(install_sh_DATA) html/*.html html/*.css html/*.png html/*.devhelp2 $(DESTDIR)$(htmldir)/; \
	    $(install_sh_DATA) html/png/* $(DESTDIR)$(htmldir)/png/; \
	elif test -f $(srcdir)/html/index.html; then \
	    $(install_sh_DATA) $(srcdir)/html/*.html $(srcdir)/html/*.css $(srcdir)/html/*.png $(srcdir)/html/*.devhelp2 $(DESTDIR)$(htmldir)/; \
	    $(install_sh_DATA) $(srcdir)/html/png/* $(DESTDIR)$(htmldir)/png/; \
	fi
	if test -f transactional-update.txt; then \
	    $(install_sh_DATA) transactional-update.txt $(DESTDIR)$(docdir)/; \
	elif test -f $(srcdir)/transactional-update.txt; then \
	    $(install_sh_DATA) $(srcdir)/transactional-update.txt \
		$(DESTDIR)$(docdir)/; \
	fi

uninstall-local:
	-rm -r $(DESTDIR)$(htmldir)
	-rm $(DESTDIR)$(docdir)/transactional-update.txt
endif
