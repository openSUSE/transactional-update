dnl Process this file with autoconf to produce a configure script.
AC_INIT(transactional-update, 2.21.1)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([sbin/transactional-update.in])
AC_PREFIX_DEFAULT(/usr)

AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

if test ${libexecdir} = '${exec_prefix}/libexec'
then
  libexecdir='${exec_prefix}/lib'
  if test "${exec_prefix}" = "NONE"
  then
    TMPFILESDIR=${prefix}/lib/tmpfiles.d
    ISSUEDIR=${prefix}/lib/issue.d
    UDEVRULESDIR=${prefix}/lib/udev/rules.d
    SYSTEMDDIR=${prefix}/lib/systemd/system
    DRACUTDIR=${prefix}/lib/dracut/modules.d
  else
    TMPFILESDIR=${exec_prefix}/lib/tmpfiles.d
    ISSUEDIR=${exec_prefix}/lib/issue.d
    UDEVRULESDIR=${exec_prefix}/lib/udev/rules.d
    SYSTEMDDIR=${exec_prefix}/lib/systemd/system
    DRACUTDIR=${exec_prefix}/lib/dracut/modules.d
  fi
else
  TMPFILESDIR=${libexecdir}/tmpfiles.d
  ISSUEDIR=${libexecdir}/issue.d
  UDEVRULESDIR=${libexecdir}/udev/rules.d
  SYSTEMDDIR=${libexecdir}/systemd/system
  DRACUTDIR=${libexecdir}/dracut/modules.d
fi
AC_SUBST(TMPFILESDIR)
AC_SUBST(ISSUEDIR)
AC_SUBST(UDEVRULESDIR)
AC_SUBST(SYSTEMDDIR)
AC_SUBST(DRACUTDIR)
LOGROTATEDDIR=${sysconfdir}/logrotate.d
AC_SUBST(LOGROTATEDDIR)

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

dnl
dnl Check for librpm
dnl
PKG_CHECK_MODULES([LIBRPM], [rpm >= 4.15], AC_DEFINE([HAVE_RPMDBCOOKIE]),
	[PKG_CHECK_MODULES([LIBRPM], [rpm])])

AC_ARG_WITH([doc],
	[AS_HELP_STRING([--with-doc], [Build documentation])], ,
	[enable_man=yes])

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

AC_PATH_PROG([BROWSER], [w3m])
if test ! -z "$BROWSER"; then
     BROWSER="$BROWSER -T text/html -dump"
else
     AC_PATH_PROG([BROWSER], [links])
     if test ! -z "$BROWSER"; then
          BROWSER="$BROWSER -no-numbering -no-references -dump"
     fi
fi

AC_OUTPUT([Makefile sbin/Makefile man/Makefile systemd/Makefile \
	logrotate/Makefile dracut/Makefile doc/Makefile etc/Makefile \
	sbin/transactional-update])
