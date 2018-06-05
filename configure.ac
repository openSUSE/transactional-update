dnl Process this file with autoconf to produce a configure script.
AC_INIT(transactional-update, 2.4)
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
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S

AX_CHECK_COMPILE_FLAG([-std=c++11], [CXXFLAGS="$CXXFLAGS -std=c++11"])

dnl
dnl Check for libzypp
dnl
PKG_CHECK_MODULES(LIBZYPP, libzypp)

dnl
dnl Check for librpm
dnl
PKG_CHECK_MODULES([LIBRPM], [rpm])

dnl
dnl Check for xsltproc
dnl
enable_man=yes
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
AM_CONDITIONAL(ENABLE_REGENERATE_MAN, test x$enable_man != xno)

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
