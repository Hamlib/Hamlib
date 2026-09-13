/*
 *  Hamlib SmartSDR VITA-49 decode tests
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

/* Unit tests for the pure decode half of the SmartSDR backend: the packet   */
/* sequence counter, the panadapter FFT header and its bins, the meter       */
/* scaling, and the VITA-49 header codec. None of this touches a socket or a */
/* rig, so every case here is bytes in and values out.                       */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "acutest.h"

#include <string.h>
#include <math.h>

#include <unistd.h>
#include <hamlib/rig.h>
/* Socket headers come from stream_proto.h, which picks the right set for the
 * host; do not include them directly. */
#include "../src/stream_proto.h"

#include "smartsdr_vita.h"


/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Write a panadapter FFT payload: the 12-byte big-endian header followed by
 * num_bins big-endian 16-bit bins. payload_bytes is what the caller claims the
 * datagram carried, which is not always header + bins: a short frame and a
 * padded frame are both things the radio sends. */
static int build_fft_payload(uint8_t *buf, int buf_len,
                             unsigned start_bin, unsigned num_bins,
                             unsigned bin_size, unsigned total_bins,
                             uint32_t frame, int payload_bytes)
{
    unsigned i;

    if (payload_bytes > buf_len)
    {
        return -1;
    }

    memset(buf, 0, (size_t)buf_len);

    buf[0] = (uint8_t)(start_bin >> 8);
    buf[1] = (uint8_t)(start_bin & 0xFF);
    buf[2] = (uint8_t)(num_bins >> 8);
    buf[3] = (uint8_t)(num_bins & 0xFF);
    buf[4] = (uint8_t)(bin_size >> 8);
    buf[5] = (uint8_t)(bin_size & 0xFF);
    buf[6] = (uint8_t)(total_bins >> 8);
    buf[7] = (uint8_t)(total_bins & 0xFF);
    buf[8]  = (uint8_t)(frame >> 24);
    buf[9]  = (uint8_t)((frame >> 16) & 0xFF);
    buf[10] = (uint8_t)((frame >> 8) & 0xFF);
    buf[11] = (uint8_t)(frame & 0xFF);

    for (i = 0; i < num_bins && 12 + (int)(i * 2) + 1 < payload_bytes; i++)
    {
        buf[12 + i * 2]     = (uint8_t)((100u + i) >> 8);
        buf[12 + i * 2 + 1] = (uint8_t)((100u + i) & 0xFF);
    }

    return payload_bytes;
}


/* A VITA-49 packet with every header field set from the arguments. */
static int build_vita_packet(uint8_t *buf, int buf_len,
                             uint8_t pkt_type, int class_id_flag,
                             int trailer_flag, uint8_t tsi, uint8_t tsf,
                             uint8_t count, uint32_t stream_id,
                             uint64_t class_id, uint32_t ts_int,
                             uint64_t ts_frac, int payload_words)
{
    int packet_words = VITA49_HEADER_WORDS + payload_words;
    int packet_bytes = packet_words * 4;
    uint32_t *words = (uint32_t *)buf;
    uint32_t word0;

    if (packet_bytes > buf_len)
    {
        return -1;
    }

    memset(buf, 0, (size_t)packet_bytes);

    word0 = ((uint32_t)(pkt_type & 0x0F) << 28)
            | ((uint32_t)(class_id_flag ? 1u : 0u) << 27)
            | ((uint32_t)(trailer_flag ? 1u : 0u) << 26)
            /* VITA-49: TSI at bits 23-22, TSF at 21-20. This builder used to
             * write them two bits high, matching a parser that read them the
             * same way, so the pair round-tripped a shared mistake. */
            | ((uint32_t)(tsi & 0x03) << 22)
            | ((uint32_t)(tsf & 0x03) << 20)
            | ((uint32_t)(count & 0x0F) << 16)
            | ((uint32_t)packet_words & 0xFFFF);

    words[0] = htonl(word0);
    words[1] = htonl(stream_id);
    words[2] = htonl((uint32_t)(class_id >> 32));
    words[3] = htonl((uint32_t)(class_id & 0xFFFFFFFF));
    words[4] = htonl(ts_int);
    words[5] = htonl((uint32_t)(ts_frac >> 32));
    words[6] = htonl((uint32_t)(ts_frac & 0xFFFFFFFF));

    return packet_bytes;
}


/* ================================================================== */
/* smartsdr_gap_from_counter                                           */
/* ================================================================== */

/* An unbroken sequence advances by one and reports nothing missing. */
void test_gap_counter_consecutive_is_zero(void)
{
    int prev;

    for (prev = 0; prev < 16; prev++)
    {
        uint8_t cur = (uint8_t)((prev + 1) & 0x0F);

        TEST_CHECK(smartsdr_gap_from_counter((uint8_t)prev, cur) == 0);
        TEST_MSG("prev=%d cur=%u reported %u lost", prev, cur,
                 smartsdr_gap_from_counter((uint8_t)prev, cur));
    }
}


/* The counter is four bits wide, so 15 is followed by 0 with nothing lost. */
void test_gap_counter_wrap_15_to_0(void)
{
    TEST_CHECK(smartsdr_gap_from_counter(15, 0) == 0);
    TEST_MSG("15 -> 0 reported %u lost", smartsdr_gap_from_counter(15, 0));

    /* And one packet lost across the wrap is one, not fifteen. */
    TEST_CHECK(smartsdr_gap_from_counter(15, 1) == 1);
    TEST_MSG("15 -> 1 reported %u lost", smartsdr_gap_from_counter(15, 1));

    TEST_CHECK(smartsdr_gap_from_counter(14, 0) == 1);
    TEST_MSG("14 -> 0 reported %u lost", smartsdr_gap_from_counter(14, 0));
}


