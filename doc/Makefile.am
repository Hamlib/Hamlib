
INCLUDES = @INCLUDES@

EXTRA_DIST = hamlib.cfg index.doxygen hamlib.css footer.html

SRCDOCLST = ../src/rig.c ../src/rotator.c ../src/tones.c ../src/locator.c \
		../src/event.c ../src/conf.c ../src/mem.c ../src/settings.c

doc: hamlib.cfg $(SRCDOCLST)
	doxygen hamlib.cfg

#man_MANS= man/man3/rig.c.3
