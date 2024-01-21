/*
 *  Hamlib Interface - network communication low-level support
 *  Copyright (c) 2021-2023 by Mikael Nousiainen
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
// cppcheck-suppress missingInclude
#include "io.h"
#endif

#ifdef __MINGW32__
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
static int wsstarted;
static int is_networked(char *address, int address_length);
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

#ifdef HAVE_PTHREAD
    pthread_mutex_t write_lock;
#endif
} multicast_publisher_args;

typedef struct multicast_publisher_priv_data_s
{
    pthread_t thread_id;
    multicast_publisher_args args;
} multicast_publisher_priv_data;

typedef struct multicast_receiver_args_s
{
    RIG *rig;
    int socket_fd;
    const char *multicast_addr;
    int multicast_port;
} multicast_receiver_args;

typedef struct multicast_receiver_priv_data_s
{
    pthread_t thread_id;
    multicast_receiver_args args;
} multicast_receiver_priv_data;

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

#define TRACE rig_debug(RIG_DEBUG_ERR, "TRACE %s(%d)\n", __func__,__LINE__);

int network_init()
{
    int retval = -RIG_EINTERNAL;
#ifdef __MINGW32__
    WSADATA wsadata;

    if (wsstarted == 0)
    {
        retval = WSAStartup(MAKEWORD(1, 1), &wsadata);

        if (retval == 0)
        {
            wsstarted = 1;
            rig_debug(RIG_DEBUG_VERBOSE, "%s: WSAStartup OK\n", __func__);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error creating socket, WSAStartup ret=%d\n",
                      __func__, retval);
            return (-RIG_EIO);
        }
    }
    else // already started
    {
        retval = RIG_OK;
    }

#else
    retval = RIG_OK;

#endif
    return retval;
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
#define MULTICAST_DATA_PIPE_TIMEOUT_USEC 100000

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
        const *mcast_publisher_args, size_t length, unsigned char *data)
{
    ssize_t result;

    result = async_pipe_wait_for_data(mcast_publisher_args->data_pipe, 100);

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

static int multicast_publisher_write_data(const multicast_publisher_args
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

static int multicast_publisher_read_data(const multicast_publisher_args
        *mcast_publisher_args, size_t length, unsigned char *data)
{
    int fd = mcast_publisher_args->data_read_fd;
    fd_set rfds, efds;
    struct timeval timeout;
    ssize_t result;
    int retval;
    int retries = 2;
    size_t offset = 0;
    size_t length_left = length;

retry:
    timeout.tv_sec = 0;
    timeout.tv_usec = MULTICAST_DATA_PIPE_TIMEOUT_USEC;

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
                  "%s(): fd error when reading multicast publisher data: %s\n",
                  __func__,
                  strerror(errno));

        return -RIG_EIO;
    }

    result = read(fd, data + offset, length_left);

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

    offset += result;
    length_left -= result;

    if (length_left > 0)
    {
        if (retries > 0)
        {
            // Execution of this routine may time out between writes to pipe, retry to get more data
            rig_debug(RIG_DEBUG_VERBOSE,
                    "%s: could not read from multicast publisher data pipe, expected %ld bytes, read %ld bytes, retrying...\n",
                    __func__, (long) length, (long) offset);
            retries--;
            goto retry;
        }

        rig_debug(RIG_DEBUG_ERR,
                "%s: could not read from multicast publisher data pipe even after retries, expected %ld bytes, read %ld bytes\n",
                __func__, (long) length, (long) offset);

        return (-RIG_EIO);
    }

    return (RIG_OK);
}

#endif

static void multicast_publisher_write_lock(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    multicast_publisher_priv_data *priv_data = (multicast_publisher_priv_data *)
            rs->multicast_publisher_priv_data;
    pthread_mutex_lock(&priv_data->args.write_lock);
}

static void multicast_publisher_write_unlock(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    multicast_publisher_priv_data *priv_data = (multicast_publisher_priv_data *)
            rs->multicast_publisher_priv_data;
    pthread_mutex_unlock(&priv_data->args.write_lock);
}

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
        return RIG_OK;
    }

    mcast_publisher_priv = (multicast_publisher_priv_data *)
                           rs->multicast_publisher_priv_data;
    mcast_publisher_args = &mcast_publisher_priv->args;

    result = multicast_publisher_write_data(
                 mcast_publisher_args, sizeof(multicast_publisher_data_packet),
                 (unsigned char *) packet);

    if (result != RIG_OK)
    {
        return result;
    }

    return RIG_OK;
}

// cppcheck-suppress unusedFunction
int network_publish_rig_poll_data(RIG *rig)
{
    const struct rig_state *rs = &rig->state;
    int result;
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

    multicast_publisher_write_lock(rig);
    result = multicast_publisher_write_packet_header(rig, &packet);
    multicast_publisher_write_unlock(rig);
    return result;
}

// cppcheck-suppress unusedFunction
int network_publish_rig_transceive_data(RIG *rig)
{
    const struct rig_state *rs = &rig->state;
    int result;
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

    multicast_publisher_write_lock(rig);
    result = multicast_publisher_write_packet_header(rig, &packet);
    multicast_publisher_write_unlock(rig);
    return result;
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

    // Acquire write lock to write all data in one go to the pipe
    multicast_publisher_write_lock(rig);

    result = multicast_publisher_write_packet_header(rig, &packet);

    if (result != RIG_OK)
    {
        multicast_publisher_write_unlock(rig);
        RETURNFUNC2(result);
    }

    mcast_publisher_priv = (multicast_publisher_priv_data *)
                           rs->multicast_publisher_priv_data;
    mcast_publisher_args = &mcast_publisher_priv->args;

    result = multicast_publisher_write_data(
                 mcast_publisher_args, sizeof(struct rig_spectrum_line), (unsigned char *) line);

    if (result != RIG_OK)
    {
        multicast_publisher_write_unlock(rig);
        RETURNFUNC2(result);
    }

    result = multicast_publisher_write_data(
                 mcast_publisher_args, line->spectrum_data_length, line->spectrum_data);

    multicast_publisher_write_unlock(rig);

    if (result != RIG_OK)
    {
        RETURNFUNC2(result);
    }

    RETURNFUNC2(RIG_OK);
}

static int multicast_publisher_read_packet(multicast_publisher_args
        const *mcast_publisher_args,
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
#ifdef __MINGW32__
    char ip4[32];
#endif

    struct multicast_publisher_args_s *args = (struct multicast_publisher_args_s *)
            arg;
    RIG *rig = args->rig;
    struct rig_state *rs = &rig->state;
    struct rig_spectrum_line spectrum_line;
    uint8_t packet_type = MULTICAST_PUBLISHER_DATA_PACKET_TYPE_SPECTRUM;
    multicast_publisher_priv_data *mcast_publisher_priv =
        (multicast_publisher_priv_data *)
        rs->multicast_publisher_priv_data;

    struct sockaddr_in dest_addr;
    int socket_fd = args->socket_fd;
    ssize_t send_result;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Starting multicast publisher\n", __FILE__,
              __LINE__);

#ifdef __MINGW32__

    if (!is_networked(ip4, sizeof(ip4)))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: no IPV4 network detected...multicast disabled\n",
                  __func__);
        return NULL;
    }

#endif

    snapshot_init();

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(args->multicast_addr);
    dest_addr.sin_port = htons(args->multicast_port);

    rs->multicast_publisher_run = 1;

    while (rs->multicast_publisher_run)
    {
        int result;

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
            hl_usleep(100 * 1000);
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

        rig_debug(RIG_DEBUG_CACHE, "%s: sending rig snapshot data: %s\n", __func__,
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
            static int flag = 0;

            if (errno != 0 || flag == 0)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: error sending UDP packet: %s\n", __func__,
                          strerror(errno));
                flag = 1;
            }
        }
    }

    rs->multicast_publisher_run = 0;
    mcast_publisher_priv->thread_id = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Stopped multicast publisher\n", __FILE__,
              __LINE__);
    return NULL;
}


#ifdef __MINGW32__

static int is_networked(char *address, int address_length)
{
    int count = 0;

    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    char ipString[INET6_ADDRSTRLEN]; // large enough for both IPv4 and IPv6
    address[0] = 0;

    // First call to determine actual memory size needed
    GetAdaptersAddresses(AF_UNSPEC, flags, NULL, NULL, &dwSize);
    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(dwSize);

    // Second call to get the actual data
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &dwSize);

    if (dwRetVal == NO_ERROR)
    {
        for (pCurrAddresses = pAddresses; pCurrAddresses != NULL;
                pCurrAddresses = pCurrAddresses->Next)
        {
//            if (pCurrAddresses->IfType == IF_TYPE_IEEE80211) // Wireless adapter
            {
                char friendlyName[256];
                wcstombs(friendlyName, pCurrAddresses->FriendlyName, sizeof(friendlyName));
                rig_debug(RIG_DEBUG_VERBOSE, "%s: network IfType = %d, name=%s\n", __func__,
                          (int)pCurrAddresses->IfType, friendlyName);

                for (pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != NULL;
                        pUnicast = pUnicast->Next)
                {
                    void *addr = NULL;

                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) // IPv4 address
                    {
                        struct sockaddr_in *sa_in = (struct sockaddr_in *)pUnicast->Address.lpSockaddr;
                        addr = &(sa_in->sin_addr);
                    }

#if 0 // going to skip IPV6 for now -- should never need it on a local network
                    else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) // IPv6 address
                    {
                        struct sockaddr_in6 *sa_in6 = (struct sockaddr_in6 *)
                                                      pUnicast->Address.lpSockaddr;
                        addr = &(sa_in6->sin6_addr);
                    }

#endif

                    // Convert IP address to string and ignore bad ones
                    if (addr)
                    {

                        if (inet_ntop(pUnicast->Address.lpSockaddr->sa_family, addr, ipString,
                                      sizeof(ipString)) != NULL)
                        {
                            // Use IP address if not 169.x.x.x
                            if (strncmp(ipString, "169", 3) != 0)
                            {
                                count++;

                                if (count > 1)
                                {
                                    rig_debug(RIG_DEBUG_WARN,
                                              "%s: more than 1 address found...multicast may not work\n", __func__);
                                }

                                rig_debug(RIG_DEBUG_VERBOSE, "%s: Address: %s\n", ipString, ipString);
                                strncpy(address, ipString, address_length);
                            }
                        }
                    }
                }

                free(pAddresses);
                return 1; // Wireless and addresses printed
            }
        }
    }

    if (pAddresses)
    {
        free(pAddresses);
    }

    return 0; // Not wireless or no addresses found
}

int is_wireless()
{
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    PIP_ADAPTER_ADDRESSES pAddresses = NULL, pCurrAddresses = NULL;

    // First call to determine actual memory size needed
    GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &dwSize);
    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(dwSize);

    // Second call to get the actual data
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &dwSize);

    if (dwRetVal == NO_ERROR)
    {
        for (pCurrAddresses = pAddresses; pCurrAddresses != NULL;
                pCurrAddresses = pCurrAddresses->Next)
        {
            // printf("Adapter name: %s\n", pCurrAddresses->AdapterName);
            // printf("Adapter description: %ls\n", pCurrAddresses->Description);
            // printf("Adapter type: ");

            if (pCurrAddresses->IfType == IF_TYPE_IEEE80211)
            {
                //       printf("Wireless\n\n");
                return 1;
            }
            else
            {
                //      printf("Not Wireless\n\n");
            }
        }
    }
    else
    {
        //printf("GetAdaptersAddresses failed with error: %lu\n", dwRetVal);
    }

    if (pAddresses)
    {
        free(pAddresses);
    }

    return 0;
}
#else
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ifaddrs.h>

int is_networked(char *ipv4, int ipv4_length)
{
    struct ifaddrs *interfaces, *iface;
    char addr_str[INET_ADDRSTRLEN];

    // Get a list of all network interfaces
    if (getifaddrs(&interfaces) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Iterate through the list of interfaces
    for (iface = interfaces; iface != NULL; iface = iface->ifa_next)
    {
        if (iface->ifa_addr
                && iface->ifa_addr->sa_family == AF_INET)   // Check it is IP4
        {
            // Convert the linked list of interfaces to a human readable string
            struct sockaddr_in *sa = (struct sockaddr_in *) iface->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), addr_str, INET_ADDRSTRLEN);

            if (strncmp(addr_str, "127", 3) == 0 && ipv4[0] == 0)
            {
                strncpy(ipv4, addr_str, ipv4_length);
                rig_debug(RIG_DEBUG_VERBOSE, "%s: Can use %s\n", __func__, ipv4);
            }
            else if (strncmp(addr_str, "127", 3) != 0)
            {
                strncpy(ipv4, addr_str, ipv4_length);
                rig_debug(RIG_DEBUG_VERBOSE, "%s: Will use %s\n", __func__, ipv4);
            }
        }
    }

    freeifaddrs(interfaces); // Free the linked list
    return strlen(ipv4) > 0 ;
}


#ifdef __linux__
#include <linux/wireless.h>
int is_wireless_linux(const char *ifname)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct iwreq pwrq;
    memset(&pwrq, 0, sizeof(pwrq));
    strncpy(pwrq.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIWNAME, &pwrq) != -1)
    {
        close(sock);
        return 1;  // Wireless
    }

    close(sock);
    return 0;  // Not wireless
}

int is_wireless()
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return 0;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        int iswireless = is_wireless_linux(ifa->ifa_name);

        //printf("%s is %s\n", ifa->ifa_name, iswireless ? "wireless" : "not wireless");
        if (iswireless) {freeifaddrs(ifaddr); return 1;}
    }

    freeifaddrs(ifaddr);
    return 0;
}
#endif
#endif

void *multicast_receiver(void *arg)
{
    char data[4096];
    char ip4[INET6_ADDRSTRLEN];

    struct multicast_receiver_args_s *args = (struct multicast_receiver_args_s *)
            arg;
    RIG *rig = args->rig;
    struct rig_state *rs = &rig->state;
    multicast_receiver_priv_data *mcast_receiver_priv =
            (multicast_receiver_priv_data *)
                    rs->multicast_receiver_priv_data;

    struct sockaddr_in dest_addr;
    int socket_fd = args->socket_fd;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Starting multicast receiver\n", __FILE__,
              __LINE__);

    if (!is_networked(ip4, sizeof(ip4)))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: no network detected...disabling multicast receive\n", __func__);
        return NULL;
    }

    int optval = 1;

#ifdef __MINGW32__

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (PCHAR)&optval,
                   sizeof(optval)) < 0)
#else
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) < 0)
#endif
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error enabling UDP address reuse: %s\n", __func__,
                  strerror(errno));
        return NULL;
    }

    // Windows does not have SO_REUSEPORT. However, SO_REUSEADDR works in a similar way.
#if defined(SO_REUSEPORT)

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval,
                   sizeof(optval)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error enabling UDP port reuse: %s\n", __func__,
                  strerror(errno));
        return NULL;
    }


#endif

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
#ifdef __MINGW32__

    // Windows wireless cannot bind to multicast group addresses for some unknown reason
    // Update: it's not wireless causing the error we see but we'll leave the detection in place
    if (is_wireless())
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: wireless detected\n", __func__);

//        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: no wireless detected so INADDR_ANY is being used\n", __func__);
    }

#else
//    dest_addr.sin_addr.s_addr = inet_addr(args->multicast_addr);
    dest_addr.sin_addr.s_addr = inet_addr(args->multicast_addr);
#endif
    dest_addr.sin_port = htons(args->multicast_port);

    if (bind(socket_fd, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error binding UDP socket to %s:%d: %s\n",
                  __func__,
                  args->multicast_addr, args->multicast_port, strerror(errno));
        return NULL;
    }

    struct ip_mreq mreq;

    memset(&mreq, 0, sizeof(mreq));

    mreq.imr_multiaddr.s_addr = inet_addr(args->multicast_addr);

#ifdef __MINGW32__

    // we're not worrying about IPV6 right now as that will likely never occur on home network
    if (strlen(ip4) > 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: multicast binding to %s\n", __func__, ip4);
        mreq.imr_interface.s_addr = inet_addr(ip4);
    }
    else
#endif
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: multicast binding to INADDR_ANY\n", __func__);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    }

#ifdef __MINGW32__

    if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (PCHAR)&mreq,
                   sizeof(mreq)) < 0)
#else
    if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
                   sizeof(mreq)) < 0)
#endif
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error joining multicast group %s:%d: %s\n",
                  __func__,
                  args->multicast_addr, args->multicast_port, strerror(errno));

        if (errno != 0)
        {
            return NULL;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: errno==0 so trying to continue\n", __func__);
    }

    rs->multicast_receiver_run = 1;

    while (rs->multicast_receiver_run)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        fd_set rfds, efds;
        struct timeval timeout;
        int select_result;
        ssize_t result;

        timeout.tv_sec = 0;
        timeout.tv_usec = MULTICAST_DATA_PIPE_TIMEOUT_USEC;
        FD_ZERO(&rfds);
        FD_SET(socket_fd, &rfds);
        efds = rfds;

        select_result = select(socket_fd + 1, &rfds, NULL, &efds, &timeout);

        if (!rs->multicast_receiver_run)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): pselect signal\n", __func__, __LINE__);
            break;
        }

        if (select_result == 0)
        {
            // Select timed out
            //rig_debug(RIG_DEBUG_ERR, "%s: select timeout\n", __FILE__);
            continue;
        }

        if (select_result <= 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s((%d): select() failed when reading UDP multicast socket data: %s\n",
                      __func__,
                      __LINE__,
                      strerror(errno));

            break;
        }

        if ((result = FD_ISSET(socket_fd, &efds)))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s(%d): fd error when reading UDP multicast socket data: (%d)=%s\n", __func__,
                      __LINE__, (int)result, strerror(errno));
            break;
        }

        result = recvfrom(socket_fd, data, sizeof(data), 0,
                          (struct sockaddr *) &client_addr, &client_len);

        if (result <= 0)
        {
            if (result < 0)
            {
                if (errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    continue;
                }

                rig_debug(RIG_DEBUG_ERR, "%s: error receiving from UDP socket %s:%d: %s\n",
                          __func__,
                          args->multicast_addr, args->multicast_port, strerror(errno));
            }

            break;
        }

        // TODO: handle commands from multicast clients
        rig_debug(RIG_DEBUG_VERBOSE, "%s: received %ld bytes of data: %.*s\n", __func__,
                  (long) result, (int) result, data);

        // TODO: if a new snapshot needs to be sent, call network_publish_rig_poll_data() and the publisher routine will send out a snapshot
        // TODO: new logic in publisher needs to be written for other types of responses
    }

    rs->multicast_receiver_run = 0;
    mcast_receiver_priv->thread_id = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Stopped multicast receiver\n", __FILE__,
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
    int mutex_status;
#ifdef __MINGW32__
    char ip4[32];
#endif

    ENTERFUNC;

    if (rs->multicast_publisher_priv_data != NULL)
    {
        rig_debug(RIG_DEBUG_WARN, "%s(%d): multicast publisher already running\n",
                __FILE__, __LINE__);
        RETURNFUNC(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s(%d): multicast publisher address=%s, port=%d\n", __FILE__,
              __LINE__,
              multicast_addr, multicast_port);

#ifdef __MINGW32__

    if (!is_networked(ip4, sizeof(ip4)))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: No network found...multicast disabled\n",
                  __func__);
        return RIG_OK;
    }

#endif

    if (multicast_addr == NULL || strcmp(multicast_addr, "0.0.0.0") == 0)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): not starting multicast publisher\n",
                  __FILE__, __LINE__);
        return RIG_OK;
    }

    status = network_init();

#ifdef __MINGW32__ // always RIG_OK if not Windows

    if (status != RIG_OK)
    {
        RETURNFUNC(status);
    }

#endif

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error opening new UDP socket: %s", __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EIO);
    }

    // Enable non-blocking mode
    u_long mode = 1;
#ifdef __MINGW32__

    if (ioctlsocket(socket_fd, FIONBIO, &mode) == SOCKET_ERROR)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error enabling non-blocking mode for socket: %s",
                  __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EIO);
    }

#else

    if (ioctl(socket_fd, FIONBIO, &mode) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error enabling non-blocking mode for socket: %s",
                  __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EIO);
    }

#endif

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

    rs->snapshot_packet_sequence_number = 0;
    rs->multicast_publisher_run = 0;
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

    mutex_status = pthread_mutex_init(&mcast_publisher_priv->args.write_lock, NULL);

    status = multicast_publisher_create_data_pipe(mcast_publisher_priv);

    if (status < 0 || mutex_status != 0)
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

    pthread_mutex_destroy(&mcast_publisher_priv->args.write_lock);

    free(rs->multicast_publisher_priv_data);
    rs->multicast_publisher_priv_data = NULL;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief Start multicast receiver
 *
 * Start multicast receiver.
 *
 * \param multicast_addr UDP address
 * \param multicast_port UDP socket port
 * \return RIG_OK or < 0 if error
 */
