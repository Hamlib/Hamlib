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

/* Internal header for the Hamlib streaming subsystem. */
/* Not part of the public API — used by stream.c, backends, and unit tests. */

#ifndef HAMLIB_STREAM_H
#define HAMLIB_STREAM_H

#include <hamlib/rig.h>
#include <pthread.h>
#include <stddef.h>

#include "stream_ringbuf.h"
#include "stream_anchor.h"
#include "stream_account.h"

/* HAMLIB_ATOMIC is defined in hamlib/rig.h */


/* Default ring buffer capacities */
#define RIG_STREAM_AUDIO_BUF_DEFAULT  (64 * 1024)   /* 64 KB for audio */
#define RIG_STREAM_IQ_BUF_DEFAULT     (4 * 1024 * 1024)  /* 4 MB for I/Q */

/* Default audio ring capacity as a duration at the configured rate/format */
#define RIG_STREAM_AUDIO_BUF_DEFAULT_MS 250

/* Time anchor / TX target ring depth (drop-oldest) */
#define RIG_STREAM_ANCHOR_DEPTH 16
#define RIG_STREAM_TX_TARGET_DEPTH 16

/* Write-status event FIFO depth (drop-oldest on overflow) */
#define RIG_STREAM_WRITE_EVENT_DEPTH 16

/* Built-in staleness watchdog thresholds (ms) */
#define RIG_STREAM_STALE_COARSE_DEFAULT_MS     1000
#define RIG_STREAM_STALE_INVALIDATE_DEFAULT_MS 5000


/* A scheduled timed-TX burst boundary.
 * sample_index : burst start in the ring-buffer producer domain.
 * seconds/picoseconds : UTC instant to transmit at (valid when TX_TIMED set).
 * flags : RIG_STREAM_TIME_FLAG_TX_TIMED | SOB | EOB. */
struct rig_stream_tx_target
{
    uint64_t sample_index;
    int64_t  seconds;
    uint64_t picoseconds;
    uint8_t  flags;
};


/* Full stream handle (opaque to applications via rig_stream_t). */
struct rig_stream
{
    int id;                         /* Stream ID (unique per type per rig) */
    rig_stream_type_t type;
    struct rig_stream_config config;
    struct rig_stream_ringbuf ringbuf;
    HAMLIB_ATOMIC int active;          /* 1 = running, 0 = closed */
    HAMLIB_ATOMIC int paused;          /* 1 = backend I/O stopped */
    HAMLIB_ATOMIC int muted;           /* 1 = TX writes discarded, RX reads
                                          return silence (both API and backend
                                          hook paths) */
    vfo_t vfo;                      /* VFO this stream is associated with */
    freq_t center_freq;             /* RF center of the I/Q window (Hz), set by
                                       panadapter-driven backends under
                                       ringbuf.lock. 0 = unset, frontend then
                                       reports the VFO frequency. */
    struct rig_stream_metadata
        last_metadata;  /* Latest metadata: sent on a producing stream (change
                     * detection), or ingested from the wire on a netrigctl
                     * consumer stream */
    void *backend_priv;             /* Backend-specific per-stream state */
    HAMLIB_ATOMIC int
    gap_count;        /* Radio-side packet gaps detected by backend */

    /* Loss accounting (all protected by ringbuf.lock).
     * The producer sample-index domain counts radio-emitted samples:
     * bytes written to the ring buffer plus index holes inserted by
     * stream_skip_samples() for losses that never produced bytes. */
    int frame_bytes;                /* Bytes per frame (sample x channels);
                                     * 0 = unknown (compressed formats) */
    int max_payload;                /* Effective frame-aligned sender payload
                                     * budget (bytes), from config.mtu */
    int caps_flags;                 /* RIG_STREAM_CAP_* from the caps entry */
    int tx_horizon_ms;              /* Max timed-TX lead time (0 = none) */
    uint64_t skipped_samples;       /* Index holes inserted by skip/mark_gap */
    uint64_t next_expected;         /* Consumer position for drop detection */
    uint64_t skip_samples_unread;   /* Sized skips since the last consume */
    uint8_t  pending_drop_flags;    /* RIG_STREAM_DROP_* since last consume */
    uint32_t gaps_unknown;          /* Gap events with unknown size */
    uint32_t remote_overruns;       /* Overruns reported by a remote server */
    uint32_t remote_underruns;      /* Underruns reported by a remote server */
    uint32_t link_loss;             /* Network client: app-link UDP loss events */
    uint32_t tx_late;               /* Timed TX bursts that missed their slot */
    uint32_t write_events_dropped;     /* Write-status events dropped on overflow */
    uint64_t dropped_samples_gap;      /* Per-cause dropped-sample totals */
    uint64_t dropped_samples_overrun;
    uint64_t dropped_samples_link;