/* Every loss from 1 to 15 packets is reported exactly, from every start. */
void test_gap_counter_reports_1_to_15(void)
{
    int prev;
    int lost;

    for (prev = 0; prev < 16; prev++)
    {
        for (lost = 1; lost <= 15; lost++)
        {
            uint8_t cur = (uint8_t)((prev + 1 + lost) & 0x0F);
            unsigned got = smartsdr_gap_from_counter((uint8_t)prev, cur);

            TEST_CHECK(got == (unsigned)lost);
            TEST_MSG("prev=%d lost=%d cur=%u reported %u", prev, lost, cur, got);
        }
    }
}


/* A loss of exactly 16 brings the counter back to the value it would have had
 * anyway, so it reads as no loss at all. This is a property of a 4-bit field,
 * not something the function can see through, and it is asserted here so the
 * limit is recorded rather than assumed away. */
void test_gap_counter_blind_to_multiple_of_16(void)
{
    int prev;

    for (prev = 0; prev < 16; prev++)
    {
        uint8_t cur16 = (uint8_t)((prev + 1 + 16) & 0x0F);
        uint8_t cur32 = (uint8_t)((prev + 1 + 32) & 0x0F);

        TEST_CHECK(smartsdr_gap_from_counter((uint8_t)prev, cur16) == 0);
        TEST_MSG("prev=%d, 16 lost, reported %u", prev,
                 smartsdr_gap_from_counter((uint8_t)prev, cur16));

        TEST_CHECK(smartsdr_gap_from_counter((uint8_t)prev, cur32) == 0);
        TEST_MSG("prev=%d, 32 lost, reported %u", prev,
                 smartsdr_gap_from_counter((uint8_t)prev, cur32));
    }

    /* 17 lost is one past the blind spot and is seen as one. */
    TEST_CHECK(smartsdr_gap_from_counter(0, (uint8_t)((0 + 1 + 17) & 0x0F)) == 1);
}


/* ================================================================== */
/* smartsdr_parse_fft_header                                           */
/* ================================================================== */

void test_fft_header_valid(void)
{
    uint8_t payload[256];
    struct smartsdr_fft_header hdr;
    int len = build_fft_payload(payload, sizeof(payload),
                                0, 50, 12, 100, 0x12345678, 112);

    TEST_CHECK(len == 112);

    memset(&hdr, 0, sizeof(hdr));
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == 0);

    TEST_CHECK(hdr.start_bin == 0);
    TEST_MSG("start_bin=%u expected 0", hdr.start_bin);
    TEST_CHECK(hdr.num_bins == 50);
    TEST_MSG("num_bins=%u expected 50", hdr.num_bins);
    TEST_CHECK(hdr.bin_size == 12);
    TEST_MSG("bin_size=%u expected 12", hdr.bin_size);
    TEST_CHECK(hdr.total_bins == 100);
    TEST_MSG("total_bins=%u expected 100", hdr.total_bins);
    TEST_CHECK(hdr.frame == 0x12345678u);
    TEST_MSG("frame=0x%x expected 0x12345678", hdr.frame);
}


/* Every field is big-endian, so a value with distinct high and low bytes must
 * come back the right way round rather than byte-swapped. */
void test_fft_header_big_endian_fields(void)
{
    uint8_t payload[4096];
    struct smartsdr_fft_header hdr;
    int len = build_fft_payload(payload, sizeof(payload),
                                0x0102, 0x0304, 0x0506, 0x0F00,
                                0x0A0B0C0Du, 12 + 0x0304 * 2);

    TEST_CHECK(len > 0);

    memset(&hdr, 0, sizeof(hdr));
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == 0);

    TEST_CHECK(hdr.start_bin == 0x0102u);
    TEST_MSG("start_bin=0x%x expected 0x0102", hdr.start_bin);
    TEST_CHECK(hdr.num_bins == 0x0304u);
    TEST_MSG("num_bins=0x%x expected 0x0304", hdr.num_bins);
    TEST_CHECK(hdr.bin_size == 0x0506u);
    TEST_MSG("bin_size=0x%x expected 0x0506", hdr.bin_size);
    TEST_CHECK(hdr.total_bins == 0x0F00u);
    TEST_MSG("total_bins=0x%x expected 0x0F00", hdr.total_bins);
    TEST_CHECK(hdr.frame == 0x0A0B0C0Du);
    TEST_MSG("frame=0x%x expected 0x0A0B0C0D", hdr.frame);
}


/* The header is 12 bytes, so 11 cannot hold one. */
void test_fft_header_rejects_short_payload(void)
{
    uint8_t payload[64];
    struct smartsdr_fft_header hdr;
    int n;

    build_fft_payload(payload, sizeof(payload), 0, 1, 12, 100, 1, 14);

    for (n = 0; n < 12; n++)
    {
        TEST_CHECK(smartsdr_parse_fft_header(payload, n, &hdr) == -1);
        TEST_MSG("payload_bytes=%d was accepted", n);
    }

    /* 12 bytes is a header with no bins, which claims one bin it cannot hold. */
    TEST_CHECK(smartsdr_parse_fft_header(payload, 12, &hdr) == -1);
    TEST_MSG("a bare 12-byte header claiming 1 bin was accepted");

    /* 14 bytes is that header plus the one bin it claims. */
    TEST_CHECK(smartsdr_parse_fft_header(payload, 14, &hdr) == 0);
    TEST_MSG("12-byte header plus its one bin was rejected");
}


