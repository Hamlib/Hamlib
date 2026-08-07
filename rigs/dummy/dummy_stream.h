/*
 *  Hamlib streaming subsystem
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

/* Streaming simulator for the dummy backend. */
/* Generates tone/silence or loops back TX→RX for testing. */

#ifndef HAMLIB_DUMMY_STREAM_H
#define HAMLIB_DUMMY_STREAM_H

#include <hamlib/rig.h>
#include <pthread.h>

/* Maximum concurrent streams per type */
#define DUMMY_MAX_STREAMS_PER_TYPE 4

/* Stream generator modes */
#define DUMMY_STREAM_TONE     0
#define DUMMY_STREAM_SILENCE  1
#define DUMMY_STREAM_LOOPBACK 2
#define DUMMY_STREAM_COUNTER  3

/* Per-stream state for the dummy backend */
/* Threading model: each stream has one worker thread that solely owns its
 * mutable scratch/accumulator fields (phase_acc, tx_consumed, conversion
 * buffers).  Cross-thread state is limited to `running` (atomic stop flag) and
 * the ring buffers (internally locked).  A loopback RX thread resolves its TX
 * peer from the priv stream-state table under priv->stream_states_lock each
 * iteration.  No other field is written from more than one thread. */
struct dummy_stream_state
{
    struct rig_stream *stream;      /* Frontend stream handle */
    RIG *rig;                       /* Owning rig (for PTT keying) */
    pthread_t thread;               /* Generator / TX scheduler thread */
    int thread_started;             /* 1 after successful pthread_create */
    HAMLIB_ATOMIC int running;      /* Thread run flag */
    uint32_t phase_acc;             /* Phase accumulator for tone gen */
    int mode;                       /* DUMMY_STREAM_TONE/SILENCE/LOOPBACK */
    float tone_freq;                /* Snapshot at stream open time */
    float tone_amp;                 /* Snapshot at stream open time */
    float iq_offset;                /* Snapshot at stream open time */
    long synth_gap;                 /* RX: inject one gap of N samples */
    int burst_ptt;                  /* TX: SOB/EOB keys PTT (from caps) */
    uint64_t tx_consumed;           /* TX: frames consumed by the scheduler */
};

/* Backend stream_open / stream_close hooks */
int dummy_stream_open(RIG *rig, struct rig_stream *stream);
int dummy_stream_close(RIG *rig, struct rig_stream *stream);

#endif /* HAMLIB_DUMMY_STREAM_H */
