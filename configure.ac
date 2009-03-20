#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(nwtool,0.0.4,peter.korsgaard@barco.com)
AM_INIT_AUTOMAKE
#AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_LANG_C

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

# Check for libhid
AC_ARG_ENABLE(usb,
	AS_HELP_STRING([--disable-usb],[Disable USB support]),
	[case "${enableval}" in
		yes) WITH_USB=yes ;;
		no)  WITH_USB=no ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-usb) ;;
	esac],
	[WITH_USB=yes])

	if test "x$WITH_USB" = "xyes" ; then
		PKG_CHECK_MODULES(LIBHID, libhid)
		AC_SUBST(LIBHID_CFLAGS)
		AC_SUBST(LIBHID_LIBS)
	fi
AM_CONDITIONAL(WITH_USB, test "x$WITH_USB" = "xyes")

#AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([memset strerror strtol])

#AC_CONFIG_FILES([Makefile])
AC_OUTPUT(Makefile src/Makefile)
