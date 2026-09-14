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
/* The UDP protocol used by Icom RS-BA1 and modern radios with a built-in */
/* network server (the proprietary Icom LAN/network control protocol). */

#ifndef _ICOM_NETWORK_PROTO_H
#define _ICOM_NETWORK_PROTO_H 1

#include <stddef.h>
#include <stdint.h>

/*
 * All multi-byte header fields (length, type, sequence, local_id, remote_id) are
 * little-endian on the wire. Inner payload fields noted big-endian in the
 * protocol spec (payloadsize, inner_sequence, ports, sample rates, send_sequence,
 * token_request, token) are handled with the be helpers below.
 */

/* Default UDP ports. */
#define ICOM_NETWORK_PORT_CONTROL 50001
#define ICOM_NETWORK_PORT_CIV     50002
#define ICOM_NETWORK_PORT_AUDIO   50003

/* Common 16-byte header, present on every packet of the protocol. Field
 * offsets are named here because the parser and builder address the wire by
 * offset rather than overlaying a struct on it: the protocol mixes endianness
 * within a single packet (the header is little-endian, several inner payload
 * fields are big-endian), which no single packed layout can express, and
 * explicit accessors keep the code correct on any host and alignment. */
#define ICOM_NETWORK_HEADER_LEN 16

#define ICOM_NETWORK_OFF_LENGTH     0x00  /* le32, whole-packet length */
#define ICOM_NETWORK_OFF_TYPE       0x04  /* le16, control opcode or packet type */
#define ICOM_NETWORK_OFF_SEQUENCE   0x06  /* le16 */
#define ICOM_NETWORK_OFF_LOCAL_ID   0x08  /* be32, our id as the radio sees it */
#define ICOM_NETWORK_OFF_REMOTE_ID  0x0c  /* be32, the radio's id */

/* CI-V data packet (0x15 header + payload). */
#define ICOM_NETWORK_CIV_LEN            0x15
#define ICOM_NETWORK_CIV_OFF_REPLY      0x10  /* u8, 0xc0 from radio, 0xc1 to it */
#define ICOM_NETWORK_CIV_OFF_PAYLOAD_LEN 0x11 /* le16 */
#define ICOM_NETWORK_CIV_OFF_SEND_SEQ   0x13  /* be16, unlike the header's */

/* Audio data packet (0x18 header + payload). */
#define ICOM_NETWORK_AUDIO_LEN          0x18
#define ICOM_NETWORK_AUDIO_OFF_IDENT    0x10  /* le16 */
#define ICOM_NETWORK_AUDIO_OFF_REPLY    0x14  /* u8, 0 on data packets */
#define ICOM_NETWORK_AUDIO_OFF_TYPE     0x15  /* u8, 0 on data packets */
#define ICOM_NETWORK_AUDIO_OFF_SEND_SEQ 0x12  /* be16 */
#define ICOM_NETWORK_AUDIO_OFF_PAYLOAD_LEN 0x16 /* be16 */

/* Request preamble shared by the login, token and connection-info requests,
 * sitting immediately after the common header. */
#define ICOM_NETWORK_REQ_OFF_PAYLOAD_SIZE  0x10  /* be32 */
#define ICOM_NETWORK_REQ_OFF_REQUEST_REPLY 0x14  /* u8, 1 = request */
#define ICOM_NETWORK_REQ_OFF_REQUEST_TYPE  0x15  /* u8 */
#define ICOM_NETWORK_REQ_OFF_AUTH_START_ID 0x16  /* be16 */
#define ICOM_NETWORK_REQ_OFF_TOKEN_REQUEST 0x1a  /* be16 */
#define ICOM_NETWORK_REQ_OFF_TOKEN         0x1c  /* be32 */

/* Login request (0x80 bytes). The credential fields are obfuscated, not
 * encrypted; see icom_network_passcode_encode(). */
