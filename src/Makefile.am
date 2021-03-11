# src/Makefile.am

RIGSRC = rig.c serial.c serial.h misc.c misc.h register.c register.h event.c \
	event.h cal.c cal.h conf.c tones.c tones.h rotator.c locator.c rot_reg.c \
	rot_conf.c rot_conf.h rot_settings.c rot_ext.c iofunc.c iofunc.h ext.c \
   	mem.c settings.c parallel.c parallel.h usb_port.c usb_port.h debug.c \
   	network.c network.h cm108.c cm108.h gpio.c gpio.h idx_builtin.h token.h \
   	par_nt.h microham.c microham.h amplifier.c amp_reg.c amp_conf.c \
   	amp_conf.h amp_settings.c extamp.c sleep.c sleep.h sprintflst.c \
   	sprintflst.h

lib_LTLIBRARIES = libhamlib.la
libhamlib_la_SOURCES = $(RIGSRC)
libhamlib_la_LDFLAGS = $(WINLDFLAGS) $(OSXLDFLAGS) -no-undefined -version-info $(ABI_VERSION):$(ABI_REVISION):$(ABI_AGE)

libhamlib_la_LIBADD = $(top_builddir)/lib/libmisc.la \
	$(BACKENDEPS) $(RIG_BACKENDEPS) $(ROT_BACKENDEPS) $(AMP_BACKENDEPS) $(NET_LIBS) $(MATH_LIBS) $(LIBUSB_LIBS) $(INDI_LIBS)

libhamlib_la_DEPENDENCIES = $(top_builddir)/lib/libmisc.la $(BACKENDEPS) $(RIG_BACKENDEPS) $(ROT_BACKENDEPS) $(AMP_BACKENDEPS)

EXTRA_DIST = Android.mk
