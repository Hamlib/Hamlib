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

/* Streaming protocol wire format — pack/unpack, format/type name mapping. */
/* Shared by libhamlib (src/) and rigctld (tests/) for consistent serialization. */

#include "hamlib/config.h"

#include "stream_proto.h"
#include <string.h>


/* --- Format index mapping tables --- */

/* Format registry: wire id, name and display order in one place.
 * Canonical order also drives stream_format_bitmask_str() output. */
struct format_info
{
    rig_stream_format_t format;
    uint8_t             id;
    const char         *name;
};

static const struct format_info format_table[] =
{
    { RIG_STREAM_FORMAT_PCM_S8,   RIG_STREAM_FMT_ID_PCM_S8,   "PCM_S8" },
    { RIG_STREAM_FORMAT_PCM_U8,   RIG_STREAM_FMT_ID_PCM_U8,   "PCM_U8" },
    { RIG_STREAM_FORMAT_PCM_S16,  RIG_STREAM_FMT_ID_PCM_S16,  "PCM_S16" },
    { RIG_STREAM_FORMAT_PCM_F32,  RIG_STREAM_FMT_ID_PCM_F32,  "PCM_F32" },
    { RIG_STREAM_FORMAT_OPUS,     RIG_STREAM_FMT_ID_OPUS,     "OPUS" },
    { RIG_STREAM_FORMAT_IQ_CS8,   RIG_STREAM_FMT_ID_IQ_CS8,   "IQ_CS8" },
    { RIG_STREAM_FORMAT_IQ_CU8,   RIG_STREAM_FMT_ID_IQ_CU8,   "IQ_CU8" },
    { RIG_STREAM_FORMAT_IQ_CS16,  RIG_STREAM_FMT_ID_IQ_CS16,  "IQ_CS16" },
    { RIG_STREAM_FORMAT_IQ_CF32,  RIG_STREAM_FMT_ID_IQ_CF32,  "IQ_CF32" },
};

#define FORMAT_TABLE_SIZE (sizeof(format_table) / sizeof(format_table[0]))


uint8_t stream_format_to_id(rig_stream_format_t format)
{
    size_t i;

    for (i = 0; i < FORMAT_TABLE_SIZE; i++)
    {
        if (format_table[i].format == format)
        {
            return format_table[i].id;
        }
    }

    return RIG_STREAM_FMT_ID_INVALID;
}


rig_stream_format_t stream_id_to_format(uint8_t index)
{
    size_t i;

    for (i = 0; i < FORMAT_TABLE_SIZE; i++)
    {
        if (format_table[i].id == index)
        {
            return format_table[i].format;
        }
    }

    return 0;
}


/* --- Stream source ID derivation --- */

#define FNV1A_32_OFFSET_BASIS 2166136261u
#define FNV1A_32_PRIME        16777619u

static uint32_t fnv1a_32_str(uint32_t hash, const char *str)
{
    const unsigned char *p = (const unsigned char *)(str ? str : "");

    while (*p)
    {
        hash ^= *p++;
        hash *= FNV1A_32_PRIME;
    }

    return hash;
}


uint16_t stream_source_id_derive(const char *hostname, int listen_port,
                                 int model_id, const char *pathname)
{
    char num[16];
    uint32_t hash = FNV1A_32_OFFSET_BASIS;

    hash = fnv1a_32_str(hash, hostname);
    hash = fnv1a_32_str(hash, "|");
    snprintf(num, sizeof(num), "%d", listen_port);
    hash = fnv1a_32_str(hash, num);
    hash = fnv1a_32_str(hash, "|");
    snprintf(num, sizeof(num), "%d", model_id);
    hash = fnv1a_32_str(hash, num);
    hash = fnv1a_32_str(hash, "|");
    hash = fnv1a_32_str(hash, pathname);

    return (uint16_t)(0x1000u + (hash % 0xF000u));
}


/* --- Big-endian (network order) integer serialization --- */

