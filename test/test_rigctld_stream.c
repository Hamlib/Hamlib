/*
 *  Hamlib rigctld streaming protocol tests
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

/* Unit tests for rigctld UDP streaming protocol. */
/* Tests serialization, registry, config parsing, and UDP socket creation. */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"
/* Socket headers come from stream_proto.h (via rigctld_stream.h), which picks
 * the right set for the host; do not include them directly. */
#include "../tests/rigctld_stream.h"
#include "stream_net.h"
#include "stream_proto.h"
#include "stream_convert.h"
#include "kvparse.h"
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>

/* On Windows, Winsock initialization for the raw-socket tests below comes
 * from the constructor in rigctld_stream.c, which is linked into this
 * binary and runs before main(). */


/* --- Packet header tests --- */

void test_header_pack_unpack_roundtrip(void)
{
    struct rig_stream_packet_header orig =
    {
        .version = RIG_STREAM_PROTOCOL_VERSION,
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .stream_id = 42,
        .source_id = 0x1234,
        .seq = 12345678,
        .timestamp = 0x123456789ABCDEF0ULL,
        .sample_rate = 48000,
        .format = RIG_STREAM_FMT_ID_PCM_S16,
        .channels = 2,
        .control = 0,
        .payload_len = 1420,
    };

    unsigned char buf[RIG_STREAM_HEADER_SIZE];
    memset(buf, 0xAA, sizeof(
               buf));  /* Fill with garbage to detect partial writes */

    stream_packet_header_pack(&orig, buf);

    struct rig_stream_packet_header decoded;
    memset(&decoded, 0xFF, sizeof(decoded));

    int ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);

    TEST_CHECK(ret == 0);
    TEST_MSG("unpack returned %d", ret);

    TEST_CHECK(decoded.version == orig.version);
    TEST_MSG("version: got %u, expected %u", decoded.version, orig.version);

    TEST_CHECK(decoded.type == orig.type);
    TEST_MSG("type: got %u, expected %u", decoded.type, orig.type);

    TEST_CHECK(decoded.stream_id == orig.stream_id);
    TEST_MSG("stream_id: got %u, expected %u", decoded.stream_id, orig.stream_id);

    TEST_CHECK(decoded.source_id == orig.source_id);
    TEST_MSG("source_id: got %u, expected %u", decoded.source_id, orig.source_id);

    TEST_CHECK(decoded.seq == orig.seq);
    TEST_MSG("seq: got %u, expected %u", decoded.seq, orig.seq);

    TEST_CHECK(decoded.timestamp == orig.timestamp);
    TEST_MSG("timestamp: got 0x%llx, expected 0x%llx",
             (unsigned long long)decoded.timestamp,
             (unsigned long long)orig.timestamp);

    TEST_CHECK(decoded.sample_rate == orig.sample_rate);
    TEST_MSG("sample_rate: got %u, expected %u",
             decoded.sample_rate, orig.sample_rate);

    TEST_CHECK(decoded.format == orig.format);
    TEST_MSG("format: got %u, expected %u", decoded.format, orig.format);

    TEST_CHECK(decoded.channels == orig.channels);
    TEST_MSG("channels: got %u, expected %u", decoded.channels, orig.channels);

    TEST_CHECK(decoded.control == orig.control);
    TEST_MSG("control: got 0x%04x, expected 0x%04x",
             decoded.control, orig.control);

    TEST_CHECK(decoded.payload_len == orig.payload_len);
    TEST_MSG("payload_len: got %u, expected %u",
             decoded.payload_len, orig.payload_len);
}


void test_header_big_endian_byte_order(void)
{
    struct rig_stream_packet_header hdr =
    {
        .version = 1,
        .type = 2,
        .stream_id = 0x0102,
        .source_id = 0x0A0B,
        .subscribe_token = 0xDEADBEEF,
        .seq = 0x01020304,
        .timestamp = 0x0102030405060708ULL,
        .sample_rate = 0x0000BB80,  /* 48000 */
        .format = 4,
        .channels = 1,
        .control = 0x0003,
        .payload_len = 0x058C,  /* 1420 */
    };

    unsigned char buf[RIG_STREAM_HEADER_SIZE];
    stream_packet_header_pack(&hdr, buf);

    /* Verify byte-level layout */
    TEST_CHECK(buf[0] == 1);          /* version */
    TEST_MSG("buf[0] = 0x%02x", buf[0]);

    TEST_CHECK(buf[1] == 2);          /* type */
    TEST_MSG("buf[1] = 0x%02x", buf[1]);

    /* stream_id: 0x0102 big-endian at bytes 2-3 */
    TEST_CHECK(buf[2] == 0x01);
    TEST_CHECK(buf[3] == 0x02);
    TEST_MSG("stream_id bytes: 0x%02x 0x%02x", buf[2], buf[3]);

    /* subscribe_token: 0xDEADBEEF big-endian at bytes 4-7 */
    TEST_CHECK(buf[4] == 0xDE);
    TEST_CHECK(buf[5] == 0xAD);
    TEST_CHECK(buf[6] == 0xBE);
    TEST_CHECK(buf[7] == 0xEF);

    /* seq: 0x01020304 big-endian at bytes 8-11 */
    TEST_CHECK(buf[8]  == 0x01);
    TEST_CHECK(buf[9]  == 0x02);
    TEST_CHECK(buf[10] == 0x03);
    TEST_CHECK(buf[11] == 0x04);

    /* timestamp: 0x0102030405060708 big-endian at bytes 12-19 */
    TEST_CHECK(buf[12] == 0x01);
    TEST_CHECK(buf[13] == 0x02);
    TEST_CHECK(buf[14] == 0x03);
    TEST_CHECK(buf[15] == 0x04);
    TEST_CHECK(buf[16] == 0x05);
    TEST_CHECK(buf[17] == 0x06);
    TEST_CHECK(buf[18] == 0x07);
    TEST_CHECK(buf[19] == 0x08);

    /* sample_rate: 0x0000BB80 big-endian at bytes 20-23 (48000) */
    TEST_CHECK(buf[20] == 0x00);
    TEST_CHECK(buf[21] == 0x00);
    TEST_CHECK(buf[22] == 0xBB);
    TEST_CHECK(buf[23] == 0x80);
    TEST_MSG("sample_rate bytes: 0x%02x%02x%02x%02x",
             buf[20], buf[21], buf[22], buf[23]);

    TEST_CHECK(buf[24] == 4);         /* format */
    TEST_CHECK(buf[25] == 1);         /* channels */

    /* source_id: 0x0A0B big-endian at bytes 26-27 */
    TEST_CHECK(buf[26] == 0x0A);
    TEST_CHECK(buf[27] == 0x0B);
    TEST_MSG("source_id bytes: 0x%02x 0x%02x", buf[26], buf[27]);

    /* control: 0x0003 big-endian at bytes 28-29 */
    TEST_CHECK(buf[28] == 0x00);
    TEST_CHECK(buf[29] == 0x03);

    /* payload_len: 0x058C big-endian at bytes 30-31 (1420) */
    TEST_CHECK(buf[30] == 0x05);
    TEST_CHECK(buf[31] == 0x8C);
}


void test_source_id_derive(void)
{
    /* Deterministic: same static configuration yields the same ID */
    uint16_t id1 = stream_source_id_derive("host1", 4532, 1035, "/dev/ttyUSB0");
    uint16_t id2 = stream_source_id_derive("host1", 4532, 1035, "/dev/ttyUSB0");
    TEST_CHECK(id1 == id2);
    TEST_MSG("id1=0x%04x id2=0x%04x", id1, id2);

    /* Derived IDs stay in 0x1000-0xFFFF, never colliding with the manual
     * range or the unset value */
    TEST_CHECK(id1 >= 0x1000);
    TEST_MSG("id1=0x%04x below derived range", id1);

    /* Each input participates in the hash */
    uint16_t id_host = stream_source_id_derive("host2", 4532, 1035,
                       "/dev/ttyUSB0");
    uint16_t id_port = stream_source_id_derive("host1", 4533, 1035,
                       "/dev/ttyUSB0");
    uint16_t id_model = stream_source_id_derive("host1", 4532, 1036,
                        "/dev/ttyUSB0");
    uint16_t id_path = stream_source_id_derive("host1", 4532, 1035,
                       "/dev/ttyUSB1");
    TEST_CHECK(id_host != id1);
    TEST_MSG("hostname not hashed: 0x%04x", id_host);
    TEST_CHECK(id_port != id1);
    TEST_MSG("listen port not hashed: 0x%04x", id_port);
    TEST_CHECK(id_model != id1);
    TEST_MSG("model not hashed: 0x%04x", id_model);
    TEST_CHECK(id_path != id1);
    TEST_MSG("pathname not hashed: 0x%04x", id_path);

    /* NULL strings are tolerated (treated as empty) and still in range */
    uint16_t id_null = stream_source_id_derive(NULL, 4532, 1035, NULL);
    TEST_CHECK(id_null >= 0x1000);
    TEST_MSG("id_null=0x%04x", id_null);
}


void test_header_unpack_too_short(void)
{
    unsigned char buf[31];
    memset(buf, 0, sizeof(buf));
    buf[0] = 1;  /* valid version */

    struct rig_stream_packet_header hdr;
    int ret = stream_packet_header_unpack(buf, 31, &hdr);

    TEST_CHECK(ret == -1);
    TEST_MSG("unpack with 31 bytes returned %d (expected -1)", ret);
}


void test_header_unpack_bad_version(void)
{
    unsigned char buf[RIG_STREAM_HEADER_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 99;  /* invalid version */

    struct rig_stream_packet_header hdr;
    int ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &hdr);

    TEST_CHECK(ret == -1);
    TEST_MSG("unpack with version 99 returned %d (expected -1)", ret);
}


void test_header_unpack_unknown_type_format(void)
{
    /* Pack a valid header; corrupt the type byte (byte 1) and the format
     * byte (byte 24) on the wire and confirm unpack rejects each. */
    struct rig_stream_packet_header hdr =
    {
        .version = RIG_STREAM_PROTOCOL_VERSION,
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .stream_id = 1,
        .format = RIG_STREAM_FMT_ID_PCM_S16,
        .channels = 1,
    };

    unsigned char buf[RIG_STREAM_HEADER_SIZE];
    stream_packet_header_pack(&hdr, buf);

    struct rig_stream_packet_header decoded;

    /* Baseline: valid type+format unpacks cleanly. */
    int ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);
    TEST_CHECK(ret == 0);
    TEST_MSG("valid header unpack returned %d (expected 0)", ret);

    /* Corrupt type byte to the sentinel (>= RIG_STREAM_TYPE_COUNT). */
    buf[1] = RIG_STREAM_TYPE_COUNT;
    ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);
    TEST_CHECK(ret == -1);
    TEST_MSG("type=%u unpack returned %d (expected -1)",
             RIG_STREAM_TYPE_COUNT, ret);

    /* Even higher type value. */
    buf[1] = 0xFF;
    ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);
    TEST_CHECK(ret == -1);
    TEST_MSG("type=255 unpack returned %d (expected -1)", ret);

    /* Restore a valid type. */
    buf[1] = RIG_STREAM_TYPE_AUDIO_RX;

    /* Corrupt format byte just past the last valid index. */
    buf[24] = RIG_STREAM_FMT_ID_IQ_CF32 + 1;
    ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);
    TEST_CHECK(ret == -1);
    TEST_MSG("format=%u unpack returned %d (expected -1)",
             RIG_STREAM_FMT_ID_IQ_CF32 + 1, ret);

    /* Far out-of-range format. */
    buf[24] = 0xFF;
    ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);
    TEST_CHECK(ret == -1);
    TEST_MSG("format=255 unpack returned %d (expected -1)", ret);

    /* Restore a valid format: header is valid again and unpacks 0. */
    buf[24] = RIG_STREAM_FMT_ID_PCM_S16;
    ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);
    TEST_CHECK(ret == 0);
    TEST_MSG("restored header unpack returned %d (expected 0)", ret);
}


void test_header_subscribe_packet(void)
{
    /* Subscribe packet: control bit 0 set, zero payload */
    struct rig_stream_packet_header hdr =
    {
        .version = 1,
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .stream_id = 0,
        .seq = 0,
        .timestamp = 0,
        .sample_rate = 0,
        .format = 0,
        .channels = 0,
        .control = RIG_STREAM_CTRL_SUBSCRIBE,
        .payload_len = 0,
    };

    unsigned char buf[RIG_STREAM_HEADER_SIZE];
    stream_packet_header_pack(&hdr, buf);

    struct rig_stream_packet_header decoded;
    int ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);

    TEST_CHECK(ret == 0);
    TEST_CHECK(decoded.control & RIG_STREAM_CTRL_SUBSCRIBE);
    TEST_CHECK(decoded.payload_len == 0);
}


void test_header_metadata_frame(void)
{
    /* Metadata frame: METADATA control bit set, payload_len = block size */
    struct rig_stream_packet_header hdr =
    {
        .version = 1,
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .stream_id = 3,
        .seq = 500,
        .timestamp = 2400000,
        .sample_rate = 48000,
        .format = RIG_STREAM_FMT_ID_PCM_S16,
        .channels = 1,
        .control = RIG_STREAM_CTRL_METADATA,
        .payload_len = RIG_STREAM_METADATA_WIRE_SIZE,
    };

    unsigned char buf[RIG_STREAM_HEADER_SIZE];
    stream_packet_header_pack(&hdr, buf);

    struct rig_stream_packet_header decoded;
    int ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);

    TEST_CHECK(ret == 0);
    TEST_CHECK(decoded.control & RIG_STREAM_CTRL_METADATA);
    TEST_CHECK(decoded.payload_len == RIG_STREAM_METADATA_WIRE_SIZE);
    TEST_CHECK(decoded.timestamp == 2400000);
}


