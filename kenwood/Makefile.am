
TSSRCLIST = ts850.c ts870s.c ts570.c ts450s.c ts440.c ts950.c ts50s.c \
		ts790.c thd7.c thf7.c thg71.c ts2000.c k2.c ts930.c

lib_LTLIBRARIES = hamlib-kenwood.la
hamlib_kenwood_la_SOURCES = $(TSSRCLIST) kenwood.c th.c
hamlib_kenwood_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_kenwood_la_LIBADD = $(top_builddir)/src/libhamlib.la @MATH_LIBS@

noinst_HEADERS = kenwood.h th.h
