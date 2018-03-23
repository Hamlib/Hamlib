EXTRA_DIST = hamlib.cfg index.doxygen hamlib.css footer.html \
	Hamlib_design.eps Hamlib_design.png

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