static void put_be16(unsigned char *buf, uint16_t v)
{
    buf[0] = (v >> 8) & 0xFF;
    buf[1] = v & 0xFF;
}

static void put_be32(unsigned char *buf, uint32_t v)
{
    buf[0] = (v >> 24) & 0xFF;
    buf[1] = (v >> 16) & 0xFF;
    buf[2] = (v >> 8) & 0xFF;
    buf[3] = v & 0xFF;
}

static void put_be64(unsigned char *buf, uint64_t v)
{
    buf[0] = (v >> 56) & 0xFF;
    buf[1] = (v >> 48) & 0xFF;
    buf[2] = (v >> 40) & 0xFF;
    buf[3] = (v >> 32) & 0xFF;
    buf[4] = (v >> 24) & 0xFF;
    buf[5] = (v >> 16) & 0xFF;
    buf[6] = (v >> 8) & 0xFF;
    buf[7] = v & 0xFF;
}

static uint16_t get_be16(const unsigned char *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

static uint32_t get_be32(const unsigned char *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
           | ((uint32_t)buf[2] << 8) | buf[3];
}

static uint64_t get_be64(const unsigned char *buf)
{
    return ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48)
           | ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32)
           | ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16)
           | ((uint64_t)buf[6] << 8)  | buf[7];
}


/* --- Packet header serialization --- */

void stream_control_header_init(struct rig_stream_packet_header *hdr,
                                uint8_t type, uint16_t stream_id,
                                uint32_t subscribe_token, uint16_t control)
{
    memset(hdr, 0, sizeof(*hdr));
    hdr->version = RIG_STREAM_PROTOCOL_VERSION;
    hdr->type = type;
    hdr->stream_id = stream_id;
    hdr->subscribe_token = subscribe_token;
    hdr->control = control;
    hdr->payload_len = 0;
}


void stream_packet_header_pack(const struct rig_stream_packet_header *hdr,
                               unsigned char *buf)
{
    /* Byte 0: version */
    buf[0] = hdr->version;

    /* Byte 1: type */
    buf[1] = hdr->type;

    /* Bytes 2-3: stream_id (big-endian) */
    put_be16(buf + 2, hdr->stream_id);

    /* Bytes 4-7: subscribe_token (big-endian) */
    put_be32(buf + 4, hdr->subscribe_token);

    /* Bytes 8-11: seq (big-endian) */
    put_be32(buf + 8, hdr->seq);

    /* Bytes 12-19: timestamp (big-endian) */
    put_be64(buf + 12, hdr->timestamp);

    /* Bytes 20-23: sample_rate (big-endian) */
    put_be32(buf + 20, hdr->sample_rate);

    /* Byte 24: format */
    buf[24] = hdr->format;

    /* Byte 25: channels */
    buf[25] = hdr->channels;

    /* Bytes 26-27: source_id (big-endian) */
    put_be16(buf + 26, hdr->source_id);

    /* Bytes 28-29: control (big-endian) */
    put_be16(buf + 28, hdr->control);

    /* Bytes 30-31: payload_len (big-endian) */
    put_be16(buf + 30, hdr->payload_len);
}


int stream_packet_header_unpack(const unsigned char *buf, size_t len,
                                struct rig_stream_packet_header *hdr)
{
    if (len < RIG_STREAM_HEADER_SIZE)
    {
        return -1;
    }

    hdr->version = buf[0];

    if (hdr->version != RIG_STREAM_PROTOCOL_VERSION)
    {
        return -1;
    }

    hdr->type = buf[1];

    hdr->stream_id = get_be16(buf + 2);

    hdr->subscribe_token = get_be32(buf + 4);

    hdr->seq = get_be32(buf + 8);

    hdr->timestamp = get_be64(buf + 12);

    hdr->sample_rate = get_be32(buf + 20);

    hdr->format = buf[24];
    hdr->channels = buf[25];

    hdr->source_id = get_be16(buf + 26);

    hdr->control = get_be16(buf + 28);

