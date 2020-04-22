/*
 *  Hamlib Interface - network communication low-level support
 *  Copyright (c) 2000-2012 by Stephane Fillod
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
#  include "config.h"
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
#  include <netinet/in.h>
#endif

#if HAVE_NETDB_H
#  include <netdb.h>
#endif

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#if defined (HAVE_SYS_SOCKET_H) && defined (HAVE_SYS_IOCTL_H)
#  include <sys/socket.h>
#  include <sys/ioctl.h>
#elif HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
#  if defined(HAVE_WSPIAPI_H)
#    include <wspiapi.h>
#  endif
#endif

#include <hamlib/rig.h>
#include "network.h"
#include "misc.h"


#ifdef __MINGW32__
static int wsstarted;
#endif

//! @cond Doxygen_Suppress
#define NET_BUFFER_SIZE 8192
//! @endcond

static void handle_error(enum rig_debug_level_e lvl, const char *msg)
{
    int e;
#ifdef __MINGW32__
    LPVOID lpMsgBuf;

    lpMsgBuf = (LPVOID)"Unknown error";
    e = WSAGetLastError();

    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
                      | FORMAT_MESSAGE_FROM_SYSTEM
                      | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      e,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      // Default language
                      (LPTSTR)&lpMsgBuf,
                      0,
                      NULL))
    {
        rig_debug(lvl, "%s: Network error %d: %s\n", msg, e, (char *)lpMsgBuf);
        LocalFree(lpMsgBuf);
    }
    else
    {
        rig_debug(lvl, "%s: Network error %d\n", msg, e);
    }

#else
    e = errno;
    rig_debug(lvl, "%s: Network error %d: %s\n", msg, e, strerror(e));
#endif
}


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
    int fd;             /* File descriptor for the port */
    int status;
    struct addrinfo hints, *res, *saved_res;
    char *hoststr = NULL, *portstr = NULL, *bracketstr1, *bracketstr2;
    char hostname[FILPATHLEN];
    char defaultportstr[8];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_VERBOSE, "%s version 1.0\n", __func__);

#ifdef __MINGW32__
    WSADATA wsadata;

    if (!(wsstarted++) && WSAStartup(MAKEWORD(1, 1), &wsadata) == SOCKET_ERROR)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error creating socket\n", __func__);
        return -RIG_EIO;
    }

#endif

    if (!rp)
    {
        return -RIG_EINVAL;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;

    if (rp->type.rig == RIG_PORT_UDP_NETWORK)
    {
        hints.ai_socktype = SOCK_DGRAM;
    }
    else
    {
        hints.ai_socktype = SOCK_STREAM;
    }

    /* default of all local interfaces */
    hoststr = NULL;

    if (rp->pathname[0] == ':')
    {
        portstr = rp->pathname + 1;
    }
    else
    {
        if (strlen(rp->pathname))
        {
            snprintf(hostname, sizeof(hostname), "%s", rp->pathname);
            hoststr = hostname;
            /* look for IPv6 numeric form [<addr>] */
            bracketstr1 = strchr(hoststr, '[');
            bracketstr2 = strrchr(hoststr, ']');

            if (bracketstr1 && bracketstr2 && bracketstr2 > bracketstr1)
            {
                hoststr = bracketstr1 + 1;
                *bracketstr2 = '\0';
                portstr = bracketstr2 + 1; /* possible port after ]: */
            }
            else
            {
                bracketstr2 = NULL;
                portstr = hoststr; /* possible port after : */
            }

            /* search last ':' */
            portstr = strrchr(portstr, ':');

            if (portstr)
            {
                *portstr++ = '\0';
            }
        }

        if (!portstr)
        {
            sprintf(defaultportstr, "%d", default_port);
            portstr = defaultportstr;
        }
    }

    status = getaddrinfo(hoststr, portstr, &hints, &res);

    if (status != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: cannot get host \"%s\": %s\n",
                  __func__,
                  rp->pathname,
                  gai_strerror(errno));
        return -RIG_ECONF;
    }

    saved_res = res;

    /* we don't want a signal when connection get broken */
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    do
    {
        char msg[1024];
        fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        if (fd < 0)
        {
            handle_error(RIG_DEBUG_ERR, "socket");
            freeaddrinfo(saved_res);
            return -RIG_EIO;
        }

        if (connect(fd, res->ai_addr, res->ai_addrlen) == 0)
        {
            break;
        }

        snprintf(msg, sizeof(msg), "connect to %s failed, (trying next interface)",
                 rp->pathname);
        handle_error(RIG_DEBUG_WARN, msg);

#ifdef __MINGW32__
        closesocket(fd);
#else
        close(fd);
#endif
    }
    while ((res = res->ai_next) != NULL);

    freeaddrinfo(saved_res);

    if (NULL == res)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: failed to connect to %s\n",
                  __func__,
                  rp->pathname);
        return -RIG_EIO;
    }

    rp->fd = fd;

    return RIG_OK;
}


/**
 * \brief Clears any data in the read buffer of the socket
 *
 * \param rp Port data structure
 */
void network_flush(hamlib_port_t *rp)
{
#ifdef __MINGW32__
    ULONG len;
#else
    uint len;
#endif

    char buffer[NET_BUFFER_SIZE] = { 0 };

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (;;)
    {
        int ret;
        len = 0;
#ifdef __MINGW32__
        ret = ioctlsocket(rp->fd, FIONREAD, &len);
#else
        ret = ioctl(rp->fd, FIONREAD, &len);
#endif

        if (ret != 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ioctl err '%s'\n", __func__, strerror(errno));
            break;
        }

        if (len > 0)
        {
            int len_read = 0;
            rig_debug(RIG_DEBUG_WARN,
                      "%s: network data clear d: ret=%d, len=%d, '%s'\n",
                      __func__,
                      ret, (int)len, buffer);
            len_read = recv(rp->fd, buffer, len < NET_BUFFER_SIZE ? len : NET_BUFFER_SIZE,
                            0);

            if (len_read < 0)   // -1 indicates error occurred
            {
                rig_debug(RIG_DEBUG_ERR, "%s: read error '%s'\n", __func__, strerror(errno));
                break;
            }

            rig_debug(RIG_DEBUG_WARN,
                      "%s: network data cleared: ret=%d, len_read=%d/0x%x, '%s'\n",
                      __func__,
                      ret, len_read, len_read, buffer);
        }
        else
        {
            break;
        }
    }
}


//! @cond Doxygen_Suppress
int network_close(hamlib_port_t *rp)
{
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

#ifdef __MINGW32__
    ret = closesocket(rp->fd);

    if (--wsstarted)
    {
        WSACleanup();
    }

#else
    ret = close(rp->fd);
#endif
    return ret;
}
//! @endcond

/** @} */
