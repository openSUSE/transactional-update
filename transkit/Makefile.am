AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS=transkit
transkit_SOURCES=main.cpp \
        transkit.cpp
noinst_HEADERS=transkit.h
transkit_CPPFLAGS = -I $(top_srcdir)/include $(ECONF_CFLAGS)
transkit_LDADD = $(top_builddir)/lib/libt-u.la $(ECONF_LIBS) -lmount
