INCLUDES = @INCLUDES@ $(INCLTDL)
RIGSRC = rig.c serial.c misc.c register.c event.c cal.c conf.c

lib_LTLIBRARIES = libhamlib.la
libhamlib_la_SOURCES = $(RIGSRC)
libhamlib_la_LDFLAGS = -no-undefined -release @VERSION@
libhamlib_la_LIBADD = $(LIBLTDL)
libhamlib_la_DEPENDENCIES = $(LIBLTDL)

lib_LIBRARIES = libhamlib.a
libhamlib_a_SOURCES = $(RIGSRC)
libhamlib_a_LIBADD = $(LIBLTDL)
libhamlib_a_DEPENDENCIES = $(LIBLTDL)

noinst_HEADERS = event.h misc.h serial.h cal.h conf.h tone_tbl.h

