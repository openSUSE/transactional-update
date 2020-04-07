AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS=transkit
transkit_SOURCES=main.cpp \
        transkit.cpp \
        Configuration.cpp
noinst_HEADERS=transkit.h \
        Configuration.h
transkit_CPPFLAGS = -I $(top_srcdir)/include $(ECONF_CFLAGS)
transkit_LDADD = $(top_builddir)/lib/libt-u.la $(ECONF_LIBS) -lmount