int network_multicast_receiver_start(RIG *rig, const char *multicast_addr,
                                     int multicast_port)
{
    struct rig_state *rs = &rig->state;
    multicast_receiver_priv_data *mcast_receiver_priv;
    int socket_fd;
    int status;

    ENTERFUNC;

    if (rs->multicast_receiver_priv_data != NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): multicast receiver already running\n",
                __FILE__,
                __LINE__);
        RETURNFUNC(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): multicast receiver address=%s, port=%d\n",
              __FILE__,
              __LINE__,
              multicast_addr, multicast_port);

    if (multicast_addr == NULL || strcmp(multicast_addr, "0.0.0.0") == 0)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): not starting multicast receiver\n",
                  __FILE__, __LINE__);
        RETURNFUNC(RIG_OK);
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

    // Enable non-blocking mode
    u_long mode = 1;
#ifdef __MINGW32__

    if (ioctlsocket(socket_fd, FIONBIO, &mode) == SOCKET_ERROR)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error enabling non-blocking mode for socket: %s",
                  __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EIO);
    }

#else

    if (ioctl(socket_fd, FIONBIO, &mode) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error enabling non-blocking mode for socket: %s",
                  __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EIO);
    }

#endif

    rs->multicast_receiver_run = 0;
    rs->multicast_receiver_priv_data = calloc(1,
                                       sizeof(multicast_receiver_priv_data));

    if (rs->multicast_receiver_priv_data == NULL)
    {
        close(socket_fd);
        RETURNFUNC(-RIG_ENOMEM);
    }

    mcast_receiver_priv = (multicast_receiver_priv_data *)
                          rs->multicast_receiver_priv_data;
    mcast_receiver_priv->args.socket_fd = socket_fd;
    mcast_receiver_priv->args.multicast_addr = multicast_addr;
    mcast_receiver_priv->args.multicast_port = multicast_port;
    mcast_receiver_priv->args.rig = rig;

    int err = pthread_create(&mcast_receiver_priv->thread_id, NULL,
                             multicast_receiver,
                             &mcast_receiver_priv->args);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d) pthread_create error %s\n", __FILE__, __LINE__,
                  strerror(errno));
        free(mcast_receiver_priv);
        rs->multicast_receiver_priv_data = NULL;
        close(socket_fd);
        RETURNFUNC(-RIG_EINTERNAL);
    }

    RETURNFUNC(RIG_OK);
}