#define ICOM_NETWORK_LOGIN_LEN          0x80
#define ICOM_NETWORK_LOGIN_OFF_USERNAME 0x40  /* 16 bytes */
#define ICOM_NETWORK_LOGIN_OFF_PASSWORD 0x50  /* 16 bytes */
#define ICOM_NETWORK_LOGIN_OFF_CLIENT   0x60  /* 16 bytes, plain */

/* Token request (0x40 bytes). */
#define ICOM_NETWORK_TOKEN_LEN        0x40
#define ICOM_NETWORK_TOKEN_OFF_AUTHID 0x20  /* 16 bytes echoed from the radio */

/* Login response (0x60 bytes). */
#define ICOM_NETWORK_LOGIN_RESP_LEN     0x60
#define ICOM_NETWORK_LOGIN_RESP_OFF_TOKEN_REQUEST 0x1a /* be16 */
#define ICOM_NETWORK_LOGIN_RESP_OFF_TOKEN         0x1c /* be32 */
#define ICOM_NETWORK_LOGIN_RESP_OFF_AUTH_START_ID 0x20 /* be16 */
#define ICOM_NETWORK_LOGIN_RESP_OFF_ERROR         0x30 /* le32, 0 = accepted */
#define ICOM_NETWORK_LOGIN_RESP_OFF_CONNECTION    0x40 /* 16 bytes, NUL-padded */

/* Values of the 16-bit "type" field at offset 0x04.
 * For small (0x10) control packets the field carries the control opcode;
 * data packets use 0x0000, retransmit 0x0001, ping 0x0007. */
enum icom_network_packet_type
{
    ICOM_NETWORK_PACKET_TYPE_DATA        = 0x0000,
    ICOM_NETWORK_PACKET_TYPE_RETRANSMIT  = 0x0001,
    ICOM_NETWORK_PACKET_TYPE_PING        = 0x0007,
};

/* Control opcodes carried in the "type" field of a 0x10-length packet.
 *
 * Each of the three sockets runs the same opening handshake: the client sends
 * PROBE and the radio answers PRESENT, which is also where the client learns
 * the radio's connection id; the client then sends READY and the radio echoes
 * it back, after which the socket carries login or stream traffic. IDLE is the
 * periodic keepalive, DISCONNECT the teardown. */
enum icom_network_control
{
    ICOM_NETWORK_CTL_IDLE       = 0x0000,
    ICOM_NETWORK_CTL_PROBE      = 0x0003,  /* client -> radio */
    ICOM_NETWORK_CTL_PRESENT    = 0x0004,  /* radio -> client, answers PROBE */
    ICOM_NETWORK_CTL_DISCONNECT = 0x0005,
    ICOM_NETWORK_CTL_READY      = 0x0006,  /* both directions */
};

/* Audio codec ids negotiated in the connection_info packet. */
enum icom_network_codec
{
    ICOM_NETWORK_CODEC_LPCM       = 0x00, /* 16-bit mono linear PCM */
    ICOM_NETWORK_CODEC_PCMU       = 0x01, /* 8-bit mono mu-law */
    ICOM_NETWORK_CODEC_LPCM8      = 0x02, /* 8-bit mono linear PCM */
    ICOM_NETWORK_CODEC_LPCM16     = 0x04, /* 16-bit mono linear PCM */
    ICOM_NETWORK_CODEC_LPCM8S     = 0x08, /* 8-bit stereo linear PCM */
    ICOM_NETWORK_CODEC_LPCM16S    = 0x10, /* 16-bit stereo linear PCM */
    ICOM_NETWORK_CODEC_PCMUS      = 0x20, /* 8-bit stereo mu-law */
    ICOM_NETWORK_CODEC_OPUS       = 0x40, /* compressed; not on stock Icom firmware */
    ICOM_NETWORK_CODEC_OPUSS      = 0x41, /* compressed; not on stock Icom firmware */
    ICOM_NETWORK_CODEC_ADPCM      = 0x80, /* mono ADPCM, decoding to 16-bit */
};

