INCLUDES = @INCLUDES@ @INCLTDL@
RIGSRC = rig.c serial.c misc.c register.c event.c cal.c conf.c tones.c \
		rotator.c locator.c rot_reg.c rot_conf.c iofunc.c ext.c

lib_LTLIBRARIES = libhamlib.la
libhamlib_la_SOURCES = $(RIGSRC)
libhamlib_la_LDFLAGS = -no-undefined -release @VERSION@ -version-info 0:0:0
libhamlib_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS) -DHAMLIB_MODULE_DIR=\"$(libdir)\"
libhamlib_la_LIBADD = @LIBLTDL@ ../lib/libmisc.la @MATH_LIBS@

noinst_HEADERS = event.h misc.h serial.h iofunc.h cal.h tones.h \
					conf.h rot_conf.h token.h

