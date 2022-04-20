AUTOMAKE_OPTIONS = subdir-objects
lib_LTLIBRARIES = libtukit.la
libtukit_la_SOURCES=Transaction.cpp \
        SnapshotManager.cpp Snapshot/Snapper.cpp \
        Mount.cpp Overlay.cpp Reboot.cpp Configuration.cpp \
        Util.cpp Supplement.cpp Bindings/CBindings.cpp
publicheadersdir=$(includedir)/tukit
publicheaders_HEADERS=Transaction.hpp \
	Snapshot.hpp SnapshotManager.hpp \
	Reboot.hpp \
	Bindings/libtukit.h
noinst_HEADERS=Snapshot/Snapper.hpp \
        Mount.hpp Overlay.hpp Log.hpp Configuration.hpp \
        Util.hpp Supplement.hpp Exceptions.hpp
libtukit_la_CPPFLAGS=-DPREFIX=\"$(prefix)\" -DCONFDIR=\"$(sysconfdir)\" $(ECONF_CFLAGS) $(LIBMOUNT_CFLAGS) $(SELINUX_CFLAGS)
libtukit_la_LDFLAGS=$(ECONF_LIBS) $(LIBMOUNT_LIBS) $(SELINUX_LIBS) \
	-version-info $(LIBTOOL_CURRENT):$(LIBTOOL_REVISION):$(LIBTOOL_AGE)
