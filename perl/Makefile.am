
# note: VPATH building does not work yet

Hamlib/Makefile: $(top_srcdir)/perl/Hamlib/Makefile.PL
	mkdir -p Hamlib
	cp -u $(top_srcdir)/perl/Hamlib/{Changes,MANIFEST,README,Hamlib.pm,Hamlib.xs,Makefile.PL,test.pl,typemap} Hamlib/. 2> /dev/null || exit 0
	cd Hamlib && perl Makefile.PL

all: Hamlib/Makefile
	make -C Hamlib

test: all
	make -C Hamlib test

clean: Hamlib/Makefile
	make -C Hamlib clean

distclean: Hamlib/Makefile
	make -C Hamlib distclean

distcheck: Hamlib/Makefile
	make -C Hamlib distcheck

install: Hamlib/Makefile
	make -C Hamlib install

EXTRA_DIST = Hamlib/Changes Hamlib/MANIFEST Hamlib/README \
				Hamlib/Hamlib.pm Hamlib/Hamlib.xs Hamlib/Makefile.PL \
				Hamlib/test.pl Hamlib/typemap

