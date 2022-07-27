/*
 *  Hamlib Interface - getaddrinfo replacement
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

/* Forcing WINVER in MinGW yanks in getaddrinfo(), but locks out Win95/Win98 */
/* #define WINVER 0x0501 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/types.h>
#include <signal.h>


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
#   if defined(HAVE_WSPIAPI_H)
#       include <wspiapi.h>
#   endif
#endif


/*
 * Replacement for getaddrinfo. Only one addrinfo is returned.
 * Weak checking.
 * Return 0 when success, otherwise -1.
 */
#ifndef HAVE_GETADDRINFO
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res)
{
    struct addrinfo *p;
    int ai_family, ai_socktype, ai_protocol, ai_flags;

    /* limitation: service must be non null */
    if (!service)
    {
        return -1;
    }

    if (hints == NULL)
    {
        ai_family = AF_UNSPEC;
        ai_socktype = 0;
        ai_protocol = 0;
        ai_flags = 0;
    }
    else
    {
        ai_family = hints->ai_family;
        ai_socktype = hints->ai_socktype;
        ai_protocol = hints->ai_protocol;
        ai_flags = hints->ai_flags;
    }

    /* limitation: this replacement function only for IPv4 */
    if (ai_family == AF_UNSPEC)
    {
        ai_family = AF_INET;
    }

    if (ai_family != AF_INET)
    {
        return -1;
    }

    p = calloc(1, sizeof(struct addrinfo));

    if (!p)
    {
        return -1;
    }

    memset(p, 0, sizeof(struct addrinfo));
    p->ai_family = ai_family;
    p->ai_socktype = ai_socktype;
    p->ai_protocol = ai_protocol;
    p->ai_addrlen = sizeof(struct sockaddr_in);
    p->ai_addr = calloc(1, p->ai_addrlen);

    if (!p->ai_addr)
    {
        free(p);
        return -1;
    }

    memset((char *) p->ai_addr, 0, p->ai_addrlen);

    ((struct sockaddr_in *)p->ai_addr)->sin_family = p->ai_family;
    /* limitation: the service must be a port _number_ */
    ((struct sockaddr_in *)p->ai_addr)->sin_port = htons(atoi(service));

    /* limitation: the node must be in numbers-and-dots notation */
    if (!node && (ai_flags & AI_PASSIVE))
    {
        ((struct sockaddr_in *)p->ai_addr)->sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        ((struct sockaddr_in *)p->ai_addr)->sin_addr.s_addr = inet_addr(node);
    }

    *res = p;

    return 0;
}

void freeaddrinfo(struct addrinfo *res)
{
    free(res->ai_addr);
    free(res);
}
#endif /* !HAVE_GETADDRINFO */

#if !defined(HAVE_DECL_GAI_STRERROR) && !defined(gai_strerror)
const char *gai_strerror(int errcode)
{
    return strerror(errcode);
}
#endif /* !HAVE_DECL_GAI_STRERROR */
