EXTRA_DIST =  sctest.c

noinst_LTLIBRARIES = libsecurity.la

libsecurity_la_SOURCES = aes.c AESStringCrypt.c password.c security.c sha256.c aes.h AESStringCrypt.h  password.h  security.h  sha256.h
libsecurity_la_CFLAGS = -I$(srcdir)
LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/security/libsecurity.la
