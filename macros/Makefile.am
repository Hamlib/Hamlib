## Please update this variable if any new macros are created

MACROS = \
	ax_append_flag.m4 \
	ax_cflags_warn_all.m4 \
	ax_pkg_swig.m4 \
	ax_pthread.m4 \
	ax_python_devel.m4 \
	ax_lib_indi.m4 \
	ax_lib_nova.m4 \
    ax_cxx_compile_stdcxx.m4 \
	gr_doxygen.m4 \
	gr_pwin32.m4 \
	hl_getaddrinfo.m4 \
	perl.m4 \
	tcl.m4


EXTRA_DIST=$(MACROS)
MAINTAINERCLEANFILES=macros.dep

@MAINT@macros.dep: Makefile.am
@MAINT@ @echo '$$(top_srcdir)/aclocal.m4: $(MACROS:%=macros/%)' > $@
