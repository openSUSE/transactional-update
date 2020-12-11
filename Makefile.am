#
# Copyright (c) 2016-2018 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#
AUTOMAKE_OPTIONS = 1.6 foreign check-news dist-xz
#
SUBDIRS = lib tukit sbin man systemd logrotate dracut doc etc

CLEANFILES = *~ t-u.pc

M4_FILES = m4/jh_path_xml_catalog.m4

EXTRA_DIST = ChangeLog gpl-2.0.txt lgpl-2.1.txt $(M4_FILES) t-u.pc.in

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = t-u.pc