void test_header_max_values(void)
{
    /* Verify no truncation of maximum field values */
    struct rig_stream_packet_header orig =
    {
        .version = 1,
        .type = RIG_STREAM_TYPE_IQ_TX,
        .stream_id = 0xFFFF,
        .seq = 0xFFFFFFFF,
        .timestamp = 0xFFFFFFFFFFFFFFFFULL,
        .sample_rate = 0xFFFFFFFF,
        .format = RIG_STREAM_FMT_ID_IQ_CF32,
        .channels = 255,
        .control = 0xFFFF,
        .payload_len = 0xFFFF,
    };

    unsigned char buf[RIG_STREAM_HEADER_SIZE];
    stream_packet_header_pack(&orig, buf);

    struct rig_stream_packet_header decoded;
    int ret = stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &decoded);

    TEST_CHECK(ret == 0);
    TEST_CHECK(decoded.stream_id == 0xFFFF);
    TEST_CHECK(decoded.seq == 0xFFFFFFFF);
    TEST_CHECK(decoded.timestamp == 0xFFFFFFFFFFFFFFFFULL);
    TEST_CHECK(decoded.sample_rate == 0xFFFFFFFF);
    TEST_CHECK(decoded.channels == 255);
    TEST_CHECK(decoded.control == 0xFFFF);
    TEST_CHECK(decoded.payload_len == 0xFFFF);
}


/* --- Format index mapping tests --- */

void test_format_index_audio_roundtrip(void)
{
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_PCM_S8)    ==
               RIG_STREAM_FMT_ID_PCM_S8);
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_PCM_U8)    ==
               RIG_STREAM_FMT_ID_PCM_U8);
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_PCM_S16) ==
               RIG_STREAM_FMT_ID_PCM_S16);
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_PCM_F32) ==
               RIG_STREAM_FMT_ID_PCM_F32);
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_OPUS) ==
               RIG_STREAM_FMT_ID_OPUS);

    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_PCM_S8)    ==
               RIG_STREAM_FORMAT_PCM_S8);
    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_PCM_U8)    ==
               RIG_STREAM_FORMAT_PCM_U8);
    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_PCM_S16) ==
               RIG_STREAM_FORMAT_PCM_S16);
    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_PCM_F32) ==
               RIG_STREAM_FORMAT_PCM_F32);
}


void test_format_index_iq_roundtrip(void)
{
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_IQ_CU8)    ==
               RIG_STREAM_FMT_ID_IQ_CU8);
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_IQ_CS8)    ==
               RIG_STREAM_FMT_ID_IQ_CS8);
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_IQ_CS16) ==
               RIG_STREAM_FMT_ID_IQ_CS16);
    TEST_CHECK(stream_format_to_id(RIG_STREAM_FORMAT_IQ_CF32) ==
               RIG_STREAM_FMT_ID_IQ_CF32);

    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_IQ_CU8)    ==
               RIG_STREAM_FORMAT_IQ_CU8);
    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_IQ_CS8)    ==
               RIG_STREAM_FORMAT_IQ_CS8);
    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_IQ_CS16) ==
               RIG_STREAM_FORMAT_IQ_CS16);
    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_IQ_CF32) ==
               RIG_STREAM_FORMAT_IQ_CF32);
}


void test_format_index_invalid(void)
{
    TEST_CHECK(stream_format_to_id(0) == RIG_STREAM_FMT_ID_INVALID);
    TEST_CHECK(stream_format_to_id(0xDEAD) == RIG_STREAM_FMT_ID_INVALID);

    TEST_CHECK(stream_id_to_format(RIG_STREAM_FMT_ID_INVALID) == 0);
    TEST_CHECK(stream_id_to_format(9) == 0);
    TEST_CHECK(stream_id_to_format(200) == 0);
}


/* --- Metadata wire format tests --- */

void test_metadata_pack_unpack_roundtrip(void)
{
    struct rig_stream_metadata orig =
    {
        .field_mask = RIG_STREAM_META_CENTER_FREQ | RIG_STREAM_META_VFO_FREQ
                      | RIG_STREAM_META_VFO_ID | RIG_STREAM_META_PTT,
        .sample_index = 0,  /* Not in wire format — carried in packet header */
        .center_freq = 14200000,
        .vfo_freq = 14201500,  /* distinct from center to catch field swaps */
        .vfo_id = RIG_VFO_A,
        .ptt = 0,
    };

    unsigned char buf[RIG_STREAM_METADATA_WIRE_SIZE];
    memset(buf, 0xAA, sizeof(buf));

    stream_metadata_pack(&orig, buf);

    struct rig_stream_metadata decoded;
    memset(&decoded, 0xFF, sizeof(decoded));

    int ret = stream_metadata_unpack(buf, RIG_STREAM_METADATA_WIRE_SIZE, &decoded);

    TEST_CHECK(ret == 0);
    TEST_MSG("unpack returned %d", ret);

    TEST_CHECK(decoded.field_mask == orig.field_mask);
    TEST_MSG("field_mask: got 0x%x, expected 0x%x",
             decoded.field_mask, orig.field_mask);

    TEST_CHECK(decoded.vfo_id == (vfo_t)orig.vfo_id);
    TEST_CHECK(decoded.ptt == orig.ptt);

    TEST_CHECK(decoded.center_freq == orig.center_freq);
    TEST_MSG("center_freq: got %f, expected %f",
             decoded.center_freq, orig.center_freq);

    TEST_CHECK(decoded.vfo_freq == orig.vfo_freq);
    TEST_MSG("vfo_freq: got %f, expected %f",
             decoded.vfo_freq, orig.vfo_freq);
}


void test_metadata_big_endian_byte_order(void)
{
    struct rig_stream_metadata meta =
    {
        .field_mask = 0x00000007,
        .vfo_id = 1,
        .ptt = 1,
        .center_freq = 14074000.0,
        .vfo_freq = 14075250.5,
    };

    unsigned char buf[RIG_STREAM_METADATA_WIRE_SIZE];
    stream_metadata_pack(&meta, buf);

    /* field_mask: 0x00000007 big-endian at offset 0 */
    TEST_CHECK(buf[0] == 0x00);
    TEST_CHECK(buf[1] == 0x00);
    TEST_CHECK(buf[2] == 0x00);
    TEST_CHECK(buf[3] == 0x07);

    /* vfo_id at offset 4 */
    TEST_CHECK(buf[4] == 1);

    /* ptt at offset 5 */
    TEST_CHECK(buf[5] == 1);

    /* center_freq: IEEE-754 double, big-endian at offset 6; vfo_freq at
     * offset 14. Expected bytes are the host double's bits, MSB first. */
    uint64_t center_bits, vfo_bits;
    memcpy(&center_bits, &meta.center_freq, sizeof(center_bits));
    memcpy(&vfo_bits, &meta.vfo_freq, sizeof(vfo_bits));

    for (int i = 0; i < 8; i++)
    {
        unsigned char ec = (center_bits >> (56 - 8 * i)) & 0xFF;
        unsigned char ev = (vfo_bits >> (56 - 8 * i)) & 0xFF;
        TEST_CHECK(buf[6 + i] == ec);
        TEST_MSG("center byte %d: got 0x%02x, expected 0x%02x",
                 i, buf[6 + i], ec);
        TEST_CHECK(buf[14 + i] == ev);
        TEST_MSG("vfo byte %d: got 0x%02x, expected 0x%02x",
                 i, buf[14 + i], ev);
    }
}


void test_metadata_unpack_too_short(void)
{
    unsigned char buf[19];
    memset(buf, 0, sizeof(buf));

    struct rig_stream_metadata meta;
    int ret = stream_metadata_unpack(buf, 19, &meta);

    TEST_CHECK(ret == -1);
    TEST_MSG("unpack with 19 bytes returned %d (expected -1)", ret);
}


void test_metadata_freq_only(void)
{
    /* Fractional value proves the double survives pack/unpack bit-exactly
     * (a uint64 field would have truncated it). */
    struct rig_stream_metadata orig =
    {
        .field_mask = RIG_STREAM_META_VFO_FREQ,
        .vfo_freq = 7074000.25,
        .vfo_id = 0,
        .ptt = 0,
    };

    unsigned char buf[RIG_STREAM_METADATA_WIRE_SIZE];
    stream_metadata_pack(&orig, buf);

    struct rig_stream_metadata decoded;
    int ret = stream_metadata_unpack(buf, RIG_STREAM_METADATA_WIRE_SIZE, &decoded);

    TEST_CHECK(ret == 0);
    TEST_CHECK(decoded.field_mask == RIG_STREAM_META_VFO_FREQ);
    TEST_CHECK(decoded.vfo_freq == 7074000.25);
    TEST_MSG("decoded vfo_freq = %f (expected 7074000.25)", decoded.vfo_freq);
}


/* --- Type and format name mapping tests --- */

void test_type_name_roundtrip(void)
{
    TEST_CHECK(strcmp(stream_type_name(RIG_STREAM_TYPE_AUDIO_RX), "AUDIO_RX") == 0);
    TEST_CHECK(strcmp(stream_type_name(RIG_STREAM_TYPE_AUDIO_TX), "AUDIO_TX") == 0);
    TEST_CHECK(strcmp(stream_type_name(RIG_STREAM_TYPE_IQ_RX),    "IQ_RX") == 0);
    TEST_CHECK(strcmp(stream_type_name(RIG_STREAM_TYPE_IQ_TX),    "IQ_TX") == 0);
    TEST_CHECK(strcmp(stream_type_name(99),                   "UNKNOWN") == 0);

    rig_stream_type_t type;
    TEST_CHECK(stream_type_parse("AUDIO_RX", &type) == 0);
    TEST_CHECK(type == RIG_STREAM_TYPE_AUDIO_RX);
    TEST_CHECK(stream_type_parse("AUDIO_TX", &type) == 0);
    TEST_CHECK(type == RIG_STREAM_TYPE_AUDIO_TX);
    TEST_CHECK(stream_type_parse("IQ_RX", &type) == 0);
    TEST_CHECK(type == RIG_STREAM_TYPE_IQ_RX);
    TEST_CHECK(stream_type_parse("IQ_TX", &type) == 0);
    TEST_CHECK(type == RIG_STREAM_TYPE_IQ_TX);
    TEST_CHECK(stream_type_parse("BOGUS", &type) == -1);
}


void test_format_name_roundtrip(void)
{
    TEST_CHECK(strcmp(stream_format_name(RIG_STREAM_FORMAT_PCM_S16),
                      "PCM_S16") == 0);
    TEST_CHECK(strcmp(stream_format_name(RIG_STREAM_FORMAT_PCM_F32),
                      "PCM_F32") == 0);
    TEST_CHECK(strcmp(stream_format_name(RIG_STREAM_FORMAT_IQ_CF32),
                      "IQ_CF32") == 0);
    TEST_CHECK(strcmp(stream_format_name(0xDEAD), "UNKNOWN") == 0);

    TEST_CHECK(stream_format_parse("PCM_S16") == RIG_STREAM_FORMAT_PCM_S16);
    TEST_CHECK(stream_format_parse("PCM_F32") == RIG_STREAM_FORMAT_PCM_F32);
    TEST_CHECK(stream_format_parse("IQ_CF32") == RIG_STREAM_FORMAT_IQ_CF32);
    TEST_CHECK(stream_format_parse("IQ_CS16") == RIG_STREAM_FORMAT_IQ_CS16);
    TEST_CHECK(stream_format_parse("BOGUS") == 0);
}


void test_format_bitmask_str(void)
{
    char buf[256];
    int ret;

    /* Single format */
    ret = stream_format_bitmask_str(RIG_STREAM_FORMAT_PCM_S16, buf, sizeof(buf));
    TEST_CHECK(ret > 0);
    TEST_CHECK(strcmp(buf, "PCM_S16") == 0);
    TEST_MSG("got '%s'", buf);

    /* Multiple formats */
    ret = stream_format_bitmask_str(
              RIG_STREAM_FORMAT_PCM_S16 | RIG_STREAM_FORMAT_PCM_F32,
              buf, sizeof(buf));
    TEST_CHECK(ret > 0);
    TEST_CHECK(strcmp(buf, "PCM_S16,PCM_F32") == 0);
    TEST_MSG("got '%s'", buf);

    /* I/Q formats */
    ret = stream_format_bitmask_str(
              RIG_STREAM_FORMAT_IQ_CS16 | RIG_STREAM_FORMAT_IQ_CF32,
              buf, sizeof(buf));
    TEST_CHECK(ret > 0);
    TEST_CHECK(strcmp(buf, "IQ_CS16,IQ_CF32") == 0);
    TEST_MSG("got '%s'", buf);

    /* Empty bitmask */
    ret = stream_format_bitmask_str(0, buf, sizeof(buf));
    TEST_CHECK(ret == 0);
    TEST_CHECK(buf[0] == '\0');

    /* Buffer too small */
    ret = stream_format_bitmask_str(RIG_STREAM_FORMAT_PCM_S16, buf, 5);
    TEST_CHECK(ret == -1);
}


/* --- Stream registry tests --- */

void test_registry_init_destroy(void)
{
    struct rigctld_stream_registry reg;
    int ret = rigctld_stream_registry_init(&reg);

    TEST_CHECK(ret == 0);
    TEST_MSG("registry init returned %d", ret);

    TEST_CHECK(rigctld_stream_registry_count(&reg) == 0);

    rigctld_stream_registry_destroy(&reg);
}


