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

/* Streaming protocol wire format — packet headers, control bits, name mapping. */
/* Shared by libhamlib (src/) and rigctld (tests/) for consistent serialization. */

#ifndef HAMLIB_STREAM_PROTO_H
#define HAMLIB_STREAM_PROTO_H

#include <hamlib/rig.h>
#include <stdint.h>
#include <stddef.h>

/* Portable socket headers, shared by the client and server transports.
 * Requires config.h (for the HAVE_* macros) to be included beforehand. */
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#  include <sys/select.h>
#elif defined(HAVE_WS2TCPIP_H)
#  include <ws2tcpip.h>
#endif

/* Portable socket close — closesocket() on Windows, close() elsewhere */
#ifdef __MINGW32__
#  define socket_close(fd) closesocket(fd)
#else
#  define socket_close(fd) close(fd)
#endif

/* Protocol version */
#define RIG_STREAM_PROTOCOL_VERSION 1

/* Packet header size in bytes */
#define RIG_STREAM_HEADER_SIZE 32

/* Control bits (in network byte order at header offset 28-29).
 * Grouped by role: keepalive pair, handshake pair, then payload-affecting
 * bits, with request/reply on adjacent bits. */
#define RIG_STREAM_CTRL_PING          0x0001  /* Client keepalive ping */
#define RIG_STREAM_CTRL_PONG          0x0002  /* Server keepalive pong reply */
#define RIG_STREAM_CTRL_SUBSCRIBE     0x0004  /* Client subscribe request */
#define RIG_STREAM_CTRL_SUBSCRIBE_ACK 0x0008  /* Server subscribe acknowledgement */
#define RIG_STREAM_CTRL_ERROR         0x0010  /* Error frame */
#define RIG_STREAM_CTRL_TIME          0x0020  /* Payload begins with a time block */
#define RIG_STREAM_CTRL_METADATA      0x0040  /* Metadata frame */
#define RIG_STREAM_CTRL_WRITE_STATUS  0x0080  /* Server->client async write-status frame */
/* Bits 0x0100-0x8000 are reserved; a receiver MUST drop a control frame whose
 * bits it does not recognize (current receivers ignore unknown bits, so a
 * reserved bit may also carry a per-data-packet flag). Earmarks: front-end
 * over-range/clip, fragment-continues, RTCP-style receiver-report, security
 * key-agreement.
 * Do not reuse a reserved bit for something else. */

/* Metadata wire format size (v1) */
#define RIG_STREAM_METADATA_WIRE_SIZE 22

/* Embedded time block size (RIG_STREAM_CTRL_TIME payload prefix) */
#define RIG_STREAM_TIME_BLOCK_SIZE 20

/* Async write-status frame payload size (RIG_STREAM_CTRL_WRITE_STATUS):
 * event(2) + flags(2) + dropped_samples(4) + lateness(8) + seconds(8)
 * + picoseconds(8) + time_source(1) + time_flags(1) + time_accuracy(1)
 * + reserved(1). sample_index rides the packet header timestamp. */
#define RIG_STREAM_WRITE_STATUS_WIRE_SIZE 36

/* Default MTU and payload calculations */
#define RIG_STREAM_DEFAULT_MTU        1500
#define RIG_STREAM_IPV6_HEADER        40
#define RIG_STREAM_UDP_HEADER         8
#define RIG_STREAM_MAX_PAYLOAD_DEFAULT \
    (RIG_STREAM_DEFAULT_MTU - RIG_STREAM_IPV6_HEADER \
     - RIG_STREAM_UDP_HEADER - RIG_STREAM_HEADER_SIZE)  /* 1420 */

/* Jumbo-frame ceiling for a single Hamlib UDP datagram (header + payload).
 * Every receiver sizes its receive buffer to this, so a sender using a larger
 * MTU interoperates with existing receivers with no wire-version change. */
#define RIG_STREAM_MAX_DATAGRAM       9216

/* Smallest configurable sender MTU (classic minimum IPv4 datagram). */
#define RIG_STREAM_MIN_MTU            576

