dnl Initialize the autoconf settings

AC_INIT([libjustify], [0.1.1])
AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])

dnl set version info for libhnj.so to package rev - $MAJOR + $MINOR : $MICRO : $MINOR
LIBJUSTIFY_VERSION_INFO=1:1:0

# Version
m4_define(libjustify_version_triplet,m4_split(AC_PACKAGE_VERSION,[[.]]))
m4_define(libjustify_major_version,m4_argn(1,libjustify_version_triplet))
m4_define(libjustify_minor_version,m4_argn(2,libjustify_version_triplet))
m4_define(libjustify_micro_version,m4_argn(3,libjustify_version_triplet))
LIBJUSTIFY_MAJOR_VERSION=libjustify_major_version
LIBJUSTIFY_MINOR_VERSION=libjustify_minor_version
LIBJUSTIFY_MICRO_VERSION=libjustify_micro_version
LIBJUSTIFY_VERSION=AC_PACKAGE_VERSION

AC_SUBST(LIBJUSTIFY_MAJOR_VERSION)
AC_SUBST(LIBJUSTIFY_MINOR_VERSION)
AC_SUBST(LIBJUSTIFY_MICRO_VERSION)
AC_SUBST(LIBJUSTIFY_VERSION)
AC_SUBST(LIBJUSTIFY_VERSION_INFO)

AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIRS([m4])

AC_PROG_CC
AC_PROG_CPP
AM_PROG_LIBTOOL

AC_CHECK_HEADER(hyphen.h, [], [ AC_MSG_ERROR(libhyphen headers not found.)], [])

AC_OUTPUT([Makefile
libjustify-config])