void test_fft_header_rejects_null(void)
{
    uint8_t payload[64];
    struct smartsdr_fft_header hdr;

    build_fft_payload(payload, sizeof(payload), 0, 1, 12, 100, 1, 14);

    TEST_CHECK(smartsdr_parse_fft_header(NULL, 14, &hdr) == -1);
    TEST_CHECK(smartsdr_parse_fft_header(payload, 14, NULL) == -1);
}


/* A frame of no bins carries nothing and must not be treated as a frame. */
void test_fft_header_rejects_zero_num_bins(void)
{
    uint8_t payload[256];
    struct smartsdr_fft_header hdr;
    int len = build_fft_payload(payload, sizeof(payload),
                                0, 0, 12, 100, 7, 112);

    TEST_CHECK(len > 0);
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == -1);
    TEST_MSG("num_bins=0 was accepted");
}


/* total_bins is the divisor the reassembly works against, so zero is fatal. */
void test_fft_header_rejects_zero_total_bins(void)
{
    uint8_t payload[256];
    struct smartsdr_fft_header hdr;
    int len = build_fft_payload(payload, sizeof(payload),
                                0, 50, 12, 0, 7, 112);

    TEST_CHECK(len > 0);
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == -1);
    TEST_MSG("total_bins=0 was accepted");
}


/* The bin store is fixed, so a frame claiming more bins than it holds is
 * refused rather than written past the end of the array. */
void test_fft_header_rejects_total_bins_over_max(void)
{
    static uint8_t payload[16384];
    struct smartsdr_fft_header hdr;
    int len;

    len = build_fft_payload(payload, sizeof(payload), 0, 10, 12,
                            SMARTSDR_MAX_SPECTRUM_BINS + 1, 7, 12 + 10 * 2);
    TEST_CHECK(len > 0);
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == -1);
    TEST_MSG("total_bins=%d was accepted", SMARTSDR_MAX_SPECTRUM_BINS + 1);

    /* Exactly the maximum is still a frame the backend can hold. */
    len = build_fft_payload(payload, sizeof(payload), 0, 10, 12,
                            SMARTSDR_MAX_SPECTRUM_BINS, 7, 12 + 10 * 2);
    TEST_CHECK(len > 0);
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == 0);
    TEST_MSG("total_bins=%d was rejected", SMARTSDR_MAX_SPECTRUM_BINS);
}


/* A fragment must fit inside the frame it belongs to. */
void test_fft_header_rejects_bins_past_total(void)
{
    uint8_t payload[512];
    struct smartsdr_fft_header hdr;
    int len;

    /* start 60 + 50 bins = 110, past a 100-bin frame. */
    len = build_fft_payload(payload, sizeof(payload), 60, 50, 12, 100, 7,
                            12 + 50 * 2);
    TEST_CHECK(len > 0);
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == -1);
    TEST_MSG("start_bin=60 num_bins=50 total_bins=100 was accepted");

    /* Ending exactly on the last bin is the final fragment of a frame. */
    len = build_fft_payload(payload, sizeof(payload), 50, 50, 12, 100, 7,
                            12 + 50 * 2);
    TEST_CHECK(len > 0);
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == 0);
    TEST_MSG("start_bin=50 num_bins=50 total_bins=100 was rejected");
}


/* The header may claim more bins than the datagram actually carried. */
void test_fft_header_rejects_payload_short_for_bins(void)
{
    uint8_t payload[512];
    struct smartsdr_fft_header hdr;
    int i;

    build_fft_payload(payload, sizeof(payload), 0, 50, 12, 100, 7, 112);

    for (i = 12; i < 112; i++)
    {
        TEST_CHECK(smartsdr_parse_fft_header(payload, i, &hdr) == -1);
        TEST_MSG("50 bins accepted from a %d-byte payload", i);
    }

    TEST_CHECK(smartsdr_parse_fft_header(payload, 112, &hdr) == 0);
    TEST_MSG("50 bins rejected from the 112 bytes that hold them");
}


/* The radio pads a frame out past its bins. A 50-bin fragment needs
 * 12 + 50 * 2 = 112 bytes and arrives in 116, so the four trailing bytes are
 * padding: they must neither be counted as bins nor make the frame invalid. */
void test_fft_header_boundary_and_padding(void)
{
    uint8_t payload[256];
    struct smartsdr_fft_header hdr;
    int len = build_fft_payload(payload, sizeof(payload),
                                0, 50, 12, 100, 9, 116);

    TEST_CHECK(len == 116);

    /* The bins end at 112; whatever follows is not part of them. */
    payload[112] = 0xDE;
    payload[113] = 0xAD;
    payload[114] = 0xBE;
    payload[115] = 0xEF;

    memset(&hdr, 0, sizeof(hdr));
    TEST_CHECK(smartsdr_parse_fft_header(payload, len, &hdr) == 0);
    TEST_CHECK(hdr.num_bins == 50);
    TEST_MSG("num_bins=%u expected 50", hdr.num_bins);

    /* The last bin is the one at offset 12 + 49 * 2, not the padding. */
    TEST_CHECK(12 + (int)hdr.num_bins * 2 == 112);
    TEST_MSG("bins end at %d, expected 112", 12 + (int)hdr.num_bins * 2);
}


/* ================================================================== */
/* smartsdr_fft_bin_to_level                                           */
/* ================================================================== */

/* Bin values are heights within the panadapter, 0 at the top where the signal
 * is strongest, and Hamlib reports larger as stronger, so the scale inverts. */
