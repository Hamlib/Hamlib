#noinst_SCRIPTS = hamlib-doc split-man.pl
#EXTRA_DIST = hamlib-doc split-man.pl NOTES

# hamlib-doc -man $(find -name '*.c' '*.h') | split-man.pl /tmp/man
# FIXME: this might be broken with VPATH make
#hamlib-doc-man: ../src/rig.c hamlib-doc split-man.pl
#	./hamlib-doc -man ../src/rig.c | ./split-man.pl man/

#hamlib-doc-html: ../src/rig.c hamlib-doc
#	./hamlib-doc -html ../src/rig.c > html/hamlib.html

doc: hamlib.cfg ../src/rig.c
	doxygen hamlib.cfg

#man_MANS= man/man3/rig.c.3
