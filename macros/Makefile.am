## Please update this variable if any new macros are created

MACROS = \
	aclocal-include.m4 \
	libtool.m4 \
	pkg.m4 \
	perl.m4 \
	tcl.m4


EXTRA_DIST=$(MACROS)
MAINTAINERCLEANFILES=macros.dep

@MAINT@macros.dep: Makefile.am
@MAINT@ @echo '$$(top_srcdir)/aclocal.m4: $(MACROS:%=macros/%)' > $@

