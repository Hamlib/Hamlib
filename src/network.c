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
#include <pthread.h>

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
#undef _WIN32_WINNT
// We need inet_pton to get defined  and 0x0600 does it
// Eventually we should be able to get rid of this hack
#define _WIN32_WINNT 0x0600
#  include <ws2tcpip.h>
#undef _WIN32_WINNT
// Then we'll go back to Server 2003
#define _WIN32_WINNT 0x0502
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

int network_init()
{
#ifdef __MINGW32__
    WSADATA wsadata;

    if (wsstarted == 0)
    {
        int ret;
        ret = WSAStartup(MAKEWORD(1, 1), &wsadata);

        if (ret == 0)
        {
            wsstarted = 1;
            rig_debug(RIG_DEBUG_VERBOSE, "%s: WSAStartup OK\n", __func__);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error creating socket, WSAStartup ret=%d\n",
                      __func__, ret);
            RETURNFUNC(-RIG_EIO);
        }
    }

#endif
    return RIG_OK;
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
    struct in6_addr serveraddr;
    struct sockaddr_in client;
    char hoststr[256], portstr[6] = "";

    ENTERFUNC;

    status = network_init();

    if (status != RIG_OK) { RETURNFUNC(status); }

    if (!rp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = NI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;

    if (rp->type.rig == RIG_PORT_UDP_NETWORK)
    {
        hints.ai_socktype = SOCK_DGRAM;
    }
    else
    {
        hints.ai_socktype = SOCK_STREAM;
    }

    if (rp->pathname[0] == ':' && rp->pathname[1] != ':')
    {
        snprintf(portstr, sizeof(portstr) - 1, "%s", rp->pathname + 1);
    }
    else
    {
        if (strlen(rp->pathname))
        {
            status = parse_hoststr(rp->pathname, hoststr, portstr);

            if (status != RIG_OK) { RETURNFUNC(status); }

            rig_debug(RIG_DEBUG_TRACE, "%s: hoststr=%s, portstr=%s\n", __func__, hoststr,
                      portstr);

        }

        if (strlen(portstr) == 0)
        {
            sprintf(portstr, "%d", default_port);
        }
    }

    status = inet_pton(AF_INET, hoststr, &serveraddr);

    if (status == 1) /* valid IPv4 address */
    {
        hints.ai_family = AF_INET;
        hints.ai_flags |= AI_NUMERICHOST;
    }
    else
    {
        status = inet_pton(AF_INET6, hoststr, &serveraddr);

        if (status == 1) /* valid IPv6 address */
        {
            hints.ai_family = AF_INET6;
            hints.ai_flags |= AI_NUMERICHOST;
        }
    }

    status = getaddrinfo(hoststr, portstr, &hints, &res);

    if (status == 0 && res->ai_family == AF_INET6)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Using IPV6\n", __func__);
        //inet_pton(AF_INET6, hoststr, &h_addr.sin6_addr);
    }

    if (status != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: cannot get host \"%s\": %s\n",
                  __func__,
                  rp->pathname,
                  gai_strerror(status));
        RETURNFUNC(-RIG_ECONF);
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
            RETURNFUNC(-RIG_EIO);
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
        RETURNFUNC(-RIG_EIO);
    }

    rp->fd = fd;

    socklen_t clientLen = sizeof(client);
    getsockname(rp->fd, (struct sockaddr *)&client, &clientLen);
    rig_debug(RIG_DEBUG_ERR, "%s: client port=%d\n", __func__, client.sin_port);
    rp->client_port = client.sin_port;

    RETURNFUNC(RIG_OK);
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
    int ret = 0;

    ENTERFUNC;

    if (rp->fd > 0)
    {
#ifdef __MINGW32__
        ret = closesocket(rp->fd);
#else
        ret = close(rp->fd);
#endif
        rig_debug(RIG_DEBUG_VERBOSE, "%s: close socket ret=%d\n", __func__, ret);
        rp->fd = 0;
    }

#ifdef __MINGW32__

    if (wsstarted)
    {
        ret = WSACleanup();
        rig_debug(RIG_DEBUG_VERBOSE, "%s: WSACleanup ret=%d\n", __func__, ret);
        wsstarted = 0;
    }

#endif
    RETURNFUNC(ret);
}
//! @endcond

volatile int multicast_server_run = 1;
pthread_t multicast_server_threadId;
extern void sync_callback(int lock);

struct multicast_server_args_s
{
    RIG *rig;
} multicast_server_args;

