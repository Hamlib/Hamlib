TENTECSRCLIST = rx320.c rx340.c rx350.c rx331.c \
		pegasus.c argonaut.c orion.c jupiter.c omnivii.c paragon.c

pkglib_LTLIBRARIES = hamlib-tentec.la
hamlib_tentec_la_SOURCES = $(TENTECSRCLIST) tentec.c tentec2.c tt550.c
hamlib_tentec_la_LDFLAGS = -no-undefined -module -avoid-version
hamlib_tentec_la_LIBADD = $(top_builddir)/src/libhamlib.la \
			  $(top_builddir)/lib/libmisc.la

EXTRA_DIST = README Android.mk

noinst_HEADERS = tentec.h tentec2.h tt550.h orion.h rx331.h
