# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

AUTOMAKE_OPTIONS = subdir-objects
lib_LTLIBRARIES = libtukit.la
libtukit_la_SOURCES=Transaction.cpp \
        SnapshotManager.cpp Snapshot/Snapper.cpp \
        Snapshot/Podman.cpp \
        Mount.cpp Reboot.cpp Configuration.cpp \
        Util.cpp Supplement.cpp Plugins.cpp Bindings/CBindings.cpp \
        BlsEntry.cpp
publicheadersdir=$(includedir)/tukit
publicheaders_HEADERS=Transaction.hpp \
	SnapshotManager.hpp Reboot.hpp \
	Bindings/libtukit.h
noinst_HEADERS=Snapshot/Snapper.hpp Snapshot/Podman.hpp Snapshot.hpp \
        Mount.hpp Log.hpp Configuration.hpp \
        Util.hpp Supplement.hpp Exceptions.hpp Plugins.hpp BlsEntry.hpp
libtukit_la_CPPFLAGS=-DPREFIX=\"$(prefix)\" -DCONFDIR=\"$(sysconfdir)\" $(ECONF_CFLAGS) $(LIBMOUNT_CFLAGS) $(SELINUX_CFLAGS)
libtukit_la_LDFLAGS=$(ECONF_LIBS) $(LIBMOUNT_LIBS) $(SELINUX_LIBS) \
	-version-info $(LIBTOOL_CURRENT):$(LIBTOOL_REVISION):$(LIBTOOL_AGE)
