# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

AUTOMAKE_OPTIONS = 1.6 foreign check-news dist-xz
#
SUBDIRS = lib tukit dbus sbin man systemd logrotate dracut snapper doc etc tests

CLEANFILES = *~ tukit.pc

M4_FILES = m4/jh_path_xml_catalog.m4

EXTRA_DIST = ChangeLog README.md gpl-2.0.txt lgpl-2.1.txt $(M4_FILES) tukit.pc.in

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tukit.pc
