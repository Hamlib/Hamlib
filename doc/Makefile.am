EXTRA_DIST = hamlib.cfg index.doxygen hamlib.css footer.html

dist_man_MANS = man1/ampctl.1 man1/ampctld.1 \
	man1/rigctl.1 man1/rigctld.1 man1/rigmem.1 man1/rigsmtr.1 \
	man1/rigswr.1 man1/rotctl.1 man1/rotctld.1 man1/rigctlcom.1 \
	man7/hamlib.7 man7/hamlib-primer.7 man7/hamlib-utilities.7

SRCDOCLST = ../src/rig.c ../src/rotator.c ../src/tones.c ../src/locator.c \
	../src/event.c ../src/conf.c ../src/mem.c ../src/settings.c

doc: hamlib.cfg $(SRCDOCLST)
	doxygen hamlib.cfg
