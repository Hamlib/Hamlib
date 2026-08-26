/*
 *  Hamlib Icom network backend - packet parser/builder
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

/* Icom network protocol Layer 1: pure packet parser/builder (no I/O). */
/* Builds and parses the UDP packets of the Icom LAN protocol from byte buffers. */

#include "hamlib/config.h"

#include <string.h>

#include "hamlib/rig.h"
#include "network_proto.h"

/* Substitution table for the username/password passcode cipher.
 * Indices 0-31 and 127+ are zero; 32-126 carry the cipher values. */
static const uint8_t icom_network_passcode_table[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x47, 0x5d, 0x4c, 0x42, 0x66, 0x20, 0x23, 0x46,
    0x4e, 0x57, 0x45, 0x3d, 0x67, 0x76, 0x60, 0x41,
    0x62, 0x39, 0x59, 0x2d, 0x68, 0x7e, 0x7c, 0x65,
    0x7d, 0x49, 0x29, 0x72, 0x73, 0x78, 0x21, 0x6e,
    0x5a, 0x5e, 0x4a, 0x3e, 0x71, 0x2c, 0x2a, 0x54,
    0x3c, 0x3a, 0x63, 0x4f, 0x43, 0x75, 0x27, 0x79,
    0x5b, 0x35, 0x70, 0x48, 0x6b, 0x56, 0x6f, 0x34,
    0x32, 0x6c, 0x30, 0x61, 0x6d, 0x7b, 0x2f, 0x4b,
    0x64, 0x38, 0x2b, 0x2e, 0x50, 0x40, 0x3f, 0x55,
    0x33, 0x37, 0x25, 0x77, 0x24, 0x26, 0x74, 0x6a,
    0x28, 0x53, 0x4d, 0x69, 0x22, 0x5c, 0x44, 0x31,
    0x36, 0x58, 0x3b, 0x7a, 0x51, 0x5f, 0x52, 0,
    /* 127-255: zero */
};

/* ------------------------------------------------------------------ */
/* byte helpers                                                        */
/* ------------------------------------------------------------------ */

void icom_network_put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}

void icom_network_put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

void icom_network_put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xff);
    p[1] = (uint8_t)(v & 0xff);
}

void icom_network_put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xff);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >> 8) & 0xff);
    p[3] = (uint8_t)(v & 0xff);
}

uint16_t icom_network_get_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

uint32_t icom_network_get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t icom_network_get_be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t icom_network_get_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
           | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

uint32_t icom_network_make_id(const uint8_t ipv4[4], uint16_t port)
{
    return (((uint32_t)ipv4[2] & 0xff) << 24)
           | (((uint32_t)ipv4[3] & 0xff) << 16)
           | (uint32_t)port;
}

/* ------------------------------------------------------------------ */
/* header                                                              */
/* ------------------------------------------------------------------ */

int icom_network_packet_build_header(uint8_t *buf, size_t bufsize,
                                     uint32_t length,
                                     uint16_t type, uint16_t sequence,
                                     uint32_t local_id, uint32_t remote_id)
{
    if (buf == NULL || bufsize < ICOM_NETWORK_HEADER_LEN)
    {
        return -RIG_EINVAL;
    }

    /* length/type/sequence are little-endian; the connection ids local_id/remote_id are
     * stored big-endian (MSB first), as the radio expects. */
    icom_network_put_le32(buf + ICOM_NETWORK_OFF_LENGTH, length);
    icom_network_put_le16(buf + ICOM_NETWORK_OFF_TYPE, type);
    icom_network_put_le16(buf + ICOM_NETWORK_OFF_SEQUENCE, sequence);
    icom_network_put_be32(buf + ICOM_NETWORK_OFF_LOCAL_ID, local_id);
    icom_network_put_be32(buf + ICOM_NETWORK_OFF_REMOTE_ID, remote_id);

    return ICOM_NETWORK_HEADER_LEN;
}