    hdr->payload_len = get_be16(buf + 30);

    /* Reject unknown stream type or format index so downstream code never
     * uses a wire value as an out-of-range table lookup. */
    if (hdr->type >= RIG_STREAM_TYPE_COUNT
            || hdr->format > RIG_STREAM_FMT_ID_IQ_CF32)
    {
        return -1;
    }

    return 0;
}


/* --- Metadata wire format serialization --- */

/* Emit an IEEE-754 double as 8 big-endian bytes. Reinterpret the double's
 * bits as a uint64 (memcpy avoids aliasing UB), then write network order. */
static void put_be_double(double value, unsigned char *buf)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    put_be64(buf, bits);
}

/* Read 8 big-endian bytes back into an IEEE-754 double. */
static double get_be_double(const unsigned char *buf)
{
    uint64_t bits = get_be64(buf);
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void stream_metadata_pack(const struct rig_stream_metadata *meta,
                          unsigned char *buf)
{
    /* Bytes 0-3: field_mask (big-endian) */
    put_be32(buf, meta->field_mask);

    /* Byte 4: vfo_id */
    buf[4] = (uint8_t)(meta->vfo_id & 0xFF);

    /* Byte 5: ptt */
    buf[5] = (uint8_t)(meta->ptt & 0xFF);

    /* Bytes 6-13: center_freq (big-endian double, I/Q window center) */
    put_be_double(meta->center_freq, buf + 6);

    /* Bytes 14-21: vfo_freq (big-endian double, primary demod) */
    put_be_double(meta->vfo_freq, buf + 14);
}


int stream_metadata_unpack(const unsigned char *buf, size_t len,
                           struct rig_stream_metadata *meta)
{
    if (len < RIG_STREAM_METADATA_WIRE_SIZE)
    {
        return -1;
    }

    /* Zero first so the timestamp and the ABI-headroom _reserved tail read as
     * zero (rig.h contract), matching stream_write_status_unpack(). */
    memset(meta, 0, sizeof(*meta));

    meta->field_mask = get_be32(buf);

    meta->vfo_id = buf[4];
    meta->ptt = buf[5];

    meta->center_freq = get_be_double(buf + 6);
    meta->vfo_freq = get_be_double(buf + 14);

    return 0;
}


/* --- UDP socket buffer sizing --- */

size_t stream_transport_buffer_bytes(int sample_rate, int frame_bytes,
                                     unsigned int duration_ms, size_t explicit_bytes)
{
    size_t bytes;

    if (explicit_bytes > 0)
    {
        bytes = explicit_bytes;
    }
    else if (sample_rate > 0 && frame_bytes > 0 && duration_ms > 0)
    {
        bytes = (size_t)sample_rate * (size_t)frame_bytes
                * (size_t)duration_ms / 1000;
    }
    else
    {
        bytes = RIG_STREAM_TRANSPORT_BUFFER_MIN;
    }

    if (bytes < RIG_STREAM_TRANSPORT_BUFFER_MIN)
    {
        bytes = RIG_STREAM_TRANSPORT_BUFFER_MIN;
    }

    if (bytes > RIG_STREAM_TRANSPORT_BUFFER_MAX)
    {
        bytes = RIG_STREAM_TRANSPORT_BUFFER_MAX;
    }

    return bytes;
}


size_t stream_transport_buffer_effective(unsigned int cfg_ms,
        unsigned int cfg_bytes,
        unsigned int tok_ms, unsigned int tok_bytes,
        int sample_rate, int frame_bytes)
{
    /* Explicit bytes win over ms; per-stream wins over the rig token. */
    size_t explicit_bytes = cfg_bytes ? cfg_bytes : tok_bytes;
    unsigned int dur = cfg_ms ? cfg_ms
                       : (tok_ms ? tok_ms : RIG_STREAM_TRANSPORT_BUFFER_DURATION_MS);

    return stream_transport_buffer_bytes(sample_rate, frame_bytes, dur,
                                         explicit_bytes);
}


int stream_max_payload_from_mtu(unsigned int mtu, int frame_bytes)
{
    unsigned int mtu_eff = (mtu == 0) ? RIG_STREAM_DEFAULT_MTU : mtu;

    if (mtu_eff < RIG_STREAM_MIN_MTU)
    {
        mtu_eff = RIG_STREAM_MIN_MTU;
    }

    if (mtu_eff > RIG_STREAM_MAX_DATAGRAM)
    {
        mtu_eff = RIG_STREAM_MAX_DATAGRAM;
    }

    int payload = (int)mtu_eff - RIG_STREAM_IPV6_HEADER
                  - RIG_STREAM_UDP_HEADER - RIG_STREAM_HEADER_SIZE;

    if (frame_bytes > 0)
    {
        payload = (payload / frame_bytes) * frame_bytes;
    }

    return payload;
}


int stream_apply_transport_buffer(int sock, int which, size_t bytes)
{
    int val = (int)bytes;
    const char *name = (which == SO_RCVBUF) ? "SO_RCVBUF" : "SO_SNDBUF";

    if (setsockopt(sock, SOL_SOCKET, which,
                   (const char *)&val, sizeof(val)) != 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: setsockopt(%s, %d) failed\n",
                  __func__, name, val);
        return -1;
    }

    /* The kernel may clamp (to its own max) or double the request; log the
     * value actually granted so integrators can spot an OS ceiling. */
    int actual = 0;
    socklen_t len = sizeof(actual);

    if (getsockopt(sock, SOL_SOCKET, which, (char *)&actual, &len) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: %s requested %d bytes, kernel granted %d\n",
                  __func__, name, val, actual);
    }

    return 0;
}