/* Frame-aligned sample payload budget (bytes) for a configured sender MTU.
 * mtu == 0 selects RIG_STREAM_DEFAULT_MTU; any other value is clamped to
 * [RIG_STREAM_MIN_MTU, RIG_STREAM_MAX_DATAGRAM]. The payload is
 * mtu − IPv6 − UDP − header, rounded down to a whole frame when
 * frame_bytes > 0 (frame_bytes <= 0 disables alignment, for compressed
 * formats). */
int stream_max_payload_from_mtu(unsigned int mtu, int frame_bytes);


/* UDP socket buffer sizing (SO_RCVBUF on receivers, SO_SNDBUF on senders).
 * Default is rate-derived: bytes/sec × duration, clamped to [MIN, MAX], so a
 * burst of scheduling latency does not overflow the kernel buffer before the
 * ring buffer absorbs it. */
#define RIG_STREAM_TRANSPORT_BUFFER_DURATION_MS 250
#define RIG_STREAM_TRANSPORT_BUFFER_MIN (256 * 1024)        /* 256 KB floor */
#define RIG_STREAM_TRANSPORT_BUFFER_MAX (8 * 1024 * 1024)   /* 8 MB ceiling */

/* Compute a socket buffer size (bytes) from the stream rate.
 * explicit_bytes > 0 overrides the rate-derived size; otherwise it is
 * sample_rate × frame_bytes × duration_ms / 1000. The result is clamped to
 * [RIG_STREAM_TRANSPORT_BUFFER_MIN, RIG_STREAM_TRANSPORT_BUFFER_MAX]. */
size_t stream_transport_buffer_bytes(int sample_rate, int frame_bytes,
                                     unsigned int duration_ms, size_t explicit_bytes);

/* Resolve the effective socket buffer bytes from the layered knobs, in
 * precedence order: per-stream explicit bytes > rig-token explicit bytes >
 * per-stream ms > rig-token ms > built-in duration (then rate-derived +
 * clamped by stream_transport_buffer_bytes). Any 0 means "unset, fall through". */
size_t stream_transport_buffer_effective(unsigned int cfg_ms,
        unsigned int cfg_bytes,
        unsigned int tok_ms, unsigned int tok_bytes,
        int sample_rate, int frame_bytes);

/* Apply a socket buffer size: which is SO_RCVBUF or SO_SNDBUF. Best-effort —
 * logs the requested and kernel-granted size (kernels clamp/double silently),
 * returns 0 on success, -1 if setsockopt failed. */
int stream_apply_transport_buffer(int sock, int which, size_t bytes);


/*
 * Format ID for the wire protocol (1-byte field at header offset 24).
 * Identifies the payload sample format; maps to/from the RIG_STREAM_FORMAT_*
 * capability bitmask.
 */
#define RIG_STREAM_FMT_ID_PCM_S8      0
#define RIG_STREAM_FMT_ID_PCM_U8      1
#define RIG_STREAM_FMT_ID_PCM_S16     2
#define RIG_STREAM_FMT_ID_PCM_F32     3
#define RIG_STREAM_FMT_ID_OPUS        4
#define RIG_STREAM_FMT_ID_IQ_CS8      5
#define RIG_STREAM_FMT_ID_IQ_CU8      6
#define RIG_STREAM_FMT_ID_IQ_CS16     7
#define RIG_STREAM_FMT_ID_IQ_CF32     8
/* IDs 9-254 reserved for future formats (24-bit PCM, packed 12-bit I/Q, etc.);
 * the binding ceiling is the 32-bit rig_stream_format_t caps bitmask, not this
 * ID space. */
#define RIG_STREAM_FMT_ID_INVALID     0xFF


/*
 * Wire byte order: the packet header, metadata block and time block are all
 * big-endian (network byte order). The sample payload that follows the header
 * is little-endian, which requires a little-endian host (see stream_convert.c).
 */

/*
 * 32-byte UDP packet header (wire format is big-endian).
 *
 * Offset  Size  Field
 * 0       1     version
 * 1       1     type          (rig_stream_type_t)
 * 2       2     stream_id
 * 4       4     subscribe_token (anti-hijack token)
 * 8       4     seq           (sequence number, wrapping)
 * 12      8     timestamp     (sample count since stream start, 64-bit)
 * 20      4     sample_rate
 * 24      1     format        (format ID, see RIG_STREAM_FMT_ID_*)
 * 25      1     channels
 * 26      2     source_id     (stream source ID; 0 = unset, identity falls
 *                             back to the source transport tuple)
 * 28      2     control       (RIG_STREAM_CTRL_* bits)
 * 30      2     payload_len
 */
