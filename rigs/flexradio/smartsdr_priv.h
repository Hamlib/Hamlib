/*
 *  Hamlib SmartSDR backend - shared private state
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

/* The state every SmartSDR module shares: the per-rig private data, the
 * per-stream data hanging off it, the enumerations that index them and the
 * configuration tokens that fill them in. Each module owns one part of this
 * state, so it lives here rather than in any one of their headers. */

#ifndef SMARTSDR_PRIV_H
#define SMARTSDR_PRIV_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdint.h>
#include <pthread.h>
/* Socket headers come from stream_proto.h, which picks the right set for the
 * host; do not include them directly. */
#include "stream_proto.h"

#include <hamlib/rig.h>
#include "stream.h"
#include "token.h"

/* RX streams that can be open at once: one audio plus one I/Q, with headroom. */
#define SMARTSDR_MAX_RX_STREAMS 4

/* Meters the backend reports as Hamlib levels. The radio advertises about 35
 * and indexes them differently per radio and firmware, so each is resolved by
 * name from the meter status lines rather than by a fixed index. */
enum
{
    SMARTSDR_MTR_LEVEL,         /* slice signal strength, dBm */
    SMARTSDR_MTR_SWR,
    SMARTSDR_MTR_ALC,
    SMARTSDR_MTR_FWDPWR,        /* dBm */
    SMARTSDR_MTR_PATEMP,        /* degC */
    SMARTSDR_MTR_VOLTS,
    SMARTSDR_MTR_AMPS,
    SMARTSDR_MTR_COUNT
};

/* Widest panadapter the backend will accept, in FFT bins. */
#define SMARTSDR_MAX_SPECTRUM_BINS 4096

/* Panadapter height in pixels. Bin values are heights within it, so it is also
 * the scale a bin is reported against. */
#define SMARTSDR_SPECTRUM_YPIXELS   255


/* Per-stream backend state, stored in rig_stream.backend_priv. */
struct smartsdr_stream_state
{
    struct rig_stream *stream;          /* Back-pointer to frontend handle */
    RIG *rig;                           /* Owning rig (for PTT keying) */
    pthread_t thread;                   /* RX or TX data thread */
    HAMLIB_ATOMIC int running;          /* Thread run flag */
    uint32_t vita_stream_id;            /* Radio-assigned VITA-49 stream ID */
    int dax_channel;                    /* DAX channel (1-8) */
    uint32_t pan_id;                    /* Panadapter ID (DAXIQ only) */
    int pan_created_by_us;              /* 1 if we created the panadapter */
    uint8_t vita_packet_count;          /* 4-bit TX sequence counter */
    uint8_t last_packet_count;          /* 4-bit VITA sequence, for gap detection */
    uint64_t last_timestamp_frac;       /* to see whether the radio's clock runs */
    /* Latest UTC-traceable stamp the radio sent, for stream_hardware_time.
     * Written by the RX thread and read by whoever asks the time, both under
     * the priv state lock. Only a GPSDO produces one, so it stays invalid on a
     * radio without one. */
    int64_t hw_seconds;
    uint64_t hw_picoseconds;
    uint8_t hw_time_valid;
    uint8_t timeline_seen;              /* the fractional stamp has advanced */
    uint8_t timeline_have_prev;         /* a stamp to compare the next against */
    int seq_initialized;                /* 0 until first packet received */
    uint32_t anchor_pkt_counter;        /* RX: host-anchor cadence counter */
    uint64_t tx_consumed;               /* TX: frames consumed (pairs) */
    int burst_ptt;                      /* TX: SOB/EOB keys PTT (from caps) */
};


/* Which SmartSDR object carries a property, and so which command sets it and
 * which status line reports it. */
enum smartsdr_object
{
    SMARTSDR_OBJ_SLICE = 0,
    SMARTSDR_OBJ_TRANSMIT,
    SMARTSDR_OBJ_INTERLOCK      /* radio-wide transmit state */
};

/* Properties tracked from radio status. Indices into smartsdr_priv_data.props,
 * so the level/func tables can name a property without a field per value. */
enum
{
    SMARTSDR_P_AUDIO_LEVEL, SMARTSDR_P_SQUELCH_LEVEL, SMARTSDR_P_NR_LEVEL,
    SMARTSDR_P_NB_LEVEL, SMARTSDR_P_APF_LEVEL, SMARTSDR_P_AUDIO_PAN,
    SMARTSDR_P_RFGAIN, SMARTSDR_P_AUDIO_MUTE, SMARTSDR_P_SQUELCH,
    SMARTSDR_P_NR, SMARTSDR_P_NB, SMARTSDR_P_WNB, SMARTSDR_P_ANF,
    SMARTSDR_P_APF, SMARTSDR_P_LOCK, SMARTSDR_P_RIT_ON, SMARTSDR_P_XIT_ON,
    SMARTSDR_P_DIVERSITY, SMARTSDR_P_AGC_MODE, SMARTSDR_P_RXANT,
    SMARTSDR_P_TXANT, SMARTSDR_P_TX_ANT_LIST,

    SMARTSDR_P_RFPOWER, SMARTSDR_P_MIC_LEVEL, SMARTSDR_P_SP_LEVEL,
    SMARTSDR_P_VOX_LEVEL, SMARTSDR_P_VOX_DELAY, SMARTSDR_P_SPEED,
    SMARTSDR_P_PITCH, SMARTSDR_P_BREAK_IN_DELAY, SMARTSDR_P_MON_GAIN_SB,
    SMARTSDR_P_MON_GAIN_CW, SMARTSDR_P_VOX_ENABLE, SMARTSDR_P_SP_ENABLE,
    SMARTSDR_P_BREAK_IN, SMARTSDR_P_SB_MONITOR, SMARTSDR_P_TUNE,

    /* Core operating state, tracked the same way as everything else. */
    SMARTSDR_P_RF_FREQUENCY, SMARTSDR_P_MODE, SMARTSDR_P_FILTER_LO,
    SMARTSDR_P_FILTER_HI, SMARTSDR_P_TX, SMARTSDR_P_RIT_FREQ,
    SMARTSDR_P_XIT_FREQ, SMARTSDR_P_STEP,

    SMARTSDR_P_STATE,           /* interlock: RECEIVE / TRANSMITTING */

    SMARTSDR_PROP_COUNT
};

/* A caller waiting for the answer to the command it sent. The radio numbers
 * every reply with the sequence of the command that earned it, so a waiter is
 * matched exactly and two callers may have commands outstanding at once
 * without taking each other's answers. Nodes live on the waiting caller's own
 * stack and are linked in only for as long as it waits. */
struct smartsdr_reply_waiter
{
    struct smartsdr_reply_waiter *next;
    int      seq;               /* the C<seq> this caller wrote */
    int      answered;          /* the reply has been delivered */
    unsigned status;            /* the R-line's status word */
    char    *resp_buf;          /* caller's buffer for the whole R-line */
    int      resp_buf_len;
};


/* Longest string-valued property is the antenna list, e.g. "ANT1,ANT2,XVTA". */
struct smartsdr_prop_value
{
    int      ival;
    double   dval;          /* for properties that need range or fraction */
    char     sval[40];
    uint8_t  seen;          /* 0 until the radio has reported it */
    int64_t  stamp_ms;      /* when it was last reported or set */
};


/* Enough of every slice to find the transmit slice and report split. Slices
 * belong to the radio, not to a client, so any of them may be the one
 * transmitting. */
#define SMARTSDR_MAX_SLICES 8

struct smartsdr_slice_info
{
    uint8_t in_use;
    uint8_t tx;             /* exactly one slice has this set */
    uint8_t active;         /* radio's selected slice; also exclusive */
    uint8_t mode_seen;
    double  freq_hz;
    rmode_t mode;
    int     filter_lo;
    int     filter_hi;
};

/* Room for a reply body. The radio's longest answers are slice and version
 * status lines, which fit well inside this; a reply that did not would be
 * truncated rather than misread. */
#define SMARTSDR_RESP_LEN 256


/* Per-rig SmartSDR private data (stored in rig_state.priv). */
struct smartsdr_priv_data
{
    int slicenum;                       /* Slice 0-7 maps to A-H */
    int seqnum;                         /* TCP command sequence number */
    /* Written by the control thread as it absorbs status, read by the API
     * thread when a stream opens, so both are atomic. A stale read here would
     * bind DAX-IQ to the wrong panadapter or miss the waterfall entirely. */
    HAMLIB_ATOMIC uint32_t slice_pan_id; /* From slice status pan= (0 if none) */
    HAMLIB_ATOMIC uint32_t slice_waterfall_id; /* display pan status waterfall= */
    HAMLIB_ATOMIC uint32_t pan_waterfall_bind_pan_id; /* awaiting waterfall */
    uint32_t client_handle;             /* From slice status client_handle= (TCP API) */
    uint8_t nat_traversal;              /* Radio must learn a NAT-translated UDP address */
    /* Values tracked from radio status, indexed by SMARTSDR_P_*. */
    struct smartsdr_prop_value props[SMARTSDR_PROP_COUNT];
    struct smartsdr_slice_info slices[SMARTSDR_MAX_SLICES];
    int split_created_slice;            /* slice we created for split, else -1 */
    int opened_slice;                   /* slice we created at open, else -1 */
    uint8_t slice_explicit;             /* the slice was named, not defaulted */
    uint8_t slice_missing_fails;        /* config: fail open instead of creating */
    uint8_t tx_dax_enabled;             /* we switched the TX source to DAX */
    char tx_audio_source[8];            /* config: mic, acc, pc or dax */

    /* What the radio's clock is worth. A GPSDO makes its timestamps traceable
     * to UTC; a TCXO makes them stable but leaves the epoch arbitrary. */
    uint8_t gpsdo_present;
    uint8_t tcxo_present;
    int split_slice_override;           /* config: use this slice, -1 = tx=1 slice */
    char info[160];                     /* model, serial and firmware, for get_info */
    double pan_center_hz;               /* DAX-IQ is referenced to this, not the slice */
    /* Set once when the radio stops answering; the stream threads poll it so
     * a reader gets an error instead of waiting forever. */
    HAMLIB_ATOMIC int session_lost;
    unsigned session_lost_reason;       /* RIG_COMM_REASON_* */
    uint8_t auto_reconnect;             /* config: rebuild the session on loss */
    HAMLIB_ATOMIC int reconnect_running;
    HAMLIB_ATOMIC int stop_reconnect;
    pthread_t reconnect_thread;
    int status_timeout_ms;              /* 0 = tracked values never expire */
    char tcp_asm_buf[16384];            /* Reassemble fragmented TCP lines */
    size_t tcp_asm_len;
    HAMLIB_ATOMIC int keepalive_running; /* Keepalive thread run flag */
    pthread_t keepalive_thread;         /* Sends ping every 10s */
    HAMLIB_ATOMIC int wan_register_running;
    pthread_t wan_register_thread;      /* WAN: client udp_register + ping */
    pthread_mutex_t tcp_mutex;          /* Serializes writers to the socket */

    /* One thread owns the read side of the control connection: it absorbs
     * status the moment it arrives and hands each R-line to the caller
     * waiting for that sequence number. Callers write for themselves, under
     * tcp_mutex, so the thread never has to be asked to send anything. */
    pthread_t control_thread;
    HAMLIB_ATOMIC int control_running;  /* Control thread run flag */
    int control_started;                /* A thread exists to be joined */
    pthread_mutex_t reply_lock;         /* Guards the waiter list */
    pthread_cond_t reply_cond;
    struct smartsdr_reply_waiter *reply_waiters;

    /* Any byte from the radio is proof it is still there, so silence rather
     * than a failed command is what ends the session. */
    int64_t last_heard_ms;
    int liveness_timeout_ms;            /* silence that means the radio is gone */
    int vita_port;                      /* radio's VITA-49 UDP port (4991) */

    /* The radio as this backend models it -- props, slices and the meter
     * table. The control thread writes it one status line at a time and a
     * reader holds it for as long as one value takes to copy. */
    pthread_mutex_t state_lock;

    /* The radio sends every VITA datagram -- audio, I/Q and metering alike --
     * to the one UDP endpoint the client registered, so the socket belongs to
     * the rig and a single thread fans packets out by stream ID. Per-stream
     * sockets would each have to claim that endpoint, and only the last one
     * to ask would receive anything. */
    int udp_sock;                       /* -1 when no stream is open */
    int udp_port;                       /* local port the radio sends to */
    struct sockaddr_in radio_udp_addr;  /* radio's VITA address */
    pthread_t dispatch_thread;
    HAMLIB_ATOMIC int dispatch_running;
    int udp_users;                      /* open streams sharing the socket */
    int udp_closing;                    /* 1 while the socket is being torn
                                           down; an opener waits it out rather
                                           than claiming a descriptor that is
                                           about to be closed */
    pthread_cond_t udp_idle;            /* signalled when a teardown finishes */
    pthread_mutex_t stream_lock;        /* guards rx_streams[] and udp_users */
    struct smartsdr_stream_state *rx_streams[SMARTSDR_MAX_RX_STREAMS];

    /* Metering. The radio streams meter values continuously once subscribed,
     * so a read returns the last value rather than asking for one. */
    uint8_t meters_subscribed;
    int16_t meter_wire_id[SMARTSDR_MTR_COUNT];  /* -1 until the radio names it */
    int16_t meter_fps[SMARTSDR_MTR_COUNT];      /* 0 = sent on change, not timed */

    /* Spectrum, from the panadapter FFT. The radio may split one frame across
     * datagrams, so bins are reassembled by their start index before a line is
     * handed to the application. */
    uint8_t spectrum_enabled;           /* config: create/bind a panadapter */
    uint32_t spectrum_pan_id;           /* the panadapter being read */
    int spectrum_pan_created;           /* 1 if this backend created it */
    double spectrum_center_hz;
    double spectrum_span_hz;
    double spectrum_min_dbm;
    double spectrum_max_dbm;
    int spectrum_height;                /* panadapter y_pixels; bins scale to it */
    uint32_t spectrum_frame;            /* frame being reassembled */
    int spectrum_have;                  /* bins filled for that frame */
    int spectrum_total;                 /* bins the frame will have */
    unsigned char spectrum_bins[SMARTSDR_MAX_SPECTRUM_BINS];
    double meter_value[SMARTSDR_MTR_COUNT];
    int64_t meter_stamp_ms[SMARTSDR_MTR_COUNT]; /* 0 until first reported */
};


/* ------------------------------------------------------------------ */
/* Configuration tokens                                                */
/* ------------------------------------------------------------------ */

/* Longest staleness the status_timeout setting accepts, matching what its
 * parameter description advertises. */
#define SMARTSDR_STATUS_TIMEOUT_MAX_MS 3600000

/* Silence after which the radio is declared gone, and the range the
 * liveness_timeout setting accepts. Anything shorter than the minimum leaves
 * no room for the round trip that proves the radio is still there. */
#define SMARTSDR_LIVENESS_TIMEOUT_MS      20000
#define SMARTSDR_LIVENESS_TIMEOUT_MIN_MS    500
#define SMARTSDR_LIVENESS_TIMEOUT_MAX_MS 3600000

#define TOK_NAT_TRAVERSAL   TOKEN_BACKEND(1)
#define TOK_STATUS_TIMEOUT  TOKEN_BACKEND(2)
#define TOK_SPLIT_SLICE     TOKEN_BACKEND(3)
#define TOK_AUTO_RECONNECT  TOKEN_BACKEND(4)
#define TOK_SLICE           TOKEN_BACKEND(5)
#define TOK_SPECTRUM        TOKEN_BACKEND(6)
#define TOK_SLICE_MISSING   TOKEN_BACKEND(7)
#define TOK_TX_AUDIO_SOURCE TOKEN_BACKEND(8)
#define TOK_LIVENESS_TIMEOUT TOKEN_BACKEND(9)
#define TOK_VITA_PORT        TOKEN_BACKEND(10)

/* Radio UDP port carrying VITA-49 IQ/audio (same as Flex GUI clients), and
 * the default for the vita_port token. */
#define SMARTSDR_VITA_UDP_PORT 4991


/* NAT keepalive cadence. The UDP nudge is what actually holds a NAT mapping
 * open — a TCP command cannot — so the nudge is frequent and the TCP
 * re-registration is a slow backstop. */
#define SMARTSDR_NAT_NUDGE_INTERVAL_MS    (30 * 1000)
#define SMARTSDR_NAT_REREGISTER_INTERVAL_MS (5 * 60 * 1000)

/* Monotonic milliseconds, for property timestamps and interval measurement. */
extern int64_t smartsdr_now_ms(void);

#endif /* SMARTSDR_PRIV_H */
