EXTRA_DIST =  sctest.c
security_test_SOURCES = security_test.c
security_test_LIBS = libsecurity.la
security_test_LDADD = -lhamlib
security_test_LDFLAGS = -L.

noinst_LTLIBRARIES = libsecurity.la

libsecurity_la_SOURCES = aes.c AESStringCrypt.c password.c security.c sha256.c
LDADD = $(top_builddir)/src/libhamlib.la
security_test_LDADD = $(LDADD)

check_PROGRAMS = security_test