void stream_time_block_pack(const struct rig_stream_time_anchor *t,
                            unsigned char *buf)
{
    /* Bytes 0-7: seconds, int64 two's complement (big-endian) */
    put_be64(buf, (uint64_t)t->seconds);

    /* Bytes 8-15: picoseconds (big-endian) */
    put_be64(buf + 8, t->picoseconds);

    /* Bytes 16-18: source, flags, accuracy */
    buf[16] = t->source;
    buf[17] = t->flags;
    buf[18] = t->accuracy;

    /* Byte 19: reserved (must be zero) */
    buf[19] = 0;
}


int stream_time_block_unpack(const unsigned char *buf, size_t len,
                             struct rig_stream_time_anchor *t)
{
    if (len < RIG_STREAM_TIME_BLOCK_SIZE)
    {
        return -1;
    }

    /* Zero first so the ABI-headroom _reserved tail reads as zero (rig.h
     * contract); the anchor surfaces to apps via rig_stream_get_time_anchor. */
    memset(t, 0, sizeof(*t));

    t->seconds = (int64_t)get_be64(buf);

    t->picoseconds = get_be64(buf + 8);

    t->source = buf[16];
    t->flags = buf[17];
    t->accuracy = buf[18];

    /* Byte 19 reserved — ignored on unpack */

    /* The sample index travels in the packet header, not the block */
    t->sample_index = 0;

    return 0;
}


void stream_write_status_pack(const struct rig_stream_write_status *st,
                              unsigned char *buf)
{
    /* Bytes 0-1: event (big-endian) */
    put_be16(buf, st->event);

    /* Bytes 2-3: flags; force the TIME_VALID bit to reflect st->time_valid */
    uint16_t flags = (uint16_t)(st->flags & ~RIG_STREAM_WRITE_STATUS_TIME_VALID);

    if (st->time_valid)
    {
        flags |= RIG_STREAM_WRITE_STATUS_TIME_VALID;
    }

    put_be16(buf + 2, flags);

    /* Bytes 4-7: dropped_samples (big-endian) */
    put_be32(buf + 4, st->dropped_samples);

    /* Bytes 8-15: lateness in samples, int64 two's complement (big-endian) */
    put_be64(buf + 8, (uint64_t)st->lateness);

    /* Bytes 16-23: seconds, int64 two's complement (big-endian) */
    put_be64(buf + 16, (uint64_t)st->seconds);

    /* Bytes 24-31: picoseconds (big-endian) */
    put_be64(buf + 24, st->picoseconds);

    /* Bytes 32-34: time source/flags/accuracy; byte 35 reserved */
    buf[32] = st->time_source;
    buf[33] = st->time_flags;
    buf[34] = st->time_accuracy;
    buf[35] = 0;
}


