# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

AUTOMAKE_OPTIONS = subdir-objects
sbin_PROGRAMS=tukit
tukit_SOURCES=main.cpp \
        tukit.cpp
noinst_HEADERS=tukit.hpp
tukit_CPPFLAGS = -I $(top_srcdir)/lib $(ECONF_CFLAGS)
tukit_LDADD = $(top_builddir)/lib/libtukit.la $(ECONF_LIBS) -lmount