/* Classification of a received packet, derived from (length, type, payload). */
enum icom_network_packet_kind
{
    ICOM_NETWORK_PACKET_KIND_UNKNOWN = 0,
    ICOM_NETWORK_PACKET_KIND_CONTROL,        /* 0x10 control (idle/probe/...) */
    ICOM_NETWORK_PACKET_KIND_PING,           /* 0x15 ping */
    ICOM_NETWORK_PACKET_KIND_RETRANSMIT,     /* type 0x01 retransmit request */
    ICOM_NETWORK_PACKET_KIND_LOGIN,          /* 0x80 login request */
    ICOM_NETWORK_PACKET_KIND_LOGIN_RESPONSE, /* 0x60 login response */
    ICOM_NETWORK_PACKET_KIND_TOKEN,          /* 0x40 token */
    ICOM_NETWORK_PACKET_KIND_CAPABILITIES,   /* 0x42 + N*0x66 capabilities */
    ICOM_NETWORK_PACKET_KIND_CONNINFO,       /* 0x90 connection info */
    ICOM_NETWORK_PACKET_KIND_STATUS,         /* 0x50 status */
    ICOM_NETWORK_PACKET_KIND_CIV,            /* 0x15 + N CI-V data */
    ICOM_NETWORK_PACKET_KIND_AUDIO,          /* 0x18 + N audio data */
    ICOM_NETWORK_PACKET_KIND_OPENCLOSE,      /* 0x16 open/close */
    ICOM_NETWORK_PACKET_KIND_WATCHDOG,       /* 0x14 watchdog */
};

/* Parsed common header. */
struct icom_network_packet_header
{
    uint32_t length;
    uint16_t type;
    uint16_t sequence;
    uint32_t local_id;
    uint32_t remote_id;
};

/* Parsed CI-V data packet. */
struct icom_network_packet_civ
{
    uint8_t  reply;      /* 0xc0 from radio, 0xc1 to radio */
    uint16_t payload_length;   /* bytes of CI-V data that follow */
    uint16_t send_sequence;    /* big-endian on the wire, unlike the header */
    const uint8_t *data; /* points into the source buffer (no copy) */
};

/* Parsed audio data packet. */
struct icom_network_packet_audio
{
    uint16_t identifier;
    uint16_t send_sequence;
    uint16_t payload_length;
    const uint8_t *data; /* points into the source buffer (no copy) */
};

/* Supported-rate bits in the radio_cap rx_rate/tx_rate bitmaps, as read with
 * icom_network_get_le16(). Most flags sit in the second wire byte; 24 kHz is
 * the one known flag in the first. A radio offering everything advertises
 * 0x8b01, which is exactly 12k|48k|16k|8k|24k. */
#define ICOM_NETWORK_RATE_12000  0x8000
#define ICOM_NETWORK_RATE_44100  0x4000
#define ICOM_NETWORK_RATE_22050  0x2000
#define ICOM_NETWORK_RATE_11025  0x1000
#define ICOM_NETWORK_RATE_48000  0x0800
#define ICOM_NETWORK_RATE_32000  0x0400
#define ICOM_NETWORK_RATE_16000  0x0200
#define ICOM_NETWORK_RATE_8000   0x0100
#define ICOM_NETWORK_RATE_24000  0x0001

/* Audio sample rates this backend offers, highest first. The net_sample_rate
 * config token indexes this list and the stream caps advertise it, so both
 * must come from here. Every rate is one the rate bitmaps can express. */
#define ICOM_NETWORK_SUPPORTED_RATES { 48000, 24000, 16000, 12000, 8000 }
#define ICOM_NETWORK_SUPPORTED_RATE_COUNT 5

/* Default audio jitter-buffer length, used for both directions when the
 * corresponding config token is left at zero. Keep in step with the defaults
 * declared for net_rx_latency / net_tx_latency. */
#define ICOM_NETWORK_DEFAULT_LATENCY_MS 150

/* Silence from the radio before a session is declared lost. Ten missed pings at
 * the 500 ms keepalive cadence: clear of ordinary jitter and a brief WiFi
 * hiccup, while still noticing within a few seconds. */
