# hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
#
#
# Simple  Make file for common parts.
#
#
#	$Id: Makefile,v 1.1 2000-07-25 01:01:00 javabear Exp $	
#
#

CC	=	gcc
INCLUDE	=	-I/usr/include -I/usr/local/include -I. -I../include
OBJ	=	serial.o
LDFLAG	=	-lm 
CFLAGS	=	-g -Wall -ansi -pedantic -O2 -D__USE_FIXED_PROTOTYPES__ 
MAKEALL	=	serial.o

all:	$(MAKEALL)

serial.o:	serial.c serial.h 
	$(CC)	$(CFLAGS)  $(INCLUDE) -c serial.c

# clean up

clean:
	make cleanlocal

# clean up local directory

.PHONY: cleanlocal
cleanlocal:
	rm -f *.o serial


