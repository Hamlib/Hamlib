# hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
#
#
# Simple  Make file for common parts.
#
#
#	$Id: Makefile,v 1.4 2000-09-23 02:26:27 javabear Exp $	
#
#

CC	=	gcc
INCLUDE	=	-I/usr/include -I/usr/local/include -I. -I../include
OBJ	=	serial.o rig.o testrig.o
LDFLAG	=	-lm 
# -ansi would break usleep, CRTSCTS, etc.
CFLAGS	=	-g -Wall -D__USE_FIXED_PROTOTYPES__ -pedantic # -ansi -O2
MAKEALL	=	$(OBJ)

all:	$(MAKEALL)

serial.o:	serial.c serial.h 
	$(CC)	$(CFLAGS)  $(INCLUDE) -c serial.c

rig.o: rig.c rig.h riglist.h
	$(CC)	$(CFLAGS)  $(INCLUDE) -c rig.c

testrig.o:	testrig.c 
	$(CC)	$(CFLAGS)  $(INCLUDE) -c testrig.c

# clean up

clean:
	make cleanlocal

# clean up local directory

.PHONY: cleanlocal
cleanlocal:
	rm -f *.o 