#define ICOM_NETWORK_DEFAULT_LIVENESS_MS 5000

/* Smallest useful liveness timeout. The radio's regular traffic is its reply to
 * our 500 ms keepalive ping, so a threshold near that interval trips on
 * ordinary jitter; this floor keeps a configured value meaningfully above it. */
#define ICOM_NETWORK_MIN_LIVENESS_MS 1000

/* The passcode obfuscation writes a fixed 16-byte field, so a username or
 * password longer than this cannot be carried on the wire at all. */
#define ICOM_NETWORK_PASSCODE_MAX 16

/* One radio entry inside a capabilities response. The entries are a fixed
 * stride apart, starting after the response's own header; see
 * ICOM_NETWORK_RADIO_CAPABILITIES.md section 3 for the full field survey. */
#define ICOM_NETWORK_CAP_ENTRY_LEN      0x66
#define ICOM_NETWORK_CAP_ENTRY_FIRST    0x42  /* offset of entry 0 */
#define ICOM_NETWORK_CAP_OFF_IDENTITY   0x00  /* 16 bytes, GUID or MAC block */
#define ICOM_NETWORK_CAP_OFF_COMMONCAP  0x07  /* le16, overlaps the GUID bytes */
#define ICOM_NETWORK_CAP_OFF_MAC        0x0a  /* 6 bytes, valid in MAC mode */
#define ICOM_NETWORK_CAP_OFF_NAME       0x10  /* 32 bytes */
#define ICOM_NETWORK_CAP_OFF_AUDIO      0x30  /* 32 bytes */
#define ICOM_NETWORK_CAP_OFF_CONNTYPE   0x50  /* le16 */
#define ICOM_NETWORK_CAP_OFF_CIV_ADDR   0x52  /* u8 */
#define ICOM_NETWORK_CAP_OFF_RX_RATE    0x53  /* le16 bitmap */
#define ICOM_NETWORK_CAP_OFF_TX_RATE    0x55  /* le16 bitmap, 0 = no TX audio */
#define ICOM_NETWORK_CAP_OFF_BAUDRATE   0x5a  /* be32 */

/* commoncap value marking a radio that is addressed by MAC rather than GUID.
 * It overlaps the GUID bytes, so the first 16 bytes of an entry are one or the
 * other, never both. */
#define ICOM_NETWORK_COMMONCAP_MAC 0x8010

/* connection_type values seen in a radio capability entry. */
#define ICOM_NETWORK_CONNTYPE_WIFI     0x0707
#define ICOM_NETWORK_CONNTYPE_ETHERNET 0x073f

/* Radio entries decoded from one capabilities response. A radio's own server
 * advertises one; an RS-BA1 PC server may advertise several. */
#define ICOM_NETWORK_MAX_RADIOS 8

/* One radio entry parsed from a capabilities packet (0x66 bytes on the wire). */
struct icom_network_packet_radio_cap
{
    /* First 16 bytes of the entry, echoed verbatim as the radio identifier in
     * the connection-info request. They hold either a GUID or a MAC-mode
     * block; the decoded views below say which. */
    uint8_t  identity[16];
    uint8_t  mac_address[6]; /* radio MAC; valid only when use_mac is set */
    int      use_mac;    /* 1 when commoncap selects MAC addressing */
    uint16_t commoncap;  /* raw flag word overlapping the GUID bytes */
    char     name[33];   /* NUL-terminated radio name */
    char     audio[33];  /* NUL-terminated audio device name */
    uint16_t connection_type;   /* 0x0707=WiFi, 0x073f=Ethernet */
    uint8_t  civ_addr;   /* radio CI-V address */
    uint16_t rx_rate;   /* RX sample-rate capability bitmap */
    uint16_t tx_rate;   /* TX sample-rate capability bitmap; 0 = no TX audio */
    uint32_t baudrate;  /* CI-V link baud rate reported by the radio */
};

