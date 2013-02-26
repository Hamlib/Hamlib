EXTRA_DIST = hamlib.cfg index.doxygen hamlib.css footer.html

dist_doc_DATA = hamlib.html hamlib.pdf

SRCDOCLST = ../src/rig.c ../src/rotator.c ../src/tones.c ../src/locator.c \
		../src/event.c ../src/conf.c ../src/mem.c ../src/settings.c

info_TEXINFOS = hamlib.texi

AM_MAKEINFOHTMLFLAGS = --no-headers --no-split

doc: hamlib.cfg $(SRCDOCLST)
	doxygen hamlib.cfg
