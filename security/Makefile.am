EXTRA_DIST =  sctest.c
#security_test_SOURCES = security_test.c aes.h AESStringCrypt.h password.h security.h sha256.h

#security_test_LIBS = libsecurity.la
#security_test_LDFLAGS = -L.
#security_test_CFLAGS = -L. -I.

#bin_PROGRAMS = security_test
noinst_LTLIBRARIES = libsecurity.la
#noinst_PROGRAMS = security_test

libsecurity_la_SOURCES = aes.c AESStringCrypt.c password.c security.c sha256.c aes.h AESStringCrypt.h  password.h  security.h  sha256.h
libsecurity_la_CFLAGS = -I$(srcdir)
LDADD = $(top_builddir)/src/libhamlib.la $(top_builddir)/security/libsecurity.la
#security_test_LDADD = $(LDADD)

#check_PROGRAMS = security_test
