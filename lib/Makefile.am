AUTOMAKE_OPTIONS = subdir-objects
lib_LTLIBRARIES = libt-u.la
libt_u_la_SOURCES=Transaction.cpp \
        Snapshot.cpp Snapshot/Snapper.cpp \
        Mount.cpp Util.cpp
noinst_HEADERS=Transaction.h \
        Snapshot.h Snapshot/Snapper.h \
        Mount.h Log.h \
        Util.h Exceptions.h
libt_u_la_CPPFLAGS=$(ECONF_CFLAGS)
libt_u_la_LDFLAGS=$(ECONF_LIBS) -lmount
