# src/Makefile.am

# $(LTDLINCL) is set by LTDL_INIT macro in configure.ac
AM_CPPFLAGS = @AM_CPPFLAGS@ $(LTDLINCL)
# AM_CPPFLAGS = @AM_CPPFLAGS@

RIGSRC = rig.c serial.c misc.c register.c event.c cal.c conf.c tones.c \
		rotator.c locator.c rot_reg.c rot_conf.c iofunc.c ext.c \
		mem.c settings.c parallel.c usb_port.c debug.c network.c \
		cm108.c

lib_LTLIBRARIES = libhamlib.la
libhamlib_la_SOURCES = $(RIGSRC)
libhamlib_la_LDFLAGS = $(WINLDFLAGS) $(OSXLDFLAGS) -no-undefined -version-info @ABI_VERSION@:@ABI_REVISION@:@ABI_AGE@
libhamlib_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS) -DHAMLIB_MODULE_DIR=\"$(pkglibdir)\"

# $(LIBLTDL) is set by LTDL_INIT macro
libhamlib_la_LIBADD = $(LIBLTDL) $(top_builddir)/lib/libmisc.la \
		      @BACKENDEPS@ @ROT_BACKENDEPS@ \
		      @NET_LIBS@ @MATH_LIBS@ $(LIBUSB_LIBS)

# libhamlib_la_DEPENDENCIES = $(LTDLDEPS) $(top_builddir)/lib/libmisc.la @BACKENDEPS@ @ROT_BACKENDEPS@
libhamlib_la_DEPENDENCIES = $(LTDLDEPS) $(top_builddir)/lib/libmisc.la
# libhamlib_la_DEPENDENCIES = $(top_builddir)/lib/libmisc.la

noinst_HEADERS = event.h misc.h serial.h iofunc.h cal.h tones.h \
		rot_conf.h token.h idx_builtin.h register.h par_nt.h \
		parallel.h usb_port.h network.h cm108.h

EXTRA_DIST = Android.mk
