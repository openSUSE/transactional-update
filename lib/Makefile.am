AUTOMAKE_OPTIONS = subdir-objects
lib_LTLIBRARIES = libtukit.la
libtukit_la_SOURCES=Transaction.cpp \
        Snapshot.cpp Snapshot/Snapper.cpp \
        Mount.cpp Overlay.cpp Configuration.cpp \
        Util.cpp Supplement.cpp
publicheadersdir=$(includedir)/tukit
publicheaders_HEADERS=Transaction.h
noinst_HEADERS=Snapshot.h Snapshot/Snapper.h \
        Mount.h Overlay.h Log.h Configuration.h \
        Util.h Supplement.h Exceptions.h
libtukit_la_CPPFLAGS=$(ECONF_CFLAGS) $(LIBMOUNT_CFLAGS) $(SELINUX_CFLAGS)
libtukit_la_LDFLAGS=$(ECONF_LIBS) $(LIBMOUNT_LIBS) $(SELINUX_LIBS)