void test_fft_bin_to_level_inverts(void)
{
    TEST_CHECK(smartsdr_fft_bin_to_level(0, 255) == 255);
    TEST_MSG("value 0 of 255 became %u, expected 255",
             smartsdr_fft_bin_to_level(0, 255));

    TEST_CHECK(smartsdr_fft_bin_to_level(255, 255) == 0);
    TEST_MSG("value 255 of 255 became %u, expected 0",
             smartsdr_fft_bin_to_level(255, 255));

    TEST_CHECK(smartsdr_fft_bin_to_level(0, 700) == 255);
    TEST_MSG("value 0 of 700 became %u, expected 255",
             smartsdr_fft_bin_to_level(0, 700));

    TEST_CHECK(smartsdr_fft_bin_to_level(700, 700) == 0);
    TEST_MSG("value 700 of 700 became %u, expected 0",
             smartsdr_fft_bin_to_level(700, 700));
}


/* A bin below the bottom of the panadapter is the weakest signal, not a value
 * that wraps round to a strong one. */
void test_fft_bin_to_level_clamps(void)
{
    TEST_CHECK(smartsdr_fft_bin_to_level(256, 255) == 0);
    TEST_MSG("value 256 of 255 became %u, expected 0",
             smartsdr_fft_bin_to_level(256, 255));

    TEST_CHECK(smartsdr_fft_bin_to_level(100000, 700) == 0);
    TEST_MSG("value 100000 of 700 became %u, expected 0",
             smartsdr_fft_bin_to_level(100000, 700));
}


/* A panadapter whose height has not been reported yet falls back to the
 * default scale rather than dividing by zero. */
void test_fft_bin_to_level_zero_height_default(void)
{
    TEST_CHECK(smartsdr_fft_bin_to_level(0, 0) == 255);
    TEST_MSG("value 0 of height 0 became %u, expected 255",
             smartsdr_fft_bin_to_level(0, 0));

    TEST_CHECK(smartsdr_fft_bin_to_level(SMARTSDR_SPECTRUM_YPIXELS, 0) == 0);
    TEST_MSG("value %d of height 0 became %u, expected 0",
             SMARTSDR_SPECTRUM_YPIXELS,
             smartsdr_fft_bin_to_level(SMARTSDR_SPECTRUM_YPIXELS, 0));

    /* The fallback is the default height, so it scales like one. */
    TEST_CHECK(smartsdr_fft_bin_to_level(100, 0)
               == smartsdr_fft_bin_to_level(100, SMARTSDR_SPECTRUM_YPIXELS));
}


void test_fft_bin_to_level_midscale(void)
{
    /* Halfway down a 200-pixel panadapter. */
    TEST_CHECK(smartsdr_fft_bin_to_level(100, 200) == 127);
    TEST_MSG("value 100 of 200 became %u, expected 127",
             smartsdr_fft_bin_to_level(100, 200));

    /* A quarter down is three quarters of full scale. */
    TEST_CHECK(smartsdr_fft_bin_to_level(100, 400) == 191);
    TEST_MSG("value 100 of 400 became %u, expected 191",
             smartsdr_fft_bin_to_level(100, 400));

    /* The level falls as the bin value rises, over the whole range. */
    {
        unsigned v;
        int prev = 256;

        for (v = 0; v <= 700; v += 7)
        {
            int lvl = (int)smartsdr_fft_bin_to_level(v, 700);

            TEST_CHECK(lvl <= prev);
            TEST_MSG("value %u gave %d after %d", v, lvl, prev);
            prev = lvl;
        }
    }
}


/* ================================================================== */
/* smartsdr_meter_divisor                                              */
/* ================================================================== */

/* The divisor follows the meter's unit: dBm, dBFS and SWR are in eighths of a
 * dB, volts and amps in 1/256, and the PA temperature in 1/64. */
void test_meter_divisor_per_slot(void)
{
    TEST_CHECK(smartsdr_meter_divisor(SMARTSDR_MTR_LEVEL) == 128.0);
    TEST_MSG("LEVEL divisor %f", smartsdr_meter_divisor(SMARTSDR_MTR_LEVEL));

    TEST_CHECK(smartsdr_meter_divisor(SMARTSDR_MTR_SWR) == 128.0);
    TEST_MSG("SWR divisor %f", smartsdr_meter_divisor(SMARTSDR_MTR_SWR));

    TEST_CHECK(smartsdr_meter_divisor(SMARTSDR_MTR_ALC) == 128.0);
    TEST_MSG("ALC divisor %f", smartsdr_meter_divisor(SMARTSDR_MTR_ALC));

    TEST_CHECK(smartsdr_meter_divisor(SMARTSDR_MTR_FWDPWR) == 128.0);
    TEST_MSG("FWDPWR divisor %f", smartsdr_meter_divisor(SMARTSDR_MTR_FWDPWR));

    TEST_CHECK(smartsdr_meter_divisor(SMARTSDR_MTR_VOLTS) == 256.0);
    TEST_MSG("VOLTS divisor %f", smartsdr_meter_divisor(SMARTSDR_MTR_VOLTS));

    TEST_CHECK(smartsdr_meter_divisor(SMARTSDR_MTR_AMPS) == 256.0);
    TEST_MSG("AMPS divisor %f", smartsdr_meter_divisor(SMARTSDR_MTR_AMPS));

    TEST_CHECK(smartsdr_meter_divisor(SMARTSDR_MTR_PATEMP) == 64.0);
    TEST_MSG("PATEMP divisor %f", smartsdr_meter_divisor(SMARTSDR_MTR_PATEMP));
}


/* Readings taken from hardware, which is where these divisors came from: an
 * SWR of exactly 1.00 with no carrier, a 14.03 V supply, an idle PA at
 * 27.6 degC and a quiet band at -99.2 dBm. */
