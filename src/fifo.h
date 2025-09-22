/*
 *  Hamlib Interface - FIFO support
 *  Copyright (c) 2023-2025 by the Hamlib group
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

#ifndef _HL_FIFO_H
#define _HL_FIFO_H

//#include "hamlib/rig.h"
//#include <pthread.h>

__BEGIN_DECLS

// FIFO currently used for send_morse queue
#define HAMLIB_FIFO_SIZE 1024

typedef struct FIFO_RIG_s
{
    char data[HAMLIB_FIFO_SIZE];
    int head;
    int tail;
    int flush;  // flush flag for stop_morse
    pthread_mutex_t mutex;
} FIFO_RIG;

/* Function prototypes */
void initFIFO(FIFO_RIG *fifo);
void resetFIFO(FIFO_RIG *fifo);
int hl_push(FIFO_RIG *fifo, const char *msg);
int hl_pop(FIFO_RIG *fifo);
int hl_peek(FIFO_RIG *fifo);

__END_DECLS

#endif /* _HL_FIFO_H */