void test_registry_insert_lookup(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    struct rigctld_stream *s = rigctld_stream_alloc();
    TEST_CHECK(s != NULL);

    s->stream_id = 0;
    s->type = RIG_STREAM_TYPE_AUDIO_RX;
    s->client_id = 100;

    int ret = rigctld_stream_registry_insert(&reg, s);
    TEST_CHECK(ret == 0);
    TEST_MSG("insert returned %d", ret);
    TEST_CHECK(rigctld_stream_registry_count(&reg) == 1);

    /* Lookup by type and stream_id */
    struct rigctld_stream *found = rigctld_stream_registry_lookup(&reg,
                                   RIG_STREAM_TYPE_AUDIO_RX, 0);
    TEST_CHECK(found == s);
    TEST_MSG("lookup returned %p, expected %p", (void *)found, (void *)s);

    /* Lookup wrong type */
    found = rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_AUDIO_TX, 0);
    TEST_CHECK(found == NULL);

    /* Lookup wrong id */
    found = rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_AUDIO_RX, 99);
    TEST_CHECK(found == NULL);

    rigctld_stream_registry_destroy(&reg);
}


void test_registry_remove(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    struct rigctld_stream *s = rigctld_stream_alloc();
    s->stream_id = 5;
    s->type = RIG_STREAM_TYPE_IQ_RX;
    s->client_id = 200;

    rigctld_stream_registry_insert(&reg, s);
    TEST_CHECK(rigctld_stream_registry_count(&reg) == 1);

    /* Remove it */
    struct rigctld_stream *removed = rigctld_stream_registry_remove(&reg,
                                     RIG_STREAM_TYPE_IQ_RX, 5);
    TEST_CHECK(removed == s);
    TEST_CHECK(rigctld_stream_registry_count(&reg) == 0);

    /* Lookup should now fail */
    TEST_CHECK(rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_IQ_RX,
               5) == NULL);

    /* Remove again should return NULL */
    TEST_CHECK(rigctld_stream_registry_remove(&reg, RIG_STREAM_TYPE_IQ_RX,
               5) == NULL);

    rigctld_stream_free(removed);
    rigctld_stream_registry_destroy(&reg);
}


void test_registry_multiple_types(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    /* Insert streams across different types */
    struct rigctld_stream *s1 = rigctld_stream_alloc();
    s1->stream_id = 0;
    s1->type = RIG_STREAM_TYPE_AUDIO_RX;
    s1->client_id = 1;

    struct rigctld_stream *s2 = rigctld_stream_alloc();
    s2->stream_id = 0;
    s2->type = RIG_STREAM_TYPE_AUDIO_TX;
    s2->client_id = 1;

    struct rigctld_stream *s3 = rigctld_stream_alloc();
    s3->stream_id = 1;
    s3->type = RIG_STREAM_TYPE_AUDIO_RX;
    s3->client_id = 2;

    rigctld_stream_registry_insert(&reg, s1);
    rigctld_stream_registry_insert(&reg, s2);
    rigctld_stream_registry_insert(&reg, s3);

    TEST_CHECK(rigctld_stream_registry_count(&reg) == 3);

    /* Each lookup returns the correct stream */
    TEST_CHECK(rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_AUDIO_RX,
               0) == s1);
    TEST_CHECK(rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_AUDIO_TX,
               0) == s2);
    TEST_CHECK(rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_AUDIO_RX,
               1) == s3);

    rigctld_stream_registry_destroy(&reg);
}


void test_registry_close_by_client(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    /* Client 10 owns two streams, client 20 owns one */
    struct rigctld_stream *s1 = rigctld_stream_alloc();
    s1->stream_id = 0;
    s1->type = RIG_STREAM_TYPE_AUDIO_RX;
    s1->client_id = 10;

    struct rigctld_stream *s2 = rigctld_stream_alloc();
    s2->stream_id = 0;
    s2->type = RIG_STREAM_TYPE_AUDIO_TX;
    s2->client_id = 10;

    struct rigctld_stream *s3 = rigctld_stream_alloc();
    s3->stream_id = 0;
    s3->type = RIG_STREAM_TYPE_IQ_RX;
    s3->client_id = 20;

    rigctld_stream_registry_insert(&reg, s1);
    rigctld_stream_registry_insert(&reg, s2);
    rigctld_stream_registry_insert(&reg, s3);

    TEST_CHECK(rigctld_stream_registry_count(&reg) == 3);

    /* Close all streams for client 10 */
    int closed = rigctld_stream_registry_close_by_client(&reg, 10);
    TEST_CHECK(closed == 2);
    TEST_MSG("close_by_client returned %d (expected 2)", closed);

    TEST_CHECK(rigctld_stream_registry_count(&reg) == 1);

    /* Client 20's stream should still exist */
    TEST_CHECK(rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_IQ_RX,
               0) == s3);

    /* Client 10's streams should be gone */
    TEST_CHECK(rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_AUDIO_RX,
               0) == NULL);
    TEST_CHECK(rigctld_stream_registry_lookup(&reg, RIG_STREAM_TYPE_AUDIO_TX,
               0) == NULL);

    /* Closing again returns 0 */
    TEST_CHECK(rigctld_stream_registry_close_by_client(&reg, 10) == 0);

    rigctld_stream_registry_destroy(&reg);
}


void test_registry_insert_full(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    /* Fill all slots for AUDIO_RX */
    int i;

    for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
    {
        struct rigctld_stream *s = rigctld_stream_alloc();
        s->stream_id = i;
        s->type = RIG_STREAM_TYPE_AUDIO_RX;
        s->client_id = 1;
        int ret = rigctld_stream_registry_insert(&reg, s);
        TEST_CHECK(ret == 0);
        TEST_MSG("insert slot %d returned %d", i, ret);
    }

    TEST_CHECK(rigctld_stream_registry_count(&reg) == RIGCTLD_MAX_STREAMS);

    /* One more should fail */
    struct rigctld_stream *overflow = rigctld_stream_alloc();
    overflow->stream_id = RIGCTLD_MAX_STREAMS;
    overflow->type = RIG_STREAM_TYPE_AUDIO_RX;
    overflow->client_id = 1;

    int ret = rigctld_stream_registry_insert(&reg, overflow);
    TEST_CHECK(ret == -1);
    TEST_MSG("insert into full registry returned %d (expected -1)", ret);

    rigctld_stream_free(overflow);

    /* But a different type should still have room */
    struct rigctld_stream *s_tx = rigctld_stream_alloc();
    s_tx->stream_id = 0;
    s_tx->type = RIG_STREAM_TYPE_AUDIO_TX;
    s_tx->client_id = 1;

    ret = rigctld_stream_registry_insert(&reg, s_tx);
    TEST_CHECK(ret == 0);
    TEST_MSG("insert into different type returned %d", ret);

    rigctld_stream_registry_destroy(&reg);
}


void test_stream_alloc_defaults(void)
{
    struct rigctld_stream *s = rigctld_stream_alloc();
    TEST_CHECK(s != NULL);

    TEST_CHECK(s->stream_id == 0);
    TEST_CHECK(s->udp_sock == -1);
    TEST_CHECK(s->udp_port == 0);
    TEST_CHECK(s->client_addr_known == 0);
    TEST_CHECK(s->running == 0);
    TEST_CHECK(s->seq == 0);
    TEST_CHECK(s->timestamp == 0);

    rigctld_stream_free(s);
}


/* --- Config parsing helper tests --- */

void test_config_from_args_audio_rx(void)
{
    struct rig_stream_config cfg;
    int ret = rigctld_stream_config_from_args("AUDIO_RX", "PCM_S16", "48000", &cfg);

    TEST_CHECK(ret == 0);
    TEST_MSG("config_from_args returned %d", ret);
    TEST_CHECK(cfg.type == RIG_STREAM_TYPE_AUDIO_RX);
    TEST_CHECK(cfg.format == RIG_STREAM_FORMAT_PCM_S16);
    TEST_CHECK(cfg.sample_rate == 48000);
    TEST_CHECK(cfg.channels == 1);
    TEST_MSG("channels: got %d, expected 1", cfg.channels);
    TEST_CHECK(cfg.frame_samples == 0);
    TEST_CHECK(cfg.buffer_bytes == 0);
}


void test_config_from_args_iq_rx(void)
{
    struct rig_stream_config cfg;
    int ret = rigctld_stream_config_from_args("IQ_RX", "IQ_CS16", "192000", &cfg);

    TEST_CHECK(ret == 0);
    TEST_CHECK(cfg.type == RIG_STREAM_TYPE_IQ_RX);
    TEST_CHECK(cfg.format == RIG_STREAM_FORMAT_IQ_CS16);
    TEST_CHECK(cfg.sample_rate == 192000);
    TEST_CHECK(cfg.channels == 1);
    TEST_MSG("channels: got %d, expected 1", cfg.channels);
}


void test_config_from_args_audio_tx(void)
{
    struct rig_stream_config cfg;
    int ret = rigctld_stream_config_from_args("AUDIO_TX", "PCM_F32", "44100", &cfg);

    TEST_CHECK(ret == 0);
    TEST_CHECK(cfg.type == RIG_STREAM_TYPE_AUDIO_TX);
    TEST_CHECK(cfg.format == RIG_STREAM_FORMAT_PCM_F32);
    TEST_CHECK(cfg.sample_rate == 44100);
    TEST_CHECK(cfg.channels == 1);
}


void test_config_from_args_bad_type(void)
{
    struct rig_stream_config cfg;
    int ret = rigctld_stream_config_from_args("BOGUS", "PCM_S16", "48000", &cfg);
    TEST_CHECK(ret == -1);
    TEST_MSG("bad type returned %d (expected -1)", ret);
}


void test_config_from_args_bad_format(void)
{
    struct rig_stream_config cfg;
    int ret = rigctld_stream_config_from_args("AUDIO_RX", "BOGUS", "48000", &cfg);
    TEST_CHECK(ret == -1);
    TEST_MSG("bad format returned %d (expected -1)", ret);
}


void test_config_from_args_bad_rate(void)
{
    struct rig_stream_config cfg;

    /* Zero rate */
    int ret = rigctld_stream_config_from_args("AUDIO_RX", "PCM_S16", "0", &cfg);
    TEST_CHECK(ret == -1);
    TEST_MSG("zero rate returned %d (expected -1)", ret);

    /* Negative rate */
    ret = rigctld_stream_config_from_args("AUDIO_RX", "PCM_S16", "-1", &cfg);
    TEST_CHECK(ret == -1);

    /* Non-numeric */
    ret = rigctld_stream_config_from_args("AUDIO_RX", "PCM_S16", "abc", &cfg);
    TEST_CHECK(ret == -1);
}


void test_config_from_args_null_args(void)
{
    struct rig_stream_config cfg;
    TEST_CHECK(rigctld_stream_config_from_args(NULL, "PCM_S16", "48000",
               &cfg) == -1);
    TEST_CHECK(rigctld_stream_config_from_args("AUDIO_RX", NULL, "48000",
               &cfg) == -1);
    TEST_CHECK(rigctld_stream_config_from_args("AUDIO_RX", "PCM_S16", NULL,
               &cfg) == -1);
}


/* --- Next stream ID tests --- */

void test_registry_next_id(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    /* First ID is 1 */
    int id = rigctld_stream_registry_next_id(&reg);
    TEST_CHECK(id == 1);
    TEST_MSG("first next_id: got %d, expected 1", id);

    /* Second ID is 2 (monotonically increasing) */
    id = rigctld_stream_registry_next_id(&reg);
    TEST_CHECK(id == 2);
    TEST_MSG("second next_id: got %d, expected 2", id);

    /* Third ID is 3 (no reuse after removal) */
    id = rigctld_stream_registry_next_id(&reg);
    TEST_CHECK(id == 3);
    TEST_MSG("third next_id: got %d, expected 3", id);

    rigctld_stream_registry_destroy(&reg);
}


/* --- UDP socket creation tests --- */

void test_udp_socket_create_happy_path(void)
{
    int fd = -1;
    int port = 0;
    int ret = rigctld_stream_udp_socket_create(&fd, &port);

    TEST_CHECK(ret == 0);
    TEST_MSG("create returned %d", ret);
    TEST_CHECK(fd >= 0);
    TEST_MSG("fd: got %d, expected >= 0", fd);
    TEST_CHECK(port > 0);
    TEST_MSG("port: got %d, expected > 0", port);
    TEST_CHECK(port > 1024);
    TEST_MSG("port: got %d, expected > 1024 (ephemeral)", port);

    if (fd >= 0)
    {
        close(fd);
    }
}


void test_udp_socket_create_unique_ports(void)
{
    int fd1 = -1, fd2 = -1;
    int port1 = 0, port2 = 0;

    TEST_CHECK(rigctld_stream_udp_socket_create(&fd1, &port1) == 0);
    TEST_CHECK(rigctld_stream_udp_socket_create(&fd2, &port2) == 0);

    TEST_CHECK(port1 != port2);
    TEST_MSG("ports should differ: got %d and %d", port1, port2);

    if (fd1 >= 0) { close(fd1); }

    if (fd2 >= 0) { close(fd2); }
}


void test_udp_socket_create_null_fd(void)
{
    int port = 0;
    int ret = rigctld_stream_udp_socket_create(NULL, &port);
    TEST_CHECK(ret == -1);
    TEST_MSG("NULL fd returned %d (expected -1)", ret);
}


void test_udp_socket_create_null_port(void)
{
    int fd = -1;
    int ret = rigctld_stream_udp_socket_create(&fd, NULL);
    TEST_CHECK(ret == -1);
    TEST_MSG("NULL port returned %d (expected -1)", ret);
}