void test_meter_divisor_decodes_hardware_readings(void)
{
    double swr = 128.0 / smartsdr_meter_divisor(SMARTSDR_MTR_SWR);
    double volts = 3591.0 / smartsdr_meter_divisor(SMARTSDR_MTR_VOLTS);
    double degc = 1767.0 / smartsdr_meter_divisor(SMARTSDR_MTR_PATEMP);
    double dbm = -12696.0 / smartsdr_meter_divisor(SMARTSDR_MTR_LEVEL);

    TEST_CHECK(fabs(swr - 1.00) < 0.005);
    TEST_MSG("raw 128 decoded to SWR %.3f, expected 1.00", swr);

    TEST_CHECK(fabs(volts - 14.03) < 0.005);
    TEST_MSG("raw 3591 decoded to %.3f V, expected 14.03", volts);

    TEST_CHECK(fabs(degc - 27.6) < 0.05);
    TEST_MSG("raw 1767 decoded to %.3f degC, expected 27.6", degc);

    TEST_CHECK(fabs(dbm - (-99.2)) < 0.05);
    TEST_MSG("raw -12696 decoded to %.3f dBm, expected -99.2", dbm);
}


/* Meters the backend does not name are still scaled, as the dB meters are. */
void test_meter_divisor_unknown_slot_is_db(void)
{
    TEST_CHECK(smartsdr_meter_divisor(SMARTSDR_MTR_COUNT) == 128.0);
    TEST_CHECK(smartsdr_meter_divisor(-1) == 128.0);
}


/* ================================================================== */
/* vita49_parse_header                                                 */
/* ================================================================== */

void test_vita_parse_extracts_every_field(void)
{
    uint8_t pkt[256];
    struct vita49_header hdr;
    uint64_t class_id = ((uint64_t)0x00001C2Du << 16) | VITA49_PCC_PAN_FFT;
    int len = build_vita_packet(pkt, sizeof(pkt),
                                VITA49_PKT_TYPE_EXT_DATA, 1, 1, 3, 2, 11,
                                0xDEADBEEFu, class_id,
                                0x11223344u, 0x0102030405060708ULL, 4);

    TEST_CHECK(len == (VITA49_HEADER_WORDS + 4) * 4);

    memset(&hdr, 0, sizeof(hdr));
    TEST_CHECK(vita49_parse_header(pkt, len, &hdr) == 0);

    TEST_CHECK(hdr.packet_type == VITA49_PKT_TYPE_EXT_DATA);
    TEST_MSG("packet_type=%u expected %u", hdr.packet_type,
             VITA49_PKT_TYPE_EXT_DATA);
    TEST_CHECK(hdr.class_id_flag == 1);
    TEST_CHECK(hdr.trailer_flag == 1);
    TEST_MSG("trailer_flag=%u expected 1", hdr.trailer_flag);
    TEST_CHECK(hdr.tsi == 3);
    TEST_MSG("tsi=%u expected 3", hdr.tsi);
    TEST_CHECK(hdr.tsf == 2);
    TEST_MSG("tsf=%u expected 2", hdr.tsf);
    TEST_CHECK(hdr.packet_count == 11);
    TEST_MSG("packet_count=%u expected 11", hdr.packet_count);
    TEST_CHECK(hdr.packet_size == VITA49_HEADER_WORDS + 4);
    TEST_MSG("packet_size=%u expected %d", hdr.packet_size,
             VITA49_HEADER_WORDS + 4);
    TEST_CHECK(hdr.stream_id == 0xDEADBEEFu);
    TEST_MSG("stream_id=0x%x expected 0xDEADBEEF", hdr.stream_id);
    TEST_CHECK(hdr.class_id == class_id);
    TEST_CHECK(hdr.pcc == VITA49_PCC_PAN_FFT);
    TEST_MSG("pcc=0x%x expected 0x%x", hdr.pcc, VITA49_PCC_PAN_FFT);
    TEST_CHECK(hdr.timestamp_int == 0x11223344u);
    TEST_MSG("timestamp_int=0x%x expected 0x11223344", hdr.timestamp_int);
    TEST_CHECK(hdr.timestamp_frac == 0x0102030405060708ULL);
}


/* Every fixed word of the header must be present before any of it is read. */
void test_vita_parse_rejects_short_buffers(void)
{
    uint8_t pkt[256];
    struct vita49_header hdr;
    int n;

    build_vita_packet(pkt, sizeof(pkt), VITA49_PKT_TYPE_IF_DATA, 1, 0, 1, 1, 0,
                      0x1234u, 0, 0, 0, 1);

    for (n = 0; n < VITA49_HEADER_BYTES; n++)
    {
        TEST_CHECK(vita49_parse_header(pkt, n, &hdr) == -1);
        TEST_MSG("a %d-byte buffer was accepted", n);
    }

    TEST_CHECK(vita49_parse_header(pkt, VITA49_HEADER_BYTES, &hdr) == 0);
    TEST_MSG("a bare %d-byte header was rejected", VITA49_HEADER_BYTES);

    TEST_CHECK(vita49_parse_header(NULL, VITA49_HEADER_BYTES, &hdr) == -1);
    TEST_CHECK(vita49_parse_header(pkt, VITA49_HEADER_BYTES, NULL) == -1);
}


/* Without the class-ID flag those two words are not a class ID and must not be
 * read as one: the packet class code decides how the payload is decoded. */
void test_vita_parse_without_class_id(void)
{
    uint8_t pkt[256];
    struct vita49_header hdr;
    int len = build_vita_packet(pkt, sizeof(pkt), VITA49_PKT_TYPE_IF_DATA,
                                0, 0, 0, 0, 0, 0x4001u,
                                0xFFFFFFFFFFFFFFFFULL, 0, 0, 1);

    TEST_CHECK(len > 0);
    TEST_CHECK(vita49_parse_header(pkt, len, &hdr) == 0);
    TEST_CHECK(hdr.class_id_flag == 0);
    TEST_CHECK(hdr.class_id == 0);
    TEST_MSG("class_id=0x%llx expected 0", (unsigned long long)hdr.class_id);
    TEST_CHECK(hdr.pcc == 0);
    TEST_MSG("pcc=0x%x expected 0", hdr.pcc);
}


