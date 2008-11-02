/*
 *  Hamlib Interface - network communication low-level support
 *  Copyright (c) 2000-2008 by Stephane Fillod
 *
 *	$Id: network.c,v 1.5 2008-11-02 12:42:45 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/**
 * \addtogroup rig_internal
 * @{
 */

/**
 * \brief Network port IO
 * \file network.c
 */

/* Forcing WINVER in MinGW yanks in getaddrinfo(), but locks out Win95/Win98 */
/* #define WINVER 0x0501 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
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
#endif

#include "hamlib/rig.h"
#include "network.h"
#include "misc.h"


#ifndef HAVE_STRUCT_ADDRINFO
struct addrinfo {
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	struct sockaddr *ai_addr;
	socklen_t ai_addrlen;
};
#endif

/*
 * Replacement for getaddrinfo. Only one addrinfo is returned.
 * Weak checking.
 */
#ifndef HAVE_GETADDRINFO
static int getaddrinfo(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res)
{
	struct addrinfo *p;

	p = malloc(sizeof(struct addrinfo));
	if (!p)
		return ENOMEM;
	/* limitation: this replacement function only for IPv4 */
	if (hints && hints->ai_family != AF_INET)
		return EINVAL;
	memset(p, 0, sizeof(struct addrinfo));
	p->ai_family = hints->ai_family;
	p->ai_socktype = hints->ai_socktype;
	p->ai_protocol = hints->ai_protocol;
	p->ai_addrlen = sizeof(struct sockaddr_in);
	p->ai_addr = malloc(p->ai_addrlen);
	if (!p->ai_addr) {
		free(p);
		return ENOMEM;
	}
	memset((char *) p->ai_addr, 0, p->ai_addrlen);

	((struct sockaddr_in*)p->ai_addr)->sin_family = p->ai_family;
	/* limitation: the service must be a port _number_ */
	((struct sockaddr_in*)p->ai_addr)->sin_port = htons(atoi(service));
	/* limitation: the node must be in numbers-and-dots notation */
	((struct sockaddr_in*)p->ai_addr)->sin_addr.s_addr = inet_addr(node);

	*res = p;
	
	return 0;
}

static void freeaddrinfo(struct addrinfo *res)
{
	free(res->ai_addr);
	free(res);
}
#endif /* !HAVE_GETADDRINFO */

#if !defined(HAVE_GAI_STRERROR) && !defined(gai_strerror)
static const char *gai_strerror(int errcode)
{
	return strerror(errcode);
}
#endif /* !HAVE_GAI_STRERROR */


/**
 * \brief Open network port using rig.state data
 *
 * Open Open network port using rig.state data.
 * NB: the signal PIPE will be ignored for the whole application.
 *
 * \param rp port data structure (must spec port id eg hostname:port)
 * \return RIG_OK or < 0 if error
 */
int network_open(hamlib_port_t *rp, int default_port)
{
	int fd;				/* File descriptor for the port */
	int status;
	struct addrinfo hints, *res;
	char *portstr;
	char hostname[FILPATHLEN] = "localhost";
	char defaultportstr[8];

	if (!rp)
		return -RIG_EINVAL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET /* PF_UNSPEC */;
	hints.ai_socktype = SOCK_STREAM;

	if (rp->pathname[0] == ':') {
		portstr = rp->pathname+1;
	} else {
		strncpy(hostname, rp->pathname, FILPATHLEN-1);

		/* search last ':', because IPv6 may have some */
		portstr = strrchr(hostname, ':');
		if (portstr) {
			*portstr++ = '\0';
		} else {
			sprintf(defaultportstr, "%d", default_port);
			portstr = defaultportstr;
		}
	}
    
	status=getaddrinfo(hostname, portstr, &hints, &res);
	if (status != 0) {
		rig_debug(RIG_DEBUG_ERR, "Cannot get host \"%s\": %s\n",
					rp->pathname, gai_strerror(errno));
		return -RIG_ECONF;
	}

	/* we don't want a signal when connection get broken */
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

	fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (fd < 0)
		return -RIG_EIO;

	status = connect(fd, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	if (status < 0) {
		rig_debug(RIG_DEBUG_ERR, "Cannot open NET device \"%s\": %s\n",
					rp->pathname, strerror(errno));
		close(fd);
		return -RIG_EIO;
	}

	rp->fd = fd;

	return RIG_OK;
}

/** @} */