//! @cond Doxygen_Suppress
// our multicast server loop
void *multicast_server(void *arg)
{
    struct multicast_server_args_s *args = (struct multicast_server_args_s *)arg;
    RIG *rig = args->rig;
    rig_debug(RIG_DEBUG_TRACE, "%s(%d): Starting multicast server\n", __FILE__,
              __LINE__);
    // we can and should use a really small cache time while we are polling
    // rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 100);

    // this is really verbose since it runs very quickly
    // so we only spit out WARN unless CACHE has been selected
    //if (!rig_need_debug(RIG_DEBUG_CACHE)) { rig_set_debug(RIG_DEBUG_WARN); }


    freq_t freqMain = 0, freqSub = 0, freqMainLast = 0, freqSubLast = 0;
    rmode_t modeMain = RIG_MODE_NONE, modeSub = RIG_MODE_NONE,
            modeMainLast = RIG_MODE_NONE, modeSubLast = RIG_MODE_NONE;
    pbwidth_t widthMain = 0, widthSub = 0, widthMainLast = 0, widthSubLast = 0;
    split_t split, splitLast = -1;

    do
    {
        int retval;
        int updateOccurred;

        updateOccurred = 0;

        if (rig->caps->get_freq)
        {
            sync_callback(1);
            retval = rig_get_freq(rig, RIG_VFO_A, &freqMain);
            sync_callback(0);

            if (retval != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_freqA error %s\n", __FILE__, __LINE__, rigerror(retval)); }

            sync_callback(1);
            retval = rig_get_freq(rig, RIG_VFO_B, &freqSub);
            sync_callback(0);

            if (retval != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_freqB error %s\n", __FILE__, __LINE__, rigerror(retval)); }

            if (freqMain != freqMainLast || freqSub != freqSubLast)
            {
                rig_debug(RIG_DEBUG_WARN,
                          "%s(%d) freqMain=%.0f was %.0f, freqSub=%.0f was %.0f\n", __FILE__, __LINE__,
                          freqMain, freqMainLast, freqSub, freqSubLast);
                updateOccurred = 1;
                freqMainLast = freqMain;
                freqSubLast = freqSub;
            }
        }

        if (rig->caps->get_mode)
        {
            sync_callback(1);
            retval = rig_get_mode(rig, RIG_VFO_A, &modeMain, &widthMain);
            sync_callback(0);

            if (retval != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_modeA error %s\n", __FILE__, __LINE__, rigerror(retval)); }

            sync_callback(1);
            retval = rig_get_mode(rig, RIG_VFO_B, &modeSub, &widthSub);
            sync_callback(0);

            if (retval != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_modeB error %s\n", __FILE__, __LINE__, rigerror(retval)); }

            if (modeMain != modeMainLast || modeSub != modeSubLast)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s(%d) modeMain=%s was %s, modeSub=%s was %s\n",
                          __FILE__, __LINE__, rig_strrmode(modeMain), rig_strrmode(modeMainLast),
                          rig_strrmode(modeSub), rig_strrmode(modeSubLast));
                updateOccurred = 1;
                modeMainLast = modeMain;
                modeSubLast = modeSub;
            }

            if (widthMain != widthMainLast || widthSub != widthSubLast)
            {
                rig_debug(RIG_DEBUG_WARN,
                          "%s(%d) widthMain=%ld was %ld, widthSub=%ld was %ld\n", __FILE__, __LINE__,
                          widthMain, widthMainLast, widthSub, widthSubLast);
                updateOccurred = 1;
                widthMainLast = widthMain;
                widthSubLast = widthSub;
            }
        }

        if (rig->caps->get_split_vfo)
        {
            vfo_t tx_vfo;
            sync_callback(1);
            retval = rig_get_split_vfo(rig, RIG_VFO_A, &split, &tx_vfo);
            sync_callback(0);

            if (retval != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_modeA error %s\n", __FILE__, __LINE__, rigerror(retval)); }

            if (split != splitLast)
            {
                rig_debug(RIG_DEBUG_WARN, "%s(%d) split=%d was %d\n", __FILE__, __LINE__, split,
                          splitLast);
                updateOccurred = 1;
                splitLast = split;
            }
        }

        if (updateOccurred)
        {
            rig_debug(RIG_DEBUG_WARN, "%s(%d): update occurred...time to send multicast\n",
                      __FILE__, __LINE__);
        }

        hl_usleep(100 * 1000);
    }
    while (multicast_server_run);

    rig_debug(RIG_DEBUG_TRACE, "%s(%d): Stopping multicast server\n", __FILE__,
              __LINE__);
    return NULL;
}
//! @endcond

/**
 * \brief Open multicast server using rig.state data
 *
 * Open Open multicast server using rig.state data.
 * NB: The signal PIPE will be ignored for the whole application.
 *
 * \param rp Port data structure (must spec port id eg hostname:port -- hostname defaults to 224.0.1.1)
 * \param default_port Default network socket port
 * \return RIG_OK or < 0 if error
 */
int network_multicast_server(RIG *rig, const char *multicast_addr,
                             int default_port, enum multicast_item_e items)
{
    int status;

    //ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s(%d):network_multicast_server under development\n", __FILE__, __LINE__);
    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d):ADDR=%s, port=%d\n", __FILE__, __LINE__,
              multicast_addr, default_port);

    if (strcmp(multicast_addr, "0.0.0.0") == 0)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): not starting multicast\n", __FILE__,
                  __LINE__);
        return RIG_OK; // don't start it
    }

    if (multicast_server_threadId != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): multicast_server already running\n", __FILE__,
                  __LINE__);
    }

    status = network_init();

    if (status != RIG_OK) { RETURNFUNC(status); }

    if (items & RIG_MULTICAST_TRANSCEIVE)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d) MULTICAST_TRANSCEIVE enabled\n", __FILE__,
                  __LINE__);
    }

    if (items & RIG_MULTICAST_SPECTRUM)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d) MULTICAST_SPECTRUM enabled\n", __FILE__,
                  __LINE__);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d) unknown MULTICAST item requested=0x%x\n",
                  __FILE__, __LINE__, items);
    }

    multicast_server_args.rig = rig;
    int err = pthread_create(&multicast_server_threadId, NULL, multicast_server,
                             &multicast_server_args);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d) pthread_create error %s\n", __FILE__, __LINE__,
                  strerror(errno));
        return -RIG_EINTERNAL;
    }

    RETURNFUNC(RIG_OK);
}

/** @} */
