# hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
#
# testlibft747 - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
# This program uses libft747.so to test an API for communicating
# via serial interface to an FT-747GX using the "CAT" interface
# box (FIF-232C) or similar
#
#
# Make file for libft747.so shared lib test suite.
#
#
#	$Id: Makefile,v 1.2 2000-07-27 00:47:56 javabear Exp $	
#
#
#
#


#TEST_LIBDIR		=	../lib
#TEST_INCLUDEDIR		=	../include

LIB_NAME	=	libft747.so
LIB_HEADER	=	ft747.h

# access to common stuff
COMMON_DIR	=	../../common/

CC	=	gcc
INCLUDE	=	-I/usr/include -I/usr/local/include -I. -I../include 
OBJ	=	testlibft747.o $(COMMON_DIR)serial.o
LDFLAG	=	-lm -lft747 -L../lib
CFLAGS	=	-g -Wall -ansi -pedantic -O2 -D__USE_FIXED_PROTOTYPES__ 
MAKEALL	=	testlibft747

all:	$(MAKEALL)

testlibft747:	$(OBJ)
	$(CC) $(CFLAGS)  $(INCLUDE)  $(LDFLAG) -o testlibft747  $(OBJ)

testlibft747.o:	testlibft747.c testlibft747.h 
	$(CC)	$(CFLAGS)  $(INCLUDE) -c testlibft747.c

# what common stuff I need.
serial.o:
	(cd $(COMMON_DIR) && $(MAKE))

# clean up

clean:
	make cleanlocal

# clean up local directory

.PHONY: cleanlocal
cleanlocal:
	rm -f *.o testlibft747

# run test program in local directory

.PHONY: runtest
runtest:
	(LD_LIBRARY_PATH="../lib" ./testlibft747 )



