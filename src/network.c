/*
 *  Hamlib Interface - network communication low-level support
 *  Copyright (c) 2021 by Mikael Nousiainen
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

#include <hamlib/config.h>

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
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
#include "asyncpipe.h"
#include "snapshot_data.h"

#ifdef HAVE_WINDOWS_H
#include "io.h"
#endif

#ifdef __MINGW32__
static int wsstarted;
#endif

//! @cond Doxygen_Suppress
#define NET_BUFFER_SIZE 8192
//! @endcond

#define MULTICAST_PUBLISHER_DATA_PACKET_TYPE_POLL       0x01
#define MULTICAST_PUBLISHER_DATA_PACKET_TYPE_TRANSCEIVE 0x02
#define MULTICAST_PUBLISHER_DATA_PACKET_TYPE_SPECTRUM   0x03

#pragma pack(push,1)
typedef struct multicast_publisher_data_packet_s
{
    uint8_t type;
    uint8_t padding;
    uint16_t data_length;
} __attribute__((packed)) multicast_publisher_data_packet;
#pragma pack(pop)

typedef struct multicast_publisher_args_s
{
    RIG *rig;
    int socket_fd;
    const char *multicast_addr;
    int multicast_port;

#if defined(WIN32) && defined(HAVE_WINDOWS_H)
    hamlib_async_pipe_t *data_pipe;
#else
    int data_write_fd;
    int data_read_fd;
#endif
} multicast_publisher_args;

typedef struct multicast_publisher_priv_data_s
{
    pthread_t thread_id;
    multicast_publisher_args args;
} multicast_publisher_priv_data;

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
            return (-RIG_EIO);
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

#ifdef __MINGW32__
    status = network_init();

    if (status != RIG_OK) { return (status); }

#endif

    if (!rp)
    {
        return (-RIG_EINVAL);
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
        SNPRINTF(portstr, sizeof(portstr) - 1, "%s", rp->pathname + 1);
    }
    else
    {
        if (strlen(rp->pathname))
        {
            status = parse_hoststr(rp->pathname, sizeof(rp->pathname), hoststr, portstr);

            if (status != RIG_OK) { return (status); }

            rig_debug(RIG_DEBUG_TRACE, "%s: hoststr=%s, portstr=%s\n", __func__, hoststr,
                      portstr);

        }

        if (strlen(portstr) == 0)
        {
            SNPRINTF(portstr, sizeof(portstr), "%d", default_port);
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
        return (-RIG_ECONF);
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
            return (-RIG_EIO);
        }

        if (connect(fd, res->ai_addr, res->ai_addrlen) == 0)
        {
            break;
        }

        SNPRINTF(msg, sizeof(msg), "connect to %s failed, (trying next interface)",
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
        return (-RIG_EIO);
    }

    rp->fd = fd;

    socklen_t clientLen = sizeof(client);
    getsockname(rp->fd, (struct sockaddr *)&client, &clientLen);
    rig_debug(RIG_DEBUG_TRACE, "%s: client port=%d\n", __func__, client.sin_port);
    rp->client_port = client.sin_port;

    return (RIG_OK);
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
                      "%s: network data cleared: ret=%d, len_read=%d/0x%x\n",
                      __func__,
                      ret, len_read, len_read);
            dump_hex((unsigned char *)buffer, len_read);
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
    return (ret);
}
//! @endcond

extern void sync_callback(int lock);

#ifdef HAVE_PTHREAD
//! @cond Doxygen_Suppress

#define MULTICAST_DATA_PIPE_TIMEOUT_MILLIS 1000

#if defined(WIN32) && defined(HAVE_WINDOWS_H)

static int multicast_publisher_create_data_pipe(multicast_publisher_priv_data
        *mcast_publisher_priv)
{
    int status;

    status = async_pipe_create(&mcast_publisher_priv->args.data_pipe,
                               PIPE_BUFFER_SIZE_DEFAULT, MULTICAST_DATA_PIPE_TIMEOUT_MILLIS);

    if (status != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: multicast publisher data pipe creation failed with status=%d, err=%s\n",
                  __func__,
                  status, strerror(errno));
        return (-RIG_EINTERNAL);
    }

    return (RIG_OK);
}

static void multicast_publisher_close_data_pipe(multicast_publisher_priv_data
        *mcast_publisher_priv)
{
    if (mcast_publisher_priv->args.data_pipe != NULL)
    {
        async_pipe_close(mcast_publisher_priv->args.data_pipe);
        mcast_publisher_priv->args.data_pipe = NULL;
    }
}

static int multicast_publisher_write_data(multicast_publisher_args
        *mcast_publisher_args, size_t length, const unsigned char *data)
{
    ssize_t result;

    result = async_pipe_write(mcast_publisher_args->data_pipe, data, length,
                              MULTICAST_DATA_PIPE_TIMEOUT_MILLIS);

    if (result < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: error writing to multicast publisher data pipe, result=%d\n", __func__,
                  (int)result);
        return (-RIG_EIO);
    }

    if (result != length)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: could not write to multicast publisher data pipe, expected %d bytes, wrote %d bytes\n",
                  __func__, (int)length, (int)result);
        return (-RIG_EIO);
    }

    return (RIG_OK);
}

static int multicast_publisher_read_data(multicast_publisher_args
        *mcast_publisher_args, size_t length, unsigned char *data)
{
    ssize_t result;

    result = async_pipe_wait_for_data(mcast_publisher_args->data_pipe,
                                      MULTICAST_DATA_PIPE_TIMEOUT_MILLIS);

    if (result < 0)
    {
        // Timeout is expected when there is no data
        if (result != -RIG_ETIMEOUT)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: error waiting for multicast publisher data, result=%ld\n", __func__,
                      (long) result);
        }

        return (result);
    }

    result = async_pipe_read(mcast_publisher_args->data_pipe, data, length,
                             MULTICAST_DATA_PIPE_TIMEOUT_MILLIS);

    if (result < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: error reading multicast publisher data, result=%ld\n", __func__,
                  (long) result);
        return (-RIG_EIO);
    }

    if (result != length)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: could not read from multicast publisher data pipe, expected %ld bytes, read %ld bytes\n",
                  __func__, (long) length, (long) result);
        return (-RIG_EIO);
    }

    return (RIG_OK);
}

#else

static int multicast_publisher_create_data_pipe(multicast_publisher_priv_data
        *mcast_publisher_priv)
{
    int data_pipe_fds[2];
    int status;

    status = pipe(data_pipe_fds);

    if (status != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: multicast publisher data pipe creation failed with status=%d, err=%s\n",
                  __func__,
                  status, strerror(errno));
        return (-RIG_EINTERNAL);
    }

    int flags = fcntl(data_pipe_fds[0], F_GETFD);
    flags |= O_NONBLOCK;

    if (fcntl(data_pipe_fds[0], F_SETFD, flags))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error setting O_NONBLOCK on pipe=%s\n", __func__,
                  strerror(errno));
    }

    mcast_publisher_priv->args.data_read_fd = data_pipe_fds[0];
    mcast_publisher_priv->args.data_write_fd = data_pipe_fds[1];

    return (RIG_OK);
}

static void multicast_publisher_close_data_pipe(multicast_publisher_priv_data
        *mcast_publisher_priv)
{
    if (mcast_publisher_priv->args.data_read_fd != -1)
    {
        close(mcast_publisher_priv->args.data_read_fd);
        mcast_publisher_priv->args.data_read_fd = -1;
    }

    if (mcast_publisher_priv->args.data_write_fd != -1)
    {
        close(mcast_publisher_priv->args.data_write_fd);
        mcast_publisher_priv->args.data_write_fd = -1;
    }
}

static int multicast_publisher_write_data(multicast_publisher_args
        *mcast_publisher_args, size_t length, const unsigned char *data)
{
    int fd = mcast_publisher_args->data_write_fd;
    ssize_t result;

    result = write(fd, data, length);

    if (result < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: error writing to multicast publisher data pipe, result=%d, err=%s\n",
                  __func__,
                  (int)result, strerror(errno));
        return (-RIG_EIO);
    }

    if (result != length)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: could not write to multicast publisher data pipe, expected %ld bytes, wrote %ld bytes\n",
                  __func__, (long) length, (long) result);
        return (-RIG_EIO);
    }

    return (RIG_OK);
}

static int multicast_publisher_read_data(multicast_publisher_args
        *mcast_publisher_args, size_t length, unsigned char *data)
{
    int fd = mcast_publisher_args->data_read_fd;
    fd_set rfds, efds;
    struct timeval timeout;
    ssize_t result;
    int retval;

    timeout.tv_sec = MULTICAST_DATA_PIPE_TIMEOUT_MILLIS / 1000;
    timeout.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    efds = rfds;

    retval = select(fd + 1, &rfds, NULL, &efds, &timeout);

    if (retval == 0)
    {
        return (-RIG_ETIMEOUT);
    }

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s(): select() failed when reading multicast publisher data: %s\n",
                  __func__,
                  strerror(errno));

        return -RIG_EIO;
    }

    if (FD_ISSET(fd, &efds))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s(): fd error when reading multicast publisher data\n", __func__);
        return -RIG_EIO;
    }

    result = read(fd, data, length);

    if (result < 0)
    {
        if (errno == EAGAIN)
        {
            return (-RIG_ETIMEOUT);
        }

        rig_debug(RIG_DEBUG_ERR, "%s: error reading multicast publisher data: %s\n",
                  __func__, strerror(errno));
        return (-RIG_EIO);
    }

    if (result != length)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: could not read from multicast publisher data pipe, expected %ld bytes, read %ld bytes\n",
                  __func__, (long) length, (long) result);
        return (-RIG_EIO);
    }

    return (RIG_OK);
}

#endif

static int multicast_publisher_write_packet_header(RIG *rig,
        multicast_publisher_data_packet *packet)
{
    struct rig_state *rs = &rig->state;
    multicast_publisher_priv_data *mcast_publisher_priv;
    multicast_publisher_args *mcast_publisher_args;
    ssize_t result;

    if (rs->multicast_publisher_priv_data == NULL)
    {
        // Silently ignore if multicast publisher is not enabled
        RETURNFUNC2(RIG_OK);
    }

    mcast_publisher_priv = (multicast_publisher_priv_data *)
                           rs->multicast_publisher_priv_data;
    mcast_publisher_args = &mcast_publisher_priv->args;

    result = multicast_publisher_write_data(
                 mcast_publisher_args, sizeof(multicast_publisher_data_packet),
                 (unsigned char *) packet);

    if (result != RIG_OK)
    {
        RETURNFUNC2(result);
    }

    RETURNFUNC2(RIG_OK);
}

int network_publish_rig_poll_data(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    multicast_publisher_data_packet packet =
    {
        .type = MULTICAST_PUBLISHER_DATA_PACKET_TYPE_POLL,
        .padding = 0,
        .data_length = 0,
    };

    if (rs->multicast_publisher_priv_data == NULL)
    {
        // Silently ignore call if multicast publisher is not enabled
        return RIG_OK;
    }

    return multicast_publisher_write_packet_header(rig, &packet);
}

int network_publish_rig_transceive_data(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    multicast_publisher_data_packet packet =
    {
        .type = MULTICAST_PUBLISHER_DATA_PACKET_TYPE_TRANSCEIVE,
        .padding = 0,
        .data_length = 0,
    };

    if (rs->multicast_publisher_priv_data == NULL)
    {
        // Silently ignore call if multicast publisher is not enabled
        return RIG_OK;
    }

    return multicast_publisher_write_packet_header(rig, &packet);
}

int network_publish_rig_spectrum_data(RIG *rig, struct rig_spectrum_line *line)
{
    int result;
    struct rig_state *rs = &rig->state;
    multicast_publisher_priv_data *mcast_publisher_priv;
    multicast_publisher_args *mcast_publisher_args;
    multicast_publisher_data_packet packet =
    {
        .type = MULTICAST_PUBLISHER_DATA_PACKET_TYPE_SPECTRUM,
        .padding = 0,
        .data_length = sizeof(struct rig_spectrum_line) + line->spectrum_data_length,
    };

    if (rs->multicast_publisher_priv_data == NULL)
    {
        // Silently ignore call if multicast publisher is not enabled
        return RIG_OK;
    }

    result = multicast_publisher_write_packet_header(rig, &packet);

    if (result != RIG_OK)
    {
        RETURNFUNC2(result);
    }

    mcast_publisher_priv = (multicast_publisher_priv_data *)
                           rs->multicast_publisher_priv_data;
    mcast_publisher_args = &mcast_publisher_priv->args;

    result = multicast_publisher_write_data(
                 mcast_publisher_args, sizeof(struct rig_spectrum_line), (unsigned char *) line);

    if (result != RIG_OK)
    {
        RETURNFUNC2(result);
    }

    result = multicast_publisher_write_data(
                 mcast_publisher_args, line->spectrum_data_length, line->spectrum_data);

    if (result != RIG_OK)
    {
        RETURNFUNC2(result);
    }

    RETURNFUNC2(RIG_OK);
}

static int multicast_publisher_read_packet(multicast_publisher_args
        *mcast_publisher_args,
        uint8_t *type, struct rig_spectrum_line *spectrum_line,
        unsigned char *spectrum_data)
{
    int result;
    multicast_publisher_data_packet packet;

    result = multicast_publisher_read_data(mcast_publisher_args, sizeof(packet),
                                           (unsigned char *) &packet);

    if (result < 0)
    {
        return (result);
    }

    switch (packet.type)
    {
    case MULTICAST_PUBLISHER_DATA_PACKET_TYPE_POLL:
    case MULTICAST_PUBLISHER_DATA_PACKET_TYPE_TRANSCEIVE:
        break;

    case MULTICAST_PUBLISHER_DATA_PACKET_TYPE_SPECTRUM:
        result = multicast_publisher_read_data(
                     mcast_publisher_args, sizeof(struct rig_spectrum_line),
                     (unsigned char *) spectrum_line);

        if (result < 0)
        {
            return (result);
        }

        if (packet.data_length - sizeof(struct rig_spectrum_line) !=
                spectrum_line->spectrum_data_length)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: multicast publisher data error, expected %d bytes of spectrum data, got %d bytes\n",
                      __func__, (int)spectrum_line->spectrum_data_length,
                      (int)(packet.data_length - sizeof(struct rig_spectrum_line)));
            return (-RIG_EPROTO);
        }

        spectrum_line->spectrum_data = spectrum_data;

        result = multicast_publisher_read_data(mcast_publisher_args,
                                               spectrum_line->spectrum_data_length, spectrum_data);

        if (result < 0)
        {
            return (result);
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unexpected multicast publisher data packet type: %d\n", __func__,
                  packet.type);
        return (-RIG_EPROTO);
    }

    *type = packet.type;

    return (RIG_OK);
}

void *multicast_publisher(void *arg)
{
    unsigned char spectrum_data[HAMLIB_MAX_SPECTRUM_DATA];
    char snapshot_buffer[HAMLIB_MAX_SNAPSHOT_PACKET_SIZE];

    struct multicast_publisher_args_s *args = (struct multicast_publisher_args_s *)
            arg;
    RIG *rig = args->rig;
    struct rig_state *rs = &rig->state;
    struct rig_spectrum_line spectrum_line;
    uint8_t packet_type;

    struct sockaddr_in dest_addr;
    int socket_fd = args->socket_fd;
    int result;
    ssize_t send_result;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Starting multicast publisher\n", __FILE__,
              __LINE__);

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(args->multicast_addr);
    dest_addr.sin_port = htons(args->multicast_port);

    while (rs->multicast_publisher_run)
    {
        result = multicast_publisher_read_packet(args, &packet_type, &spectrum_line,
                 spectrum_data);

        if (result != RIG_OK)
        {
            if (result == -RIG_ETIMEOUT)
            {
                continue;
            }

            // TODO: how to detect closing of pipe, indicate with error code
            // TODO: error handling, flush pipe in case of error?
            hl_usleep(500 * 1000);
            continue;
        }

        result = snapshot_serialize(sizeof(snapshot_buffer), snapshot_buffer, rig,
                                    packet_type == MULTICAST_PUBLISHER_DATA_PACKET_TYPE_SPECTRUM ? &spectrum_line :
                                    NULL);

        if (result != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error serializing rig snapshot data, result=%d\n",
                      __func__, result);
            continue;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: sending rig snapshot data: %s\n", __func__,
                  snapshot_buffer);

        send_result = sendto(
                          socket_fd,
                          snapshot_buffer,
                          strlen(snapshot_buffer),
                          0,
                          (struct sockaddr *) &dest_addr,
                          sizeof(dest_addr)
                      );

        if (send_result < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error sending UDP packet: %s\n", __func__,
                      strerror(errno));
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Stopping multicast publisher\n", __FILE__,
              __LINE__);
    return NULL;
}

//! @endcond

/**
 * \brief Start multicast publisher
 *
 * Start multicast publisher.
 *
 * \param multicast_addr UDP address
 * \param multicast_port UDP socket port
 * \return RIG_OK or < 0 if error
 */