/* ================================================================== */
/* vita49_build_tx_packet                                              */
/* ================================================================== */

void test_vita_build_tx_header_and_length(void)
{
    float samples[8] = { 0.5f, -0.5f, 0.25f, -0.25f,
                         0.125f, -0.125f, 1.0f, -1.0f
                       };
    uint8_t pkt[256];
    uint64_t class_id = ((uint64_t)0x00001C2Du << 16) | VITA49_PCC_AUDIO_F32;
    struct vita49_header hdr;
    int len = vita49_build_tx_packet(pkt, sizeof(pkt), 0xCAFEBABEu, class_id,
                                     6, samples, 8);

    TEST_CHECK(len == VITA49_HEADER_BYTES + 8 * (int)sizeof(float));
    TEST_MSG("len=%d expected %d", len,
             VITA49_HEADER_BYTES + 8 * (int)sizeof(float));

    TEST_CHECK(vita49_parse_header(pkt, len, &hdr) == 0);
    TEST_CHECK(hdr.packet_type == VITA49_PKT_TYPE_IF_DATA);
    TEST_CHECK(hdr.class_id_flag == 1);
    TEST_CHECK(hdr.trailer_flag == 0);
    TEST_CHECK(hdr.packet_count == 6);
    TEST_CHECK(hdr.packet_size == VITA49_HEADER_WORDS + 8);
    TEST_CHECK(hdr.stream_id == 0xCAFEBABEu);
    TEST_CHECK(hdr.class_id == class_id);
    TEST_CHECK(hdr.pcc == VITA49_PCC_AUDIO_F32);

    /* TX timestamps are not sent; the radio plays the samples out as they
     * arrive. */
    TEST_CHECK(hdr.tsi == 0);
    TEST_CHECK(hdr.tsf == 0);
    TEST_CHECK(hdr.timestamp_int == 0);
    TEST_CHECK(hdr.timestamp_frac == 0);
}


/* What was built must parse back to what went in, header and payload alike. */
void test_vita_build_tx_roundtrips(void)
{
    float original[8] = { 1.0f, -1.0f, 0.0f, 0.5f,
                          -0.5f, 0.001f, -0.001f, 3.14159f
                        };
    float result[8];
    uint8_t pkt[256];
    uint64_t class_id = ((uint64_t)0x00001C2Du << 16) | VITA49_PCC_AUDIO_F32;
    struct vita49_header hdr;
    int len = vita49_build_tx_packet(pkt, sizeof(pkt), 0x04000001u, class_id,
                                     0, original, 8);
    int i;

    TEST_CHECK(len > 0);
    TEST_CHECK(vita49_parse_header(pkt, len, &hdr) == 0);
    TEST_CHECK(hdr.stream_id == 0x04000001u);

    vita49_swap_float32(pkt + VITA49_HEADER_BYTES, result, 8);

    for (i = 0; i < 8; i++)
    {
        TEST_CHECK(result[i] == original[i]);
        TEST_MSG("sample %d: got %f expected %f", i, result[i], original[i]);
    }
}


void test_vita_build_tx_standard_frame_size(void)
{
    static float samples[SMARTSDR_TX_SAMPLES];
    static uint8_t pkt[SMARTSDR_TX_PACKET_BYTES + 64];
    int len;

    memset(samples, 0, sizeof(samples));
    len = vita49_build_tx_packet(pkt, sizeof(pkt), 0x42u, 0, 15,
                                 samples, SMARTSDR_TX_SAMPLES);

    TEST_CHECK(len == SMARTSDR_TX_PACKET_BYTES);
    TEST_MSG("len=%d expected %d", len, SMARTSDR_TX_PACKET_BYTES);
}


