
# note: VPATH building does not work yet

Hamlib-pl.mk: $(top_srcdir)/perl/Makefile.PL
	cp -u $(top_srcdir)/perl/{Changes,MANIFEST,README,Hamlib.pm,Hamlib.xs,Makefile.PL,test.pl,typemap} . 2> /dev/null || exit 0
	perl Makefile.PL MAKEFILE=Hamlib-pl.mk INST_MAN3DIR=$(mandir) \
			LIB=$(libdir) CCFLAGS=$(CFLAGS) INC="$(INCLUDES)" \
			LIBS="-L$(top_builddir)/src/.libs -lhamlib"

all: Hamlib-pl.mk
	make -f Hamlib-pl.mk

test: all
	make -f Hamlib-pl.mk test

clean: Hamlib-pl.mk
	make -f Hamlib-pl.mk clean

distclean: Hamlib-pl.mk
	make -f Hamlib-pl.mk distclean

distcheck: Hamlib-pl.mk
	make -f Hamlib-pl.mk distcheck

install: Hamlib-pl.mk
	make -f Hamlib-pl.mk install

EXTRA_DIST = Changes MANIFEST README Hamlib.pm Hamlib.xs Makefile.PL \
				test.pl typemap