int stream_write_status_unpack(const unsigned char *buf, size_t len,
                               struct rig_stream_write_status *st)
{
    if (len < RIG_STREAM_WRITE_STATUS_WIRE_SIZE)
    {
        return -1;
    }

    /* Zero first so sample_index (which travels in the packet header, not the
     * block) and the ABI-headroom _reserved tail read as zero (rig.h
     * contract), matching stream_metadata_unpack(). */
    memset(st, 0, sizeof(*st));

    st->event = get_be16(buf);
    st->flags = get_be16(buf + 2);
    st->time_valid = (st->flags & RIG_STREAM_WRITE_STATUS_TIME_VALID) ? 1 : 0;

    st->dropped_samples = get_be32(buf + 4);

    st->lateness = (int64_t)get_be64(buf + 8);

    st->seconds = (int64_t)get_be64(buf + 16);

    st->picoseconds = get_be64(buf + 24);

    st->time_source = buf[32];
    st->time_flags = buf[33];
    st->time_accuracy = buf[34];
    /* byte 35 reserved */

    return 0;
}


/* --- Type name mapping --- */

struct type_name_entry
{
    rig_stream_type_t type;
    const char *name;
};

static const struct type_name_entry type_names[] =
{
    { RIG_STREAM_TYPE_AUDIO_RX, "AUDIO_RX" },
    { RIG_STREAM_TYPE_AUDIO_TX, "AUDIO_TX" },
    { RIG_STREAM_TYPE_IQ_RX,    "IQ_RX" },
    { RIG_STREAM_TYPE_IQ_TX,    "IQ_TX" },
};

#define TYPE_NAMES_SIZE (sizeof(type_names) / sizeof(type_names[0]))


const char *stream_type_name(rig_stream_type_t type)
{
    size_t i;

    for (i = 0; i < TYPE_NAMES_SIZE; i++)
    {
        if (type_names[i].type == type)
        {
            return type_names[i].name;
        }
    }

    return "UNKNOWN";
}


int stream_type_parse(const char *name, rig_stream_type_t *type)
{
    size_t i;

    for (i = 0; i < TYPE_NAMES_SIZE; i++)
    {
        if (strcmp(type_names[i].name, name) == 0)
        {
            *type = type_names[i].type;
            return 0;
        }
    }

    return -1;
}


/* --- Format name mapping --- */

const char *stream_format_name(rig_stream_format_t format)
{
    size_t i;

    for (i = 0; i < FORMAT_TABLE_SIZE; i++)
    {
        if (format_table[i].format == format)
        {
            return format_table[i].name;
        }
    }

    return "UNKNOWN";
}


rig_stream_format_t stream_format_parse(const char *name)
{
    size_t i;

    for (i = 0; i < FORMAT_TABLE_SIZE; i++)
    {
        if (strcmp(format_table[i].name, name) == 0)
        {
            return format_table[i].format;
        }
    }

    return 0;
}


/* --- Format bitmask to string --- */

