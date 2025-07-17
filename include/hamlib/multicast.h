/*
 *  Hamlib Interface - Multicast API header
 *  Copyright (c) 2023 by Mike Black, W9MDB
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/* SPDX-License-Identifier: LGPL-2.1-or-later */


#ifndef MULTICAST_H
#define MULTICAST_H

//include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <errno.h>
//#include <unistd.h>
#include <hamlib/rig.h>
//#include <sys/socket.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

struct multicast_vfo
{
    char *name;
    double freq;
    char *mode;
    int width;
    int widthLower;
    int widthUpper;
    unsigned char rx; // true if in rx mode
    unsigned char tx; // true in in tx mode
};

struct multicast_broadcast
{
    char *ID;
    struct multicast_vfo **vfo;
};

// returns # of bytes sent
extern HAMLIB_EXPORT (int) multicast_init(RIG *rig, char *addr, int port);
extern HAMLIB_EXPORT (int) multicast_send(RIG *rig, const char *msg, int msglen);
extern HAMLIB_EXPORT (int) multicast_stop(RIG *rig);

#endif  // MULTICAST_H
