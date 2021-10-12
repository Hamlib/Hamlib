EXTRA_DIST = hamlib.cfg index.doxygen hamlib.css footer.html hamlib.png

dist_man_MANS = man1/ampctl.1 man1/ampctld.1 \
	man1/rigctl.1 man1/rigctld.1 man1/rigmem.1 man1/rigsmtr.1 \
	man1/rigswr.1 man1/rotctl.1 man1/rotctld.1 man1/rigctlcom.1 \
	man7/hamlib.7 man7/hamlib-primer.7 man7/hamlib-utilities.7

SRCDOCLST = \
	../include/hamlib/amplifier.h \
	../include/hamlib/amplist.h \
	../include/hamlib/rig.h \
	../include/hamlib/rotator.h \
	../include/hamlib/rotlist.h \
	../src/amp_conf.c \
	../src/amp_settings.c \
	../src/amplifier.c \
	../src/cal.c \
	../src/cm108.c \
	../src/conf.c \
	../src/debug.c \
	../src/event.c \
	../src/ext.c \
	../src/extamp.c \
	../src/locator.c \
	../src/mem.c \
	../src/misc.c \
	../src/network.c \
	../src/parallel.c \
	../src/rig.c \
	../src/rot_conf.c \
	../src/rot_ext.c \
	../src/rot_settings.c \
	../src/rotator.c \
	../src/tones.c \
	../src/serial.c \
	../src/settings.c \
	../src/sleep.c \
	../src/token.h \
	../src/tones.c \
	../src/usb_port.c

# Just the file names, the paths are assigned below.
SCRIPTSLST = build-w32.sh build-w64.sh


# Use GNU source-highlight to generate highlighted shell scripts for the
# Doxygen manual.
doc: hamlib.cfg $(SRCDOCLST)
	for script in $(SCRIPTSLST) ; \
	  do \
	    source-highlight -n -f html -i $(top_srcdir)/scripts/$${script} -o $(top_builddir)/scripts/$${script}.html ; \
	    source-highlight -n -f latex -i $(top_srcdir)/scripts/$${script} -o $(top_builddir)/scripts/$${script}.tex ; \
	  done
	doxygen hamlib.cfg

clean-local:
	-rm -rf $(top_builddir)/doc/html $(top_builddir)/doc/latex
