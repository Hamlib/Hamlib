# src/Makefile.am

# $(LTDLINCL) is set by LTDL_INIT macro in configure.ac
AM_CPPFLAGS = @AM_CPPFLAGS@ $(LTDLINCL)

RIGSRC = rig.c serial.c serial.h misc.c misc.h register.c register.h event.c \
	event.h cal.c cal.h conf.c tones.c tones.h rotator.c locator.c rot_reg.c \
	rot_conf.c rot_conf.h iofunc.c iofunc.h ext.c mem.c settings.c \
	parallel.c parallel.h usb_port.c usb_port.h debug.c network.c network.h \
	cm108.c cm108.h idx_builtin.h token.h par_nt.h

lib_LTLIBRARIES = libhamlib.la
libhamlib_la_SOURCES = $(RIGSRC)
libhamlib_la_LDFLAGS = $(WINLDFLAGS) $(OSXLDFLAGS) -no-undefined -version-info @ABI_VERSION@:@ABI_REVISION@:@ABI_AGE@
# libhamlib_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS) -DHAMLIB_MODULE_DIR=\"$(pkglibdir)\"

# $(LIBLTDL) is set by LTDL_INIT macro
libhamlib_la_LIBADD = $(LIBLTDL) $(top_builddir)/lib/libmisc.la \
		      @BACKENDEPS@ @ROT_BACKENDEPS@ \
		      @NET_LIBS@ @MATH_LIBS@ $(LIBUSB_LIBS)

libhamlib_la_DEPENDENCIES = $(LTDLDEPS) $(top_builddir)/lib/libmisc.la

EXTRA_DIST = Android.mk
