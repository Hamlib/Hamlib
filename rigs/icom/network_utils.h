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

/* Time helpers shared by the Icom network backend layers. Kept apart from the
 * session and backend layers because both need them and neither should depend
 * on the other. Hamlib's core has a clock_gettime() portability shim but no
 * equivalent of these. */

#ifndef _ICOM_NETWORK_UTILS_H
#define _ICOM_NETWORK_UTILS_H 1

#include <stdint.h>
#include <time.h>

/* Monotonic milliseconds, for measuring intervals. Monotonic rather than
 * wall-clock so a system time change cannot make a keepalive or timeout
 * misfire; the absolute value is meaningless, only differences matter. */
int64_t icom_network_now_ms(void);

/* Absolute deadline timeout_ms from now, for pthread_cond_timedwait(). Uses
 * CLOCK_REALTIME because that is what a condition variable expects unless it
 * was created with pthread_condattr_setclock(), which these are not. */
void icom_network_deadline_from_now(struct timespec *ts, int timeout_ms);

#endif /* _ICOM_NETWORK_UTILS_H */
