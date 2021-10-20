dnl Process this file with autoconf to produce a configure script.
AC_INIT(transactional-update, 4.0.0)
# Increase on any interface change and reset revision
LIBTOOL_CURRENT=4
# Increase or reset on any VERSION update
LIBTOOL_REVISION=0
# Increase if interface change is backwards compatible, reset otherwise
LIBTOOL_AGE=0
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_FILES([tukit.pc])
AC_PREFIX_DEFAULT(/usr)

AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_SUBST(LIBTOOL_CURRENT)
AC_SUBST(LIBTOOL_REVISION)
AC_SUBST(LIBTOOL_AGE)

PKG_CHECK_VAR([SYSTEMDDIR], [systemd], [systemdsystemunitdir], [],
	[AC_MSG_ERROR([Could not determine value for 'systemdsystemunitdir' - is the 'systemd.pc' file installed?])])
PKG_CHECK_VAR([TMPFILESDIR], [systemd], [tmpfilesdir], [],
	[AC_MSG_ERROR([Could not determine value for 'tmpfilesdir' - is the 'systemd.pc' file installed?])])
PKG_CHECK_VAR([DRACUTDIR], [dracut], [dracutmodulesdir], [],
	[AC_MSG_ERROR([Could not determine value for 'dracutmodulesdir' - is the 'dracut.pc' file installed?])])
PKG_CHECK_VAR([UDEVDIR], [udev], [udevdir], [],
	[AC_MSG_ERROR([Could not determine value for 'udevdir' - is the 'udev.pc' file installed?])])
PKG_CHECK_VAR([DBUSDATADIR], [dbus-1], [datadir], [],
	[AC_MSG_ERROR([Could not determine value for 'datadir' - is the 'dbus-1.pc' file installed?])])
PKG_CHECK_VAR([DBUSSYSTEMBUSSERVICEDIR], [dbus-1], [system_bus_services_dir], [],
	[AC_MSG_ERROR([Could not determine value for 'system_bus_services_dir' - is the 'dbus-1.pc' file installed?])])

UDEVRULESDIR=${UDEVDIR}/rules.d
LOGROTATEDDIR=${sysconfdir}/logrotate.d
DBUSCONFDIR=${DBUSDATADIR}/dbus-1/system.d

AC_SUBST(TMPFILESDIR)
AC_SUBST(UDEVRULESDIR)
AC_SUBST(SYSTEMDDIR)
AC_SUBST(SYSTEMDSYSTEMCONFDIR)
AC_SUBST(DRACUTDIR)
AC_SUBST(LOGROTATEDDIR)
AC_SUBST(DBUSCONFDIR)
AC_SUBST(DBUSSYSTEMBUSSERVICEDIR)

AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_17(, mandatory)
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
LT_INIT([disable-static])

PKG_CHECK_MODULES([ECONF], [libeconf])
PKG_CHECK_MODULES([SELINUX], [libselinux])
PKG_CHECK_MODULES([LIBMOUNT], [mount])
PKG_CHECK_MODULES([LIBRPM], [rpm >= 4.15], AC_DEFINE([HAVE_RPMDBCOOKIE]),
	[PKG_CHECK_MODULES([LIBRPM], [rpm])])
PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd])

AC_ARG_WITH([doc],
	[AS_HELP_STRING([--with-doc], [Build documentation])], ,
	[enable_man=yes])

dnl
dnl Checking for pthread support
dnl
m4_ifdef([AX_PTHREAD],
	[AX_PTHREAD],
	[AC_MSG_ERROR([Missing macro AX_PTHREAD: please install
	  autoconf-archive to enable pthreads checks.])])

dnl
dnl Check for xsltproc
dnl
AC_PATH_PROG([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
     enable_man=no
fi
AC_PATH_PROG([XMLLINT], [xmllint],[/bin/true])
dnl check for DocBook DTD and stylesheets in the local catalog.
JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.3//EN],
                [DocBook XML DTD V4.3], [], enable_man=no)
JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl],
                [DocBook XSL Stylesheets], [], enable_man=no)
AC_PATH_PROG([BROWSER], [w3m])
if test ! -z "$BROWSER"; then
	BROWSER="$BROWSER -T text/html -dump"
else
	AC_PATH_PROG([BROWSER], [elinks])
	if test ! -z "$BROWSER"; then
		BROWSER="$BROWSER -no-numbering -no-references -dump"
	else
		enable_man=no
	fi
fi

m4_ifdef([AX_PYTHON_MODULE],
	[AX_PYTHON_MODULE([lxml])],
	[m4_warn([unsupported], [Missing macro AX_PYTHON_MODULE: please install
	  autoconf-archive to enable Python module checks.])])
AS_IF([test "${HAVE_PYMOD_LXML}" != "yes"], enable_man=no)

AS_IF([test "x$enable_man" = "xno" -a "x$with_doc" = "xyes"],
	[AC_MSG_ERROR([missing dependencies for building documentation])])
AS_IF([test "x$enable_man" = "xno" -a "x$with_doc" != "xno"],
	[AC_MSG_WARN([not building man pages and documentation])])

AM_CONDITIONAL(ENABLE_REGENERATE_MAN,
	test "x$enable_man" != "xno" -a "x$with_doc" != "xno")

AC_OUTPUT([Makefile lib/Makefile tukit/Makefile sbin/Makefile man/Makefile \
	systemd/Makefile logrotate/Makefile dracut/Makefile doc/Makefile \
	etc/Makefile dbus/Makefile sbin/transactional-update])
