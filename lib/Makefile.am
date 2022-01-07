EXTRA_DIST = getopt.c getopt.h getopt_long.c usleep.c \
	termios.c win32termios.h gettimeofday.c getaddrinfo.c

noinst_LTLIBRARIES = libmisc.la

libmisc_la_SOURCES = cJSON.c cJSON.h asyncpipe.c asyncpipe.h
libmisc_la_LIBADD = $(LTLIBOBJS) $(NET_LIBS)