void test_vita_build_tx_rejects_bad_arguments(void)
{
    float samples[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    uint8_t small[16];
    uint8_t pkt[256];

    TEST_CHECK(vita49_build_tx_packet(small, sizeof(small), 0, 0, 0,
                                      samples, 4) == -1);
    TEST_MSG("a 16-byte buffer took a 44-byte packet");

    TEST_CHECK(vita49_build_tx_packet(NULL, 256, 0, 0, 0, samples, 4) == -1);
    TEST_CHECK(vita49_build_tx_packet(pkt, 256, 0, 0, 0, NULL, 4) == -1);
    TEST_CHECK(vita49_build_tx_packet(pkt, 256, 0, 0, 0, samples, 0) == -1);
    TEST_CHECK(vita49_build_tx_packet(pkt, 256, 0, 0, 0, samples, -1) == -1);
}


/* The counter is four bits on the wire, so anything wider is masked. */
void test_vita_build_tx_masks_packet_count(void)
{
    float samples[1] = { 0.0f };
    uint8_t pkt[64];
    struct vita49_header hdr;
    int len = vita49_build_tx_packet(pkt, sizeof(pkt), 0, 0, 0xFF, samples, 1);

    TEST_CHECK(len > 0);
    TEST_CHECK(vita49_parse_header(pkt, len, &hdr) == 0);
    TEST_CHECK(hdr.packet_count == 0x0F);
    TEST_MSG("packet_count=%u expected 15", hdr.packet_count);
}


/* ================================================================== */
/* vita49_swap_float32                                                 */
/* ================================================================== */

/* The wire is big-endian. A known bit pattern proves the bytes are reversed
 * rather than merely copied. */
void test_swap_float32_byte_order(void)
{
    /* 0x3F800000 is 1.0f; on the wire that is 3F 80 00 00. */
    static const uint8_t wire[8] =
    {
        0x3F, 0x80, 0x00, 0x00,     /*  1.0f */
        0xBF, 0x00, 0x00, 0x00      /* -0.5f */
    };
    float out[2] = { 999.0f, 999.0f };

    vita49_swap_float32(wire, out, 2);

    TEST_CHECK(out[0] == 1.0f);
    TEST_MSG("first sample %f, expected 1.0", out[0]);
    TEST_CHECK(out[1] == -0.5f);
    TEST_MSG("second sample %f, expected -0.5", out[1]);
}


void test_swap_float32_roundtrip_and_guards(void)
{
    float original[5] = { 1.0f, -1.0f, 0.0f, 3.14159f, -0.001f };
    uint32_t wire[5];
    float result[5];
    int i;

    for (i = 0; i < 5; i++)
    {
        uint32_t raw;
        memcpy(&raw, &original[i], sizeof(float));
        wire[i] = htonl(raw);
    }

    vita49_swap_float32((const uint8_t *)wire, result, 5);

    for (i = 0; i < 5; i++)
    {
        TEST_CHECK(result[i] == original[i]);
        TEST_MSG("sample %d: got %f expected %f", i, result[i], original[i]);
    }

    /* A run of no samples must leave the destination alone. */
    result[0] = 42.0f;
    vita49_swap_float32((const uint8_t *)wire, result, 0);
    TEST_CHECK(result[0] == 42.0f);

    vita49_swap_float32(NULL, result, 5);
    TEST_CHECK(result[0] == 42.0f);

    vita49_swap_float32((const uint8_t *)wire, NULL, 5);
}


/* ================================================================== */
/* vita49_convert_s16_to_f32_stereo                                    */
/* ================================================================== */

/* Reduced-bandwidth DAX audio is big-endian mono int16, and the frontend takes
 * float32 stereo, so each sample is scaled and written to both channels. */
void test_convert_s16_duplicates_mono_to_stereo(void)
{
    static const int16_t mono[5] = { 0, 16384, -16384, 32767, -32768 };
    static const float expected[5] =
    {
        0.0f, 0.5f, -0.5f, 32767.0f / 32768.0f, -1.0f
    };
    uint8_t wire[10];
    float stereo[10];
    int i;

    for (i = 0; i < 5; i++)
    {
        wire[i * 2]     = (uint8_t)(((uint16_t)mono[i] >> 8) & 0xFF);
        wire[i * 2 + 1] = (uint8_t)((uint16_t)mono[i] & 0xFF);
    }

    vita49_convert_s16_to_f32_stereo(wire, stereo, 5);

    for (i = 0; i < 5; i++)
    {
        TEST_CHECK(fabsf(stereo[i * 2] - expected[i]) < 1e-6f);
        TEST_MSG("L[%d]=%f expected %f", i, stereo[i * 2], expected[i]);

        TEST_CHECK(stereo[i * 2] == stereo[i * 2 + 1]);
        TEST_MSG("L[%d]=%f but R[%d]=%f", i, stereo[i * 2], i,
                 stereo[i * 2 + 1]);
    }
}


/* Byte order matters: 0x0100 read the wrong way round is 1, not 256. */
void test_convert_s16_byte_order(void)
{
    static const uint8_t wire[2] = { 0x01, 0x00 };   /* 256 big-endian */
    float stereo[2] = { 999.0f, 999.0f };

    vita49_convert_s16_to_f32_stereo(wire, stereo, 1);

    TEST_CHECK(fabsf(stereo[0] - (256.0f / 32768.0f)) < 1e-7f);
    TEST_MSG("got %f, expected %f", stereo[0], 256.0f / 32768.0f);
}


void test_convert_s16_guards(void)
{
    static const uint8_t wire[2] = { 0x01, 0x00 };
    float stereo[2] = { 42.0f, 42.0f };

    vita49_convert_s16_to_f32_stereo(wire, stereo, 0);
    TEST_CHECK(stereo[0] == 42.0f);

    vita49_convert_s16_to_f32_stereo(NULL, stereo, 1);
    TEST_CHECK(stereo[0] == 42.0f);

    vita49_convert_s16_to_f32_stereo(wire, NULL, 1);
}


/* ================================================================== */
/* Test list                                                           */
/* ================================================================== */

/* DAX I/Q is the one payload the radio sends little-endian, and its floats hold
 * left-justified fixed point, so full scale is 32768 rather than 1.0. Getting
 * either half wrong is silent: the byte order turns samples into denormals that
 * print as 0.000000, and the scale leaves them ~90 dB too loud. */
void test_daxiq_is_little_endian_and_normalised(void)
{
    /* -64.0f and 32768.0f as little-endian float32. */
    const uint8_t wire[8] =
    {
        0x00, 0x00, 0x80, 0xc2,     /* -64.0f  */
        0x00, 0x00, 0x00, 0x47      /* 32768.0f = full scale */
    };
    float out[2] = { 9.0f, 9.0f };

    vita49_convert_daxiq_float32(wire, out, 2);

    TEST_CHECK(out[0] == -64.0f / 32768.0f);
    TEST_MSG("got %.9f, want %.9f", out[0], -64.0f / 32768.0f);

    /* Full scale must land exactly on 1.0, which is what the API promises. */
    TEST_CHECK(out[1] == 1.0f);
    TEST_MSG("full scale got %.9f, want 1.0", out[1]);
}

/* The same bytes through the big-endian path are denormal, which is what the
 * radio's I/Q looked like while it was being byte-swapped. */
void test_daxiq_bytes_read_big_endian_are_denormal(void)
{
    const uint8_t wire[4] = { 0x00, 0x00, 0x80, 0xc2 };
    float out = 9.0f;

    vita49_swap_float32(wire, &out, 1);

    TEST_CHECK(out != 0.0f && fabsf(out) < 1.0e-37f);
    TEST_MSG("expected a denormal, got %.9e", out);
}

/* Header bytes captured from a FLEX-8400M DAX audio packet. Reading the
 * timestamp selectors two bits high made every packet report TSI_NONE, and the
 * absence of a time of day looked like a property of the radio rather than of
 * this parser. The integer timestamp in the same packet is a real Unix epoch,
 * which is only meaningful if TSI says UTC. */
void test_header_timestamp_selectors_match_the_radio(void)
{
    static const uint8_t pkt[28] =
    {
        0x38, 0x50, 0x01, 0x07,   /* word0: type 3, class present, TSI/TSF */
        0x00, 0x00, 0x07, 0x00,   /* stream id */
        0x00, 0x00, 0x1C, 0x2D,   /* FlexRadio OUI */
        0x53, 0x4C, 0x03, 0xE3,   /* info class 0x534C, PCC 0x03E3 audio */
        0x6A, 0x72, 0x61, 0x20,   /* integer seconds: a Unix epoch */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x19, 0x01    /* fractional */
    };
    struct vita49_header hdr;

    TEST_ASSERT(vita49_parse_header(pkt, sizeof(pkt), &hdr) == 0);

    TEST_CHECK(hdr.pcc == 0x03E3);
    TEST_MSG("class code got 0x%04X", hdr.pcc);

    TEST_CHECK(hdr.tsi == 1);
    TEST_MSG("TSI got %u, expected 1 (UTC) -- bits 23-22 of word0", hdr.tsi);

    TEST_CHECK(hdr.tsf == 1);
    TEST_MSG("TSF got %u, expected 1 -- bits 21-20 of word0", hdr.tsf);

    /* Sanity: the seconds field really is an epoch, not a counter. */
    TEST_CHECK(hdr.timestamp_int > 1700000000u);
    TEST_MSG("integer timestamp %u does not look like a Unix epoch",
             hdr.timestamp_int);
}

TEST_LIST =
{
    { "header_timestamp_selectors_match_the_radio", test_header_timestamp_selectors_match_the_radio },
    { "daxiq_is_little_endian_and_normalised", test_daxiq_is_little_endian_and_normalised },
    { "daxiq_bytes_read_big_endian_are_denormal", test_daxiq_bytes_read_big_endian_are_denormal },
    { "gap_counter_consecutive_is_zero",      test_gap_counter_consecutive_is_zero },
    { "gap_counter_wrap_15_to_0",             test_gap_counter_wrap_15_to_0 },
    { "gap_counter_reports_1_to_15",          test_gap_counter_reports_1_to_15 },
    { "gap_counter_blind_to_multiple_of_16",  test_gap_counter_blind_to_multiple_of_16 },
    { "fft_header_valid",                     test_fft_header_valid },
    { "fft_header_big_endian_fields",         test_fft_header_big_endian_fields },
    { "fft_header_rejects_short_payload",     test_fft_header_rejects_short_payload },
    { "fft_header_rejects_null",              test_fft_header_rejects_null },
    { "fft_header_rejects_zero_num_bins",     test_fft_header_rejects_zero_num_bins },
    { "fft_header_rejects_zero_total_bins",   test_fft_header_rejects_zero_total_bins },
    { "fft_header_rejects_total_bins_over_max", test_fft_header_rejects_total_bins_over_max },
    { "fft_header_rejects_bins_past_total",   test_fft_header_rejects_bins_past_total },
    { "fft_header_rejects_payload_short_for_bins", test_fft_header_rejects_payload_short_for_bins },
    { "fft_header_boundary_and_padding",      test_fft_header_boundary_and_padding },
    { "fft_bin_to_level_inverts",             test_fft_bin_to_level_inverts },
    { "fft_bin_to_level_clamps",              test_fft_bin_to_level_clamps },
    { "fft_bin_to_level_zero_height_default", test_fft_bin_to_level_zero_height_default },
    { "fft_bin_to_level_midscale",            test_fft_bin_to_level_midscale },
    { "meter_divisor_per_slot",               test_meter_divisor_per_slot },
    { "meter_divisor_decodes_hardware_readings", test_meter_divisor_decodes_hardware_readings },
    { "meter_divisor_unknown_slot_is_db",     test_meter_divisor_unknown_slot_is_db },
    { "vita_parse_extracts_every_field",      test_vita_parse_extracts_every_field },
    { "vita_parse_rejects_short_buffers",     test_vita_parse_rejects_short_buffers },
    { "vita_parse_without_class_id",          test_vita_parse_without_class_id },
    { "vita_build_tx_header_and_length",      test_vita_build_tx_header_and_length },
    { "vita_build_tx_roundtrips",             test_vita_build_tx_roundtrips },
    { "vita_build_tx_standard_frame_size",    test_vita_build_tx_standard_frame_size },
    { "vita_build_tx_rejects_bad_arguments",  test_vita_build_tx_rejects_bad_arguments },
    { "vita_build_tx_masks_packet_count",     test_vita_build_tx_masks_packet_count },
    { "swap_float32_byte_order",              test_swap_float32_byte_order },
    { "swap_float32_roundtrip_and_guards",    test_swap_float32_roundtrip_and_guards },
    { "convert_s16_duplicates_mono_to_stereo", test_convert_s16_duplicates_mono_to_stereo },
    { "convert_s16_byte_order",               test_convert_s16_byte_order },
    { "convert_s16_guards",                   test_convert_s16_guards },
    { NULL, NULL }
};
