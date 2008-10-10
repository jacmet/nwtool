#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(nwtool,0.0.1,peter.korsgaard@barco.com)
AM_INIT_AUTOMAKE

# dependencies

m4_define([LIBHID_VERSION_REQ], 0.2.0)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_LANG_C

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
PKG_CHECK_MODULES(LIBHID, libhid)


#AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([memset strerror strtol])

#AC_CONFIG_FILES([Makefile])
AC_OUTPUT(Makefile src/Makefile)
