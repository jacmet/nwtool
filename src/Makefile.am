sbin_PROGRAMS = nwtool
nwtool_SOURCES = nwtool-serial.c nwtool.c

if WITH_USB

AM_CFLAGS = $(LIBHID_CFLAGS) -DWITH_USB
nwtool_SOURCES += nwtool-usb.c
nwtool_LDADD = $(LIBHID_LIBS)

endif
