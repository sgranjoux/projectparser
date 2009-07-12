dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

AC_INIT(projectparser, 0.1)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

GLIB_REQUIRED=2.16.0
GTK_REQUIRED=2.12.10
GCONF_REQUIRED=2.12.0

AC_SUBST(GLIB_REQUIRED)
AC_SUBST(GTK_REQUIRED)
AC_SUBST(GCONF_REQUIRED)

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_LEX
AC_PROG_YACC
AC_HEADER_STDC

dnl ***************************************************************************
dnl Internatinalization
dnl ***************************************************************************
GETTEXT_PACKAGE=projectparser
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

BISON_I18N


AM_PROG_LIBTOOL

PKG_CHECK_MODULES(GLIB, glib-2.0 >= $GLIB_REQUIRED gobject-2.0 >= $GLIB_REQUIRED gmodule-2.0 >= $GLIB_REQUIRED gthread-2.0 >= $GLIB_REQUIRED gio-2.0 >= $GLIB_REQUIRED)

PKG_CHECK_MODULES(GTK, gtk+-2.0 >= $GTK_REQUIRED)

PKG_CHECK_MODULES(GCONF, gconf-2.0 >= $GCONF_REQUIRED)

dnl Check for function forkpty in libutil
AC_CHECK_LIB(util, forkpty)

AC_CONFIG_FILES([
Makefile
src/Makefile
libanjuta/Makefile
tests/Makefile
po/Makefile.in
])

TESTLOGFILE=tests.log
AC_SUBST(TESTLOGFILE)

AC_CONFIG_FILES([
tests/test0.sh
tests/test1.sh
tests/test2.sh
tests/test3.sh
tests/test4.sh
tests/test5.sh], [
chmod +x tests/test0.sh
chmod +x tests/test1.sh
chmod +x tests/test2.sh
chmod +x tests/test3.sh
chmod +x tests/test4.sh
chmod +x tests/test5.sh])

AC_OUTPUT