int network_multicast_publisher_start(RIG *rig, const char *multicast_addr,
                                      int multicast_port, enum multicast_item_e items)
{
    struct rig_state *rs = &rig->state;
    multicast_publisher_priv_data *mcast_publisher_priv;
    int socket_fd;
    int status;

    ENTERFUNC;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d):multicast address=%s, port=%d\n", __FILE__,
              __LINE__,
              multicast_addr, multicast_port);

    if (strcmp(multicast_addr, "0.0.0.0") == 0)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): not starting multicast publisher\n",
                  __FILE__, __LINE__);
        return RIG_OK;
    }

    if (rs->multicast_publisher_priv_data != NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): multicast publisher already running\n",
                  __FILE__,
                  __LINE__);
        RETURNFUNC(-RIG_EINVAL);
    }

    status = network_init();

    if (status != RIG_OK)
    {
        RETURNFUNC(status);
    }

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error opening new UDP socket: %s", __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EIO);
    }

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

    rs->snapshot_packet_sequence_number = 0;
    rs->multicast_publisher_run = 1;
    rs->multicast_publisher_priv_data = calloc(1,
                                        sizeof(multicast_publisher_priv_data));

    if (rs->multicast_publisher_priv_data == NULL)
    {
        close(socket_fd);
        RETURNFUNC(-RIG_ENOMEM);
    }

    mcast_publisher_priv = (multicast_publisher_priv_data *)
                           rs->multicast_publisher_priv_data;
    mcast_publisher_priv->args.socket_fd = socket_fd;
    mcast_publisher_priv->args.multicast_addr = multicast_addr;
    mcast_publisher_priv->args.multicast_port = multicast_port;
    mcast_publisher_priv->args.rig = rig;

    status = multicast_publisher_create_data_pipe(mcast_publisher_priv);

    if (status < 0)
    {
        free(rs->multicast_publisher_priv_data);
        rs->multicast_publisher_priv_data = NULL;
        close(socket_fd);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: multicast publisher data pipe creation failed, result=%d\n", __func__,
                  status);
        RETURNFUNC(-RIG_EINTERNAL);
    }

    int err = pthread_create(&mcast_publisher_priv->thread_id, NULL,
                             multicast_publisher,
                             &mcast_publisher_priv->args);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d) pthread_create error %s\n", __FILE__, __LINE__,
                  strerror(errno));
        multicast_publisher_close_data_pipe(mcast_publisher_priv);
        free(mcast_publisher_priv);
        rs->multicast_publisher_priv_data = NULL;
        close(socket_fd);
        RETURNFUNC(-RIG_EINTERNAL);
    }

    RETURNFUNC(RIG_OK);
}

