#
#
# Make file for FT-747GX CAT program shared lib
#
# creates:	libft747.so
#
#	$Id: Makefile,v 1.1 2000-07-18 20:53:46 frank Exp $	
#
#
# .h files go in INSTALL_INCLUDEDIR
# .so files go in INSTALL_LIBDIR
#
#


INSTALL_LIBDIR		=	./lib/
INSTALL_INCLUDEDIR	=	./include/

LIB_NAME	=	libft747.so
LIB_SONAME	=	libft747.so.1
LIB_RELEASE	=	libft747.so.1.0.1
LIB_HEADER	=	ft747.h
LIB_SRC		=	ft747.c
LIB_OBJECTS	=	ft747.o

all:	lib

.PHONY:	lib
lib:	
	gcc -fPIC -g -Wall -c $(LIB_SRC)
	gcc -shared -Wl,-soname,$(LIB_SONAME) -o $(LIB_RELEASE) $(LIB_OBJECTS) -lc

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

# install libft747.h in INSTALL_INCLUDEDIR

.PHONY:	install_header
install_header:
	cp -f $(LIB_HEADER) $(INSTALL_INCLUDEDIR)


# clean up local directory, my include and lib
# directories also.

clean:
	make cleanlocal
	make cleanlib
	make cleaninclude

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