/* --- Edge case tests for existing functions --- */

void test_format_bitmask_str_exact_fit(void)
{
    char buf[8];  /* "PCM_S16" is 7 chars + NUL = exactly 8 */
    int ret = stream_format_bitmask_str(RIG_STREAM_FORMAT_PCM_S16, buf, 8);
    TEST_CHECK(ret == 7);
    TEST_MSG("exact fit returned %d (expected 7)", ret);
    TEST_CHECK(strcmp(buf, "PCM_S16") == 0);
    TEST_MSG("got '%s'", buf);

    /* One byte short should fail */
    ret = stream_format_bitmask_str(RIG_STREAM_FORMAT_PCM_S16, buf, 7);
    TEST_CHECK(ret == -1);
    TEST_MSG("one byte short returned %d (expected -1)", ret);
}


void test_config_from_args_max_rate(void)
{
    struct rig_stream_config cfg;

    /* 20 MHz is the upper bound (plausible for wideband I/Q) */
    int ret = rigctld_stream_config_from_args("AUDIO_RX", "PCM_S16", "20000000",
              &cfg);
    TEST_CHECK(ret == 0);
    TEST_CHECK(cfg.sample_rate == 20000000);
    TEST_MSG("sample_rate: got %d, expected 20000000", cfg.sample_rate);

    /* Above 20 MHz should be rejected */
    ret = rigctld_stream_config_from_args("AUDIO_RX", "PCM_S16", "20000001", &cfg);
    TEST_CHECK(ret == -1);

    /* INT_MAX should be rejected */
    char rate_str[32];
    snprintf(rate_str, sizeof(rate_str), "%d", INT_MAX);
    ret = rigctld_stream_config_from_args("AUDIO_RX", "PCM_S16", rate_str, &cfg);
    TEST_CHECK(ret == -1);
}


void test_registry_next_id_full(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);
    int i;

    /* Fill all slots for AUDIO_RX */
    for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
    {
        struct rigctld_stream *s = rigctld_stream_alloc();
        s->stream_id = rigctld_stream_registry_next_id(&reg);
        s->type = RIG_STREAM_TYPE_AUDIO_RX;
        int ret = rigctld_stream_registry_insert(&reg, s);
        TEST_CHECK(ret == 0);
    }

    /* Insert should fail when all slots for a type are full */
    struct rigctld_stream *overflow = rigctld_stream_alloc();
    overflow->stream_id = rigctld_stream_registry_next_id(&reg);
    overflow->type = RIG_STREAM_TYPE_AUDIO_RX;
    int ret = rigctld_stream_registry_insert(&reg, overflow);
    TEST_CHECK(ret == -1);
    TEST_MSG("insert into full type: got %d, expected -1", ret);
    rigctld_stream_free(overflow);

    /* Different type should still have room */
    struct rigctld_stream *iq = rigctld_stream_alloc();
    iq->stream_id = rigctld_stream_registry_next_id(&reg);
    iq->type = RIG_STREAM_TYPE_IQ_RX;
    ret = rigctld_stream_registry_insert(&reg, iq);
    TEST_CHECK(ret == 0);
    TEST_MSG("insert into different type: got %d, expected 0", ret);

    rigctld_stream_registry_destroy(&reg);
}


void test_registry_metadata_interval_default(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    TEST_CHECK(reg.metadata_interval_ms == RIGCTLD_METADATA_INTERVAL_DEFAULT);
    TEST_MSG("metadata_interval_ms=%d, expected %d",
             reg.metadata_interval_ms, RIGCTLD_METADATA_INTERVAL_DEFAULT);

    rigctld_stream_registry_destroy(&reg);
}


void test_payload_alignment(void)
{
    /* Verify sample-boundary alignment of max_payload.
     * The feeder uses: max_payload = (max_payload / sample_size) * sample_size
     * to ensure payloads contain only complete samples. */
    int base = RIG_STREAM_MAX_PAYLOAD_DEFAULT;  /* 1420 */

    /* PCM_S8 mono: sample_size=1, 1420/1*1 = 1420 */
    TEST_CHECK((base / 1) * 1 == 1420);

    /* PCM_S16 mono: sample_size=2, 1420/2*2 = 1420 */
    TEST_CHECK((base / 2) * 2 == 1420);

    /* PCM_F32 mono: sample_size=4, 1420/4*4 = 1420 */
    TEST_CHECK((base / 4) * 4 == 1420);

    /* IQ_CS16 (2ch): sample_size=4, 1420/4*4 = 1420 */
    TEST_CHECK((base / 4) * 4 == 1420);

    /* IQ_CF32 (2ch): sample_size=8, 1420/8*8 = 1416 */
    TEST_CHECK((base / 8) * 8 == 1416);
    TEST_MSG("IQ_CF32 aligned: got %d, expected 1416", (base / 8) * 8);

    /* Verify base value matches definition */
    TEST_CHECK(base == 1420);
    TEST_MSG("RIG_STREAM_MAX_PAYLOAD_DEFAULT=%d, expected 1420", base);
}


/* --- Multicast address parser tests --- */

void test_multicast_addr_parse_ipv4(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen;
    int ret = rigctld_stream_multicast_addr_parse("239.1.2.3:5000", &addr,
              &addrlen);

    TEST_CHECK(ret == 0);
    TEST_MSG("parse returned %d", ret);
    TEST_CHECK(addr.ss_family == AF_INET);
    TEST_MSG("family: got %d, expected %d", addr.ss_family, AF_INET);

    struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
    TEST_CHECK(ntohs(sin->sin_port) == 5000);
    TEST_MSG("port: got %d, expected 5000", ntohs(sin->sin_port));

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
    TEST_CHECK(strcmp(ip_str, "239.1.2.3") == 0);
    TEST_MSG("addr: got '%s', expected '239.1.2.3'", ip_str);
}


void test_multicast_addr_parse_ipv6(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen;
    int ret = rigctld_stream_multicast_addr_parse("[ff02::1]:5000", &addr,
              &addrlen);

    TEST_CHECK(ret == 0);
    TEST_MSG("parse returned %d", ret);
    TEST_CHECK(addr.ss_family == AF_INET6);
    TEST_MSG("family: got %d, expected %d", addr.ss_family, AF_INET6);

    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
    TEST_CHECK(ntohs(sin6->sin6_port) == 5000);
    TEST_MSG("port: got %d, expected 5000", ntohs(sin6->sin6_port));

    TEST_CHECK(IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr));
    TEST_MSG("address should be multicast");
}


void test_multicast_addr_parse_invalid(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen;

    /* Not a valid address */
    TEST_CHECK(rigctld_stream_multicast_addr_parse("not-an-address:5000",
               &addr, &addrlen) == -1);

    /* Missing port */
    TEST_CHECK(rigctld_stream_multicast_addr_parse("239.1.2.3",
               &addr, &addrlen) == -1);

    /* NULL spec */
    TEST_CHECK(rigctld_stream_multicast_addr_parse(NULL, &addr, &addrlen) == -1);

    /* Empty string */
    TEST_CHECK(rigctld_stream_multicast_addr_parse("", &addr, &addrlen) == -1);

    /* Port only */
    TEST_CHECK(rigctld_stream_multicast_addr_parse(":5000", &addr, &addrlen) == -1);
}


void test_multicast_addr_parse_non_multicast(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen;

    /* Unicast IPv4 — not in 224.0.0.0/4 range */
    TEST_CHECK(rigctld_stream_multicast_addr_parse("192.168.1.1:5000",
               &addr, &addrlen) == -1);

    /* Loopback */
    TEST_CHECK(rigctld_stream_multicast_addr_parse("127.0.0.1:5000",
               &addr, &addrlen) == -1);

    /* Unicast IPv6 */
    TEST_CHECK(rigctld_stream_multicast_addr_parse("[::1]:5000",
               &addr, &addrlen) == -1);
}


/* --- Multicast socket creation tests --- */

void test_multicast_socket_create_ipv4(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen;
    int ret = rigctld_stream_multicast_addr_parse("239.1.2.3:5000", &addr,
              &addrlen);
    TEST_CHECK(ret == 0);

    int fd = -1;
    int port = 0;
    ret = rigctld_stream_multicast_socket_create(&addr, addrlen, 4, &fd, &port);

    TEST_CHECK(ret == 0);
    TEST_MSG("create returned %d", ret);
    TEST_CHECK(fd >= 0);
    TEST_MSG("fd: got %d, expected >= 0", fd);
    TEST_CHECK(port == 5000);
    TEST_MSG("port: got %d, expected 5000", port);

    /* Verify TTL was set */
    int ttl_val = 0;
    socklen_t ttl_len = sizeof(ttl_val);
    getsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
               (char *)&ttl_val, &ttl_len);
    TEST_CHECK(ttl_val == 4);
    TEST_MSG("TTL: got %d, expected 4", ttl_val);

    if (fd >= 0)
    {
        close(fd);
    }
}


void test_multicast_socket_create_ipv6(void)
{
    struct sockaddr_storage addr;
    socklen_t addrlen;
    int ret = rigctld_stream_multicast_addr_parse("[ff02::1]:6000", &addr,
              &addrlen);
    TEST_CHECK(ret == 0);

    int fd = -1;
    int port = 0;
    ret = rigctld_stream_multicast_socket_create(&addr, addrlen, 2, &fd, &port);

    TEST_CHECK(ret == 0);
    TEST_MSG("create returned %d", ret);
    TEST_CHECK(fd >= 0);
    TEST_MSG("fd: got %d, expected >= 0", fd);
    TEST_CHECK(port == 6000);
    TEST_MSG("port: got %d, expected 6000", port);

    /* Verify hop limit was set */
    int hops = 0;
    socklen_t hops_len = sizeof(hops);
    getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
               (char *)&hops, &hops_len);
    TEST_CHECK(hops == 2);
    TEST_MSG("hops: got %d, expected 2", hops);

    if (fd >= 0)
    {
        close(fd);
    }
}


void test_multicast_in_use(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    struct sockaddr_storage addr1, addr2;
    socklen_t addrlen1, addrlen2;
    rigctld_stream_multicast_addr_parse("239.1.2.3:5000", &addr1, &addrlen1);
    rigctld_stream_multicast_addr_parse("239.1.2.4:5001", &addr2, &addrlen2);

    /* Empty registry — nothing in use */
    TEST_CHECK(rigctld_stream_registry_multicast_in_use(&reg, &addr1) == 0);

    /* Insert a multicast stream */
    struct rigctld_stream *s = rigctld_stream_alloc();
    s->stream_id = 1;
    s->type = RIG_STREAM_TYPE_AUDIO_RX;
    s->multicast = 1;
    s->multicast_addr = addr1;
    s->multicast_addr_len = addrlen1;
    rigctld_stream_registry_insert(&reg, s);

    /* Same addr:port should be in use */
    TEST_CHECK(rigctld_stream_registry_multicast_in_use(&reg, &addr1) == 1);

    /* Different addr:port should be available */
    TEST_CHECK(rigctld_stream_registry_multicast_in_use(&reg, &addr2) == 0);

    /* Unicast stream should not affect multicast check */
    struct rigctld_stream *s2 = rigctld_stream_alloc();
    s2->stream_id = 2;
    s2->type = RIG_STREAM_TYPE_AUDIO_RX;
    s2->multicast = 0;
    rigctld_stream_registry_insert(&reg, s2);

    TEST_CHECK(rigctld_stream_registry_multicast_in_use(&reg, &addr2) == 0);

    rigctld_stream_registry_destroy(&reg);
}


void test_registry_multicast_ttl_default(void)
{
    struct rigctld_stream_registry reg;
    rigctld_stream_registry_init(&reg);

    TEST_CHECK(reg.multicast_ttl == 1);
    TEST_MSG("multicast_ttl=%d, expected 1", reg.multicast_ttl);

    rigctld_stream_registry_destroy(&reg);
}


void test_header_ping_packet(void)
{
    struct rig_stream_packet_header hdr;
    unsigned char buf[RIG_STREAM_HEADER_SIZE];

    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = RIG_STREAM_TYPE_AUDIO_RX;
    hdr.stream_id = 5;
    hdr.control = RIG_STREAM_CTRL_PING;

    stream_packet_header_pack(&hdr, buf);

    struct rig_stream_packet_header out;
    TEST_CHECK(stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &out) == 0);
    TEST_CHECK(out.control == RIG_STREAM_CTRL_PING);
    TEST_CHECK(out.stream_id == 5);
    TEST_CHECK(out.type == RIG_STREAM_TYPE_AUDIO_RX);
}


void test_header_pong_packet(void)
{
    struct rig_stream_packet_header hdr;
    unsigned char buf[RIG_STREAM_HEADER_SIZE];

    memset(&hdr, 0, sizeof(hdr));
    hdr.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr.type = RIG_STREAM_TYPE_IQ_RX;
    hdr.stream_id = 42;
    hdr.control = RIG_STREAM_CTRL_PONG;

    stream_packet_header_pack(&hdr, buf);

    struct rig_stream_packet_header out;
    TEST_CHECK(stream_packet_header_unpack(buf, RIG_STREAM_HEADER_SIZE, &out) == 0);
    TEST_CHECK(out.control == RIG_STREAM_CTRL_PONG);
    TEST_CHECK(out.stream_id == 42);
}