int stream_format_bitmask_str(rig_stream_format_t formats,
                              char *buf, size_t buflen)
{
    size_t pos = 0;
    int first = 1;
    size_t i;

    if (buflen == 0)
    {
        return -1;
    }

    buf[0] = '\0';

    /* Every list-valued key of the protocol spells the empty list NONE
     * (see stream_list_none()) — never an empty value. */
    if (formats == 0)
    {
        int n = snprintf(buf, buflen, "%s", STREAM_LIST_NONE);

        return (n < 0 || (size_t)n >= buflen) ? -1 : n;
    }

    for (i = 0; i < FORMAT_TABLE_SIZE; i++)
    {
        if (formats & format_table[i].format)
        {
            const char *name = format_table[i].name;
            size_t nlen = strlen(name);
            size_t need = (first ? 0 : 1) + nlen;  /* comma + name */

            if (pos + need >= buflen)
            {
                return -1;
            }

            if (!first)
            {
                buf[pos++] = ',';
            }

            memcpy(buf + pos, name, nlen);
            pos += nlen;
            first = 0;
        }
    }

    buf[pos] = '\0';
    return (int)pos;
}


/* Capability-flag names shared by the wire emitter/parser and the caps
 * dumps: RIG_STREAM_CAP_* with the prefix stripped. */
static const struct
{
    uint64_t flag;
    const char *name;
} caps_flag_table[] =
{
    { RIG_STREAM_CAP_TIMED_TX_COARSE, "TIMED_TX_COARSE" },
    { RIG_STREAM_CAP_TIMED_TX_SAMPLE, "TIMED_TX_SAMPLE" },
    { RIG_STREAM_CAP_BURST_PTT,       "BURST_PTT" },
    { RIG_STREAM_CAP_HW_TIME,         "HW_TIME" },
};

int stream_caps_flags_str(uint64_t caps_flags, char *buf, size_t buflen)
{
    size_t pos = 0;
    size_t i;

    if (buflen == 0)
    {
        return -1;
    }

    buf[0] = '\0';

    if (caps_flags == 0)
    {
        int n = snprintf(buf, buflen, "%s", STREAM_LIST_NONE);

        return (n < 0 || (size_t)n >= buflen) ? -1 : n;
    }

    for (i = 0; i < sizeof(caps_flag_table) / sizeof(caps_flag_table[0]); i++)
    {
        if (!(caps_flags & caps_flag_table[i].flag))
        {
            continue;
        }

        int n = snprintf(buf + pos, buflen - pos, "%s%s",
                         pos > 0 ? "," : "", caps_flag_table[i].name);

        if (n < 0 || (size_t)n >= buflen - pos)
        {
            return -1;
        }

        pos += (size_t)n;
    }

    return (int)pos;
}

uint64_t stream_caps_flag_parse(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof(caps_flag_table) / sizeof(caps_flag_table[0]); i++)
    {
        if (strcmp(name, caps_flag_table[i].name) == 0)
        {
            return caps_flag_table[i].flag;
        }
    }

    return 0;
}

/* Append an ascending 0-terminated int list as comma-separated decimals.
 * Returns 0, or -1 on overflow. */
/* One token spells the empty list in EVERY list-valued key of the streaming
 * protocol — caps name lists and integer lists, the conversions: stage list,
 * and require_native. A key whose value is missing entirely, or empty as an
 * older peer wrote it, means the same thing. */
const char *const STREAM_LIST_NONE = "NONE";

int stream_list_none(const char *value, size_t len)
{
    return len == 4 && strncmp(value, STREAM_LIST_NONE, 4) == 0;
}


static int append_int_list(char *buf, size_t buflen, size_t *pos,
                           const int *list, int max_entries)
{
    if (max_entries <= 0 || list[0] == 0)
    {
        int n = snprintf(buf + *pos, buflen - *pos, "%s", STREAM_LIST_NONE);

        if (n < 0 || (size_t)n >= buflen - *pos)
        {
            return -1;
        }

        *pos += (size_t)n;
        return 0;
    }

    for (int i = 0; i < max_entries && list[i] != 0; i++)
    {
        int n = snprintf(buf + *pos, buflen - *pos, "%s%d",
                         i > 0 ? "," : "", list[i]);

        if (n < 0 || (size_t)n >= buflen - *pos)
        {
            return -1;
        }

        *pos += (size_t)n;
    }

    return 0;
}

