/*
 *  Hamlib Icom network protocol tests
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

/* Unit tests for the Icom network protocol Layer 1 parser/builder. */
/* Covers byte helpers, header, classify, control/ping/openclose/retransmit, */
/* CI-V framing + packets, audio packets, and the passcode cipher. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"
#include "network_proto.h"
#include <string.h>
#include <stdint.h>

#include "hamlib/rig.h"   /* RIG_OK and error codes */


/* --- byte helpers --- */

void test_byte_helpers(void)
{
    uint8_t b[4];

    icom_network_put_le16(b, 0x1234);
    TEST_CHECK(b[0] == 0x34 && b[1] == 0x12);
    TEST_CHECK(icom_network_get_le16(b) == 0x1234);

    icom_network_put_be16(b, 0x1234);
    TEST_CHECK(b[0] == 0x12 && b[1] == 0x34);
    TEST_CHECK(icom_network_get_be16(b) == 0x1234);

    icom_network_put_le32(b, 0xAABBCCDD);
    TEST_CHECK(b[0] == 0xDD && b[1] == 0xCC && b[2] == 0xBB && b[3] == 0xAA);
    TEST_CHECK(icom_network_get_le32(b) == 0xAABBCCDD);

    icom_network_put_be32(b, 0xAABBCCDD);
    TEST_CHECK(b[0] == 0xAA && b[1] == 0xBB && b[2] == 0xCC && b[3] == 0xDD);
    TEST_CHECK(icom_network_get_be32(b) == 0xAABBCCDD);
}

void test_make_id(void)
{
    /* ID = (ip[2]<<24) | (ip[3]<<16) | port; 1.100 + 50001 -> 23380817 */
    uint8_t ip[4] = { 192, 168, 1, 100 };
    TEST_CHECK(icom_network_make_id(ip, 50001) == 23380817u);
}


/* --- header --- */

void test_header_roundtrip_layout(void)
{
    uint8_t buf[16];
    struct icom_network_packet_header h;

    TEST_CHECK(icom_network_packet_build_header(buf, sizeof(buf), 0x10, 0x0003,
               0x1234, 0xAABBCCDD, 0x11223344) == 16);

    /* length/type/sequence little-endian; local_id/remote_id big-endian (MSB first) */
    TEST_CHECK(buf[0x00] == 0x10 && buf[0x01] == 0x00);
    TEST_CHECK(buf[0x04] == 0x03 && buf[0x05] == 0x00);
    TEST_CHECK(buf[0x06] == 0x34 && buf[0x07] == 0x12);
    TEST_CHECK(buf[0x08] == 0xAA && buf[0x0b] == 0xDD);
    TEST_CHECK(buf[0x0c] == 0x11 && buf[0x0f] == 0x44);

    TEST_CHECK(icom_network_packet_parse_header(buf, sizeof(buf), &h) == RIG_OK);
    TEST_CHECK(h.length == 0x10 && h.type == 0x0003 && h.sequence == 0x1234);
    TEST_CHECK(h.local_id == 0xAABBCCDD && h.remote_id == 0x11223344);
}

void test_header_short_buffer(void)
{
    uint8_t buf[8];
    struct icom_network_packet_header h;
    TEST_CHECK(icom_network_packet_build_header(buf, sizeof(buf), 0x10, 0, 0, 0,
               0) == -RIG_EINVAL);
    TEST_CHECK(icom_network_packet_parse_header(buf, 8, &h) == -RIG_EINVAL);
}


/* --- classify --- */

static void make_fixed(uint8_t *buf, uint32_t length, uint16_t type)
{
    memset(buf, 0, length < 16 ? 16 : length);
    icom_network_put_le32(buf + 0, length);
    icom_network_put_le16(buf + 4, type);
}