/* Parsed capabilities packet. radio_count is what the server claimed;
 * radio_parsed is how many entries were decoded, which is lower when the server
 * advertises more than ICOM_NETWORK_MAX_RADIOS or the packet was truncated. */
struct icom_network_packet_capabilities
{
    uint16_t radio_count;
    uint16_t radio_parsed;
    struct icom_network_packet_radio_cap radio[ICOM_NETWORK_MAX_RADIOS];
};

/* Pick a radio from a parsed capabilities list. When index is >= 0 it selects
 * by position, otherwise name is matched case-insensitively. Returns the entry
 * index, or a negative Hamlib error when nothing matches. */
int icom_network_select_radio(const struct icom_network_packet_capabilities *caps,
                              int index, const char *name);

/* True when the bitmap advertises hz. Returns 0 for a rate the protocol has no
 * flag for, so an unknown rate is never treated as supported. */
int icom_network_rate_supported(uint16_t bitmap, int hz);

/* Render a bitmap as a human-readable rate list ("48000, 24000") into buf for
 * error messages. Always NUL-terminates; returns buf. */
const char *icom_network_rate_list(uint16_t bitmap, char *buf, size_t buflen);

/* Parameters for building a connection-info (radio select) request. */
struct icom_network_connection_info_request
{
    uint16_t inner_sequence;
    uint16_t token_request;
    uint32_t token;
    const uint8_t *identity;  /* 16-byte radio id block, NULL = zeros */
    const char *name;      /* radio name, e.g. "IC-7610" */
    const char *username;
    uint8_t  rx_enable;
    uint8_t  tx_enable;
    uint8_t  rx_codec;
    uint8_t  tx_codec;
    uint32_t rx_rate;     /* chosen RX sample rate (Hz) */
    uint32_t tx_rate;     /* chosen TX sample rate (Hz) */
    uint16_t civ_port;      /* client CI-V port */
    uint16_t audio_port;    /* client audio port */
    uint32_t txbuffer_ms;  /* TX jitter buffer length */
    uint8_t  convert_audio;      /* audio conversion flag */
};

/* Parsed status packet (server stream confirmation). */
struct icom_network_packet_status
{
    uint32_t error;      /* 0 = success, 0xffffffff = failure */
    uint8_t  disconnect;       /* disconnect notification flag */
    uint16_t civ_port;
    uint16_t audio_port;
};

/* Parsed login response. */
struct icom_network_packet_login_response
{
    uint16_t token_request;
    uint32_t token;
    uint16_t auth_start_id;
    uint32_t error;          /* 0 = success, non-zero = auth failure */
    char     connection[17]; /* NUL-terminated connection-type string */
};

/* Token packet request_type magic values. */
enum icom_network_token_type
{
    ICOM_NETWORK_TOKEN_REMOVE             = 0x01,
    ICOM_NETWORK_TOKEN_CREATE             = 0x02,
    ICOM_NETWORK_TOKEN_STREAM_DISCONNECT  = 0x04,
    ICOM_NETWORK_TOKEN_RENEW              = 0x05,
};

/* ---- byte helpers (exposed for tests) ---- */
void icom_network_put_le16(uint8_t *p, uint16_t v);
void icom_network_put_le32(uint8_t *p, uint32_t v);
void icom_network_put_be16(uint8_t *p, uint16_t v);
void icom_network_put_be32(uint8_t *p, uint32_t v);
uint16_t icom_network_get_le16(const uint8_t *p);
uint32_t icom_network_get_le32(const uint8_t *p);
uint16_t icom_network_get_be16(const uint8_t *p);
uint32_t icom_network_get_be32(const uint8_t *p);

/* Connection id the radio uses to identify this client, derived from the
 * socket's own IPv4 address and port. */
uint32_t icom_network_make_id(const uint8_t ipv4[4], uint16_t port);

/* ---- header ---- */
int icom_network_packet_build_header(uint8_t *buf, size_t bufsize,
                                     uint32_t length,
                                     uint16_t type, uint16_t sequence,
                                     uint32_t local_id, uint32_t remote_id);
