#
#
# Make file for ICOM program shared lib
#
# creates:	libicom.so
#
#	$Id: Makefile,v 1.1 2000-09-16 01:43:48 f4cfe Exp $	
#
#
# .h files go in INSTALL_INCLUDEDIR
# .so files go in INSTALL_LIBDIR
#
#


INSTALL_LIBDIR		=	./lib/
INSTALL_INCLUDEDIR	=	./include/
INCLUDE			=	-I../common

LIB_NAME	=	libicom.so
LIB_SONAME	=	libicom.so.1
LIB_RELEASE	=	libicom.so.1.0.1
LIB_HEADER	=	icom.h
LIB_SRC		=	icom.c ic706.c
LIB_OBJECTS	=	icom.o ic706.o

CC		=	gcc
CFLAGS 		=	-fPIC -g -Wall $(INCLUDE) # -O2

all:	lib

.PHONY:	lib
lib: $(LIB_OBJECTS) $(LIB_HEADER)
	$(CC) -shared -Wl,-soname,$(LIB_SONAME) -o $(LIB_RELEASE) $(LIB_OBJECTS) -lc

icom.c: icom.h icom_defs.h
ic706.c: icom.h icom_defs.h

# install header and lib

install:
	make install_lib
	make install_header

# install lib in MYLIBDIR

.PHONY:	install_lib
install_lib:
	mv $(LIB_RELEASE) $(INSTALL_LIBDIR)
	cd $(INSTALL_LIBDIR); /sbin/ldconfig -n .
	cd $(INSTALL_LIBDIR); ln -s $(LIB_SONAME) $(LIB_NAME)

# install libicom.h in INSTALL_INCLUDEDIR

.PHONY:	install_header
install_header:
	cp -f $(LIB_HEADER) $(INSTALL_INCLUDEDIR)

# build lib and install and build test suite, but DONT run it

.PHONY:	build_all
build_all:
	make all
	make install
	(cd test && $(MAKE) all)

# build everything and run test suite ( stand back ..)

.PHONY:	verify
verify:
	make build_all
	(cd test && $(MAKE) runtest)

# clean up local directory, my include and lib
# directories also.

clean:
	make cleanlocal
	make cleanlib
	make cleaninclude

# clean up local directory, my include and lib and test
# directories also.
.PHONY:	cleanall
cleanall:
	make cleanlocal
	make cleanlib
	make cleaninclude
	make cleantest


# clean up local directory

.PHONY: cleanlocal
cleanlocal:
	rm -f *.o *.so*

# clean up local lib directory

.PHONY: cleanlib
cleanlib:
	cd $(INSTALL_LIBDIR); rm -f $(LIB_NAME)*



# clean up local include directory

.PHONY: cleaninclude
cleaninclude:
	cd $(INSTALL_INCLUDEDIR); rm -f $(LIB_HEADER)


# clean up test directory

.PHONY: cleantest
cleantest:
	(cd test && $(MAKE) clean)


