AUTOMAKE_OPTIONS = subdir-objects
lib_LTLIBRARIES = libt-u.la
libt_u_la_SOURCES=Transaction.cpp \
        Snapshot.cpp Snapshot/Snapper.cpp \
        Mount.cpp Overlay.cpp Configuration.cpp \
        Util.cpp Supplement.cpp
publicheadersdir=$(includedir)/t-u
publicheaders_HEADERS=Transaction.h
noinst_HEADERS=Snapshot.h Snapshot/Snapper.h \
        Mount.h Overlay.h Log.h Configuration.h \
        Util.h Supplement.h Exceptions.h
libt_u_la_CPPFLAGS=$(ECONF_CFLAGS)
libt_u_la_LDFLAGS=$(ECONF_LIBS) -lmount
