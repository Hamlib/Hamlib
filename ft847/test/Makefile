# hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
#
# testlibft847 - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
# This program uses libft847.so to test an API for communicating
# via serial interface to an FT-847 using the "CAT" interface.
#
#
# Make file for libft847.so shared lib test suite.
#
#
#	$Id: Makefile,v 1.1 2000-07-25 01:23:20 javabear Exp $	
#
#
#
#


#TEST_LIBDIR		=	../lib
#TEST_INCLUDEDIR		=	../include

LIB_NAME	=	libft847.so
LIB_HEADER	=	ft847.h

# access to common stuff
COMMON_DIR	=	../../common/

#LIB_SONAME	=	libft847.so.1
#LIB_RELEASE	=	libft847.so.1.0.1


CC	=	gcc
INCLUDE	=	-I/usr/include -I/usr/local/include -I. -I../include
OBJ	=	testlibft847.o $(COMMON_DIR)serial.o
LDFLAG	=	-lm -lft847 -L../lib
CFLAGS	=	-g -Wall -ansi -pedantic -O2 -D__USE_FIXED_PROTOTYPES__ 
MAKEALL	=	testlibft847

all:	$(MAKEALL)

testlibft847:	$(OBJ)
	$(CC) $(CFLAGS)  $(INCLUDE)  $(LDFLAG) -o testlibft847  $(OBJ)

testlibft847.o:	testlibft847.c testlibft847.h 
	$(CC)	$(CFLAGS)  $(INCLUDE) -c testlibft847.c

# what common stuff I need.
serial.o:
	(cd $(COMMON_DIR) && $(MAKE))

# clean up

clean:
	make cleanlocal

# clean up local directory

.PHONY: cleanlocal
cleanlocal:
	rm -f *.o testlibft847

# run test program in local directory

.PHONY: runtest
runtest:
	(LD_LIBRARY_PATH="../lib" ./testlibft847 )


























