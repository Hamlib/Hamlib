INCLUDES = @INCLUDES@ @INCLTDL@
RIGSRC = rig.c serial.c misc.c register.c event.c cal.c conf.c tones.c \
		rotator.c locator.c rot_reg.c rot_conf.c iofunc.c ext.c \
		mem.c settings.c parallel.c usb_port.c debug.c network.c

lib_LTLIBRARIES = libhamlib.la
libhamlib_la_SOURCES = $(RIGSRC)
libhamlib_la_LDFLAGS = $(WINLDFLAGS) -no-undefined -version-info @ABI_VERSION@:8:0
libhamlib_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS) -DHAMLIB_MODULE_DIR=\"$(libdir)\"
libhamlib_la_LIBADD = @LIBLTDL@ $(top_builddir)/lib/libmisc.la \
		      @NET_LIBS@ @MATH_LIBS@ $(LIBUSB_LIBS)

noinst_HEADERS = event.h misc.h serial.h iofunc.h cal.h tones.h \
		rot_conf.h token.h idx_builtin.h register.h par_nt.h \
		parallel.h usb_port.h network.h

