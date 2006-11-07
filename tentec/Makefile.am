TENTECSRCLIST = rx320.c rx340.c rx350.c \
		pegasus.c argonaut.c orion.c jupiter.c

lib_LTLIBRARIES = hamlib-tentec.la
hamlib_tentec_la_SOURCES = $(TENTECSRCLIST) tentec.c tentec2.c tt550.c
hamlib_tentec_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_tentec_la_LIBADD = $(top_builddir)/src/libhamlib.la \
			  $(top_builddir)/lib/libmisc.la

noinst_HEADERS = tentec.h tentec2.h tt550.h orion.h