void test_header_ping_pong_distinct(void)
{
    /* The control field is a bitmask: every control bit must occupy a distinct,
     * non-overlapping bit so they can be OR'd together. One guard covers the
     * whole set. */
    unsigned all = RIG_STREAM_CTRL_PING | RIG_STREAM_CTRL_PONG
                   | RIG_STREAM_CTRL_SUBSCRIBE | RIG_STREAM_CTRL_SUBSCRIBE_ACK
                   | RIG_STREAM_CTRL_ERROR | RIG_STREAM_CTRL_TIME
                   | RIG_STREAM_CTRL_METADATA | RIG_STREAM_CTRL_WRITE_STATUS;
    unsigned popcount = 0;

    for (unsigned m = all; m != 0; m &= (m - 1))
    {
        popcount++;
    }

    /* Eight distinct bits set means none of the eight constants collide. */
    TEST_CHECK(popcount == 8);
    TEST_MSG("OR of 8 control bits has %u bits set (expected 8)", popcount);

    /* Freeze the v1 wire values: third-party implementers depend on these. */
    TEST_CHECK(RIG_STREAM_CTRL_PING == 0x0001);
    TEST_CHECK(RIG_STREAM_CTRL_PONG == 0x0002);
    TEST_CHECK(RIG_STREAM_CTRL_SUBSCRIBE == 0x0004);
    TEST_CHECK(RIG_STREAM_CTRL_SUBSCRIBE_ACK == 0x0008);
    TEST_CHECK(RIG_STREAM_CTRL_ERROR == 0x0010);
    TEST_CHECK(RIG_STREAM_CTRL_TIME == 0x0020);
    TEST_CHECK(RIG_STREAM_CTRL_METADATA == 0x0040);
    TEST_CHECK(RIG_STREAM_CTRL_WRITE_STATUS == 0x0080);
}


void test_kv_parse_single_pair(void)
{
    FILE *f = tmpfile();
    fprintf(f, "key=value\n");
    rewind(f);

    struct hamlib_kv_pair kv[4];
    int n = hamlib_parse_kv_args(f, kv, 4);

    TEST_CHECK(n == 1);
    TEST_MSG("n=%d, expected 1", n);
    TEST_CHECK(strcmp(kv[0].key, "key") == 0);
    TEST_CHECK(strcmp(kv[0].value, "value") == 0);

    fclose(f);
}


void test_kv_parse_multiple_pairs(void)
{
    FILE *f = tmpfile();
    fprintf(f, "a=1 b=2 c=3\n");
    rewind(f);

    struct hamlib_kv_pair kv[4];
    int n = hamlib_parse_kv_args(f, kv, 4);

    TEST_CHECK(n == 3);
    TEST_MSG("n=%d, expected 3", n);
    TEST_CHECK(strcmp(kv[0].key, "a") == 0);
    TEST_CHECK(strcmp(kv[0].value, "1") == 0);
    TEST_CHECK(strcmp(kv[1].key, "b") == 0);
    TEST_CHECK(strcmp(kv[1].value, "2") == 0);
    TEST_CHECK(strcmp(kv[2].key, "c") == 0);
    TEST_CHECK(strcmp(kv[2].value, "3") == 0);

    fclose(f);
}


void test_kv_parse_empty_line(void)
{
    FILE *f = tmpfile();
    fprintf(f, "\n");
    rewind(f);

    struct hamlib_kv_pair kv[4];
    int n = hamlib_parse_kv_args(f, kv, 4);

    TEST_CHECK(n == 0);
    TEST_MSG("n=%d, expected 0", n);

    fclose(f);
}


void test_kv_parse_no_equals(void)
{
    FILE *f = tmpfile();
    fprintf(f, "notapair\n");
    rewind(f);

    struct hamlib_kv_pair kv[4];
    int n = hamlib_parse_kv_args(f, kv, 4);

    TEST_CHECK(n == 0);
    TEST_MSG("n=%d, expected 0", n);

    fclose(f);
}


void test_kv_parse_max_pairs(void)
{
    FILE *f = tmpfile();
    fprintf(f, "a=1 b=2 c=3 d=4 e=5\n");
    rewind(f);

    struct hamlib_kv_pair kv[3];
    int n = hamlib_parse_kv_args(f, kv, 3);

    TEST_CHECK(n == 3);
    TEST_MSG("n=%d, expected 3 (capped at max_pairs)", n);
    TEST_CHECK(strcmp(kv[2].key, "c") == 0);
    TEST_CHECK(strcmp(kv[2].value, "3") == 0);

    fclose(f);
}


void test_kv_parse_leading_whitespace(void)
{
    FILE *f = tmpfile();
    fprintf(f, "  key=value\n");
    rewind(f);

    struct hamlib_kv_pair kv[4];
    int n = hamlib_parse_kv_args(f, kv, 4);

    TEST_CHECK(n == 1);
    TEST_MSG("n=%d, expected 1", n);
    TEST_CHECK(strcmp(kv[0].key, "key") == 0);
    TEST_CHECK(strcmp(kv[0].value, "value") == 0);

    fclose(f);
}


void test_kv_parse_mixed_valid_invalid(void)
{
    FILE *f = tmpfile();
    fprintf(f, "a=1 notpair b=2\n");
    rewind(f);

    struct hamlib_kv_pair kv[4];
    int n = hamlib_parse_kv_args(f, kv, 4);

    /* Should parse "a=1" then stop at "notpair" (no '=') */
    TEST_CHECK(n == 1);
    TEST_MSG("n=%d, expected 1 (stops at non-kv token)", n);
    TEST_CHECK(strcmp(kv[0].key, "a") == 0);

    fclose(f);
}


/* Verify parser rejects key or value exceeding buffer size. */
void test_kv_parse_overflow_rejected(void)
{
    FILE *f = tmpfile();
    /* Key longer than 64 chars */
    char longkey[80];
    memset(longkey, 'k', 70);
    longkey[70] = '\0';
    fprintf(f, "%s=val\n", longkey);
    rewind(f);

    struct hamlib_kv_pair kv[4];
    int n = hamlib_parse_kv_args(f, kv, 4);

    TEST_CHECK(n == 0);
    TEST_MSG("n=%d, expected 0 (oversized key rejected)", n);
    fclose(f);

    /* Value longer than 512 chars */
    f = tmpfile();
    char longval[520];
    memset(longval, 'v', 515);
    longval[515] = '\0';
    fprintf(f, "key=%s\n", longval);
    rewind(f);

    n = hamlib_parse_kv_args(f, kv, 4);

    TEST_CHECK(n == 0);
    TEST_MSG("n=%d, expected 0 (oversized value rejected)", n);
    fclose(f);
}


/* Verify parser accepts key= with empty value. */
void test_kv_parse_empty_value(void)
{
    FILE *f = tmpfile();
    fprintf(f, "key=\n");
    rewind(f);

    struct hamlib_kv_pair kv[4];
    int n = hamlib_parse_kv_args(f, kv, 4);

    TEST_CHECK(n == 1);
    TEST_MSG("n=%d, expected 1", n);
    TEST_CHECK(strcmp(kv[0].key, "key") == 0);
    TEST_CHECK(strcmp(kv[0].value, "") == 0);
    TEST_MSG("value='%s', expected empty string", kv[0].value);

    fclose(f);
}


/* --- Network stream helper tests --- */

void test_net_parse_open_response_valid(void)
{
    int stream_id = -1, source_id = -1, udp_port = -1;
    int ret = rig_stream_net_parse_open_response("42", &stream_id, &source_id,
              &udp_port);
    TEST_CHECK(ret == 0);
    TEST_CHECK(stream_id == 42);
    TEST_MSG("stream_id=%d, expected 42", stream_id);
    TEST_CHECK(source_id == 0);
    TEST_MSG("source_id=%d, expected 0 when line absent", source_id);
}

void test_net_parse_open_response_all_lines(void)
{
    int stream_id = -1, source_id = -1, udp_port = -1;
    int ret = rig_stream_net_parse_open_response("42\n4660\n5001",
              &stream_id, &source_id,
              &udp_port);
    TEST_CHECK(ret == 0);
    TEST_CHECK(stream_id == 42);
    TEST_MSG("stream_id=%d, expected 42", stream_id);
    TEST_CHECK(source_id == 4660);
    TEST_MSG("source_id=%d, expected 4660", source_id);
    TEST_CHECK(udp_port == 5001);
    TEST_MSG("udp_port=%d, expected 5001", udp_port);
}

void test_net_parse_open_response_negative_id(void)
{
    int stream_id = -1, source_id = -1, udp_port = -1;
    int ret = rig_stream_net_parse_open_response("-1", &stream_id, &source_id,
              &udp_port);
    TEST_CHECK(ret == -1);
    TEST_MSG("ret=%d, expected -1 for negative stream_id", ret);
}

void test_net_parse_open_response_empty(void)
{
    int stream_id = -1, source_id = -1, udp_port = -1;
    int ret = rig_stream_net_parse_open_response("", &stream_id, &source_id,
              &udp_port);
    TEST_CHECK(ret == -1);
    TEST_MSG("ret=%d, expected -1 for empty input", ret);
}

void test_net_parse_open_response_non_numeric(void)
{
    int stream_id = -1, source_id = -1, udp_port = -1;
    int ret = rig_stream_net_parse_open_response("abc", &stream_id, &source_id,
              &udp_port);
    TEST_CHECK(ret == -1);
    TEST_MSG("ret=%d, expected -1 for non-numeric", ret);
}