int icom_network_packet_parse_header(const uint8_t *buf, size_t length,
                                     struct icom_network_packet_header *hdr);
enum icom_network_packet_kind icom_network_packet_classify(const uint8_t *buf,
        size_t length);

/* ---- small control packets ---- */
int icom_network_packet_build_control(uint8_t *buf, size_t bufsize,
                                      uint16_t control, uint16_t sequence,
                                      uint32_t local_id, uint32_t remote_id);
int icom_network_packet_build_ping(uint8_t *buf, size_t bufsize, uint8_t reply,
                                   uint32_t time_ms, uint16_t sequence,
                                   uint32_t local_id, uint32_t remote_id);
int icom_network_packet_parse_ping(const uint8_t *buf, size_t length,
                                   uint8_t *reply, uint32_t *time_ms);
int icom_network_packet_build_openclose(uint8_t *buf, size_t bufsize,
                                        uint8_t magic, uint16_t send_sequence,
                                        uint16_t sequence, uint32_t local_id,
                                        uint32_t remote_id);

/* ---- retransmit ---- */
int icom_network_packet_build_retransmit(uint8_t *buf, size_t bufsize,
        const uint16_t *sequences, size_t sequence_count,
        uint32_t local_id, uint32_t remote_id);

/* ---- CI-V data packets ---- */
/* The payload carries the complete CI-V frame including the FE FE preamble
 * and FD postamble (the radios tunnel full frames; nothing is stripped). */
int icom_network_packet_build_civ(uint8_t *buf, size_t bufsize,
                                  const uint8_t *civ_frame, size_t civ_length,
                                  uint8_t reply, uint16_t send_sequence, uint16_t sequence,
                                  uint32_t local_id, uint32_t remote_id);
int icom_network_packet_parse_civ(const uint8_t *buf, size_t length,
                                  struct icom_network_packet_civ *civ);

/* ---- audio data packets ---- */
int icom_network_packet_build_audio(uint8_t *buf, size_t bufsize,
                                    const uint8_t *audio, size_t audio_length,
                                    uint16_t identifier, uint16_t send_sequence,
                                    uint16_t sequence, uint32_t local_id,
                                    uint32_t remote_id);
int icom_network_packet_parse_audio(const uint8_t *buf, size_t length,
                                    struct icom_network_packet_audio *audio);

/* ---- authentication / negotiation ---- */
/* Encode up to 16 chars of username/password with the substitution cipher. */
void icom_network_passcode_encode(uint8_t out[16], const char *in);

int icom_network_packet_build_login(uint8_t *buf, size_t bufsize,
                                    const char *username, const char *password,
                                    const char *client_name, uint16_t inner_sequence,
                                    uint16_t token_request, uint32_t token,
                                    uint16_t sequence, uint32_t local_id,
                                    uint32_t remote_id);
int icom_network_packet_parse_login_response(
    const uint8_t *buf, size_t length,
    struct icom_network_packet_login_response *response);

/* Token packet. authid is the 16-byte auth/guid blob echoed from the server
 * (may be NULL to send zeros). request_type is one of icom_network_token_type. */
int icom_network_packet_build_token(uint8_t *buf, size_t bufsize,
                                    uint8_t request_type, uint16_t inner_sequence,
                                    uint16_t token_request, uint32_t token,
                                    const uint8_t *authid, uint16_t sequence,
                                    uint32_t local_id, uint32_t remote_id);

int icom_network_packet_parse_status(const uint8_t *buf, size_t length,
                                     struct icom_network_packet_status *status);

int icom_network_packet_parse_capabilities(
    const uint8_t *buf, size_t length,
    struct icom_network_packet_capabilities *caps);

int icom_network_packet_build_connection_info(
    uint8_t *buf, size_t bufsize,
    const struct icom_network_connection_info_request *request,
    uint16_t sequence, uint32_t local_id, uint32_t remote_id);

#endif /* _ICOM_NETWORK_PROTO_H */
