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
# Simple  Makefile for common parts, and modified to create 
# libhamlib.so -- FS
#
#
#
#	$Id: Makefile,v 1.11 2000-09-29 03:33:58 javabear Exp $	
#
#


INSTALL_LIBDIR		=	./lib/
INSTALL_INCLUDEDIR	=	./include/
INCLUDE		=	-I/usr/include -I/usr/local/include -I. -I../common

LIB_NAME        =	libhamlib.so
LIB_SONAME      =	libhamlib.so.0
LIB_RELEASE     =	libhamlib.so.0.0.1            
LIB_HEADER      =	rig.h serial.h riglist.h  
LIB_SRC         =	rig.c serial.c
LIB_OBJECTS     =	serial.o rig.o 

 

CC	=	gcc
INCLUDE	=	-I/usr/include -I/usr/local/include -I. -I./include
OBJ	=	serial.o rig.o #testrig.o
LDFLAG	=	-lm 

CFLAGS	=	-g -Wall -D__USE_FIXED_PROTOTYPES__ -pedantic -fPIC -g -Wall
MAKEALL	=	$(OBJ)

all:	hamlib

.PHONY:	hamlib
hamlib:	
	$(CC) $(CFLAGS) $(INCLUDE) -c $(LIB_SRC)
	$(CC) -shared -Wl,-soname,$(LIB_SONAME) -o $(LIB_RELEASE) $(LIB_OBJECTS) -lc

# install header and lib

install:
	make install_lib
	make install_header

# install lib in INSTALL_LIBDIR

.PHONY:	install_lib
install_lib:
	mv $(LIB_RELEASE) $(INSTALL_LIBDIR)
	cd $(INSTALL_LIBDIR); /sbin/ldconfig -n .
	cd $(INSTALL_LIBDIR); ln -s $(LIB_SONAME) $(LIB_NAME)

# install hamlib headers in INSTALL_INCLUDEDIR


.PHONY:	install_header
install_header:
	cp -f $(LIB_HEADER) $(INSTALL_INCLUDEDIR)

serial.o:	serial.c serial.h 
	$(CC)	$(CFLAGS)  $(INCLUDE) -c serial.c

rig.o: rig.c rig.h riglist.h
	$(CC)	$(CFLAGS)  $(INCLUDE) -c rig.c


# clean up local,lib and include

clean:
	make cleanlocal
	make cleanlib
	make cleaninclude

# clean up local directory

.PHONY: cleanlocal
cleanlocal:
	rm -f *.o 
	rm -f $(LIB_RELEASE)

# clean up local lib directory

.PHONY: cleanlib
cleanlib:
	cd $(INSTALL_LIBDIR); rm -f $(LIB_NAME)*


# clean up local include directory

.PHONY: cleaninclude
cleaninclude:
	cd $(INSTALL_INCLUDEDIR); rm -f $(LIB_HEADER)