void test_classify(void)
{
    uint8_t buf[256];

    make_fixed(buf, 0x10, ICOM_NETWORK_CTL_PROBE);
    TEST_CHECK(icom_network_packet_classify(buf, 0x10)
               == ICOM_NETWORK_PACKET_KIND_CONTROL);

    make_fixed(buf, 0x15, ICOM_NETWORK_PACKET_TYPE_PING);
    TEST_CHECK(icom_network_packet_classify(buf, 0x15)
               == ICOM_NETWORK_PACKET_KIND_PING);

    make_fixed(buf, 0x10, ICOM_NETWORK_PACKET_TYPE_RETRANSMIT);
    TEST_CHECK(icom_network_packet_classify(buf, 0x10)
               == ICOM_NETWORK_PACKET_KIND_RETRANSMIT);

    make_fixed(buf, 0x40, 0);
    TEST_CHECK(icom_network_packet_classify(buf, 0x40)
               == ICOM_NETWORK_PACKET_KIND_TOKEN);
    make_fixed(buf, 0x50, 0);
    TEST_CHECK(icom_network_packet_classify(buf, 0x50)
               == ICOM_NETWORK_PACKET_KIND_STATUS);
    make_fixed(buf, 0x60, 0);
    TEST_CHECK(icom_network_packet_classify(buf, 0x60)
               == ICOM_NETWORK_PACKET_KIND_LOGIN_RESPONSE);
    make_fixed(buf, 0x80, 0);
    TEST_CHECK(icom_network_packet_classify(buf, 0x80)
               == ICOM_NETWORK_PACKET_KIND_LOGIN);
    make_fixed(buf, 0x90, 0);
    TEST_CHECK(icom_network_packet_classify(buf, 0x90)
               == ICOM_NETWORK_PACKET_KIND_CONNINFO);

    /* capabilities: 0x42 + 1*0x66 = 0xa8 */
    make_fixed(buf, 0xa8, 0);
    TEST_CHECK(icom_network_packet_classify(buf, 0xa8)
               == ICOM_NETWORK_PACKET_KIND_CAPABILITIES);

    /* too short */
    TEST_CHECK(icom_network_packet_classify(buf, 4)
               == ICOM_NETWORK_PACKET_KIND_UNKNOWN);
}

void test_classify_data_packets(void)
{
    uint8_t buf[64];
    uint8_t civ[3] = { 0x98, 0xe0, 0x03 };
    uint8_t pcm[4] = { 1, 2, 3, 4 };

    TEST_CHECK(icom_network_packet_build_civ(buf, sizeof(buf), civ, 3, 0xc0, 7,
               1, 0, 0) > 0);
    TEST_CHECK(icom_network_packet_classify(buf, 0x18)
               == ICOM_NETWORK_PACKET_KIND_CIV);

    TEST_CHECK(icom_network_packet_build_audio(buf, sizeof(buf), pcm, 4, 0x0244,
               5, 1, 0, 0) > 0);
    TEST_CHECK(icom_network_packet_classify(buf, 0x1c)
               == ICOM_NETWORK_PACKET_KIND_AUDIO);

    TEST_CHECK(icom_network_packet_build_openclose(buf, sizeof(buf), 0x04, 1, 1,
               0, 0) == 0x16);
    TEST_CHECK(icom_network_packet_classify(buf, 0x16)
               == ICOM_NETWORK_PACKET_KIND_OPENCLOSE);
}


/* --- control / ping / openclose --- */

void test_build_control(void)
{
    uint8_t buf[16];
    TEST_CHECK(icom_network_packet_build_control(buf, sizeof(buf),
               ICOM_NETWORK_CTL_PROBE, 0, 0xDEADBEEF, 0) == 0x10);
    TEST_CHECK(icom_network_get_le32(buf) == 0x10);
    TEST_CHECK(icom_network_get_le16(buf + 4) == ICOM_NETWORK_CTL_PROBE);
    /* local_id stored big-endian */
    TEST_CHECK(icom_network_get_be32(buf + 8) == 0xDEADBEEF);
}

void test_ping_roundtrip(void)
{
    uint8_t buf[0x15];
    uint8_t reply = 0xff;
    uint32_t t = 0;

    TEST_CHECK(icom_network_packet_build_ping(buf, sizeof(buf), 0x00,
               0x12345678, 9, 0, 0) == 0x15);
    TEST_CHECK(buf[0x10] == 0x00);
    /* time stored little-endian */
    TEST_CHECK(buf[0x11] == 0x78 && buf[0x14] == 0x12);

    TEST_CHECK(icom_network_packet_parse_ping(buf, sizeof(buf), &reply, &t)
               == RIG_OK);
    TEST_CHECK(reply == 0x00 && t == 0x12345678);
}

void test_openclose_layout(void)
{
    uint8_t buf[0x16];
    TEST_CHECK(icom_network_packet_build_openclose(buf, sizeof(buf), 0x04,
               0x0102, 3, 0, 0) == 0x16);
    TEST_CHECK(buf[0x10] == 0x01 && buf[0x11] == 0xc0 && buf[0x12] == 0x00);
    /* send_sequence big-endian */
    TEST_CHECK(buf[0x13] == 0x01 && buf[0x14] == 0x02);
    TEST_CHECK(buf[0x15] == 0x04);
}


/* --- retransmit --- */

void test_retransmit_single(void)
{
    uint8_t buf[16];
    uint16_t sequences[1] = { 0x0009 };
    TEST_CHECK(icom_network_packet_build_retransmit(buf, sizeof(buf), sequences, 1,
               0, 0) == 0x10);
    TEST_CHECK(icom_network_get_le16(buf + 4) ==
               ICOM_NETWORK_PACKET_TYPE_RETRANSMIT);
    TEST_CHECK(icom_network_get_le16(buf + 6) == 0x0009);
}

void test_retransmit_multi(void)
{
    uint8_t buf[64];
    uint16_t sequences[3] = { 5, 6, 7 };
    int n = icom_network_packet_build_retransmit(buf, sizeof(buf), sequences, 3, 0,
            0);
    TEST_CHECK(n == 0x10 + 3 * 4);
    TEST_CHECK(icom_network_get_le32(buf) == (uint32_t)(0x10 + 3 * 4));
    /* each entry: sequence le16 repeated twice */
    TEST_CHECK(icom_network_get_le16(buf + 0x10) == 5
               && icom_network_get_le16(buf + 0x12) == 5);
    TEST_CHECK(icom_network_get_le16(buf + 0x14) == 6
               && icom_network_get_le16(buf + 0x16) == 6);
    TEST_CHECK(icom_network_get_le16(buf + 0x18) == 7
               && icom_network_get_le16(buf + 0x1a) == 7);
}


/* --- CI-V packets (payload is the complete FE FE..FD frame) --- */

void test_civ_packet_roundtrip(void)
{
    uint8_t buf[64];
    /* full CI-V frame: read freq, FE FE <address> <ctrl> 03 FD */
    uint8_t civ[6] = { 0xfe, 0xfe, 0x98, 0xe0, 0x03, 0xfd };
    struct icom_network_packet_civ p;

    TEST_CHECK(icom_network_packet_build_civ(buf, sizeof(buf), civ, 6, 0xc1,
               0x0007, 1, 0, 0) == 0x15 + 6);
    TEST_CHECK(buf[0x10] == 0xc1);
    /* payload_length little-endian, send_sequence big-endian */
    TEST_CHECK(buf[0x11] == 0x06 && buf[0x12] == 0x00);
    TEST_CHECK(buf[0x13] == 0x00 && buf[0x14] == 0x07);

    TEST_CHECK(icom_network_packet_parse_civ(buf, 0x15 + 6, &p) == RIG_OK);
    TEST_CHECK(p.reply == 0xc1 && p.payload_length == 6
               && p.send_sequence == 0x0007);
    TEST_CHECK(memcmp(p.data, civ, 6) == 0);
}

void test_civ_packet_truncated(void)
{
    uint8_t buf[64];
    uint8_t civ[3] = { 0x98, 0xe0, 0x03 };
    struct icom_network_packet_civ p;
    TEST_CHECK(icom_network_packet_build_civ(buf, sizeof(buf), civ, 3, 0xc0, 1,
               1, 0, 0) == 0x18);
    /* claim a shorter buffer than payload_length implies */
    TEST_CHECK(icom_network_packet_parse_civ(buf, 0x16, &p) == -RIG_EPROTO);
}


/* --- audio packets --- */

void test_audio_packet_roundtrip(void)
{
    uint8_t buf[64];
    uint8_t pcm[4] = { 0x11, 0x22, 0x33, 0x44 };
    struct icom_network_packet_audio a;

    TEST_CHECK(icom_network_packet_build_audio(buf, sizeof(buf), pcm, 4, 0x0244,
               0x0005, 1, 0, 0) == 0x1c);
    /* identifier little-endian, send_sequence + payload_length big-endian */
    TEST_CHECK(buf[0x10] == 0x44 && buf[0x11] == 0x02);
    TEST_CHECK(buf[0x12] == 0x00 && buf[0x13] == 0x05);
    TEST_CHECK(buf[0x16] == 0x00 && buf[0x17] == 0x04);

    TEST_CHECK(icom_network_packet_parse_audio(buf, 0x1c, &a) == RIG_OK);
    TEST_CHECK(a.identifier == 0x0244 && a.send_sequence == 0x0005
               && a.payload_length == 4);
    TEST_CHECK(memcmp(a.data, pcm, 4) == 0);
}


/* --- passcode --- */

void test_passcode_vectors(void)
{
    uint8_t out[16];

    /* hand-computed from the substitution table:
     * 't'(116)+0 -> [116]=0x22 ; 'e'(101)+1=102 -> [102]=0x3f ;
     * 's'(115)+2=117 -> [117]=0x5c ; 't'(116)+3=119 -> [119]=0x31 */
    icom_network_passcode_encode(out, "test");
    TEST_CHECK(out[0] == 0x22 && out[1] == 0x3f && out[2] == 0x5c
               && out[3] == 0x31);
    TEST_CHECK(out[4] == 0x00); /* zero padded */

    /* 'a'(97)+0 -> [97]=0x38 */
    icom_network_passcode_encode(out, "a");
    TEST_CHECK(out[0] == 0x38 && out[1] == 0x00);

    icom_network_passcode_encode(out, NULL);
    TEST_CHECK(out[0] == 0x00 && out[15] == 0x00);
}


/* --- auth / negotiation --- */

void test_login_layout(void)
{
    uint8_t buf[0x80];
    TEST_CHECK(icom_network_packet_build_login(buf, sizeof(buf), "test", "test",
               "hamlib", 0x0102, 0x1234, 0xAABBCCDD, 9, 0, 0) == 0x80);
    /* payloadsize big-endian 0x70 at 0x10 */
    TEST_CHECK(buf[0x10] == 0x00 && buf[0x13] == 0x70);
    TEST_CHECK(buf[0x14] == 0x01 && buf[0x15] == 0x00);
    /* inner_sequence big-endian at 0x16 */
    TEST_CHECK(buf[0x16] == 0x01 && buf[0x17] == 0x02);
    /* token_request big-endian at 0x1a, token big-endian at 0x1c */
    TEST_CHECK(buf[0x1a] == 0x12 && buf[0x1b] == 0x34);
    TEST_CHECK(buf[0x1c] == 0xAA && buf[0x1f] == 0xDD);
    /* username passcode-encoded "test" -> 22 3f 5c 31 at 0x40 */
    TEST_CHECK(buf[0x40] == 0x22 && buf[0x43] == 0x31);
    /* client_name name at 0x60 */
    TEST_CHECK(memcmp(buf + 0x60, "hamlib", 6) == 0 && buf[0x66] == 0x00);
}

void test_login_response_parse(void)
{
    uint8_t buf[0x60];
    struct icom_network_packet_login_response r;

    memset(buf, 0, sizeof(buf));
    icom_network_packet_build_header(buf, sizeof(buf), 0x60, 0, 0, 0, 0);
    icom_network_put_be16(buf + 0x1a, 0x1234);     /* token_request */
    icom_network_put_be32(buf + 0x1c, 0xAABBCCDD); /* token */
    icom_network_put_be16(buf + 0x20, 0x5678);     /* auth_start_id */
    icom_network_put_le32(buf + 0x30, 0);          /* error = success */
    memcpy(buf + 0x40, "FTTH", 4);

    TEST_CHECK(icom_network_packet_parse_login_response(buf, sizeof(buf), &r)
               == RIG_OK);
    TEST_CHECK(r.token_request == 0x1234 && r.token == 0xAABBCCDD);
    TEST_CHECK(r.auth_start_id == 0x5678 && r.error == 0);
    TEST_CHECK(strcmp(r.connection, "FTTH") == 0);
}

void test_token_build(void)
{
    uint8_t buf[0x40];
    uint8_t authid[16];
    int i;

    for (i = 0; i < 16; i++) { authid[i] = (uint8_t)(0x10 + i); }

    TEST_CHECK(icom_network_packet_build_token(buf, sizeof(buf),
               ICOM_NETWORK_TOKEN_CREATE, 0x0102, 0x1234, 0xAABBCCDD, authid,
               1, 0, 0) == 0x40);
    TEST_CHECK(buf[0x14] == 0x01 && buf[0x15] == ICOM_NETWORK_TOKEN_CREATE);
    TEST_CHECK(buf[0x16] == 0x01 && buf[0x17] == 0x02); /* inner_sequence BE */
    TEST_CHECK(buf[0x1c] == 0xAA && buf[0x1f] == 0xDD); /* token BE */
    TEST_CHECK(memcmp(buf + 0x20, authid, 16) == 0);
}

void test_status_parse(void)
{
    uint8_t buf[0x50];
    struct icom_network_packet_status s;

    memset(buf, 0, sizeof(buf));
    icom_network_packet_build_header(buf, sizeof(buf), 0x50, 0, 0, 0, 0);
    icom_network_put_le32(buf + 0x30, 0);       /* error = success */
    buf[0x40] = 0x00;                           /* disconnect */
    icom_network_put_be16(buf + 0x42, 50002);   /* civ port */
    icom_network_put_be16(buf + 0x46, 50003);   /* audio port */

    TEST_CHECK(icom_network_packet_parse_status(buf, sizeof(buf), &s) == RIG_OK);
    TEST_CHECK(s.error == 0 && s.disconnect == 0x00);
    TEST_CHECK(s.civ_port == 50002 && s.audio_port == 50003);
}

/* Fill one 0x66-byte radio entry with the fields the backend consumes. */
static void build_radio_entry(uint8_t *r, const char *name, uint8_t civ,
                              uint16_t rx_rate, uint16_t tx_rate)
{
    memset(r, 0, 0x66);
    memcpy(r + 0x10, name, strlen(name));
    memcpy(r + 0x30, "ICOM_VAUDIO", 11);
    icom_network_put_le16(r + 0x50, 0x073f);   /* connection_type = Ethernet */
    r[0x52] = civ;
    icom_network_put_le16(r + 0x53, rx_rate);
    icom_network_put_le16(r + 0x55, tx_rate);
    icom_network_put_be32(r + 0x5a, 19200);    /* CI-V baud rate */
}

static void build_capabilities(uint8_t *buf, size_t length, uint16_t count)
{
    memset(buf, 0, length);
    icom_network_packet_build_header(buf, length, length, 0, 0, 0, 0);
    icom_network_put_be16(buf + 0x40, count);
}

void test_capabilities_parse(void)
{
    uint8_t buf[0x42 + 0x66];
    struct icom_network_packet_capabilities c;
    uint8_t *r = buf + 0x42;

    build_capabilities(buf, sizeof(buf), 1);
    build_radio_entry(r, "IC-7610", 0x98, 0x8b01, 0x8b01);

    TEST_CHECK(icom_network_packet_parse_capabilities(buf, sizeof(buf), &c)
               == RIG_OK);
    TEST_CHECK(c.radio_count == 1);
    TEST_CHECK(c.radio_parsed == 1);
    TEST_CHECK(strcmp(c.radio[0].name, "IC-7610") == 0);
    TEST_CHECK(strcmp(c.radio[0].audio, "ICOM_VAUDIO") == 0);
    TEST_CHECK(c.radio[0].connection_type == 0x073f && c.radio[0].civ_addr == 0x98);
    TEST_CHECK(c.radio[0].rx_rate == 0x8b01 && c.radio[0].tx_rate == 0x8b01);
    TEST_CHECK(c.radio[0].baudrate == 19200);
    TEST_CHECK(c.radio[0].use_mac == 0);
}

/* A name padded with trailing spaces must compare equal to the bare name. */
void test_capabilities_parse_padded_name(void)
{
    uint8_t buf[0x42 + 0x66];
    struct icom_network_packet_capabilities c;
    uint8_t *r = buf + 0x42;

    build_capabilities(buf, sizeof(buf), 1);
    build_radio_entry(r, "IC-7610", 0x98, 0x8b01, 0);
    memset(r + 0x10 + 7, ' ', 8);

    TEST_CHECK(icom_network_packet_parse_capabilities(buf, sizeof(buf), &c)
               == RIG_OK);
    TEST_CHECK(strcmp(c.radio[0].name, "IC-7610") == 0);
}

/* commoncap 0x8010 marks the first 16 bytes as a MAC block, not a GUID. The
 * whole block is still kept verbatim because it is echoed back on connect. */
void test_capabilities_parse_mac_mode(void)
{
    static const uint8_t mac[6] = { 0x00, 0x90, 0xc7, 0x15, 0xae, 0x02 };
    uint8_t buf[0x42 + 0x66];
    struct icom_network_packet_capabilities c;
    uint8_t *r = buf + 0x42;

    build_capabilities(buf, sizeof(buf), 1);
    build_radio_entry(r, "IC-7610", 0x98, 0x8b01, 0x8b01);
    icom_network_put_le16(r + 0x07, ICOM_NETWORK_COMMONCAP_MAC);
    memcpy(r + 0x0a, mac, sizeof(mac));

    TEST_CHECK(icom_network_packet_parse_capabilities(buf, sizeof(buf), &c)
               == RIG_OK);
    TEST_CHECK(c.radio[0].commoncap == ICOM_NETWORK_COMMONCAP_MAC);
    TEST_CHECK(c.radio[0].use_mac == 1);
    TEST_CHECK(memcmp(c.radio[0].mac_address, mac, sizeof(mac)) == 0);
    TEST_CHECK(memcmp(c.radio[0].identity, r, 16) == 0);
}

void test_capabilities_parse_multiple(void)
{
    uint8_t buf[0x42 + 3 * 0x66];
    struct icom_network_packet_capabilities c;

    build_capabilities(buf, sizeof(buf), 3);
    build_radio_entry(buf + 0x42 + 0 * 0x66, "IC-7610", 0x98, 0x8b01, 0x8b01);
    build_radio_entry(buf + 0x42 + 1 * 0x66, "IC-9700", 0xa2, 0x8b01, 0);
    build_radio_entry(buf + 0x42 + 2 * 0x66, "IC-705", 0xa4, 0x0800, 0x0800);

    TEST_CHECK(icom_network_packet_parse_capabilities(buf, sizeof(buf), &c)
               == RIG_OK);
    TEST_CHECK(c.radio_count == 3 && c.radio_parsed == 3);
    TEST_CHECK(strcmp(c.radio[1].name, "IC-9700") == 0
               && c.radio[1].civ_addr == 0xa2);
    TEST_CHECK(strcmp(c.radio[2].name, "IC-705") == 0
               && c.radio[2].civ_addr == 0xa4);
}

/* A packet claiming more entries than it carries must keep the good ones and
 * report how many were actually decoded. */
void test_capabilities_parse_truncated(void)
{
    uint8_t buf[0x42 + 2 * 0x66];
    struct icom_network_packet_capabilities c;

    build_capabilities(buf, sizeof(buf), 2);
    build_radio_entry(buf + 0x42, "IC-7610", 0x98, 0x8b01, 0x8b01);
    build_radio_entry(buf + 0x42 + 0x66, "IC-9700", 0xa2, 0x8b01, 0);

    TEST_CHECK(icom_network_packet_parse_capabilities(buf, 0x42 + 0x66 + 4, &c)
               == RIG_OK);
    TEST_CHECK(c.radio_count == 2);
    TEST_CHECK(c.radio_parsed == 1);
    TEST_CHECK(strcmp(c.radio[0].name, "IC-7610") == 0);
}

void test_capabilities_parse_invalid(void)
{
    uint8_t buf[0x42 + 0x66];
    struct icom_network_packet_capabilities c;

    build_capabilities(buf, sizeof(buf), 0);

    TEST_CHECK(icom_network_packet_parse_capabilities(NULL, sizeof(buf), &c)
               == -RIG_EINVAL);
    TEST_CHECK(icom_network_packet_parse_capabilities(buf, sizeof(buf), NULL)
               == -RIG_EINVAL);
    TEST_CHECK(icom_network_packet_parse_capabilities(buf, 0x10, &c)
               == -RIG_EINVAL);
    /* radio_count 0 is well-formed, just empty */
    TEST_CHECK(icom_network_packet_parse_capabilities(buf, sizeof(buf), &c)
               == RIG_OK);
    TEST_CHECK(c.radio_parsed == 0);
}

void test_select_radio(void)
{
    uint8_t buf[0x42 + 2 * 0x66];
    struct icom_network_packet_capabilities c;

    build_capabilities(buf, sizeof(buf), 2);
    build_radio_entry(buf + 0x42, "IC-7610", 0x98, 0x8b01, 0x8b01);
    build_radio_entry(buf + 0x42 + 0x66, "IC-9700", 0xa2, 0x8b01, 0);
    TEST_ASSERT(icom_network_packet_parse_capabilities(buf, sizeof(buf), &c)
                == RIG_OK);

    /* index wins over name */
    TEST_CHECK(icom_network_select_radio(&c, 1, "IC-7610") == 1);
    TEST_CHECK(icom_network_select_radio(&c, 0, NULL) == 0);
    /* name match is case-insensitive */
    TEST_CHECK(icom_network_select_radio(&c, -1, "IC-9700") == 1);
    TEST_CHECK(icom_network_select_radio(&c, -1, "ic-9700") == 1);
    /* out of range, unknown name, and no selector at all all fail */
    TEST_CHECK(icom_network_select_radio(&c, 2, NULL) == -RIG_EINVAL);
    TEST_CHECK(icom_network_select_radio(&c, -1, "IC-705") == -RIG_ENTARGET);
    TEST_CHECK(icom_network_select_radio(&c, -1, "") == -RIG_EINVAL);
    TEST_CHECK(icom_network_select_radio(&c, -1, NULL) == -RIG_EINVAL);

    memset(&c, 0, sizeof(c));
    TEST_CHECK(icom_network_select_radio(&c, 0, "IC-7610") == -RIG_EPROTO);
    TEST_CHECK(icom_network_select_radio(NULL, 0, "IC-7610") == -RIG_EPROTO);
}

/* 0x8b01 is what an IC-7610 advertises: 48/24/16/12/8 kHz and nothing else. */
void test_rate_bitmap(void)
{
    char buf[128];

    TEST_CHECK(icom_network_rate_supported(0x8b01, 48000) == 1);
    TEST_CHECK(icom_network_rate_supported(0x8b01, 24000) == 1);
    TEST_CHECK(icom_network_rate_supported(0x8b01, 16000) == 1);
    TEST_CHECK(icom_network_rate_supported(0x8b01, 12000) == 1);
    TEST_CHECK(icom_network_rate_supported(0x8b01, 8000) == 1);
    TEST_CHECK(icom_network_rate_supported(0x8b01, 44100) == 0);
    TEST_CHECK(icom_network_rate_supported(0x8b01, 32000) == 0);
    /* a rate the protocol has no flag for is never supported */
    TEST_CHECK(icom_network_rate_supported(0xffff, 96000) == 0);
    TEST_CHECK(icom_network_rate_supported(0, 48000) == 0);

    TEST_CHECK(strcmp(icom_network_rate_list(0x8b01, buf, sizeof(buf)),
                      "48000, 24000, 16000, 12000, 8000") == 0);
    TEST_CHECK(strcmp(icom_network_rate_list(ICOM_NETWORK_RATE_48000, buf,
                      sizeof(buf)), "48000") == 0);
    TEST_CHECK(strcmp(icom_network_rate_list(0, buf, sizeof(buf)), "none") == 0);
}

/* The list must truncate rather than overflow a short buffer. */
void test_rate_list_truncates(void)
{
    char buf[10];

    TEST_CHECK(icom_network_rate_list(0x8b01, buf, sizeof(buf)) == buf);
    TEST_CHECK(strlen(buf) < sizeof(buf));
    TEST_CHECK(strcmp(buf, "48000") == 0);
    TEST_CHECK(icom_network_rate_list(0x8b01, buf, 0) == buf);
    TEST_CHECK(icom_network_rate_list(0x8b01, NULL, sizeof(buf)) == NULL);
}

void test_connection_info_build(void)
{
    uint8_t buf[0x90];
    struct icom_network_connection_info_request request;

    memset(&request, 0, sizeof(request));
    request.inner_sequence = 0x0102;
    request.token_request = 0x1234;
    request.token = 0xAABBCCDD;
    request.name = "IC-7610";
    request.username = "test";
    request.rx_enable = 1;
    request.rx_codec = 0x04;       /* LPCM16 */
    request.rx_rate = 48000;
    request.civ_port = 50002;
    request.audio_port = 50003;

    TEST_CHECK(icom_network_packet_build_connection_info(buf, sizeof(buf), &request,
               7, 0,
               0) == 0x90);
    /* payloadsize big-endian 0x80, request_type 0x03 */
    TEST_CHECK(buf[0x10] == 0x00 && buf[0x13] == 0x80);
    TEST_CHECK(buf[0x14] == 0x01 && buf[0x15] == 0x03);
    TEST_CHECK(buf[0x16] == 0x01 && buf[0x17] == 0x02); /* inner_sequence BE */
    TEST_CHECK(memcmp(buf + 0x40, "IC-7610", 7) == 0);  /* radio name */
    TEST_CHECK(buf[0x60] == 0x22);                      /* username passcode */
    TEST_CHECK(buf[0x70] == 0x01 && buf[0x72] == 0x04); /* rx_enable, rx_codec */
    /* sample rate / ports big-endian uint32 */
    TEST_CHECK(icom_network_get_be32(buf + 0x74) == 48000);
    TEST_CHECK(icom_network_get_be32(buf + 0x7c) == 50002);
    TEST_CHECK(icom_network_get_be32(buf + 0x80) == 50003);
}


TEST_LIST =
{
    { "byte_helpers",            test_byte_helpers },
    { "make_id",                 test_make_id },
    { "header_roundtrip_layout", test_header_roundtrip_layout },
    { "header_short_buffer",     test_header_short_buffer },
    { "classify",                test_classify },
    { "classify_data_packets",   test_classify_data_packets },
    { "build_control",           test_build_control },
    { "ping_roundtrip",          test_ping_roundtrip },
    { "openclose_layout",        test_openclose_layout },
    { "retransmit_single",       test_retransmit_single },
    { "retransmit_multi",        test_retransmit_multi },
    { "civ_packet_roundtrip",    test_civ_packet_roundtrip },
    { "civ_packet_truncated",    test_civ_packet_truncated },
    { "audio_packet_roundtrip",  test_audio_packet_roundtrip },
    { "passcode_vectors",        test_passcode_vectors },
    { "login_layout",            test_login_layout },
    { "login_response_parse",    test_login_response_parse },
    { "token_build",             test_token_build },
    { "status_parse",            test_status_parse },
    { "capabilities_parse",      test_capabilities_parse },
    { "capabilities_parse_padded_name", test_capabilities_parse_padded_name },
    { "capabilities_parse_mac_mode",    test_capabilities_parse_mac_mode },
    { "capabilities_parse_multiple",    test_capabilities_parse_multiple },
    { "capabilities_parse_truncated",   test_capabilities_parse_truncated },
    { "capabilities_parse_invalid",     test_capabilities_parse_invalid },
    { "select_radio",            test_select_radio },
    { "rate_bitmap",             test_rate_bitmap },
    { "rate_list_truncates",     test_rate_list_truncates },
    { "connection_info_build",          test_connection_info_build },
    { NULL, NULL }
};
