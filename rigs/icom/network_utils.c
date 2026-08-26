/*
 *  Hamlib Icom network backend - shared time helpers
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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

/* See network_utils.h. */

#include "hamlib/config.h"

#include <time.h>
#include <stdint.h>

#include "network_utils.h"

#define ICOM_NETWORK_NS_PER_SEC  1000000000L
#define ICOM_NETWORK_NS_PER_MS   1000000L
#define ICOM_NETWORK_MS_PER_SEC  1000

int64_t icom_network_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * ICOM_NETWORK_MS_PER_SEC
           + ts.tv_nsec / ICOM_NETWORK_NS_PER_MS;
}

void icom_network_deadline_from_now(struct timespec *ts, int timeout_ms)
{
    clock_gettime(CLOCK_REALTIME, ts);

    ts->tv_sec += timeout_ms / ICOM_NETWORK_MS_PER_SEC;
    ts->tv_nsec += (long)(timeout_ms % ICOM_NETWORK_MS_PER_SEC)
                   * ICOM_NETWORK_NS_PER_MS;

    if (ts->tv_nsec >= ICOM_NETWORK_NS_PER_SEC)
    {
        ts->tv_sec += 1;
        ts->tv_nsec -= ICOM_NETWORK_NS_PER_SEC;
    }
}
