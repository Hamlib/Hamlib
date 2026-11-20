/*
 *  Hamlib Harris backend - RF-5800H and RF-350K socket shim for the simulators
 *  Copyright (c) 2026 by Cam Hosken, W6HOS
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

/*
 * A Winsock SOCKET is not a CRT descriptor, so read()/write() fail on it;
 * these wrappers use recv/send there and read/write elsewhere.  Every
 * transfer is looped until the whole count has moved.  Nothing in rf5800.c
 * includes this file: the two simulators share it.
 */

#ifndef _RF5800_SOCK_H
#define _RF5800_SOCK_H

#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET sock_t;
#else
#include <signal.h>
#include <unistd.h>
typedef int sock_t;
#endif

static inline int sock_startup(void)
{
#ifdef _WIN32
    WSADATA wsa;

    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
#else
    /* a write to a peer that has gone must return -1, not kill the program */
    signal(SIGPIPE, SIG_IGN);
    return 0;
#endif
}

static inline void sock_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

/* Reads exactly n bytes; returns 0, or -1 at EOF or on error. */
static inline int sock_read(sock_t s, char *buf, size_t n)
{
    size_t done = 0;

    while (done < n)
    {
#ifdef _WIN32
        int k = recv(s, buf + done, (int)(n - done), 0);
#else
        int k = (int) read(s, buf + done, n - done);
#endif

        if (k <= 0)
        {
            return -1;
        }

        done += (size_t) k;
    }

    return 0;
}

/* Writes all n bytes; returns 0, or -1 on error. */
static inline int sock_write(sock_t s, const char *buf, size_t n)
{
    size_t done = 0;

    while (done < n)
    {
#ifdef _WIN32
        int k = send(s, buf + done, (int)(n - done), 0);
#else
        int k = (int) write(s, buf + done, n - done);
#endif

        if (k <= 0)
        {
            return -1;
        }

        done += (size_t) k;
    }

    return 0;
}

static inline void sock_close(sock_t s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

#endif /* _RF5800_SOCK_H */