struct rig_stream_packet_header
{
    uint8_t  version;
    uint8_t  type;
    uint16_t stream_id;
    uint32_t subscribe_token;
    uint32_t seq;
    uint64_t timestamp;
    uint32_t sample_rate;
    uint8_t  format;
    uint8_t  channels;
    uint16_t source_id;
    uint16_t control;
    uint16_t payload_len;
};


/* Serialize header struct to 32-byte wire format (big-endian).
 * buf must point to at least RIG_STREAM_HEADER_SIZE bytes. */
void stream_packet_header_pack(const struct rig_stream_packet_header *hdr,
                               unsigned char *buf);

/* Deserialize 32-byte wire format to header struct.
 * Returns 0 on success, -1 if len < 32 or version unknown. */
int stream_packet_header_unpack(const unsigned char *buf, size_t len,
                                struct rig_stream_packet_header *hdr);

/* Initialize a header-only control packet (PING, PONG, SUBSCRIBE, ACK).
 * Zeroes hdr, sets the current protocol version and an empty payload. */
void stream_control_header_init(struct rig_stream_packet_header *hdr,
                                uint8_t type, uint16_t stream_id,
                                uint32_t subscribe_token, uint16_t control);

/* Derive a stable stream source ID from static sender configuration
 * (FNV-1a over "hostname|listen_port|model_id|pathname"). The result is
 * folded into 0x1000-0xFFFF so a derived ID never collides with a manually
 * assigned one (convention 0x0001-0x0FFF) or the unset value 0. NULL
 * strings are treated as empty. */
uint16_t stream_source_id_derive(const char *hostname, int listen_port,
                                 int model_id, const char *pathname);

/* Convert RIG_STREAM_FORMAT_* bitmask value to wire format ID.
 * Returns RIG_STREAM_FMT_ID_INVALID for unknown formats. */
uint8_t stream_format_to_id(rig_stream_format_t format);

/* Convert wire format ID back to RIG_STREAM_FORMAT_* bitmask value.
 * Returns 0 for unknown IDs. */
rig_stream_format_t stream_id_to_format(uint8_t index);

/* Pack metadata struct to v1 wire format (RIG_STREAM_METADATA_WIRE_SIZE = 22
 * bytes, big-endian). Timestamp is NOT included — carried in the packet header.
 * buf must point to at least RIG_STREAM_METADATA_WIRE_SIZE bytes. */
void stream_metadata_pack(const struct rig_stream_metadata *meta,
                          unsigned char *buf);

/* Unpack metadata from v1 wire format.
 * Caller must set meta->timestamp from the packet header separately.
 * Returns 0 on success, -1 if len < RIG_STREAM_METADATA_WIRE_SIZE. */
int stream_metadata_unpack(const unsigned char *buf, size_t len,
                           struct rig_stream_metadata *meta);

/* Pack the embedded time block (20 bytes, big-endian) carried at the start
 * of the payload when RIG_STREAM_CTRL_TIME is set. The sample index is NOT
 * in the block — it travels in the packet header timestamp field.
 * buf must point to at least RIG_STREAM_TIME_BLOCK_SIZE bytes. */
void stream_time_block_pack(const struct rig_stream_time_anchor *t,
                            unsigned char *buf);

/* Unpack the embedded time block. Caller must set t->sample_index from the
 * packet header separately (it is zeroed here).
 * Returns 0 on success, -1 if len < RIG_STREAM_TIME_BLOCK_SIZE. */
int stream_time_block_unpack(const unsigned char *buf, size_t len,
                             struct rig_stream_time_anchor *t);

/* Pack/unpack the async write-status block (big-endian,
 * RIG_STREAM_WRITE_STATUS_WIRE_SIZE bytes). sample_index and the struct's
 * _reserved tail are not on the wire; the caller sets sample_index from the
 * packet header. unpack zeroes those and returns 0 on success, -1 if
 * len < RIG_STREAM_WRITE_STATUS_WIRE_SIZE. */
