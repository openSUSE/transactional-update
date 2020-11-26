AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS=tukit
tukit_SOURCES=main.cpp \
        tukit.cpp
noinst_HEADERS=tukit.h
tukit_CPPFLAGS = -I $(top_srcdir)/include $(ECONF_CFLAGS)
tukit_LDADD = $(top_builddir)/lib/libt-u.la $(ECONF_LIBS) -lmount