int icom_network_packet_parse_header(const uint8_t *buf, size_t length,
                                     struct icom_network_packet_header *hdr)
{
    if (buf == NULL || hdr == NULL || length < ICOM_NETWORK_HEADER_LEN)
    {
        return -RIG_EINVAL;
    }

    hdr->length    = icom_network_get_le32(buf + ICOM_NETWORK_OFF_LENGTH);
    hdr->type   = icom_network_get_le16(buf + ICOM_NETWORK_OFF_TYPE);
    hdr->sequence    = icom_network_get_le16(buf + ICOM_NETWORK_OFF_SEQUENCE);
    hdr->local_id = icom_network_get_be32(buf + ICOM_NETWORK_OFF_LOCAL_ID);
    hdr->remote_id = icom_network_get_be32(buf + ICOM_NETWORK_OFF_REMOTE_ID);

    return RIG_OK;
}

enum icom_network_packet_kind icom_network_packet_classify(const uint8_t *buf,
        size_t length)
{
    uint32_t packet_length;
    uint16_t type;

    if (buf == NULL || length < ICOM_NETWORK_HEADER_LEN)
    {
        return ICOM_NETWORK_PACKET_KIND_UNKNOWN;
    }

    packet_length = icom_network_get_le32(buf + ICOM_NETWORK_OFF_LENGTH);
    type = icom_network_get_le16(buf + ICOM_NETWORK_OFF_TYPE);

    if (type == ICOM_NETWORK_PACKET_TYPE_PING)
    {
        return ICOM_NETWORK_PACKET_KIND_PING;
    }

    if (type == ICOM_NETWORK_PACKET_TYPE_RETRANSMIT)
    {
        return ICOM_NETWORK_PACKET_KIND_RETRANSMIT;
    }

    /* packet_length is the total packet length; a well-formed datagram has packet_length == length.
     * The size heuristics below inspect payload bytes at offsets derived from
     * packet_length, so a datagram claiming more than was received (packet_length > length) would
     * read past the buffer. Reject it before any packet_length-gated payload access. */
    if (packet_length > length)
    {
        return ICOM_NETWORK_PACKET_KIND_UNKNOWN;
    }

    /* Fixed-size management/control packets (type 0x00). */
    switch (packet_length)
    {
    case 0x10: return ICOM_NETWORK_PACKET_KIND_CONTROL;

    case 0x14: return ICOM_NETWORK_PACKET_KIND_WATCHDOG;

    case 0x40: return ICOM_NETWORK_PACKET_KIND_TOKEN;

    case 0x50: return ICOM_NETWORK_PACKET_KIND_STATUS;

    case 0x60: return ICOM_NETWORK_PACKET_KIND_LOGIN_RESPONSE;

    case 0x80: return ICOM_NETWORK_PACKET_KIND_LOGIN;

    case 0x90: return ICOM_NETWORK_PACKET_KIND_CONNINFO;

    default: break;
    }

    /* Open/close (0x16) carries the constant 0x01c0 marker at offset 0x10. */
    if (packet_length == 0x16 && buf[0x10] == 0x01 && buf[0x11] == 0xc0)
    {
        return ICOM_NETWORK_PACKET_KIND_OPENCLOSE;
    }

    /* Capabilities: 0x42 + N*0x66. */
    if (packet_length >= 0x42 + 0x66 && (packet_length - 0x42) % 0x66 == 0)
    {
        return ICOM_NETWORK_PACKET_KIND_CAPABILITIES;
    }

    /* Data packets: distinguished authoritatively by socket at Layer 2.
     * Heuristic here: a CI-V reply byte (0xc0/0xc1) marks CI-V; else audio. */
    if (packet_length >= 0x15)
    {
        if (buf[0x10] == 0xc0 || buf[0x10] == 0xc1)
        {
            return ICOM_NETWORK_PACKET_KIND_CIV;
        }

        if (packet_length >= 0x18)
        {
            return ICOM_NETWORK_PACKET_KIND_AUDIO;
        }
    }

    return ICOM_NETWORK_PACKET_KIND_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* small control packets                                               */
/* ------------------------------------------------------------------ */

int icom_network_packet_build_control(uint8_t *buf, size_t bufsize,
                                      uint16_t control, uint16_t sequence,
                                      uint32_t local_id, uint32_t remote_id)
{
    if (bufsize < 0x10)
    {
        return -RIG_EINVAL;
    }

    return icom_network_packet_build_header(buf, bufsize, 0x10, control, sequence,
                                            local_id, remote_id) == 0x10
           ? 0x10 : -RIG_EINVAL;
}

int icom_network_packet_build_ping(uint8_t *buf, size_t bufsize, uint8_t reply,
                                   uint32_t time_ms, uint16_t sequence,
                                   uint32_t local_id, uint32_t remote_id)
{
    if (bufsize < ICOM_NETWORK_CIV_LEN)
    {
        return -RIG_EINVAL;
    }

    icom_network_packet_build_header(buf, bufsize, 0x15,
                                     ICOM_NETWORK_PACKET_TYPE_PING, sequence,
                                     local_id, remote_id);
    buf[0x10] = reply;
    icom_network_put_le32(buf + 0x11, time_ms);

    return 0x15;
}

int icom_network_packet_parse_ping(const uint8_t *buf, size_t length,
                                   uint8_t *reply, uint32_t *time_ms)
{
    if (buf == NULL || length < ICOM_NETWORK_CIV_LEN)
    {
        return -RIG_EINVAL;
    }

    if (reply) { *reply = buf[0x10]; }

    if (time_ms) { *time_ms = icom_network_get_le32(buf + 0x11); }

    return RIG_OK;
}

int icom_network_packet_build_openclose(uint8_t *buf, size_t bufsize,
                                        uint8_t magic, uint16_t send_sequence,
                                        uint16_t sequence, uint32_t local_id,
                                        uint32_t remote_id)
{
    if (bufsize < 0x16)
    {
        return -RIG_EINVAL;
    }

    icom_network_packet_build_header(buf, bufsize, 0x16,
                                     ICOM_NETWORK_PACKET_TYPE_DATA, sequence,
                                     local_id, remote_id);
    buf[0x10] = 0x01;        /* constant 0x01c0 marker */
    buf[0x11] = 0xc0;
    buf[0x12] = 0x00;
    icom_network_put_be16(buf + 0x13, send_sequence);
    buf[ICOM_NETWORK_REQ_OFF_REQUEST_TYPE] = magic;       /* 0x04 start, 0x00 stop */

    return 0x16;
}

/* ------------------------------------------------------------------ */
/* retransmit                                                          */
/* ------------------------------------------------------------------ */

int icom_network_packet_build_retransmit(uint8_t *buf, size_t bufsize,
        const uint16_t *sequences, size_t sequence_count,
        uint32_t local_id, uint32_t remote_id)
{
    size_t i;

    if (buf == NULL || sequences == NULL || sequence_count == 0)
    {
        return -RIG_EINVAL;
    }

    if (sequence_count == 1)
    {
        if (bufsize < 0x10)
        {
            return -RIG_EINVAL;
        }

        /* Single missing sequence travels in the header sequence field. */
        return icom_network_packet_build_header(buf, bufsize, 0x10,
                                                ICOM_NETWORK_PACKET_TYPE_RETRANSMIT,
                                                sequences[0], local_id, remote_id) == 0x10
               ? 0x10 : -RIG_EINVAL;
    }

    if (bufsize < 0x10 + sequence_count * 4)
    {
        return -RIG_EINVAL;
    }

    icom_network_packet_build_header(buf, bufsize,
                                     (uint32_t)(0x10 + sequence_count * 4),
                                     ICOM_NETWORK_PACKET_TYPE_RETRANSMIT, 0,
                                     local_id, remote_id);

    for (i = 0; i < sequence_count; i++)
    {
        /* Each entry: the 16-bit sequence in little-endian, repeated twice. */
        icom_network_put_le16(buf + 0x10 + i * 4, sequences[i]);
        icom_network_put_le16(buf + 0x10 + i * 4 + 2, sequences[i]);
    }

    return (int)(0x10 + sequence_count * 4);
}

/* ------------------------------------------------------------------ */
/* CI-V data packets                                                   */
/* ------------------------------------------------------------------ */

int icom_network_packet_build_civ(uint8_t *buf, size_t bufsize,
                                  const uint8_t *civ_frame, size_t civ_length,
                                  uint8_t reply, uint16_t send_sequence, uint16_t sequence,
                                  uint32_t local_id, uint32_t remote_id)
{
    if (buf == NULL || civ_frame == NULL || civ_length == 0)
    {
        return -RIG_EINVAL;
    }

    if (bufsize < ICOM_NETWORK_CIV_LEN + civ_length)
    {
        return -RIG_EINVAL;
    }

    icom_network_packet_build_header(buf, bufsize, (uint32_t)(0x15 + civ_length),
                                     ICOM_NETWORK_PACKET_TYPE_DATA, sequence,
                                     local_id, remote_id);
    buf[0x10] = reply;
    icom_network_put_le16(buf + 0x11, (uint16_t)civ_length);
    icom_network_put_be16(buf + 0x13, send_sequence);
    memcpy(buf + ICOM_NETWORK_CIV_LEN, civ_frame, civ_length);

    return (int)(0x15 + civ_length);
}

int icom_network_packet_parse_civ(const uint8_t *buf, size_t length,
                                  struct icom_network_packet_civ *civ)
{
    uint16_t payload_length;

    if (buf == NULL || civ == NULL || length < ICOM_NETWORK_CIV_LEN)
    {
        return -RIG_EINVAL;
    }

    payload_length = icom_network_get_le16(buf + ICOM_NETWORK_CIV_OFF_PAYLOAD_LEN);

    if ((size_t)ICOM_NETWORK_CIV_LEN + payload_length > length)
    {
        return -RIG_EPROTO;
    }

    civ->reply   = buf[ICOM_NETWORK_CIV_OFF_REPLY];
    civ->payload_length = payload_length;
    civ->send_sequence = icom_network_get_be16(buf + ICOM_NETWORK_CIV_OFF_SEND_SEQ);
    civ->data    = buf + ICOM_NETWORK_CIV_LEN;

    return RIG_OK;
}

/* ------------------------------------------------------------------ */
/* audio data packets                                                  */
/* ------------------------------------------------------------------ */

int icom_network_packet_build_audio(uint8_t *buf, size_t bufsize,
                                    const uint8_t *audio, size_t audio_length,
                                    uint16_t identifier, uint16_t send_sequence,
                                    uint16_t sequence, uint32_t local_id,
                                    uint32_t remote_id)
{
    if (buf == NULL || audio == NULL || audio_length == 0)
    {
        return -RIG_EINVAL;
    }

    if (bufsize < ICOM_NETWORK_AUDIO_LEN + audio_length)
    {
        return -RIG_EINVAL;
    }

    icom_network_packet_build_header(buf, bufsize, (uint32_t)(ICOM_NETWORK_AUDIO_LEN + audio_length),
                                     ICOM_NETWORK_PACKET_TYPE_DATA, sequence,
                                     local_id, remote_id);
    icom_network_put_le16(buf + ICOM_NETWORK_AUDIO_OFF_IDENT, identifier);
    icom_network_put_be16(buf + ICOM_NETWORK_AUDIO_OFF_SEND_SEQ, send_sequence);
    buf[ICOM_NETWORK_AUDIO_OFF_REPLY] = 0x00;
    buf[ICOM_NETWORK_AUDIO_OFF_TYPE]  = 0x00;
    icom_network_put_be16(buf + ICOM_NETWORK_AUDIO_OFF_PAYLOAD_LEN,
                          (uint16_t)audio_length);
    memcpy(buf + ICOM_NETWORK_AUDIO_LEN, audio, audio_length);

    return (int)(ICOM_NETWORK_AUDIO_LEN + audio_length);
}

int icom_network_packet_parse_audio(const uint8_t *buf, size_t length,
                                    struct icom_network_packet_audio *audio)
{
    uint16_t payload_length;

    if (buf == NULL || audio == NULL || length < ICOM_NETWORK_AUDIO_LEN)
    {
        return -RIG_EINVAL;
    }

    payload_length = icom_network_get_be16(buf + ICOM_NETWORK_AUDIO_OFF_PAYLOAD_LEN);

    if ((size_t)ICOM_NETWORK_AUDIO_LEN + payload_length > length)
    {
        return -RIG_EPROTO;
    }

    audio->identifier   = icom_network_get_le16(buf + ICOM_NETWORK_AUDIO_OFF_IDENT);
    audio->send_sequence = icom_network_get_be16(buf + ICOM_NETWORK_AUDIO_OFF_SEND_SEQ);
    audio->payload_length = payload_length;
    audio->data    = buf + ICOM_NETWORK_AUDIO_LEN;

    return RIG_OK;
}

/* ------------------------------------------------------------------ */
/* passcode                                                            */
/* ------------------------------------------------------------------ */

void icom_network_passcode_encode(uint8_t out[16], const char *in)
{
    size_t i;
    size_t length;

    memset(out, 0, 16);

    if (in == NULL)
    {
        return;
    }

    length = strlen(in);

    for (i = 0; i < length && i < 16; i++)
    {
        int p = (uint8_t)in[i] + (int)i;

        if (p > 126)
        {
            p = 32 + p % 127;
        }

        out[i] = icom_network_passcode_table[p & 0xff];
    }
}

/* ------------------------------------------------------------------ */
/* authentication / negotiation                                        */
/* ------------------------------------------------------------------ */

/* Copy a NUL-padded fixed-width string field. */
static void put_fixed_str(uint8_t *p, size_t width, const char *s)
{
    size_t n = s ? strlen(s) : 0;

    if (n > width) { n = width; }

    memset(p, 0, width);

    if (n) { memcpy(p, s, n); }
}

int icom_network_packet_build_login(uint8_t *buf, size_t bufsize,
                                    const char *username, const char *password,
                                    const char *client_name, uint16_t inner_sequence,
                                    uint16_t token_request, uint32_t token,
                                    uint16_t sequence, uint32_t local_id,
                                    uint32_t remote_id)
{
    if (buf == NULL || bufsize < ICOM_NETWORK_LOGIN_LEN)
    {
        return -RIG_EINVAL;
    }

    memset(buf, 0, ICOM_NETWORK_LOGIN_LEN);
    icom_network_packet_build_header(buf, bufsize, ICOM_NETWORK_LOGIN_LEN,
                                     ICOM_NETWORK_PACKET_TYPE_DATA, sequence,
                                     local_id, remote_id);
    icom_network_put_be32(buf + ICOM_NETWORK_REQ_OFF_PAYLOAD_SIZE, 0x70);
    buf[ICOM_NETWORK_REQ_OFF_REQUEST_REPLY] = 0x01;                        /* requestreply */
    buf[ICOM_NETWORK_REQ_OFF_REQUEST_TYPE] = 0x00;                        /* request_type */
    icom_network_put_be16(buf + ICOM_NETWORK_REQ_OFF_AUTH_START_ID,
                          inner_sequence); /* inner auth sequence is big-endian on the wire */
    icom_network_put_be16(buf + ICOM_NETWORK_REQ_OFF_TOKEN_REQUEST, token_request);
    icom_network_put_be32(buf + ICOM_NETWORK_REQ_OFF_TOKEN, token);
    icom_network_passcode_encode(buf + ICOM_NETWORK_LOGIN_OFF_USERNAME, username);
    icom_network_passcode_encode(buf + ICOM_NETWORK_LOGIN_OFF_PASSWORD, password);
    put_fixed_str(buf + ICOM_NETWORK_LOGIN_OFF_CLIENT, 16, client_name);

    return ICOM_NETWORK_LOGIN_LEN;
}

int icom_network_packet_parse_login_response(
    const uint8_t *buf, size_t length,
    struct icom_network_packet_login_response *response)
{
    if (buf == NULL || response == NULL || length < ICOM_NETWORK_LOGIN_RESP_LEN)
    {
        return -RIG_EINVAL;
    }

    response->token_request  = icom_network_get_be16(buf + ICOM_NETWORK_LOGIN_RESP_OFF_TOKEN_REQUEST);
    response->token       = icom_network_get_be32(buf + ICOM_NETWORK_LOGIN_RESP_OFF_TOKEN);
    response->auth_start_id = icom_network_get_be16(buf + ICOM_NETWORK_LOGIN_RESP_OFF_AUTH_START_ID);
    /* error is success (0) / non-zero failure; compare against 0 only. */
    response->error       = icom_network_get_le32(buf + ICOM_NETWORK_LOGIN_RESP_OFF_ERROR);

    memcpy(response->connection, buf + ICOM_NETWORK_LOGIN_RESP_OFF_CONNECTION, 16);
    response->connection[16] = '\0';

    return RIG_OK;
}

int icom_network_packet_build_token(uint8_t *buf, size_t bufsize,
                                    uint8_t request_type, uint16_t inner_sequence,
                                    uint16_t token_request, uint32_t token,
                                    const uint8_t *authid, uint16_t sequence,
                                    uint32_t local_id, uint32_t remote_id)
{
    if (buf == NULL || bufsize < ICOM_NETWORK_TOKEN_LEN)
    {
        return -RIG_EINVAL;
    }

    memset(buf, 0, ICOM_NETWORK_TOKEN_LEN);
    icom_network_packet_build_header(buf, bufsize, ICOM_NETWORK_TOKEN_LEN,
                                     ICOM_NETWORK_PACKET_TYPE_DATA, sequence,
                                     local_id, remote_id);
    icom_network_put_be32(buf + ICOM_NETWORK_REQ_OFF_PAYLOAD_SIZE, 0x30);
    buf[ICOM_NETWORK_REQ_OFF_REQUEST_REPLY] = 0x01;                        /* requestreply */
    buf[ICOM_NETWORK_REQ_OFF_REQUEST_TYPE] = request_type;                 /* create/renew/remove */
    icom_network_put_be16(buf + ICOM_NETWORK_REQ_OFF_AUTH_START_ID,
                          inner_sequence); /* inner auth sequence is big-endian on the wire */
    icom_network_put_be16(buf + ICOM_NETWORK_REQ_OFF_TOKEN_REQUEST, token_request);
    icom_network_put_be32(buf + ICOM_NETWORK_REQ_OFF_TOKEN, token);

    if (authid) { memcpy(buf + ICOM_NETWORK_TOKEN_OFF_AUTHID, authid, 16); }

    return ICOM_NETWORK_TOKEN_LEN;
}

int icom_network_packet_parse_status(const uint8_t *buf, size_t length,
                                     struct icom_network_packet_status *status)
{
    if (buf == NULL || status == NULL || length < 0x50)
    {
        return -RIG_EINVAL;
    }

    status->error      = icom_network_get_le32(buf + 0x30);
    status->disconnect       = buf[0x40];
    status->civ_port   = icom_network_get_be16(buf + 0x42);
    status->audio_port = icom_network_get_be16(buf + 0x46);

    return RIG_OK;
}

/* Rate flags paired with the rate each one advertises. */
static const struct
{
    uint16_t bit;
    int      hz;
} icom_network_rate_bits[] =
{
    { ICOM_NETWORK_RATE_48000, 48000 },
    { ICOM_NETWORK_RATE_44100, 44100 },
    { ICOM_NETWORK_RATE_32000, 32000 },
    { ICOM_NETWORK_RATE_24000, 24000 },
    { ICOM_NETWORK_RATE_22050, 22050 },
    { ICOM_NETWORK_RATE_16000, 16000 },
    { ICOM_NETWORK_RATE_12000, 12000 },
    { ICOM_NETWORK_RATE_11025, 11025 },
    { ICOM_NETWORK_RATE_8000,   8000 },
};

int icom_network_rate_supported(uint16_t bitmap, int hz)
{
    size_t i;

    for (i = 0; i < sizeof(icom_network_rate_bits)
            / sizeof(icom_network_rate_bits[0]); i++)
    {
        if (icom_network_rate_bits[i].hz == hz)
        {
            return (bitmap & icom_network_rate_bits[i].bit) ? 1 : 0;
        }
    }

    return 0;   /* a rate the protocol cannot express is never supported */
}

const char *icom_network_rate_list(uint16_t bitmap, char *buf, size_t buflen)
{
    size_t i;
    size_t used = 0;

    if (buf == NULL || buflen == 0)
    {
        return buf;
    }

    buf[0] = '\0';

    for (i = 0; i < sizeof(icom_network_rate_bits)
            / sizeof(icom_network_rate_bits[0]); i++)
    {
        int n;

        if (!(bitmap & icom_network_rate_bits[i].bit))
        {
            continue;
        }

        n = snprintf(buf + used, buflen - used, "%s%d",
                     used ? ", " : "", icom_network_rate_bits[i].hz);

        if (n < 0 || (size_t)n >= buflen - used)
        {
            /* snprintf already wrote a truncated rate; drop it so the list
             * never ends in a partial number that reads as a real rate. */
            buf[used] = '\0';
            break;
        }

        used += (size_t)n;
    }

    if (used == 0)
    {
        snprintf(buf, buflen, "none");
    }

    return buf;
}

/* Copy a fixed-width, space- or NUL-padded wire string into a C string. */
static void icom_network_copy_padded_string(char *dst, size_t dstlen,
        const uint8_t *src, size_t srclen)
{
    size_t n;

    if (dstlen == 0)
    {
        return;
    }

    n = srclen < dstlen - 1 ? srclen : dstlen - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';

    while (n > 0 && (dst[n - 1] == ' ' || dst[n - 1] == '\0'))
    {
        dst[--n] = '\0';
    }
}

/* Decode one 0x66-byte radio entry. The first 16 bytes are either a GUID or,
 * when commoncap says so, a block holding the radio's MAC. */
static void icom_network_parse_radio_cap(const uint8_t *r,
        struct icom_network_packet_radio_cap *out)
{
    out->commoncap = icom_network_get_le16(r + ICOM_NETWORK_CAP_OFF_COMMONCAP);
    out->use_mac = (out->commoncap == ICOM_NETWORK_COMMONCAP_MAC) ? 1 : 0;

    memcpy(out->identity, r + ICOM_NETWORK_CAP_OFF_IDENTITY, 16);
    memcpy(out->mac_address, r + ICOM_NETWORK_CAP_OFF_MAC, 6);

    icom_network_copy_padded_string(out->name, sizeof(out->name), r + ICOM_NETWORK_CAP_OFF_NAME, 32);
    icom_network_copy_padded_string(out->audio, sizeof(out->audio), r + ICOM_NETWORK_CAP_OFF_AUDIO, 32);
    /* connection_type and the rate bitmaps are little-endian uint16 here; the
     * chosen rate in the connection_info request we send is a big-endian
     * uint32 instead. */
    out->connection_type = icom_network_get_le16(r + ICOM_NETWORK_CAP_OFF_CONNTYPE);
    out->civ_addr = r[ICOM_NETWORK_CAP_OFF_CIV_ADDR];
    out->rx_rate = icom_network_get_le16(r + ICOM_NETWORK_CAP_OFF_RX_RATE);
    out->tx_rate = icom_network_get_le16(r + ICOM_NETWORK_CAP_OFF_TX_RATE);
    out->baudrate = icom_network_get_be32(r + ICOM_NETWORK_CAP_OFF_BAUDRATE);
}

int icom_network_packet_parse_capabilities(
    const uint8_t *buf, size_t length,
    struct icom_network_packet_capabilities *caps)
{
    size_t i;

    if (buf == NULL || caps == NULL || length < 0x42)
    {
        return -RIG_EINVAL;
    }

    memset(caps, 0, sizeof(*caps));
    caps->radio_count = icom_network_get_be16(buf + 0x40);

    for (i = 0; i < caps->radio_count && i < ICOM_NETWORK_MAX_RADIOS; i++)
    {
        size_t off = ICOM_NETWORK_CAP_ENTRY_FIRST + i * ICOM_NETWORK_CAP_ENTRY_LEN;

        if (length < off + ICOM_NETWORK_CAP_ENTRY_LEN)
        {
            break;      /* truncated; keep whatever decoded cleanly */
        }

        icom_network_parse_radio_cap(buf + off, &caps->radio[i]);
        caps->radio_parsed++;
    }

    return RIG_OK;
}

int icom_network_select_radio(const struct icom_network_packet_capabilities
                              *caps,
                              int index, const char *name)
{
    int i;

    if (caps == NULL || caps->radio_parsed == 0)
    {
        return -RIG_EPROTO;
    }

    if (index >= 0)
    {
        return index < (int)caps->radio_parsed ? index : -RIG_EINVAL;
    }

    if (name == NULL || name[0] == '\0')
    {
        return -RIG_EINVAL;
    }

    for (i = 0; i < (int)caps->radio_parsed; i++)
    {
        if (strcasecmp(caps->radio[i].name, name) == 0)
        {
            return i;
        }
    }

    return -RIG_ENTARGET;
}

int icom_network_packet_build_connection_info(
    uint8_t *buf, size_t bufsize,
    const struct icom_network_connection_info_request *request,
    uint16_t sequence, uint32_t local_id, uint32_t remote_id)
{
    if (buf == NULL || request == NULL || bufsize < 0x90)
    {
        return -RIG_EINVAL;
    }

    memset(buf, 0, 0x90);
    icom_network_packet_build_header(buf, bufsize, 0x90,
                                     ICOM_NETWORK_PACKET_TYPE_DATA, sequence,
                                     local_id, remote_id);
    icom_network_put_be32(buf + 0x10, 0x80); /* payloadsize */
    buf[ICOM_NETWORK_REQ_OFF_REQUEST_REPLY] = 0x01;                        /* requestreply */
    buf[ICOM_NETWORK_REQ_OFF_REQUEST_TYPE] = 0x03;                        /* request_type (connection_info) */
    icom_network_put_be16(buf + ICOM_NETWORK_REQ_OFF_AUTH_START_ID,
                          request->inner_sequence); /* inner auth sequence is big-endian on the wire */
    icom_network_put_be16(buf + ICOM_NETWORK_REQ_OFF_TOKEN_REQUEST, request->token_request);
    icom_network_put_be32(buf + ICOM_NETWORK_REQ_OFF_TOKEN, request->token);

    if (request->identity)
    {
        memcpy(buf + 0x20, request->identity, 16);
    }

    put_fixed_str(buf + 0x40, 32, request->name);
    icom_network_passcode_encode(buf + 0x60, request->username);
    buf[0x70] = request->rx_enable;
    buf[0x71] = request->tx_enable;
    buf[0x72] = request->rx_codec;
    buf[0x73] = request->tx_codec;
    /* sample rates and ports are 32-bit big-endian on the wire. */
    icom_network_put_be32(buf + 0x74, request->rx_rate);
    icom_network_put_be32(buf + 0x78, request->tx_rate);
    icom_network_put_be32(buf + 0x7c, request->civ_port);
    icom_network_put_be32(buf + 0x80, request->audio_port);
    icom_network_put_be32(buf + 0x84, request->txbuffer_ms);
    buf[0x88] = request->convert_audio;

    return 0x90;
}