void stream_write_status_pack(const struct rig_stream_write_status *st,
                              unsigned char *buf);
int stream_write_status_unpack(const unsigned char *buf, size_t len,
                               struct rig_stream_write_status *st);

/* Combination rule: the TIME bit is valid only on data and time-only
 * packets — never combined with frames that define their own payloads.
 * Returns 1 when the control word is acceptable, 0 when it must be
 * dropped (and logged) by the receiver. */
static inline int stream_ctrl_time_valid(uint16_t control)
{
    if (!(control & RIG_STREAM_CTRL_TIME))
    {
        return 1;
    }

    return !(control & (RIG_STREAM_CTRL_SUBSCRIBE
                        | RIG_STREAM_CTRL_SUBSCRIBE_ACK
                        | RIG_STREAM_CTRL_METADATA
                        | RIG_STREAM_CTRL_WRITE_STATUS
                        | RIG_STREAM_CTRL_ERROR
                        | RIG_STREAM_CTRL_PING
                        | RIG_STREAM_CTRL_PONG));
}

/* Write comma-separated format names for all bits set in the bitmask.
 * Returns number of characters written (excluding NUL), or -1 if buf too small. */
int stream_format_bitmask_str(rig_stream_format_t formats,
                              char *buf, size_t buflen);

/* Convert rig_stream_type_t to string (e.g. RIG_STREAM_TYPE_AUDIO_RX -> "AUDIO_RX").
 * Returns "UNKNOWN" for invalid types. */
const char *stream_type_name(rig_stream_type_t type);

/* Parse type name string to rig_stream_type_t.
 * Returns -1 for unknown names. */
int stream_type_parse(const char *name, rig_stream_type_t *type);

/* Convert RIG_STREAM_FORMAT_* bitmask value to string (e.g. "PCM_S16").
 * Returns "UNKNOWN" for unrecognized formats. */
const char *stream_format_name(rig_stream_format_t format);

/* Parse format name string to RIG_STREAM_FORMAT_* bitmask value.
 * Returns 0 for unknown names. */
rig_stream_format_t stream_format_parse(const char *name);

/* Write comma-separated RIG_STREAM_CAP_* flag names (prefix stripped);
 * empty string when no flag is set. Returns characters written or -1. */
int stream_caps_flags_str(uint64_t caps_flags, char *buf, size_t buflen);

/* Parse one flag name to its RIG_STREAM_CAP_* bit; 0 for unknown names. */
uint64_t stream_caps_flag_parse(const char *name);

/* Format one capability entry as the canonical key=value line used by the
 * rigctld \stream_caps response and the caps dumps — the ONE textual
 * rendering of struct rig_stream_caps. include_native appends the
 * native_* keys (the served/both-views form); without it the line is the
 * backend-declaration form. Returns characters written or -1 if buf is
 * too small. */
int stream_caps_format_line(const struct rig_stream_caps *e,
                            int include_native, char *buf, size_t buflen);

/* Write comma-separated RIG_STREAM_CONV_* stage names (prefix stripped);
 * empty string when no conversion is active. Returns chars written or -1. */
int stream_conversions_str(int conv, char *buf, size_t buflen);

/* Parse a comma-separated stage-name list back to the bitmask; unknown
 * names are skipped (forward compatibility), empty text is CONV_NONE. */
int stream_conversions_parse(const char *text);


/* Stream type classification helpers */

static inline int stream_type_is_rx(rig_stream_type_t t)
{
    return (t == RIG_STREAM_TYPE_AUDIO_RX || t == RIG_STREAM_TYPE_IQ_RX);
}

static inline int stream_type_is_tx(rig_stream_type_t t)
{
    return (t == RIG_STREAM_TYPE_AUDIO_TX || t == RIG_STREAM_TYPE_IQ_TX);
}

static inline int stream_type_is_iq(rig_stream_type_t t)
{
    return (t == RIG_STREAM_TYPE_IQ_RX || t == RIG_STREAM_TYPE_IQ_TX);
}


#endif /* HAMLIB_STREAM_PROTO_H */