    /* Time anchors (protected by ringbuf.lock) */
    struct rig_stream_time_anchor anchors[RIG_STREAM_ANCHOR_DEPTH];
    int anchor_head;                /* Next write slot */
    int anchor_count;               /* Valid entries (<= depth) */
    unsigned int stale_coarse_ms;       /* Effective watchdog thresholds, */
    unsigned int stale_invalidate_ms;   /* resolved at stream open */

    /* TX burst targets, FIFO (protected by ringbuf.lock). */
    struct rig_stream_tx_target tx_targets[RIG_STREAM_TX_TARGET_DEPTH];
    int tx_target_head;             /* Next write slot */
    int tx_target_count;            /* Valid entries (<= depth) */

    /* Write-status event FIFO (TX streams; protected by ringbuf.lock).
     * Bounded, drop-oldest; consumers block on write_event_available. */
    struct rig_stream_write_status write_events[RIG_STREAM_WRITE_EVENT_DEPTH];
    int write_event_head;           /* Next write slot */
    int write_event_count;          /* Valid entries (<= depth) */
    pthread_cond_t write_event_available;  /* Signalled on push / close */
    int write_event_closing;        /* 1 = stream closing; wake waiters */

    /* Close rendezvous (protected by ringbuf.lock). A close marks the stream
     * closing, wakes blocked rig_stream_read()/wait_write_status() callers,
     * and waits on quiesced until blocked_waiters reaches 0 before destroying
     * the ring/condvars and freeing the stream. */
    RIG *rig;                       /* Owning rig, for the backend close hook */
    int blocked_waiters;            /* Threads inside a blocking read/wait call */
    pthread_cond_t quiesced;        /* Signalled when a blocked caller exits */

    /* In-flight public-API-call count, guarded by rig_stream_state.stream_mutex
     * (NOT ringbuf.lock, which close destroys). Each rig_stream_* call
     * increments this under stream_mutex after confirming the stream is still
     * registered; close removes the stream from the registry, then waits on
     * rig_stream_state.stream_idle for in_use to reach 0 before teardown, so a
     * call in progress can never touch freed memory. */
    int in_use;
};


/* Per-rig streaming state (pointed to by rig_state.stream_state). */
struct rig_stream_state
{
    struct rig_stream *streams[RIG_STREAM_TYPE_COUNT][HAMLIB_MAX_STREAMS];
    pthread_mutex_t stream_mutex;
    pthread_cond_t stream_idle;     /* Signalled when a stream's in_use hits 0 */
    int stream_next_id[RIG_STREAM_TYPE_COUNT];
};


/* Allocate and initialize stream state. Returns 0 on success, -1 on failure. */
int rig_stream_state_init(struct rig_stream_state **state);

/* Free stream state and close any open streams. */
void rig_stream_state_cleanup(struct rig_stream_state *state);

/* Compare two metadata structs, ignoring sample_index.
 * Returns 1 if any data field differs, 0 if identical. */
int stream_metadata_changed(const struct rig_stream_metadata *a,
                            const struct rig_stream_metadata *b);


/* --- Loss accounting (backend- and client-facing) --- */

/* Account a loss of dropped_samples samples (0 = size unknown) at the current
 * producer position, attributed to drop_flag (RIG_STREAM_DROP_GAP, _OVERRUN
 * for remote-replayed overruns, or _LINK). Sized losses advance the
 * sample-index domain without writing bytes, so they surface through the
 * same dropped_samples path as a local ring overrun. */
void stream_skip_samples(struct rig_stream *stream, uint64_t dropped_samples,
                         uint8_t drop_flag);

/* Producer index of the oldest readable byte; caller holds ringbuf.lock. */
uint64_t stream_first_readable_index_locked(struct rig_stream *stream);

/* Initialize / destroy the write-status event and close-rendezvous condvars
 * (clock matched to the ringbuf). rig_stream_open()/close() call these;
 * white-box tests that build a bare struct rig_stream (after stream_ringbuf_init) must
 * init them before waiting. */
void stream_write_event_init(struct rig_stream *stream);
void stream_write_event_destroy(struct rig_stream *stream);

/* Record a write-status event: bump the matching stat counter, push onto the
 * per-stream FIFO (drop-oldest on overflow, counting write_events_dropped), and
 * wake a blocked rig_stream_wait_write_status(). Takes ringbuf.lock.
 * remote != 0 marks the event as occurring on the remote radio and routes it
 * to the remote_* stat counters. */
void stream_record_write_status(struct rig_stream *stream,
                                const struct rig_stream_write_status *ev,
                                int remote);


#endif /* HAMLIB_STREAM_H */
