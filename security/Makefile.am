EXTRA_DIST =  sctest.c
security_test_SOURCES = security_test.c aes.h AESStringCrypt.h password.h security.h sha256.h md5.c md5.h

security_test_LIBS = libsecurity.la
security_test_LDFLAGS = -L.
security_test_CFLAGS = -L. -I.

noinst_LTLIBRARIES = libsecurity.la

libsecurity_la_SOURCES = aes.c AESStringCrypt.c password.c security.c sha256.c md5.c
LDADD = $(top_builddir)/src/libhamlib.la
security_test_LDADD = $(LDADD)
