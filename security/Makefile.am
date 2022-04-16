ACLOCAL_AMFLAGS = -I.
EXTRA_DIST =  sctest.c

noinst_LTLIBRARIES = libsecurity.la

libsecurity_la_SOURCES = aes.c AESStringCrypt.c password.c security.c sha256.c md5.c aes.h AESStringCrypt.h password.h security.h sha256.h md5.h
LDADD = $(top_builddir)/src/libhamlib.la