int stream_caps_format_line(const struct rig_stream_caps *e,
                            int include_native, char *buf, size_t buflen)
{
    char fmtbuf[256];
    char flagbuf[128];
    size_t pos = 0;
    int n;

    if (!e || !buf || buflen == 0)
    {
        return -1;
    }

    if (stream_format_bitmask_str(e->formats, fmtbuf, sizeof(fmtbuf)) < 0
            || stream_caps_flags_str(e->caps_flags, flagbuf,
                                     sizeof(flagbuf)) < 0)
    {
        return -1;
    }

    n = snprintf(buf, buflen, "type=%s formats=%s rates=",
                 stream_type_name(e->type), fmtbuf);

    if (n < 0 || (size_t)n >= buflen)
    {
        return -1;
    }

    pos = (size_t)n;

    if (append_int_list(buf, buflen, &pos, e->sample_rates,
                        HAMLIB_MAX_STREAM_RATES) != 0)
    {
        return -1;
    }

    n = snprintf(buf + pos, buflen - pos, " channels=");

    if (n < 0 || (size_t)n >= buflen - pos)
    {
        return -1;
    }

    pos += (size_t)n;

    if (append_int_list(buf, buflen, &pos, e->channels,
                        HAMLIB_MAX_STREAM_CHANNEL_COUNTS) != 0)
    {
        return -1;
    }

    n = snprintf(buf + pos, buflen - pos,
                 " max_streams=%d flags=%s tx_horizon_ms=%d",
                 e->max_streams, flagbuf, e->tx_schedule_horizon_ms);

    if (n < 0 || (size_t)n >= buflen - pos)
    {
        return -1;
    }

    pos += (size_t)n;

    if (!include_native)
    {
        return (int)pos;
    }

    if (stream_format_bitmask_str(e->native_formats, fmtbuf,
                                  sizeof(fmtbuf)) < 0)
    {
        return -1;
    }

    n = snprintf(buf + pos, buflen - pos, " native_formats=%s native_rates=",
                 fmtbuf);

    if (n < 0 || (size_t)n >= buflen - pos)
    {
        return -1;
    }

    pos += (size_t)n;

    if (append_int_list(buf, buflen, &pos, e->native_sample_rates,
                        HAMLIB_MAX_STREAM_RATES) != 0)
    {
        return -1;
    }

    n = snprintf(buf + pos, buflen - pos, " native_channels=");

    if (n < 0 || (size_t)n >= buflen - pos)
    {
        return -1;
    }

    pos += (size_t)n;

    if (append_int_list(buf, buflen, &pos, e->native_channels,
                        HAMLIB_MAX_STREAM_CHANNEL_COUNTS) != 0)
    {
        return -1;
    }

    return (int)pos;
}


/* Conversion-stage names shared by the rigctld TCP responses and the
 * netrigctl client: RIG_STREAM_CONV_* with the prefix stripped. */
static const struct
{
    int flag;
    const char *name;
} conv_table[] =
{
    { RIG_STREAM_CONV_FORMAT,   "FORMAT" },
    { RIG_STREAM_CONV_RATE,     "RATE" },
    { RIG_STREAM_CONV_CHANNELS, "CHANNELS" },
};

int stream_conversions_str(int conv, char *buf, size_t buflen)
{
    size_t pos = 0;
    size_t i;

    if (buflen == 0)
    {
        return -1;
    }

    buf[0] = '\0';

    /* A native stream is spelled NONE, not an empty value: the same token
     * a client writes in require_native, and one an eye or a log filter
     * cannot mistake for a missing field. */
    if (conv == RIG_STREAM_CONV_NONE)
    {
        int n = snprintf(buf, buflen, "%s", STREAM_LIST_NONE);

        return (n < 0 || (size_t)n >= buflen) ? -1 : n;
    }

    for (i = 0; i < sizeof(conv_table) / sizeof(conv_table[0]); i++)
    {
        if (!(conv & conv_table[i].flag))
        {
            continue;
        }

        int n = snprintf(buf + pos, buflen - pos, "%s%s",
                         pos > 0 ? "," : "", conv_table[i].name);

        if (n < 0 || (size_t)n >= buflen - pos)
        {
            return -1;
        }

        pos += (size_t)n;
    }

    return (int)pos;
}

