INCLUDES = @INCLUDES@ @INCLTDL@
RIGSRC = rig.c serial.c misc.c register.c event.c cal.c conf.c tones.c

lib_LTLIBRARIES = libhamlib.la
libhamlib_la_SOURCES = $(RIGSRC)
libhamlib_la_LDFLAGS = -no-undefined -release @VERSION@ -version-info 0:0:0
libhamlib_la_CFLAGS = -DIN_HAMLIB $(AM_CFLAGS)
libhamlib_la_LIBADD = @LIBLTDL@ ../lib/libmisc.la

noinst_HEADERS = event.h misc.h serial.h cal.h conf.h tones.h