void test_net_udp_connect_localhost(void)
{
    /* Create a temporary UDP socket to get a valid port */
    int server_sock;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT(server_sock >= 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    TEST_ASSERT(bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    TEST_ASSERT(getsockname(server_sock, (struct sockaddr *)&addr, &addrlen) == 0);
    int port = ntohs(addr.sin_port);

    struct rig_stream_net_session sess;
    memset(&sess, 0, sizeof(sess));
    sess.udp_sock = -1;

    int ret = rig_stream_net_udp_connect(&sess, "127.0.0.1", port);
    TEST_CHECK(ret == 0);
    TEST_CHECK(sess.udp_sock >= 0);
    TEST_CHECK(sess.server_addr_len > 0);

    if (sess.udp_sock >= 0)
    {
        close(sess.udp_sock);
    }

    close(server_sock);
}

void test_net_udp_connect_invalid_host(void)
{
    struct rig_stream_net_session sess;
    memset(&sess, 0, sizeof(sess));
    sess.udp_sock = -1;

    int ret = rig_stream_net_udp_connect(&sess, "invalid.host.example.test", 12345);
    TEST_CHECK(ret == -1);
    TEST_MSG("ret=%d, expected -1 for invalid host", ret);
}

void test_net_send_ping_packet_format(void)
{
    /* Create a UDP socket pair for testing */
    int recv_sock;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT(recv_sock >= 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    TEST_ASSERT(bind(recv_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    TEST_ASSERT(getsockname(recv_sock, (struct sockaddr *)&addr, &addrlen) == 0);

    struct rig_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.type = RIG_STREAM_TYPE_AUDIO_RX;
    stream.config.sample_rate = 48000;
    stream.config.format = RIG_STREAM_FORMAT_PCM_S16;
    stream.config.channels = 1;

    struct rig_stream_net_session sess;
    memset(&sess, 0, sizeof(sess));
    sess.remote_stream_id = 7;
    sess.format_id = stream_format_to_id(stream.config.format);
    sess.sample_size = rig_stream_format_sample_size(stream.config.format);
    TEST_ASSERT(rig_stream_net_udp_connect(&sess, "127.0.0.1",
                                           ntohs(addr.sin_port)) == 0);

    int ret = rig_stream_net_send_ping(&sess, &stream);
    TEST_CHECK(ret == 0);

    /* Receive and verify the packet */
    unsigned char buf[64];
    ssize_t n = recvfrom(recv_sock, (char *)buf, sizeof(buf), 0, NULL, NULL);
    TEST_CHECK(n == RIG_STREAM_HEADER_SIZE);

    struct rig_stream_packet_header hdr;
    TEST_CHECK(stream_packet_header_unpack(buf, (size_t)n, &hdr) == 0);
    TEST_CHECK(hdr.control == RIG_STREAM_CTRL_PING);
    TEST_CHECK(hdr.stream_id == 7);
    TEST_CHECK(hdr.source_id == 0);
    TEST_MSG("client frame source_id=%u, expected 0", hdr.source_id);
    TEST_CHECK(hdr.payload_len == 0);
    TEST_CHECK(hdr.version == RIG_STREAM_PROTOCOL_VERSION);

    close(sess.udp_sock);
    close(recv_sock);
}

/* A datagram larger than the classic 1500-byte MTU is received intact into a
 * RIG_STREAM_MAX_DATAGRAM-sized buffer. This is the receiver-liberality
 * contract that lets a larger-MTU sender interoperate with existing receivers
 * without a wire-version change; every receive path sizes its buffer this way. */
void test_jumbo_datagram_received_intact(void)
{
    int tx = socket(AF_INET, SOCK_DGRAM, 0);
    int rx = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT(tx >= 0 && rx >= 0);

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    TEST_ASSERT(bind(rx, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    TEST_ASSERT(getsockname(rx, (struct sockaddr *)&addr, &addrlen) == 0);

    /* 4000 bytes: well above 1500, within the jumbo ceiling. */
    unsigned char out[4000];
    TEST_ASSERT(sizeof(out) > RIG_STREAM_DEFAULT_MTU);
    TEST_ASSERT(sizeof(out) <= RIG_STREAM_MAX_DATAGRAM);

    for (size_t i = 0; i < sizeof(out); i++)
    {
        out[i] = (unsigned char)(i & 0xFF);
    }

    ssize_t sent = sendto(tx, (const char *)out, sizeof(out), 0,
                          (struct sockaddr *)&addr, addrlen);
    TEST_CHECK(sent == (ssize_t)sizeof(out));

    unsigned char buf[RIG_STREAM_MAX_DATAGRAM];
    ssize_t n = recvfrom(rx, (char *)buf, sizeof(buf), 0, NULL, NULL);
    TEST_CHECK(n == (ssize_t)sizeof(out));  /* not truncated to the MTU */
    TEST_CHECK(memcmp(buf, out, sizeof(out)) == 0);

    close(tx);
    close(rx);
}

void test_net_send_data_packet_format(void)
{
    /* Create a UDP socket pair for testing */
    int recv_sock;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    recv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT(recv_sock >= 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    TEST_ASSERT(bind(recv_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    TEST_ASSERT(getsockname(recv_sock, (struct sockaddr *)&addr, &addrlen) == 0);

    struct rig_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.type = RIG_STREAM_TYPE_AUDIO_TX;
    stream.config.sample_rate = 48000;
    stream.config.format = RIG_STREAM_FORMAT_PCM_S16;
    stream.config.channels = 1;
    stream.max_payload =
        RIG_STREAM_MAX_PAYLOAD_DEFAULT;  /* as rig_stream_open sets */

    struct rig_stream_net_session sess;
    memset(&sess, 0, sizeof(sess));
    sess.remote_stream_id = 3;
    sess.tx_seq = 0;
    sess.tx_timestamp = 0;
    sess.format_id = stream_format_to_id(stream.config.format);
    sess.sample_size = rig_stream_format_sample_size(stream.config.format);
    TEST_ASSERT(rig_stream_net_udp_connect(&sess, "127.0.0.1",
                                           ntohs(addr.sin_port)) == 0);

    /* Send 100 bytes of data */
    unsigned char data[100];
    memset(data, 0xAB, sizeof(data));
    int ret = rig_stream_net_send_data(&sess, &stream, data, sizeof(data),
                                       NULL);
    TEST_CHECK(ret == (int)sizeof(data));

    /* Receive and verify */
    unsigned char buf[1500];
    ssize_t n = recvfrom(recv_sock, (char *)buf, sizeof(buf), 0, NULL, NULL);
    TEST_CHECK(n == RIG_STREAM_HEADER_SIZE + (ssize_t)sizeof(data));

    struct rig_stream_packet_header hdr;
    TEST_CHECK(stream_packet_header_unpack(buf, (size_t)n, &hdr) == 0);
    TEST_CHECK(hdr.stream_id == 3);
    TEST_CHECK(hdr.seq == 0);
    TEST_CHECK(hdr.payload_len == 100);
    TEST_CHECK(hdr.control == 0);
    TEST_CHECK(hdr.sample_rate == 48000);
    TEST_CHECK(hdr.format == RIG_STREAM_FMT_ID_PCM_S16);
    TEST_CHECK(hdr.channels == 1);

    /* Verify payload */
    TEST_CHECK(memcmp(buf + RIG_STREAM_HEADER_SIZE, data, sizeof(data)) == 0);

    /* Verify seq incremented */
    TEST_CHECK(sess.tx_seq == 1);

    close(sess.udp_sock);
    close(recv_sock);
}

void test_net_session_cleanup(void)
{
    struct rig_stream_net_session *sess = calloc(1, sizeof(*sess));
    TEST_ASSERT(sess != NULL);
    sess->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT(sess->udp_sock >= 0);

    int fd = sess->udp_sock;

    /* The fd is open before cleanup. SO_TYPE succeeds only on an open
     * socket, and unlike fcntl(F_GETFD) it is also valid on Winsock. */
    int so_type = 0;
    socklen_t so_len = sizeof(so_type);
    TEST_CHECK(getsockopt(fd, SOL_SOCKET, SO_TYPE,
                          (char *)&so_type, &so_len) == 0);

    /* cleanup() frees sess, so capture the fd first. After cleanup the
     * underlying socket must be closed: querying the fd fails. */
    rig_stream_net_session_cleanup(sess);

    so_len = sizeof(so_type);
    int rc = getsockopt(fd, SOL_SOCKET, SO_TYPE, (char *)&so_type, &so_len);
    TEST_CHECK(rc == -1);
    TEST_MSG("post-cleanup getsockopt(SO_TYPE) returned %d (expected -1)", rc);
}


/* --- stream_caps line parser tests --- */

void test_net_parse_caps_line_audio_rx(void)
{
    const char *line =
        "type=AUDIO_RX formats=PCM_S16,PCM_F32 rates=8000,48000 channels=1,2 max_streams=4";
    struct rig_stream_caps caps;
    memset(&caps, 0, sizeof(caps));

    int ret = rig_stream_net_parse_caps_line(line, &caps);
    TEST_CHECK(ret == 0);
    TEST_CHECK(caps.type == RIG_STREAM_TYPE_AUDIO_RX);
    TEST_MSG("type=%d, expected %d", caps.type, RIG_STREAM_TYPE_AUDIO_RX);

    TEST_CHECK((caps.formats & RIG_STREAM_FORMAT_PCM_S16) != 0);
    TEST_CHECK((caps.formats & RIG_STREAM_FORMAT_PCM_F32) != 0);
    /* Only those two should be set */
    TEST_CHECK(caps.formats == (RIG_STREAM_FORMAT_PCM_S16 |
                                RIG_STREAM_FORMAT_PCM_F32));

    TEST_CHECK(caps.sample_rates[0] == 8000);
    TEST_CHECK(caps.sample_rates[1] == 48000);
    TEST_CHECK(caps.sample_rates[2] == 0);  /* Sentinel */

    TEST_CHECK(caps.channels[0] == 1 && caps.channels[1] == 2
               && caps.channels[2] == 0);
    TEST_CHECK(caps.max_streams == 4);

    /* A caps line from an older server has no native keys: the native
     * view must stay zeroed so the client falls back to local
     * derivation. */
    TEST_CHECK(caps.native_formats == 0);
    TEST_CHECK(caps.native_sample_rates[0] == 0);
    TEST_CHECK(caps.native_channels[0] == 0);
}

void test_net_parse_caps_line_native_keys(void)
{
    const char *line =
        "type=AUDIO_RX formats=PCM_S16,PCM_F32 rates=8000,44100,48000 "
        "channels=1,2 max_streams=4 native_formats=PCM_F32 native_rates=48000 "
        "native_channels=2";
    struct rig_stream_caps caps;
    memset(&caps, 0xff, sizeof(caps));  /* Parser must fully reset it */

    int ret = rig_stream_net_parse_caps_line(line, &caps);
    TEST_CHECK(ret == 0);
    TEST_CHECK(caps.type == RIG_STREAM_TYPE_AUDIO_RX);

    /* Effective view */
    TEST_CHECK(caps.formats == (RIG_STREAM_FORMAT_PCM_S16 |
                                RIG_STREAM_FORMAT_PCM_F32));
    TEST_CHECK(caps.sample_rates[0] == 8000);
    TEST_CHECK(caps.sample_rates[1] == 44100);
    TEST_CHECK(caps.sample_rates[2] == 48000);
    TEST_CHECK(caps.sample_rates[3] == 0);
    TEST_CHECK(caps.channels[0] == 1 && caps.channels[1] == 2
               && caps.channels[2] == 0);

    /* Native view */
    TEST_CHECK(caps.native_formats == RIG_STREAM_FORMAT_PCM_F32);
    TEST_MSG("native_formats=0x%x", (unsigned)caps.native_formats);
    TEST_CHECK(caps.native_sample_rates[0] == 48000);
    TEST_CHECK(caps.native_sample_rates[1] == 0);
    TEST_CHECK(caps.native_channels[0] == 2
               && caps.native_channels[1] == 0);
}

void test_net_parse_caps_line_iq_tx(void)
{
    const char *line =
        "type=IQ_TX formats=IQ_CS16 rates=192000,384000,768000 channels=2 max_streams=1";
    struct rig_stream_caps caps;
    memset(&caps, 0, sizeof(caps));

    int ret = rig_stream_net_parse_caps_line(line, &caps);
    TEST_CHECK(ret == 0);
    TEST_CHECK(caps.type == RIG_STREAM_TYPE_IQ_TX);

    TEST_CHECK(caps.formats == RIG_STREAM_FORMAT_IQ_CS16);

    TEST_CHECK(caps.sample_rates[0] == 192000);
    TEST_CHECK(caps.sample_rates[1] == 384000);
    TEST_CHECK(caps.sample_rates[2] == 768000);
    TEST_CHECK(caps.sample_rates[3] == 0);

    TEST_CHECK(caps.channels[0] == 2 && caps.channels[1] == 0);
    TEST_CHECK(caps.max_streams == 1);
}

/* The canonical line format and the wire parser are the two halves of
 * ONE rendering: everything the formatter emits must come back through
 * the parser field-exact, for every field the struct carries — flags
 * and TX horizon included, and both with and without the native view. */
void test_caps_line_roundtrip(void)
{
    struct rig_stream_caps in;
    struct rig_stream_caps out;
    char line[1024];

    /* Declaration form: non-contiguous channels, flags, horizon. */
    memset(&in, 0, sizeof(in));
    in.type = RIG_STREAM_TYPE_AUDIO_TX;
    in.formats = RIG_STREAM_FORMAT_PCM_F32 | RIG_STREAM_FORMAT_OPUS;
    in.sample_rates[0] = 24000;
    in.sample_rates[1] = 48000;
    in.channels[0] = 1;
    in.channels[1] = 4;
    in.max_streams = 2;
    in.caps_flags = RIG_STREAM_CAP_TIMED_TX_COARSE
                    | RIG_STREAM_CAP_BURST_PTT;
    in.tx_schedule_horizon_ms = 15000;

    TEST_CHECK(stream_caps_format_line(&in, 0, line, sizeof(line)) > 0);
    TEST_MSG("line: '%s'", line);

    memset(&out, 0, sizeof(out));
    TEST_CHECK(rig_stream_net_parse_caps_line(line, &out) == 0);
    TEST_CHECK(out.type == in.type);
    TEST_CHECK(out.formats == in.formats);
    TEST_CHECK(memcmp(out.sample_rates, in.sample_rates,
                      sizeof(in.sample_rates)) == 0);
    TEST_CHECK(memcmp(out.channels, in.channels, sizeof(in.channels)) == 0);
    TEST_CHECK(out.max_streams == in.max_streams);
    TEST_CHECK(out.caps_flags == in.caps_flags);
    TEST_MSG("flags: got 0x%x, expected 0x%x", out.caps_flags,
             in.caps_flags);
    TEST_CHECK(out.tx_schedule_horizon_ms == in.tx_schedule_horizon_ms);

    /* Served form: native view appended; empty flags stay empty. */
    in.caps_flags = 0;
    in.tx_schedule_horizon_ms = 0;
    in.native_formats = RIG_STREAM_FORMAT_PCM_F32;
    in.native_sample_rates[0] = 48000;
    in.native_channels[0] = 1;
    in.native_channels[1] = 2;

    TEST_CHECK(stream_caps_format_line(&in, 1, line, sizeof(line)) > 0);
    TEST_MSG("line: '%s'", line);

    memset(&out, 0, sizeof(out));
    TEST_CHECK(rig_stream_net_parse_caps_line(line, &out) == 0);
    TEST_CHECK(out.caps_flags == 0);
    TEST_CHECK(out.tx_schedule_horizon_ms == 0);
    TEST_CHECK(out.native_formats == in.native_formats);
    TEST_CHECK(memcmp(out.native_sample_rates, in.native_sample_rates,
                      sizeof(in.native_sample_rates)) == 0);
    TEST_CHECK(memcmp(out.native_channels, in.native_channels,
                      sizeof(in.native_channels)) == 0);
}

/* Unknown flag names on the wire are skipped, known ones kept — a
 * future server may add stages without breaking this client. */
void test_caps_line_unknown_flag_skipped(void)
{
    const char *line =
        "type=AUDIO_RX formats=PCM_S16 rates=48000 channels=1 "
        "max_streams=1 flags=SHINY_FUTURE_FLAG,BURST_PTT tx_horizon_ms=0";
    struct rig_stream_caps caps;
    memset(&caps, 0, sizeof(caps));

    TEST_CHECK(rig_stream_net_parse_caps_line(line, &caps) == 0);
    TEST_CHECK(caps.caps_flags == RIG_STREAM_CAP_BURST_PTT);
    TEST_MSG("caps_flags = 0x%x, expected BURST_PTT only", caps.caps_flags);
}

void test_net_parse_caps_line_missing_type(void)
{
    const char *line =
        "formats=PCM_S16 rates=48000 channels=1,2 max_streams=4";
    struct rig_stream_caps caps;
    memset(&caps, 0, sizeof(caps));

    int ret = rig_stream_net_parse_caps_line(line, &caps);
    TEST_CHECK(ret == -1);
    TEST_MSG("ret=%d, expected -1 for missing type", ret);
}


/* --- Test list --- */

/* --- Embedded time block tests --- */

void test_time_block_round_trip(void)
{
    struct rig_stream_time_anchor orig =
    {
        .sample_index = 7777,    /* travels in the packet header, not the block */
        .seconds = 1736000000,
        .picoseconds = 999999999999ULL,
        .source = RIG_STREAM_TIME_SRC_GPS,
        .flags = RIG_STREAM_TIME_FLAG_LOCKED | RIG_STREAM_TIME_FLAG_SAMPLE_REFERENCED
                 | RIG_STREAM_TIME_FLAG_DISCONTINUITY | RIG_STREAM_TIME_FLAG_DISC_OVERRUN,
        .accuracy = RIG_STREAM_TIME_ACC_100NS,
    };

    unsigned char buf[RIG_STREAM_TIME_BLOCK_SIZE];
    memset(buf, 0xAA, sizeof(buf));
    stream_time_block_pack(&orig, buf);

    TEST_CHECK(buf[19] == 0);   /* reserved byte must be zero */

    struct rig_stream_time_anchor dec;
    memset(&dec, 0xFF, sizeof(dec));
    TEST_CHECK(stream_time_block_unpack(buf, sizeof(buf), &dec) == 0);

    TEST_CHECK(dec.seconds == orig.seconds);
    TEST_CHECK(dec.picoseconds == orig.picoseconds);
    TEST_CHECK(dec.source == orig.source);
    TEST_CHECK(dec.flags == orig.flags);
    TEST_CHECK(dec.accuracy == orig.accuracy);
    /* sample_index is not carried in the block */
    TEST_CHECK(dec.sample_index == 0);
}


void test_time_block_negative_seconds(void)
{
    struct rig_stream_time_anchor orig =
    {
        .seconds = -1,           /* 1 s before the epoch */
        .picoseconds = 500000000000ULL,
        .source = RIG_STREAM_TIME_SRC_HOST,
        .flags = 0,
        .accuracy = RIG_STREAM_TIME_ACC_MS,
    };

    unsigned char buf[RIG_STREAM_TIME_BLOCK_SIZE];
    stream_time_block_pack(&orig, buf);

    struct rig_stream_time_anchor dec;
    TEST_CHECK(stream_time_block_unpack(buf, sizeof(buf), &dec) == 0);
    TEST_CHECK_(dec.seconds == -1, "seconds=%lld", (long long)dec.seconds);
    TEST_CHECK(dec.picoseconds == 500000000000ULL);
}


void test_time_block_short_buffer(void)
{
    unsigned char buf[RIG_STREAM_TIME_BLOCK_SIZE] = {0};
    struct rig_stream_time_anchor dec;

    TEST_CHECK(stream_time_block_unpack(buf, RIG_STREAM_TIME_BLOCK_SIZE - 1,
                                        &dec) == -1);
    TEST_CHECK(stream_time_block_unpack(buf, 0, &dec) == -1);
}


void test_time_block_wire_layout(void)
{
    struct rig_stream_time_anchor t =
    {
        .seconds = 1,
        .picoseconds = 2,
        .source = RIG_STREAM_TIME_SRC_PTP,
        .flags = RIG_STREAM_TIME_FLAG_SOB,
        .accuracy = RIG_STREAM_TIME_ACC_US,
    };

    unsigned char buf[RIG_STREAM_TIME_BLOCK_SIZE];
    stream_time_block_pack(&t, buf);

    /* Big-endian: seconds int64 at 0, picoseconds uint64 at 8,
     * source u8 at 16, flags u8 at 17, accuracy u8 at 18, reserved at 19 */
    for (int i = 0; i < 7; i++)
    {
        TEST_CHECK(buf[i] == 0);
    }

    TEST_CHECK(buf[7] == 1);

    for (int i = 8; i < 15; i++)
    {
        TEST_CHECK(buf[i] == 0);
    }

    TEST_CHECK(buf[15] == 2);
    TEST_CHECK(buf[16] == RIG_STREAM_TIME_SRC_PTP);
    TEST_CHECK(buf[17] == RIG_STREAM_TIME_FLAG_SOB);
    TEST_CHECK(buf[18] == RIG_STREAM_TIME_ACC_US);
    TEST_CHECK(buf[19] == 0);
}


void test_write_status_round_trip(void)
{
    struct rig_stream_write_status orig =
    {
        .event = RIG_STREAM_WRITE_EVENT_LATE,
        .flags = 0,
        .dropped_samples = 96,
        .lateness = 4800,
        .time_valid = 1,
        .seconds = 1736000000,
        .picoseconds = 999999999999ULL,
        .time_source = RIG_STREAM_TIME_SRC_HOST,
        .time_flags = 0,
        .time_accuracy = RIG_STREAM_TIME_ACC_MS,
    };

    unsigned char buf[RIG_STREAM_WRITE_STATUS_WIRE_SIZE];
    memset(buf, 0xAA, sizeof(buf));
    stream_write_status_pack(&orig, buf);

    struct rig_stream_write_status dec;
    memset(&dec, 0xFF, sizeof(dec));
    TEST_CHECK(stream_write_status_unpack(buf, sizeof(buf), &dec) == 0);

    TEST_CHECK(dec.event == orig.event);
    TEST_CHECK(dec.dropped_samples == orig.dropped_samples);
    TEST_CHECK_(dec.lateness == orig.lateness, "lateness=%lld",
                (long long)dec.lateness);
    TEST_CHECK(dec.time_valid == 1);
    TEST_CHECK(dec.seconds == orig.seconds);
    TEST_CHECK(dec.picoseconds == orig.picoseconds);
    TEST_CHECK(dec.time_source == orig.time_source);
    TEST_CHECK(dec.time_accuracy == orig.time_accuracy);
    TEST_CHECK(dec.sample_index == 0);   /* rides the header, not the block */
    TEST_CHECK(dec._reserved[0] == 0);
    TEST_CHECK(dec._reserved[3] == 0);
}


void test_write_status_negative_lateness(void)
{
    /* A burst that arrived early carries negative lateness. */
    struct rig_stream_write_status orig =
    {
        .event = RIG_STREAM_WRITE_EVENT_UNDERRUN,
        .seconds = -1,
        .picoseconds = 500000000000ULL,
        .lateness = -1234,
        .time_valid = 1,
    };

    unsigned char buf[RIG_STREAM_WRITE_STATUS_WIRE_SIZE];
    stream_write_status_pack(&orig, buf);

    struct rig_stream_write_status dec;
    TEST_CHECK(stream_write_status_unpack(buf, sizeof(buf), &dec) == 0);
    TEST_CHECK_(dec.seconds == -1, "seconds=%lld", (long long)dec.seconds);
    TEST_CHECK_(dec.lateness == -1234, "lateness=%lld", (long long)dec.lateness);
    TEST_CHECK(dec.event == RIG_STREAM_WRITE_EVENT_UNDERRUN);
    TEST_CHECK(dec.time_valid == 1);
}


void test_write_status_short_buffer(void)
{
    unsigned char buf[RIG_STREAM_WRITE_STATUS_WIRE_SIZE] = {0};
    struct rig_stream_write_status dec;

    TEST_CHECK(stream_write_status_unpack(buf,
                                          RIG_STREAM_WRITE_STATUS_WIRE_SIZE - 1,
                                          &dec) == -1);
    TEST_CHECK(stream_write_status_unpack(buf, 0, &dec) == -1);
}


void test_write_status_wire_layout(void)
{
    struct rig_stream_write_status st =
    {
        .event = 3,
        .flags = 0,
        .dropped_samples = 7,
        .lateness = 5,
        .seconds = 1,
        .picoseconds = 2,
        .time_source = 4,
        .time_flags = 6,
        .time_accuracy = 8,
    };

    unsigned char buf[RIG_STREAM_WRITE_STATUS_WIRE_SIZE];
    stream_write_status_pack(&st, buf);

    /* event u16 @0, flags u16 @2, dropped u32 @4, lateness i64 @8,
     * seconds i64 @16, picoseconds u64 @24, source/flags/accuracy @32..34 */
    TEST_CHECK(buf[0] == 0 && buf[1] == 3);
    TEST_CHECK(buf[2] == 0 && buf[3] == 0);   /* time_valid unset */
    TEST_CHECK(buf[4] == 0 && buf[5] == 0 && buf[6] == 0 && buf[7] == 7);
    TEST_CHECK(buf[15] == 5);   /* lateness low byte */
    TEST_CHECK(buf[23] == 1);   /* seconds low byte */
    TEST_CHECK(buf[31] == 2);   /* picoseconds low byte */
    TEST_CHECK(buf[32] == 4 && buf[33] == 6 && buf[34] == 8);
    TEST_CHECK(buf[35] == 0);
}


void test_ctrl_time_combination_rule(void)
{
    /* TIME alone or on a plain data packet is valid */
    TEST_CHECK(stream_ctrl_time_valid(RIG_STREAM_CTRL_TIME) == 1);
    TEST_CHECK(stream_ctrl_time_valid(0) == 1);
    TEST_CHECK(stream_ctrl_time_valid(RIG_STREAM_CTRL_METADATA) == 1);

    /* TIME combined with any frame that defines its own payload is not */
    TEST_CHECK(stream_ctrl_time_valid(RIG_STREAM_CTRL_TIME
                                      | RIG_STREAM_CTRL_METADATA) == 0);
    TEST_CHECK(stream_ctrl_time_valid(RIG_STREAM_CTRL_TIME
                                      | RIG_STREAM_CTRL_ERROR) == 0);
    TEST_CHECK(stream_ctrl_time_valid(RIG_STREAM_CTRL_TIME
                                      | RIG_STREAM_CTRL_SUBSCRIBE) == 0);
    TEST_CHECK(stream_ctrl_time_valid(RIG_STREAM_CTRL_TIME
                                      | RIG_STREAM_CTRL_SUBSCRIBE_ACK) == 0);
    TEST_CHECK(stream_ctrl_time_valid(RIG_STREAM_CTRL_TIME
                                      | RIG_STREAM_CTRL_PING) == 0);
    TEST_CHECK(stream_ctrl_time_valid(RIG_STREAM_CTRL_TIME
                                      | RIG_STREAM_CTRL_PONG) == 0);
}


/* The subscribe token is an anti-hijack authenticator: it must be
 * unpredictable, never the all-zero "unset" sentinel, and must not repeat
 * across calls the way an unseeded rand() sequence would. */
void test_subscribe_token_entropy(void)
{
    enum { N = 512 };
    uint32_t tokens[N];
    int i, j;
    int duplicates = 0;
    int zeros = 0;

    for (i = 0; i < N; i++)
    {
        tokens[i] = rigctld_stream_generate_token();

        if (tokens[i] == 0)
        {
            zeros++;
        }
    }

    /* Token 0 is reserved for "unset" and must never be handed out. */
    TEST_CHECK(zeros == 0);
    TEST_MSG("generator returned the reserved 0 token %d times", zeros);

    for (i = 0; i < N; i++)
    {
        for (j = i + 1; j < N; j++)
        {
            if (tokens[i] == tokens[j])
            {
                duplicates++;
            }
        }
    }

    /* 512 draws from a 32-bit space should essentially never collide; a
     * constant or low-entropy generator (e.g. unseeded rand()) would. */
    TEST_CHECK(duplicates == 0);
    TEST_MSG("token generator produced %d duplicate(s) in %d draws",
             duplicates, N);
}


/* Unconditional metadata-refresh cadence decision (feeder helper). */
void test_metadata_refresh_due(void)
{
    /* interval 0 = every data packet that carried samples */
    TEST_CHECK(rigctld_stream_metadata_refresh_due(0, 48000, 1) == 1);
    TEST_CHECK(rigctld_stream_metadata_refresh_due(0, 48000, 0) == 0);

    /* 100 ms at 48 kHz = 4800 frames: due at/after the boundary */
    TEST_CHECK(rigctld_stream_metadata_refresh_due(100, 48000, 4799) == 0);
    TEST_CHECK(rigctld_stream_metadata_refresh_due(100, 48000, 4800) == 1);
    TEST_CHECK(rigctld_stream_metadata_refresh_due(100, 48000, 100000) == 1);

    /* unknown/zero sample rate never fires */
    TEST_CHECK(rigctld_stream_metadata_refresh_due(100, 0, 1000000) == 0);
}


/* Rate-derived socket buffer sizing with floor/ceiling clamps. */
void test_transport_buffer_bytes(void)
{
    /* 96 kHz mono CF32: 96000×8×0.25 = 192000 -> clamped up to 256 KB floor */
    TEST_CHECK(stream_transport_buffer_bytes(96000, 8, 250,
               0) == RIG_STREAM_TRANSPORT_BUFFER_MIN);

    /* dual-channel CF32 at 96 kHz: 96000×16×0.25 = 384000 (above floor) */
    TEST_CHECK(stream_transport_buffer_bytes(96000, 16, 250, 0) == 384000);

    /* explicit override wins (within range) and clamps to ceiling */
    TEST_CHECK(stream_transport_buffer_bytes(48000, 4, 250, 1000000) == 1000000);
    TEST_CHECK(stream_transport_buffer_bytes(48000, 4, 250,
               (size_t)100 * 1024 * 1024)
               == RIG_STREAM_TRANSPORT_BUFFER_MAX);

    /* huge derived size clamps to the 8 MB ceiling (2 MHz × 8 × 1 s = 16 MB) */
    TEST_CHECK(stream_transport_buffer_bytes(2000000, 8, 1000, 0)
               == RIG_STREAM_TRANSPORT_BUFFER_MAX);

    /* zero/unknown rate falls back to the floor */
    TEST_CHECK(stream_transport_buffer_bytes(0, 8, 250,
               0) == RIG_STREAM_TRANSPORT_BUFFER_MIN);
}


/* Frame-aligned sender payload budget derived from a configured MTU, with
 * default (0), floor, and jumbo-ceiling clamps. */
void test_max_payload_from_mtu(void)
{
    /* mtu 0 = default 1500: 1500 - 40 - 8 - 32 = 1420, frame-aligned */
    TEST_CHECK(stream_max_payload_from_mtu(0, 4) == 1420);
    TEST_CHECK(stream_max_payload_from_mtu(0, 8) == 1416);   /* 1420/8*8 */
    TEST_CHECK(stream_max_payload_from_mtu(0, 0) == 1420);   /* no alignment */

    /* jumbo within range: 9000 - 80 = 8920 */
    TEST_CHECK(stream_max_payload_from_mtu(9000, 8) == 8920);

    /* above the ceiling clamps to RIG_STREAM_MAX_DATAGRAM (9216 - 80 = 9136) */
    TEST_CHECK(stream_max_payload_from_mtu(100000, 4) == 9136);

    /* below the floor clamps to RIG_STREAM_MIN_MTU (576 - 80 = 496) */
    TEST_CHECK(stream_max_payload_from_mtu(100, 4) == 496);
}


/* Layered transport_buffer resolution: per-stream bytes > token bytes > per-stream ms >
 * token ms > built-in 250 ms; then rate-derived + clamped [256KB, 8MB].
 * All expected values below are above the 256 KB floor. */
void test_transport_buffer_effective(void)
{
    /* per-stream explicit bytes win over everything */
    TEST_CHECK(stream_transport_buffer_effective(300, 1000000, 500, 4000000, 96000,
               16)
               == 1000000);
    /* no per-stream bytes -> rig-token bytes */
    TEST_CHECK(stream_transport_buffer_effective(0, 0, 0, 2000000, 48000,
               4) == 2000000);
    /* per-stream ms wins over token ms (96k x 16 x 300ms = 460800) */
    TEST_CHECK(stream_transport_buffer_effective(300, 0, 1000, 0, 96000,
               16) == 460800);
    /* token ms when no per-stream value (96k x 16 x 500ms = 768000) */
    TEST_CHECK(stream_transport_buffer_effective(0, 0, 500, 0, 96000,
               16) == 768000);
    /* nothing set -> built-in 250 ms (96k x 16 x 250ms = 384000) */
    TEST_CHECK(stream_transport_buffer_effective(0, 0, 0, 0, 96000, 16) == 384000);
}


/* Receive one datagram with a timeout; -1 on timeout/error. */
static ssize_t recv_timeout(int fd, unsigned char *buf, size_t len, int ms)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0)
    {
        return -1;
    }

    return recvfrom(fd, (char *)buf, len, 0, NULL, NULL);
}

/* Receive a TX_STATUS datagram and assert its header framing, returning the
 * decoded event_code (-1 on timeout). */
static int recv_write_status_event(int fd, const struct rigctld_stream *stream,
                                   struct rig_stream_write_status *out)
{
    unsigned char buf[256];
    ssize_t n = recv_timeout(fd, buf, sizeof(buf), 500);

    if (n < 0)
    {
        return -1;
    }

    TEST_CHECK(n == RIG_STREAM_HEADER_SIZE + RIG_STREAM_WRITE_STATUS_WIRE_SIZE);

    struct rig_stream_packet_header hdr;
    TEST_CHECK(stream_packet_header_unpack(buf, (size_t)n, &hdr) == 0);
    TEST_CHECK(hdr.control & RIG_STREAM_CTRL_WRITE_STATUS);
    TEST_CHECK(hdr.stream_id == stream->stream_id);
    TEST_CHECK(hdr.subscribe_token == stream->subscribe_token);
    TEST_CHECK(hdr.payload_len == RIG_STREAM_WRITE_STATUS_WIRE_SIZE);

    struct rig_stream_write_status st;
    TEST_CHECK(stream_write_status_unpack(buf + RIG_STREAM_HEADER_SIZE,
                                          (size_t)n - RIG_STREAM_HEADER_SIZE,
                                          &st) == 0);
    st.sample_index = hdr.timestamp;

    if (out) { *out = st; }

    return (int)st.event;
}

/* The server emit path: rigctld_stream_emit_write_status packs and sends one
 * well-formed WRITE_STATUS datagram per event, carrying the full per-event
 * detail (sample_index via the header, lateness/dropped in the block). */
void test_write_status_emit(void)
{
    int rx_fd = -1, rx_port = 0, tx_fd = -1, tx_port = 0;
    TEST_ASSERT(rigctld_stream_udp_socket_create(&rx_fd, &rx_port) == 0);
    TEST_ASSERT(rigctld_stream_udp_socket_create(&tx_fd, &tx_port) == 0);

    struct rigctld_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.udp_sock = tx_fd;
    stream.type = RIG_STREAM_TYPE_AUDIO_TX;
    stream.stream_id = 5;
    stream.subscribe_token = 0xDEADBEEF;
    stream.config.sample_rate = 48000;
    stream.config.channels = 1;
    stream.config.format = RIG_STREAM_FORMAT_PCM_S16;
    stream.format_id = RIG_STREAM_FMT_ID_PCM_S16;

    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin6_family = AF_INET6;
    dst.sin6_addr = in6addr_loopback;
    dst.sin6_port = htons((uint16_t)rx_port);
    memcpy(&stream.client_addr, &dst, sizeof(dst));
    stream.client_addr_len = sizeof(dst);
    stream.client_addr_known = 1;

    struct rig_stream_write_status ev;
    memset(&ev, 0, sizeof(ev));
    ev.event = RIG_STREAM_WRITE_EVENT_LATE;
    ev.sample_index = 12345;
    ev.lateness = 480;
    rigctld_stream_emit_write_status(&stream, &ev);

    struct rig_stream_write_status got;
    TEST_CHECK(recv_write_status_event(rx_fd, &stream, &got)
               == RIG_STREAM_WRITE_EVENT_LATE);
    TEST_CHECK_(got.sample_index == 12345, "sample_index=%llu",
                (unsigned long long)got.sample_index);
    TEST_CHECK_(got.lateness == 480, "lateness=%lld", (long long)got.lateness);

    ev.event = RIG_STREAM_WRITE_EVENT_OVERRUN;
    ev.dropped_samples = 64;
    rigctld_stream_emit_write_status(&stream, &ev);
    TEST_CHECK(recv_write_status_event(rx_fd, &stream, &got)
               == RIG_STREAM_WRITE_EVENT_OVERRUN);
    TEST_CHECK(got.dropped_samples == 64);

    /* Nothing more emitted. */
    unsigned char buf[64];
    TEST_CHECK(recv_timeout(rx_fd, buf, sizeof(buf), 100) == -1);

    close(rx_fd);
    close(tx_fd);
}


TEST_LIST =
{
    /* Subscribe token */
    { "subscribe_token_entropy",    test_subscribe_token_entropy },

    /* Packet header */
    { "header_pack_unpack_roundtrip", test_header_pack_unpack_roundtrip },
    { "time_block_round_trip", test_time_block_round_trip },
    { "time_block_negative_seconds", test_time_block_negative_seconds },
    { "time_block_short_buffer", test_time_block_short_buffer },
    { "time_block_wire_layout", test_time_block_wire_layout },
    { "write_status_round_trip", test_write_status_round_trip },
    { "write_status_negative_lateness", test_write_status_negative_lateness },
    { "write_status_short_buffer", test_write_status_short_buffer },
    { "write_status_wire_layout", test_write_status_wire_layout },
    { "write_status_emit", test_write_status_emit },
    { "ctrl_time_combination_rule", test_ctrl_time_combination_rule },
    { "header_big_endian_byte_order", test_header_big_endian_byte_order },
    { "source_id_derive",             test_source_id_derive },
    { "header_unpack_too_short",      test_header_unpack_too_short },
    { "header_unpack_bad_version",    test_header_unpack_bad_version },
    { "header_unpack_unknown_type_format", test_header_unpack_unknown_type_format },
    { "header_subscribe_packet",      test_header_subscribe_packet },
    { "header_metadata_frame",        test_header_metadata_frame },
    { "header_max_values",            test_header_max_values },

    /* Format index mapping */
    { "format_index_audio_roundtrip", test_format_index_audio_roundtrip },
    { "format_index_iq_roundtrip",    test_format_index_iq_roundtrip },
    { "format_index_invalid",         test_format_index_invalid },

    /* Metadata wire format */
    { "rigctld_stream_metadata_refresh_due",           test_metadata_refresh_due },
    { "transport_buffer_bytes",                  test_transport_buffer_bytes },
    { "transport_buffer_effective",              test_transport_buffer_effective },
    { "max_payload_from_mtu",           test_max_payload_from_mtu },
    { "metadata_pack_unpack_roundtrip", test_metadata_pack_unpack_roundtrip },
    { "metadata_big_endian_byte_order", test_metadata_big_endian_byte_order },
    { "metadata_unpack_too_short",      test_metadata_unpack_too_short },
    { "metadata_freq_only",             test_metadata_freq_only },

    /* Type and format name mapping */
    { "type_name_roundtrip",   test_type_name_roundtrip },
    { "format_name_roundtrip", test_format_name_roundtrip },
    { "format_bitmask_str",    test_format_bitmask_str },

    /* Stream registry */
    { "registry_init_destroy",    test_registry_init_destroy },
    { "registry_insert_lookup",   test_registry_insert_lookup },
    { "registry_remove",          test_registry_remove },
    { "registry_multiple_types",  test_registry_multiple_types },
    { "registry_close_by_client", test_registry_close_by_client },
    { "registry_insert_full",     test_registry_insert_full },
    { "stream_alloc_defaults",    test_stream_alloc_defaults },

    /* Config parsing helper */
    { "config_from_args_audio_rx", test_config_from_args_audio_rx },
    { "config_from_args_iq_rx",    test_config_from_args_iq_rx },
    { "config_from_args_audio_tx", test_config_from_args_audio_tx },
    { "config_from_args_bad_type", test_config_from_args_bad_type },
    { "config_from_args_bad_format", test_config_from_args_bad_format },
    { "config_from_args_bad_rate", test_config_from_args_bad_rate },
    { "config_from_args_null_args", test_config_from_args_null_args },

    /* Next stream ID */
    { "registry_next_id",          test_registry_next_id },

    /* UDP socket creation */
    { "udp_socket_create_happy_path", test_udp_socket_create_happy_path },
    { "udp_socket_create_unique_ports", test_udp_socket_create_unique_ports },
    { "udp_socket_create_null_fd",  test_udp_socket_create_null_fd },
    { "udp_socket_create_null_port", test_udp_socket_create_null_port },

    /* Edge cases */
    { "format_bitmask_str_exact_fit", test_format_bitmask_str_exact_fit },
    { "config_from_args_max_rate",  test_config_from_args_max_rate },
    { "registry_next_id_full",      test_registry_next_id_full },

    /* Metadata interval */
    { "registry_metadata_interval_default", test_registry_metadata_interval_default },

    /* Payload alignment */
    { "payload_alignment",              test_payload_alignment },

    /* Multicast address parser */
    { "multicast_addr_parse_ipv4",      test_multicast_addr_parse_ipv4 },
    { "multicast_addr_parse_ipv6",      test_multicast_addr_parse_ipv6 },
    { "multicast_addr_parse_invalid",   test_multicast_addr_parse_invalid },
    { "multicast_addr_parse_non_multicast", test_multicast_addr_parse_non_multicast },

    /* Multicast socket creation */
    { "multicast_socket_create_ipv4",   test_multicast_socket_create_ipv4 },
    { "multicast_socket_create_ipv6",   test_multicast_socket_create_ipv6 },

    /* Multicast registry */
    { "multicast_in_use",               test_multicast_in_use },
    { "registry_multicast_ttl_default", test_registry_multicast_ttl_default },

    /* PING/PONG control bits */
    { "header_ping_packet",         test_header_ping_packet },
    { "header_pong_packet",         test_header_pong_packet },
    { "header_ping_pong_distinct",  test_header_ping_pong_distinct },

    /* Key=value parser */
    { "kv_parse_single_pair",       test_kv_parse_single_pair },
    { "kv_parse_multiple_pairs",    test_kv_parse_multiple_pairs },
    { "kv_parse_empty_line",        test_kv_parse_empty_line },
    { "kv_parse_no_equals",         test_kv_parse_no_equals },
    { "kv_parse_max_pairs",         test_kv_parse_max_pairs },
    { "kv_parse_leading_whitespace", test_kv_parse_leading_whitespace },
    { "kv_parse_mixed_valid_invalid", test_kv_parse_mixed_valid_invalid },
    { "kv_parse_overflow_rejected",  test_kv_parse_overflow_rejected },
    { "kv_parse_empty_value",        test_kv_parse_empty_value },

    /* Network stream helpers */
    { "net_parse_open_response_valid", test_net_parse_open_response_valid },
    { "net_parse_open_response_all_lines", test_net_parse_open_response_all_lines },
    { "net_parse_open_response_negative_id", test_net_parse_open_response_negative_id },
    { "net_parse_open_response_empty", test_net_parse_open_response_empty },
    { "net_parse_open_response_non_numeric", test_net_parse_open_response_non_numeric },
    { "net_udp_connect_localhost",     test_net_udp_connect_localhost },
    { "net_udp_connect_invalid_host",  test_net_udp_connect_invalid_host },
    { "net_send_ping_packet_format",   test_net_send_ping_packet_format },
    { "jumbo_datagram_received_intact", test_jumbo_datagram_received_intact },
    { "net_send_data_packet_format",   test_net_send_data_packet_format },
    { "net_session_cleanup",           test_net_session_cleanup },

    /* stream_caps line parser */
    { "net_parse_caps_line_audio_rx",  test_net_parse_caps_line_audio_rx },
    { "net_parse_caps_line_iq_tx",     test_net_parse_caps_line_iq_tx },
    { "caps_line_roundtrip",           test_caps_line_roundtrip },
    { "caps_line_unknown_flag_skipped", test_caps_line_unknown_flag_skipped },
    { "net_parse_caps_line_native_keys", test_net_parse_caps_line_native_keys },
    { "net_parse_caps_line_missing_type", test_net_parse_caps_line_missing_type },

    { NULL, NULL }
};