/**
 * \brief Stop multicast publisher
 *
 * Stop multicast publisher
 *
 * \return RIG_OK or < 0 if error
 */
int network_multicast_publisher_stop(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    multicast_publisher_priv_data *mcast_publisher_priv;

    ENTERFUNC;

    rs->multicast_publisher_run = 0;

    mcast_publisher_priv = (multicast_publisher_priv_data *)
                           rs->multicast_publisher_priv_data;

    if (mcast_publisher_priv == NULL)
    {
        RETURNFUNC(RIG_OK);
    }

    if (mcast_publisher_priv->thread_id != 0)
    {
        int err = pthread_join(mcast_publisher_priv->thread_id, NULL);

        if (err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): pthread_join error %s\n", __FILE__, __LINE__,
                      strerror(errno));
            // just ignore it
        }

        mcast_publisher_priv->thread_id = 0;
    }

    multicast_publisher_close_data_pipe(mcast_publisher_priv);

    if (mcast_publisher_priv->args.socket_fd >= 0)
    {
        close(mcast_publisher_priv->args.socket_fd);
        mcast_publisher_priv->args.socket_fd = -1;
    }

    free(rs->multicast_publisher_priv_data);
    rs->multicast_publisher_priv_data = NULL;

    RETURNFUNC(RIG_OK);
}
#endif
/** @} */
