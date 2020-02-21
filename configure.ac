dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(transactional-update, 3.0)

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_17(, mandatory)

PKG_CHECK_MODULES(ECONF, libeconf)

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT