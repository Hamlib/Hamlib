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

/* UDP streaming protocol for rigctld — registry, feeders, lifecycle management. */
/* Used by rigctld feeder threads and unit tests. */

#ifndef RIGCTLD_STREAM_H
#define RIGCTLD_STREAM_H

#include <hamlib/rig.h>
#include "stream_proto.h"
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <time.h>

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#elif defined(HAVE_WS2TCPIP_H)
#  include <ws2tcpip.h>
#endif


/* --- Stream command codes for rigctld --- */

#define RIGCTLD_CMD_STREAM_CAPS         0xb0
#define RIGCTLD_CMD_STREAM_OPEN         0xb1
#define RIGCTLD_CMD_STREAM_CLOSE        0xb2
#define RIGCTLD_CMD_STREAM_STATUS       0xb3
#define RIGCTLD_CMD_STREAM_PAUSE        0xb4
#define RIGCTLD_CMD_STREAM_RESUME       0xb5
#define RIGCTLD_CMD_STREAM_MUTE         0xb6
#define RIGCTLD_CMD_STREAM_UNMUTE       0xb7
#define RIGCTLD_CMD_STREAM_METADATA_GET 0xb8
#define RIGCTLD_CMD_STREAM_FLUSH        0xb9
#define RIGCTLD_CMD_STREAM_LIST         0xba


/* --- Stream Registry --- */

/* These are rigctld daemon operational limits (not library constants): they
 * bound the daemon's own resource use and live with the daemon, not libhamlib. */

/* Maximum concurrent streams per type in the registry */
#define RIGCTLD_MAX_STREAMS 32

/* Subscribe and keepalive timeout bounds and default (seconds). The same
 * window covers the wait for the initial SUBSCRIBE and the silence a
 * subscribed client may go without being dropped. Dividing it by the client's
 * ping interval gives the number of consecutive lost pings a stream survives,
 * so a lossy link wants a wider window rather than a faster ping. */
#define RIGCTLD_SUBSCRIBE_TIMEOUT_MIN     5
#define RIGCTLD_SUBSCRIBE_TIMEOUT_MAX     3600
#define RIGCTLD_SUBSCRIBE_TIMEOUT_DEFAULT 30

/* Metadata polling interval bounds and default (milliseconds) */
#define RIGCTLD_METADATA_INTERVAL_MIN     25    /* 40/s max rate */
#define RIGCTLD_METADATA_INTERVAL_MAX     1000  /* 1/s min rate */
#define RIGCTLD_METADATA_INTERVAL_DEFAULT 100

/* Unconditional metadata refresh cadence, measured in stream-data duration
 * (default milliseconds). Heals a lost change-detected frame without waiting
 * for the next change. 0 = emit a metadata frame with every data packet. */
#define RIGCTLD_METADATA_REFRESH_DEFAULT  100

/* Forward seq gaps larger than this are treated as a single discontinuity
 * rather than a literal lost-packet count (anti-poisoning bound). */
#define RIGCTLD_MAX_PLAUSIBLE_SEQ_GAP (1u << 20)


/* HAMLIB_ATOMIC is defined in hamlib/rig.h */


/* Per-stream state tracked by rigctld. */
struct rigctld_stream
{
    int stream_id;
    uint16_t source_id;     /* Stream source ID stamped on every server
                               frame (0 = unset, tuple identity) */
    rig_stream_type_t type;
    struct rig_stream_config config;

    /* Backend stream handle (from rig_stream_open) */
    rig_stream_t *backend_stream;

    /* RIG handle for rig_stream_read/write calls in feeder thread */
    RIG *rig;

    /* Client that owns this stream (for disconnect cleanup) */
    int client_id;

    /* Anti-hijack token (random, generated at stream_open) */
    uint32_t subscribe_token;

    /* UDP socket */
    int udp_sock;
    int udp_port;                           /* Local bound port */
    struct sockaddr_storage client_addr;    /* Client address (from first packet) */
    socklen_t client_addr_len;
    int client_addr_known;                  /* 0 until first packet received */
    struct sockaddr_storage tcp_client_addr; /* TCP client IP (for validation) */

    /* Multicast */
    int multicast;                          /* 1 = multicast, 0 = unicast */
    struct sockaddr_storage multicast_addr;
    socklen_t multicast_addr_len;
    int multicast_ttl;

    /* Subscribe tracking (unicast RX only) */
    time_t last_subscribe;
    int subscribe_timeout_s;

    /* Pump thread */
    pthread_t feeder_thread;
    HAMLIB_ATOMIC int running;
    HAMLIB_ATOMIC int auto_closed;  /* 1 if feeder self-terminated (timeout) */
    int feeder_started;     /* 1 after pthread_create, 0 after join */

    /* Packet state — data and metadata share a single sequence counter so
     * the receiver can detect any dropped packet (data or metadata) via gaps. */
    uint32_t seq;                           /* Outgoing sequence number */
    uint32_t expected_seq;                  /* Expected incoming sequence */
    uint64_t timestamp;                     /* Sample counter */

    /* Statistics.  Atomic so command handlers can read them lock-free while
     * the feeder thread updates them (the feeder mutates them inside the
     * registry lock; the atomic qualifier covers the unlocked reads). */
    HAMLIB_ATOMIC int packet_count;
    HAMLIB_ATOMIC uint32_t gap_count;
    HAMLIB_ATOMIC int send_drops;

    /* Metadata */
    int metadata_interval_ms;
    int metadata_refresh_ms;   /* Unconditional refresh cadence (stream-data
                                * ms); 0 = every data packet */
    struct rig_stream_metadata last_sent_meta;

    /* Cached invariants (computed once at feeder start) */
    uint8_t format_id;               /* Wire format index for headers */
    int sample_size;                     /* Bytes per sample (0 = unknown) */
};


/* Registry of all open streams for a rigctld instance. */
struct rigctld_stream_registry
{
    struct rigctld_stream *streams[RIG_STREAM_TYPE_COUNT][RIGCTLD_MAX_STREAMS];
    int next_id;    /* Monotonically increasing global stream ID counter */
    int metadata_interval_ms;  /* Default metadata interval for new streams */
    int metadata_refresh_ms;   /* Default metadata refresh cadence for new streams */
    int transport_buffer_ms;            /* Default socket-buffer duration (ms of stream data) */
    int transport_buffer_bytes;         /* Default socket-buffer size override (0 = derive) */
    int multicast_ttl;         /* Default multicast TTL for new streams */
    int source_id;             /* Stream source ID for new streams (0 = unset) */
    int keepalive_timeout_s;   /* Default silence before a client is dropped */
    pthread_mutex_t lock;
    int initialized;           /* 1 between registry_init() and _destroy() */
};


/* Global stream registry shared by rigctld command handlers. */
extern struct rigctld_stream_registry g_stream_registry;

/* Initialize a stream registry (zeroes slots, inits mutex).
 * Returns 0 on success, -1 on failure. */
int rigctld_stream_registry_init(struct rigctld_stream_registry *reg);

/* Destroy registry and free all stream entries. */
void rigctld_stream_registry_destroy(struct rigctld_stream_registry *reg);

/* Insert a stream into the registry.
 * Returns 0 on success, -1 if no free slot. */
int rigctld_stream_registry_insert(struct rigctld_stream_registry *reg,
                                   struct rigctld_stream *stream);

/* Look up a stream by type and stream_id.
 * Returns pointer to stream, or NULL if not found. */
struct rigctld_stream *rigctld_stream_registry_lookup(
    struct rigctld_stream_registry *reg,
    rig_stream_type_t type, int stream_id);

/* Lock/unlock the registry mutex.
 * Use with find_by_id_unlocked to hold the lock while accessing a stream. */
void rigctld_stream_registry_lock(struct rigctld_stream_registry *reg);
void rigctld_stream_registry_unlock(struct rigctld_stream_registry *reg);

/* Look up a stream by ID, searching all types.  Caller must hold reg->lock. */
struct rigctld_stream *rigctld_stream_registry_find_by_id_unlocked(
    struct rigctld_stream_registry *reg, int stream_id);

/* Look up a stream by ID, searching all types (locks internally).
 * Returns pointer to stream, or NULL if not found. */
struct rigctld_stream *rigctld_stream_registry_find_by_id(
    struct rigctld_stream_registry *reg, int stream_id);

/* Atomically find, check ownership, and remove a stream by ID.
 * Returns the removed pointer (caller must stop feeder + free), or NULL.
 * On NULL: *err is -RIG_EINVAL (not found) or -RIG_EACCESS (not owner). */
struct rigctld_stream *rigctld_stream_registry_find_remove_by_id(
    struct rigctld_stream_registry *reg, int stream_id,
    int client_id, int *err);

/* Remove a stream from the registry by type and stream_id.
 * Returns the removed pointer (caller must free), or NULL if not found. */
struct rigctld_stream *rigctld_stream_registry_remove(
    struct rigctld_stream_registry *reg,
    rig_stream_type_t type, int stream_id);

/* Remove and free all streams belonging to a given client_id.
 * Returns the number of streams closed. */
int rigctld_stream_registry_close_by_client(struct rigctld_stream_registry *reg,
        int client_id);

/* Remove and free every stream while leaving the registry reusable.
 * Returns the number of streams closed. */
int rigctld_stream_registry_close_all(struct rigctld_stream_registry *reg);

/* Count active streams in the registry. */
int rigctld_stream_registry_count(struct rigctld_stream_registry *reg);

/* Allocate and zero-initialize a rigctld_stream.
 * Caller must populate fields and insert into registry. */
struct rigctld_stream *rigctld_stream_alloc(void);

/* Free a rigctld_stream (does NOT close UDP socket or stop feeder thread).
 * For full cleanup, stop the feeder and close resources before calling. */
void rigctld_stream_free(struct rigctld_stream *stream);

/* Close backend stream, UDP socket, and free memory.
 * Does NOT stop the feeder thread — caller must handle that. */
void rigctld_stream_cleanup_resources(struct rigctld_stream *stream);

/* Create a UDP socket bound to an ephemeral port on INADDR_ANY.
 * On success, sets *sock_fd and *port and returns 0.
 * On failure, returns -1. */
int rigctld_stream_udp_socket_create(int *sock_fd, int *port);

/* Allocate the next globally unique stream ID.
 * Returns -1 if all slots for the stream's type are occupied. */
int rigctld_stream_registry_next_id(struct rigctld_stream_registry *reg);

/* Whether an unconditional metadata refresh is due after a data packet.
 * refresh_ms == 0 -> every data packet that carried samples; otherwise when
 * the stream-time of frames since the last metadata frame reaches refresh_ms.
 * samples_since is in frames (per-channel sample instants). */
int rigctld_stream_metadata_refresh_due(int refresh_ms, int sample_rate,
                                        uint64_t samples_since);

/* Generate an unpredictable, non-zero subscribe token from the platform
 * cryptographic RNG. Token 0 is reserved as the "unset" sentinel. */
uint32_t rigctld_stream_generate_token(void);

/* Parse stream_open text arguments into a rig_stream_config.
 * type_str: "AUDIO_RX", "IQ_TX", etc.
 * format_str: "PCM_S16", "IQ_CF32", etc.
 * rate_str: sample rate as decimal string (e.g. "48000").
 * Channels always default to 1 (the text command has no channels argument).
 * Returns 0 on success, -1 on invalid arguments. */
int rigctld_stream_config_from_args(const char *type_str,
                                    const char *format_str,
                                    const char *rate_str,
                                    struct rig_stream_config *config);


/* --- Multicast support --- */

/* Parse "ADDR:PORT" or "[ADDR]:PORT" into sockaddr_storage.
 * Validates address is in multicast range (IPv4 224.0.0.0/4, IPv6 ff00::/8).
 * Returns 0 on success, -1 on error. */
int rigctld_stream_multicast_addr_parse(const char *spec,
                                        struct sockaddr_storage *addr,
                                        socklen_t *addrlen);

/* Create a UDP socket for multicast sending.
 * Socket family matches the multicast address family.
 * Sets TTL/hops and SO_REUSEADDR. Binds to the multicast port.
 * Returns 0 on success, -1 on error. */
int rigctld_stream_multicast_socket_create(
    const struct sockaddr_storage *mcast_addr,
    socklen_t addrlen,
    int ttl,
    int *sock_fd,
    int *port);

/* Check if a multicast group:port is already in use by another stream.
 * Returns 1 if in use, 0 if available. */
int rigctld_stream_registry_multicast_in_use(
    struct rigctld_stream_registry *reg,
    const struct sockaddr_storage *mcast_addr);


/* Send a control reply (PONG, SUBSCRIBE_ACK, etc.) to the stream's client.
 * control_flag is one of the RIG_STREAM_CTRL_* bits (e.g. RIG_STREAM_CTRL_PONG).
 * Returns 0 on success, -1 on failure. */
int rigctld_stream_send_control_reply(struct rigctld_stream *stream,
                                      uint16_t control_flag);

/* Forward one write-status event to the client as a WRITE_STATUS packet.
 * The TX feeder normally drains these from the backend stream's event FIFO;
 * exposed as a thin seam for tests. Returns 0 on success. */
int rigctld_stream_emit_write_status(struct rigctld_stream *stream,
                                     const struct rig_stream_write_status *ev);

/* Auto-close a stream from within its feeder thread.
 * Marks the stream as dead (auto_closed=1) and closes backend/socket
 * under the registry lock.  The stream stays in the registry so
 * stream_close or close_by_client can join the thread and free it.
 * Caller (feeder thread) must return NULL immediately after this call. */
int rigctld_stream_auto_close(struct rigctld_stream_registry *reg,
                              struct rigctld_stream *stream);

/* --- Pump thread management --- */

/* Start the feeder thread for a stream.
 * For RX types (AUDIO_RX, IQ_RX): reads from backend ring buffer, sends UDP.
 * For TX types (AUDIO_TX, IQ_TX): receives UDP, writes to backend ring buffer.
 * Returns 0 on success, -1 on failure. */
int rigctld_stream_feeder_start(struct rigctld_stream *stream);

/* Stop the feeder thread for a stream.
 * Sets running=0 and joins the thread. Safe to call if feeder was never started.
 * Returns 0 on success, -1 on failure. */
int rigctld_stream_feeder_stop(struct rigctld_stream *stream);


#endif /* RIGCTLD_STREAM_H */
