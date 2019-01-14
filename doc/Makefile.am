EXTRA_DIST = hamlib.cfg index.doxygen hamlib.css footer.html \
	Hamlib_design.eps Hamlib_design.png

dist_man_MANS = man1/rigctl.1 man1/rigctld.1 man1/rigmem.1 man1/rigsmtr.1 \
	man1/rigswr.1 man1/rotctl.1 man1/rotctld.1 man1/rigctlcom.1 \
	man7/hamlib.7 man7/hamlib-primer.7 man7/hamlib-utilities.7

htmldir = $(docdir)/html
dist_html_DATA = Hamlib_design.png hamlib.html

SRCDOCLST = ../src/rig.c ../src/rotator.c ../src/tones.c ../src/locator.c \
	../src/event.c ../src/conf.c ../src/mem.c ../src/settings.c

info_TEXINFOS = hamlib.texi
hamlib_TEXINFOS = nutshell.texi getting_started.texi utility_programs.texi \
	fdl.texi

AM_MAKEINFOHTMLFLAGS = --no-headers --no-split

doc: hamlib.cfg $(SRCDOCLST)
	doxygen hamlib.cfg

# Don't attempt to generate DVI file with 'make distcheck'
dvi:
