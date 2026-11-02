/*
 *  Hamlib VITA-49 codec tests
 *  Copyright (c) 2026 by Mikael Nousiainen
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

/* Unit tests for the VITA-49 codec used by the SmartSDR streaming backend. */
/* Tests header parsing, byte-swap, format conversion, and TX packet building. */

#include "acutest.h"

#include <string.h>
#include <math.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "smartsdr_vita.h"


/* ------------------------------------------------------------------ */
/* Helper: build a valid VITA-49 RX packet in a buffer                 */
/* ------------------------------------------------------------------ */

/* Construct a minimal valid VITA-49 packet with given parameters.
 * payload_samples is the number of float32 samples in the payload.
 * Returns total packet size in bytes. */
static int build_test_packet(uint8_t *buf, int buf_len,
                             uint8_t pkt_type, uint32_t stream_id,
                             uint64_t class_id, uint8_t count,
                             uint32_t ts_int, uint64_t ts_frac,
                             const float *payload, int payload_samples)
{
    int payload_words = payload_samples;
    int packet_words = VITA49_HEADER_WORDS + payload_words;
    int packet_bytes = packet_words * 4;

    if (packet_bytes > buf_len)
    {
        return -1;
    }

    uint32_t *words = (uint32_t *)buf;

    /* Word 0: type | C=1 | T=0 | TSI=01 | TSF=01 | count | size.
     * VITA-49 puts TSI at bits 23-22 and TSF at 21-20; this used to set them
     * two bits high, matching a parser that read them the same way. */
    uint32_t word0 = ((uint32_t)pkt_type << 28)
                     | (1u << 27)             /* C = 1 */
                     | (1u << 22)             /* TSI = 01 */
                     | (1u << 20)             /* TSF = 01 */
                     | ((uint32_t)(count & 0x0F) << 16)
                     | ((uint32_t)packet_words & 0xFFFF);
    words[0] = htonl(word0);
    words[1] = htonl(stream_id);
    words[2] = htonl((uint32_t)(class_id >> 32));
    words[3] = htonl((uint32_t)(class_id & 0xFFFFFFFF));
    words[4] = htonl(ts_int);
    words[5] = htonl((uint32_t)(ts_frac >> 32));
    words[6] = htonl((uint32_t)(ts_frac & 0xFFFFFFFF));

    /* Payload: write float32 in big-endian */
    uint32_t *payload_dst = &words[VITA49_HEADER_WORDS];

    for (int i = 0; i < payload_samples; i++)
    {
        uint32_t raw;
        memcpy(&raw, &payload[i], sizeof(float));
        payload_dst[i] = htonl(raw);
    }

    return packet_bytes;
}


/* Build a big-endian int16 payload (for PCC 0x0123 tests). */
static int build_test_packet_s16(uint8_t *buf, int buf_len,
                                 uint32_t stream_id, uint64_t class_id,
                                 uint8_t count,
                                 const int16_t *samples, int num_samples)
{
    /* int16 samples: 2 bytes each, padded to 32-bit words */
    int payload_bytes = num_samples * 2;
    int payload_words = (payload_bytes + 3) / 4;
    int packet_words = VITA49_HEADER_WORDS + payload_words;
    int packet_bytes = packet_words * 4;

    if (packet_bytes > buf_len)
    {
        return -1;
    }

    uint32_t *words = (uint32_t *)buf;

    uint32_t word0 = ((uint32_t)VITA49_PKT_TYPE_IF_DATA << 28)
                     | (1u << 27) | (1u << 22) | (1u << 20)
                     | ((uint32_t)(count & 0x0F) << 16)
                     | ((uint32_t)packet_words & 0xFFFF);
    words[0] = htonl(word0);
    words[1] = htonl(stream_id);
    words[2] = htonl((uint32_t)(class_id >> 32));
    words[3] = htonl((uint32_t)(class_id & 0xFFFFFFFF));
    words[4] = 0;
    words[5] = 0;
    words[6] = 0;

    /* Write int16 samples in big-endian after header */
    uint8_t *payload_dst = buf + VITA49_HEADER_BYTES;

    for (int i = 0; i < num_samples; i++)
    {
        uint16_t be = htons((uint16_t)samples[i]);
        memcpy(&payload_dst[i * 2], &be, 2);
    }

    return packet_bytes;
}


/* ================================================================== */
/* Test: parse valid IF_DATA header                                    */
/* ================================================================== */

void test_parse_valid_if_data(void)
{
    uint8_t pkt[256];
    float payload[] = { 1.0f, -1.0f, 0.5f, -0.5f };
    uint32_t stream_id = 0xDEADBEEF;
    uint64_t class_id = (0x00001C2DUL << 16) | VITA49_PCC_AUDIO_F32;

    int pkt_len = build_test_packet(pkt, sizeof(pkt),
                                    VITA49_PKT_TYPE_IF_DATA,
                                    stream_id, class_id, 3,
                                    42, 12345678ULL,
                                    payload, 4);
    TEST_CHECK(pkt_len > 0);

    struct vita49_header hdr;
    int rc = vita49_parse_header(pkt, pkt_len, &hdr);
    TEST_CHECK(rc == 0);
    TEST_CHECK(hdr.packet_type == VITA49_PKT_TYPE_IF_DATA);
    TEST_CHECK(hdr.class_id_flag == 1);
    TEST_CHECK(hdr.trailer_flag == 0);
    TEST_CHECK(hdr.tsi == 1);
    TEST_CHECK(hdr.tsf == 1);
    TEST_CHECK(hdr.packet_count == 3);
    TEST_CHECK(hdr.packet_size == VITA49_HEADER_WORDS + 4);
    TEST_CHECK(hdr.stream_id == stream_id);
    TEST_MSG("stream_id: got 0x%x, expected 0x%x", hdr.stream_id, stream_id);
    TEST_CHECK(hdr.pcc == VITA49_PCC_AUDIO_F32);
    TEST_MSG("pcc: got 0x%x, expected 0x%x", hdr.pcc, VITA49_PCC_AUDIO_F32);
    TEST_CHECK(hdr.timestamp_int == 42);
    TEST_CHECK(hdr.timestamp_frac == 12345678ULL);
}


/* ================================================================== */
/* Test: parse truncated packet fails                                  */
/* ================================================================== */

void test_parse_truncated(void)
{
    uint8_t buf[16];  /* Less than VITA49_HEADER_BYTES */
    memset(buf, 0, sizeof(buf));

    struct vita49_header hdr;
    TEST_CHECK(vita49_parse_header(buf, sizeof(buf), &hdr) == -1);
    TEST_CHECK(vita49_parse_header(buf, 0, &hdr) == -1);
    TEST_CHECK(vita49_parse_header(NULL, 28, &hdr) == -1);
    TEST_CHECK(vita49_parse_header(buf, 28, NULL) == -1);
}


/* ================================================================== */
/* Test: parse EXT_DATA packet type                                    */
/* ================================================================== */

void test_parse_ext_data(void)
{
    uint8_t pkt[256];
    float payload[] = { 0.0f };
    uint64_t class_id = (0x00001C2DUL << 16) | VITA49_PCC_AUDIO_F32;

    int pkt_len = build_test_packet(pkt, sizeof(pkt),
                                    VITA49_PKT_TYPE_EXT_DATA,
                                    0x12345678, class_id, 0,
                                    0, 0, payload, 1);
    TEST_CHECK(pkt_len > 0);

    struct vita49_header hdr;
    int rc = vita49_parse_header(pkt, pkt_len, &hdr);
    TEST_CHECK(rc == 0);
    TEST_CHECK(hdr.packet_type == VITA49_PKT_TYPE_EXT_DATA);
}


/* ================================================================== */
/* Test: parse PCC 0x0123 (int16 mono)                                 */
/* ================================================================== */

void test_parse_pcc_0123(void)
{
    uint8_t pkt[256];
    float payload[] = { 0.0f };
    uint64_t class_id = (0x00001C2DUL << 16) | VITA49_PCC_AUDIO_S16;

    int pkt_len = build_test_packet(pkt, sizeof(pkt),
                                    VITA49_PKT_TYPE_IF_DATA,
                                    0xABCD0001, class_id, 7,
                                    0, 0, payload, 1);
    TEST_CHECK(pkt_len > 0);

    struct vita49_header hdr;
    int rc = vita49_parse_header(pkt, pkt_len, &hdr);
    TEST_CHECK(rc == 0);
    TEST_CHECK(hdr.pcc == VITA49_PCC_AUDIO_S16);
    TEST_MSG("pcc: got 0x%x, expected 0x%x", hdr.pcc, VITA49_PCC_AUDIO_S16);
}


/* ================================================================== */
/* Test: no class ID flag → pcc = 0                                    */
/* ================================================================== */

void test_parse_no_class_id(void)
{
    uint8_t pkt[VITA49_HEADER_BYTES];
    memset(pkt, 0, sizeof(pkt));

    /* Word 0: type=1, C=0, size=7 */
    uint32_t word0 = (0x1u << 28) | (0u << 27) | 7;
    uint32_t *words = (uint32_t *)pkt;
    words[0] = htonl(word0);

    struct vita49_header hdr;
    int rc = vita49_parse_header(pkt, VITA49_HEADER_BYTES, &hdr);
    TEST_CHECK(rc == 0);
    TEST_CHECK(hdr.class_id_flag == 0);
    TEST_CHECK(hdr.class_id == 0);
    TEST_CHECK(hdr.pcc == 0);
}


/* ================================================================== */
/* Test: float32 byte-swap round-trip                                  */
/* ================================================================== */

void test_swap_float32_roundtrip(void)
{
    float original[] = { 1.0f, -1.0f, 0.0f, 3.14159f, -0.001f };
    int n = sizeof(original) / sizeof(original[0]);

    /* Convert to big-endian (simulate VITA-49 wire format) */
    uint32_t be_buf[5];

    for (int i = 0; i < n; i++)
    {
        uint32_t raw;
        memcpy(&raw, &original[i], sizeof(float));
        be_buf[i] = htonl(raw);
    }

    /* Swap back to host */
    float result[5];
    vita49_swap_float32((uint8_t *)be_buf, result, n);

    for (int i = 0; i < n; i++)
    {
        TEST_CHECK(result[i] == original[i]);
        TEST_MSG("sample %d: got %f, expected %f", i, result[i], original[i]);
    }
}


/* ================================================================== */
/* Test: float32 byte-swap with zero samples                           */
/* ================================================================== */

void test_swap_float32_zero(void)
{
    float result[1] = { 999.0f };
    vita49_swap_float32((uint8_t *)result, result, 0);
    /* Should not crash or modify anything */
    TEST_CHECK(result[0] == 999.0f);
}


/* ================================================================== */
/* Test: int16 mono → float32 stereo conversion                        */
/* ================================================================== */

void test_convert_s16_to_f32_stereo(void)
{
    /* Build big-endian int16 samples */
    int16_t mono_samples[] = { 0, 16384, -16384, 32767, -32768 };
    int n = sizeof(mono_samples) / sizeof(mono_samples[0]);

    uint8_t be_buf[10];

    for (int i = 0; i < n; i++)
    {
        uint16_t be = htons((uint16_t)mono_samples[i]);
        memcpy(&be_buf[i * 2], &be, 2);
    }

    float stereo[10];
    vita49_convert_s16_to_f32_stereo(be_buf, stereo, n);

    /* Check each pair: L == R, value is normalized */
    float expected[] = { 0.0f, 0.5f, -0.5f, 32767.0f / 32768.0f,
                         -1.0f
                       };

    for (int i = 0; i < n; i++)
    {
        TEST_CHECK(fabsf(stereo[2 * i] - expected[i]) < 1e-4f);
        TEST_MSG("L[%d]: got %f, expected %f", i, stereo[2 * i], expected[i]);
        TEST_CHECK(stereo[2 * i] == stereo[2 * i + 1]);
        TEST_MSG("L[%d] != R[%d]: %f vs %f", i, i,
                 stereo[2 * i], stereo[2 * i + 1]);
    }
}


/* ================================================================== */
/* Test: TX packet builder — header fields                             */
/* ================================================================== */

void test_build_tx_header(void)
{
    float samples[4] = { 0.5f, -0.5f, 0.25f, -0.25f };
    uint8_t pkt[256];
    uint32_t stream_id = 0xCAFEBABE;
    uint64_t class_id = (0x00001C2DUL << 16) | VITA49_PCC_AUDIO_F32;

    int pkt_len = vita49_build_tx_packet(pkt, sizeof(pkt),
                                         stream_id, class_id, 5,
                                         samples, 4);

    TEST_CHECK(pkt_len == VITA49_HEADER_BYTES + 4 * (int)sizeof(float));
    TEST_MSG("pkt_len: got %d, expected %d",
             pkt_len, (int)(VITA49_HEADER_BYTES + 4 * sizeof(float)));

    /* Parse back to verify header */
    struct vita49_header hdr;
    int rc = vita49_parse_header(pkt, pkt_len, &hdr);
    TEST_CHECK(rc == 0);
    TEST_CHECK(hdr.packet_type == VITA49_PKT_TYPE_IF_DATA);
    TEST_MSG("type: got %d, expected %d", hdr.packet_type,
             VITA49_PKT_TYPE_IF_DATA);
    TEST_CHECK(hdr.class_id_flag == 1);
    TEST_CHECK(hdr.packet_count == 5);
    TEST_CHECK(hdr.packet_size == VITA49_HEADER_WORDS + 4);
    TEST_CHECK(hdr.stream_id == stream_id);
    TEST_CHECK(hdr.class_id == class_id);
}


/* ================================================================== */
/* Test: TX packet builder — payload round-trip                        */
/* ================================================================== */

void test_build_tx_roundtrip(void)
{
    float original[8] = { 1.0f, -1.0f, 0.0f, 0.5f,
                          -0.5f, 0.001f, -0.001f, 3.14f
                        };
    uint8_t pkt[256];
    uint64_t class_id = (0x00001C2DUL << 16) | VITA49_PCC_AUDIO_F32;

    int pkt_len = vita49_build_tx_packet(pkt, sizeof(pkt),
                                         0x12345678, class_id, 0,
                                         original, 8);
    TEST_CHECK(pkt_len > 0);

    /* Extract and byte-swap payload back */
    float result[8];
    vita49_swap_float32(pkt + VITA49_HEADER_BYTES, result, 8);

    for (int i = 0; i < 8; i++)
    {
        TEST_CHECK(result[i] == original[i]);
        TEST_MSG("sample %d: got %f, expected %f", i, result[i], original[i]);
    }
}


/* ================================================================== */
/* Test: TX packet builder — standard frame size                       */
/* ================================================================== */

void test_build_tx_standard_frame(void)
{
    float samples[SMARTSDR_TX_SAMPLES];
    memset(samples, 0, sizeof(samples));
    samples[0] = 0.5f;
    samples[SMARTSDR_TX_SAMPLES - 1] = -0.5f;

    uint8_t pkt[2048];
    uint64_t class_id = (0x00001C2DUL << 16) | VITA49_PCC_AUDIO_F32;

    int pkt_len = vita49_build_tx_packet(pkt, sizeof(pkt),
                                         0x42, class_id, 15,
                                         samples, SMARTSDR_TX_SAMPLES);

    TEST_CHECK(pkt_len == SMARTSDR_TX_PACKET_BYTES);
    TEST_MSG("pkt_len: got %d, expected %d",
             pkt_len, SMARTSDR_TX_PACKET_BYTES);
}


/* ================================================================== */
/* Test: TX packet builder — buffer too small                          */
/* ================================================================== */

void test_build_tx_buffer_too_small(void)
{
    float samples[4] = { 0.0f };
    uint8_t pkt[16];  /* Way too small */

    int pkt_len = vita49_build_tx_packet(pkt, sizeof(pkt),
                                         0, 0, 0, samples, 4);
    TEST_CHECK(pkt_len == -1);
}


/* ================================================================== */
/* Test: TX packet builder — null parameters                           */
/* ================================================================== */

void test_build_tx_null(void)
{
    float samples[4] = { 0.0f };
    uint8_t pkt[256];

    TEST_CHECK(vita49_build_tx_packet(NULL, 256, 0, 0, 0, samples, 4) == -1);
    TEST_CHECK(vita49_build_tx_packet(pkt, 256, 0, 0, 0, NULL, 4) == -1);
    TEST_CHECK(vita49_build_tx_packet(pkt, 256, 0, 0, 0, samples, 0) == -1);
    TEST_CHECK(vita49_build_tx_packet(pkt, 256, 0, 0, 0, samples, -1) == -1);
}


/* ================================================================== */
/* Test: packet count wraps at 4 bits                                  */
/* ================================================================== */

void test_packet_count_wraps(void)
{
    float samples[1] = { 0.0f };
    uint8_t pkt[256];

    /* Count = 0xFF should be masked to lower 4 bits = 0x0F */
    int pkt_len = vita49_build_tx_packet(pkt, sizeof(pkt),
                                         0, 0, 0xFF, samples, 1);
    TEST_CHECK(pkt_len > 0);

    struct vita49_header hdr;
    vita49_parse_header(pkt, pkt_len, &hdr);
    TEST_CHECK(hdr.packet_count == 0x0F);
    TEST_MSG("count: got %d, expected 15", hdr.packet_count);
}


/* ================================================================== */
/* Test list                                                           */
/* ================================================================== */

TEST_LIST =
{
    { "parse_valid_if_data",         test_parse_valid_if_data },
    { "parse_truncated",             test_parse_truncated },
    { "parse_ext_data",              test_parse_ext_data },
    { "parse_pcc_0123",              test_parse_pcc_0123 },
    { "parse_no_class_id",           test_parse_no_class_id },
    { "swap_float32_roundtrip",      test_swap_float32_roundtrip },
    { "swap_float32_zero",           test_swap_float32_zero },
    { "convert_s16_to_f32_stereo",   test_convert_s16_to_f32_stereo },
    { "build_tx_header",             test_build_tx_header },
    { "build_tx_roundtrip",          test_build_tx_roundtrip },
    { "build_tx_standard_frame",     test_build_tx_standard_frame },
    { "build_tx_buffer_too_small",   test_build_tx_buffer_too_small },
    { "build_tx_null",               test_build_tx_null },
    { "packet_count_wraps",          test_packet_count_wraps },
    { NULL, NULL }
};
