# hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
#
# libhamlib - (C) 2000 Frank Singleton and Stephane Fillod
# This shared library provides a frontend API for communicating
# to backend shared libs for control of Ham Radio Equipment,
# via serial  interface.
#
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# 
#
# Simple  Make file for common parts, and modified to create 
# libhamlib.so -- FS
#
#
#
#	$Id: Makefile,v 1.5 2000-09-23 04:55:44 javabear Exp $	
#
#

LIB_NAME        =	libhamlib.so
LIB_SONAME      =	libhamlib.so.1
LIB_RELEASE     =	libhamlib.so.1.0.1

INCLUDE		=	-I/usr/include -I/usr/local/include -I. -I../common


LIB_HEADER      =        rig.h serial.h
LIB_SRC         =        rig.c serial.c
LIB_OBJECTS     =        serial.o rig.o 


CC	=	gcc
INCLUDE	=	-I/usr/include -I/usr/local/include -I. -I../include
OBJ	=	serial.o rig.o testrig.o
LDFLAG	=	-lm 
# -ansi would break usleep, CRTSCTS, etc.
CFLAGS	=	-g -Wall -D__USE_FIXED_PROTOTYPES__ -pedantic -fPIC -g -Wall
MAKEALL	=	$(OBJ)

all:	$(MAKEALL)

serial.o:	serial.c serial.h 
	$(CC)	$(CFLAGS)  $(INCLUDE) -c serial.c

rig.o: rig.c rig.h riglist.h
	$(CC)	$(CFLAGS)  $(INCLUDE) -c rig.c

testrig.o:	testrig.c rig.h
	$(CC)	$(CFLAGS)  $(INCLUDE) -c testrig.c

# clean up

clean:
	make cleanlocal

# clean up local directory

.PHONY: cleanlocal
cleanlocal:
	rm -f *.o 
	rm -f $(LIB_RELEASE)


# test making a shared lib (at last) for the frontend -- FS
# still lots to cleanup. 

.PHONY: hamlib
hamlib:	 
	$(CC) $(CFLAGS) $(INCLUDE) -c $(LIB_SRC)
	$(CC) -shared -Wl,-soname,$(LIB_SONAME) -o $(LIB_RELEASE) $(LIB_OBJECTS) -lc

