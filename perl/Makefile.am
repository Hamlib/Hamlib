
Hamlib/Makefile: Hamlib/Makefile.PL
	cd Hamlib && perl Makefile.PL

all: Hamlib/Makefile
	make -C Hamlib

test: all
	make -C Hamlib test

