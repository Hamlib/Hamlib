# Check for getaddrinfo replacement.             -*- Autoconf -*-

# Copyright (c) 2010 by Stephane Fillod
# 
# This file is part of Hamlib
# 
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Library General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


AC_DEFUN([HL_GETADDRINFO],
[

AC_CHECK_TYPES([struct addrinfo],[],[],[
     #if HAVE_NETDB_H
     # include <netdb.h>
     #endif
     #ifdef HAVE_SYS_SOCKET_H
     # include <sys/socket.h>
     #elif HAVE_WS2TCPIP_H
     # include <ws2tcpip.h>
     #endif
  ])

AC_LINK_IFELSE([AC_LANG_PROGRAM([[
    #include <sys/types.h>
    #if HAVE_NETDB_H
    # include <netdb.h>
    #endif
    #ifdef HAVE_SYS_SOCKET_H
    # include <sys/socket.h>
    #elif HAVE_WS2TCPIP_H
    # include <ws2tcpip.h>
    #endif
    #include <stddef.h>
    ]],
    [[getaddrinfo("", "", NULL, NULL);]])],
    [ac_cv_func_getaddrinfo=yes
     AC_DEFINE(HAVE_GETADDRINFO,[1],[Define if getaddrinfo is available])],
    [ac_cv_func_getaddrinfo=no])


AC_CHECK_DECLS([gai_strerror], [], [], [[
            #include <sys/types.h>
            #ifdef HAVE_SYS_SOCKET_H
            #include <sys/socket.h>
            #endif
            #ifdef HAVE_NETDB_H
            #include <netdb.h>
            #endif
            #ifdef HAVE_WS2TCPIP_H
            #include <ws2tcpip.h>
            #endif
            #include <stddef.h>
          ]])

dnl Checks for replacements
AC_REPLACE_FUNCS([getaddrinfo])

AH_BOTTOM(
[
/* Define missing prototypes, implemented in replacement lib */
#ifdef  __cplusplus
extern "C" {
#endif

#ifndef HAVE_STRUCT_ADDRINFO
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#elif HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
};
#endif

#ifndef HAVE_GETADDRINFO

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#elif HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#ifndef AI_PASSIVE
#define AI_PASSIVE 0x0001
#endif

int getaddrinfo(const char *node, const char *service,
    const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
#endif

#if !defined(HAVE_DECL_GAI_STRERROR) && !defined(gai_strerror)
const char *gai_strerror(int errcode);
#endif /* !HAVE_DECL_GAI_STRERROR */

#ifdef  __cplusplus
}
#endif
])


])
