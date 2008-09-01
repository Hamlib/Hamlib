
TSSRCLIST = ts850.c ts870s.c ts570.c ts450s.c ts950.c ts50s.c \
		ts790.c ts2000.c k2.c k3.c ts930.c \
		ts680.c ts690.c ts140.c ts480.c

IC10SRCLIST = ts440.c ts940.c ts711.c ts811.c r5000.c

THSRCLIST = thd7.c thf7.c thg71.c tmd700.c tmv7.c

lib_LTLIBRARIES = hamlib-kenwood.la
hamlib_kenwood_la_SOURCES = $(TSSRCLIST) $(THSRCLIST) $(IC10SRCLIST) \
			    	kenwood.c th.c ic10.c

hamlib_kenwood_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_kenwood_la_LIBADD = $(top_builddir)/src/libhamlib.la @MATH_LIBS@

noinst_HEADERS = kenwood.h th.h ic10.h
