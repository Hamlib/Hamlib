# hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
#
#
# Simple  Make file for common parts.
#
#
#	$Id: Makefile,v 1.3 2000-09-16 01:21:07 f4cfe Exp $	
#
#

CC	=	gcc
INCLUDE	=	-I/usr/include -I/usr/local/include -I. -I../include
OBJ	=	serial.o
LDFLAG	=	-lm 
# -ansi would break usleep, CRTSCTS, etc.
CFLAGS	=	-g -Wall -D__USE_FIXED_PROTOTYPES__ -pedantic # -ansi -O2
MAKEALL	=	serial.o

all:	$(MAKEALL)

serial.o:	serial.c serial.h 
	$(CC)	$(CFLAGS)  $(INCLUDE) -c serial.c

rig.o: rig.c rig.h riglist.h
	$(CC)	$(CFLAGS)  $(INCLUDE) -c rig.c


# clean up

clean:
	make cleanlocal

# clean up local directory

.PHONY: cleanlocal
cleanlocal:
	rm -f *.o serial