int stream_conversions_parse(const char *text)
{
    int conv = RIG_STREAM_CONV_NONE;
    const char *tok = text;

    if (!text)
    {
        return conv;
    }

    while (*tok != '\0')
    {
        while (*tok == ' ' || *tok == ',')
        {
            tok++;
        }

        size_t len = strcspn(tok, ", \r\n");

        if (len == 0)
        {
            break;
        }

        size_t i;

        /* NONE contributes no stage; an empty value means the same thing
         * (older servers wrote one). Other unknown names are skipped so
         * future servers may add stages. */
        if (stream_list_none(tok, len))
        {
            tok += len;
            continue;
        }

        for (i = 0; i < sizeof(conv_table) / sizeof(conv_table[0]); i++)
        {
            if (len == strlen(conv_table[i].name)
                    && strncmp(tok, conv_table[i].name, len) == 0)
            {
                conv |= conv_table[i].flag;
                break;
            }
        }

        tok += len;
    }

    return conv;
}


/* --- require_native stage list --- */

/* The wire spells a rig_stream_config.require_native mask with the same stage
 * names as the conversions: line, plus ALL (every stage, present and future)
 * and NONE (the empty list). */
int stream_native_req_str(uint32_t mask, char *buf, size_t buflen)
{
    static const uint32_t known = (uint32_t)(RIG_STREAM_CONV_FORMAT
                                  | RIG_STREAM_CONV_RATE
                                  | RIG_STREAM_CONV_CHANNELS);

    if (buflen == 0)
    {
        return -1;
    }

    /* Only RIG_STREAM_CONV_ALL sets bits outside the named stages, and it
     * must survive the round trip as ALL: an older peer that rendered it as
     * the stages it happens to know would quietly narrow the demand. */
    if (mask & ~known)
    {
        int n = snprintf(buf, buflen, "ALL");

        return (n < 0 || (size_t)n >= buflen) ? -1 : n;
    }

    /* RIG_STREAM_CONV_NONE renders as NONE through the shared formatter. */
    return stream_conversions_str((int)mask, buf, buflen);
}

/* Strict counterpart of stream_conversions_parse(): an unknown stage name is
 * an error rather than a skip. A requirement the peer cannot honour must
 * never be silently dropped — the client would hold a guarantee that nothing
 * enforces. Returns 0 with *mask set, or -1 (nothing written). */
int stream_native_req_parse(const char *text, uint32_t *mask)
{
    uint32_t out = RIG_STREAM_CONV_NONE;
    const char *tok = text;

    if (!text || !mask)
    {
        return -1;
    }

    while (*tok != '\0')
    {
        while (*tok == ' ' || *tok == ',')
        {
            tok++;
        }

        size_t len = strcspn(tok, ", \r\n");

        if (len == 0)
        {
            break;
        }

        size_t i;
        int found = 0;

        if (len == 3 && strncmp(tok, "ALL", len) == 0)
        {
            out |= RIG_STREAM_CONV_ALL;
            found = 1;
        }
        else if (stream_list_none(tok, len))
        {
            found = 1;                /* RIG_STREAM_CONV_NONE: no stage */
        }

        for (i = 0; !found && i < sizeof(conv_table) / sizeof(conv_table[0]);
                i++)
        {
            if (len == strlen(conv_table[i].name)
                    && strncmp(tok, conv_table[i].name, len) == 0)
            {
                out |= (uint32_t)conv_table[i].flag;
                found = 1;
            }
        }

        if (!found)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unknown conversion stage '%.*s'\n",
                      __func__, (int)len, tok);
            return -1;
        }

        tok += len;
    }

    *mask = out;
    return 0;
}