/**
 * \brief Stop multicast receiver
 *
 * Stop multicast receiver
 *
 * \return RIG_OK or < 0 if error
 */
int network_multicast_receiver_stop(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    multicast_receiver_priv_data *mcast_receiver_priv;

    ENTERFUNC;

    rs->multicast_receiver_run = 0;

    mcast_receiver_priv = (multicast_receiver_priv_data *)
                          rs->multicast_receiver_priv_data;

    if (mcast_receiver_priv == NULL)
    {
        RETURNFUNC(RIG_OK);
    }

    // Close the socket first to stop the routine
    if (mcast_receiver_priv->args.socket_fd >= 0)
    {
#ifdef __MINGW32__
        shutdown(mcast_receiver_priv->args.socket_fd, SD_BOTH);
#else
        shutdown(mcast_receiver_priv->args.socket_fd, SHUT_RDWR);
#endif
        close(mcast_receiver_priv->args.socket_fd);
    }

    if (mcast_receiver_priv->thread_id != 0)
    {
        int err = pthread_join(mcast_receiver_priv->thread_id, NULL);

        if (err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): pthread_join error %s\n", __FILE__, __LINE__,
                      strerror(errno));
            // just ignore it
        }

        mcast_receiver_priv->thread_id = 0;
    }

    if (mcast_receiver_priv->args.socket_fd >= 0)
    {
        mcast_receiver_priv->args.socket_fd = -1;
    }

    free(rs->multicast_receiver_priv_data);
    rs->multicast_receiver_priv_data = NULL;

    RETURNFUNC(RIG_OK);
}

#endif
/** @} */
