/*
 *  Hamlib Interface - network communication low-level support
 *  Copyright (c) 2000-2010 by Stephane Fillod
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


#ifdef __MINGW32__
static int wsstarted;
#endif

/**
 * \brief Open network port using rig.state data
 *
 * Open Open network port using rig.state data.
 * NB: The signal PIPE will be ignored for the whole application.
 *
 * \param rp Port data structure (must spec port id eg hostname:port)
 * \param default_port Default network socket port
 * \return RIG_OK or < 0 if error
 */
int network_open(hamlib_port_t *rp, int default_port)
{
	int fd;				/* File descriptor for the port */
	int status;
	struct addrinfo hints, *res;
	char *portstr;
	char hostname[FILPATHLEN] = "127.0.0.1";
	char defaultportstr[8];

#ifdef __MINGW32__
	WSADATA wsadata;
	if (!(wsstarted++) && WSAStartup(MAKEWORD(1,1), &wsadata) == SOCKET_ERROR) {
		rig_debug(RIG_DEBUG_ERR, "Error creating socket\n");
		return -RIG_EIO;
	}
#endif

	if (!rp)
		return -RIG_EINVAL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
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

int network_close(hamlib_port_t *rp)
{
	int ret;
#ifdef __MINGW32__
	ret = closesocket(rp->fd);
	if (--wsstarted)
		WSACleanup();
#else
	ret = close(rp->fd);
#endif
	return ret;
}

/** @} */
