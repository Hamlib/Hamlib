/*
 *  Hamlib streaming codec tests
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

/* Device audio codec unit tests for the Hamlib streaming subsystem. */
/* Covers state lifecycle, mu-law/A-law decode+encode, sizing, reset, stereo. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"
#include "stream_codec.h"
#include "stream_convert.h"
#include <string.h>
#include <stdint.h>


/* --- Codec state lifecycle + NONE passthrough --- */

void test_open_close_none(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_NONE, 1);
    TEST_CHECK(st != NULL);
    rig_audio_codec_reset(st);   /* stateless: no-op, must not crash */
    rig_audio_codec_close(st);
    rig_audio_codec_close(NULL); /* NULL accepted */
}

void test_open_invalid_channels(void)
{
    TEST_CHECK(rig_audio_codec_open(RIG_AUDIO_CODEC_NONE, 0) == NULL);
    TEST_CHECK(rig_audio_codec_open(RIG_AUDIO_CODEC_NONE, 3) == NULL);
    TEST_CHECK(rig_audio_codec_open(RIG_AUDIO_CODEC_NONE, -1) == NULL);
}

void test_adpcm_open(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ADPCM_IMA, 1);
    TEST_CHECK(st != NULL);
    rig_audio_codec_close(st);
    /* IMA ADPCM is implemented for a single channel only. */
    TEST_CHECK(rig_audio_codec_open(RIG_AUDIO_CODEC_ADPCM_IMA, 2) == NULL);
}

void test_adpcm_roundtrip(void)
{
    struct rig_audio_codec_state *enc =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ADPCM_IMA, 1);
    struct rig_audio_codec_state *dec =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ADPCM_IMA, 1);
    int16_t src[256];
    uint8_t blk[256];
    int16_t out[300];
    size_t enc_bytes = 0;
    size_t out_bytes = 0;
    int maxerr = 0;
    int i;
    int ret;

    TEST_CHECK(enc != NULL && dec != NULL);

    /* a smooth triangle wave the predictor can track */
    for (i = 0; i < 256; i++)
    {
        int phase = i % 64;
        int tri = phase < 32 ? phase : 64 - phase;
        src[i] = (int16_t)((tri - 16) * 400);
    }

    ret = rig_audio_convert_from_pcm(enc, RIG_STREAM_FORMAT_PCM_S16,
                                     src, sizeof(src), blk, sizeof(blk),
                                     &enc_bytes);
    TEST_CHECK(ret == RIG_OK);
    /* 4-byte preamble + one nibble per sample after the first (rounded up) */
    TEST_CHECK(enc_bytes == 4 + 256 / 2);
    TEST_MSG("enc_bytes=%zu", enc_bytes);

    ret = rig_audio_convert_to_pcm(dec, blk, enc_bytes,
                                   RIG_STREAM_FORMAT_PCM_S16, out, sizeof(out),
                                   &out_bytes);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(out_bytes >= 256 * sizeof(int16_t));

    /* the first sample is carried verbatim in the block header */
    TEST_CHECK(out[0] == src[0]);

    /* ADPCM is lossy but must track the smooth signal closely */
    for (i = 0; i < 256; i++)
    {
        int e = out[i] - src[i];

        if (e < 0) { e = -e; }

        if (e > maxerr) { maxerr = e; }
    }

    TEST_CHECK(maxerr < 2000);
    TEST_MSG("max abs round-trip error = %d", maxerr);

    rig_audio_codec_close(enc);
    rig_audio_codec_close(dec);
}

void test_none_passthrough_s16(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_NONE, 1);
    TEST_CHECK(st != NULL);

    int16_t src[4] = { 0, 100, -200, 32767 };
    int16_t dst[4] = { 0 };
    size_t out = 0;
    int ret = rig_audio_convert_to_pcm(st, src, sizeof(src),
                                       RIG_STREAM_FORMAT_PCM_S16,
                                       dst, sizeof(dst), &out);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(out == sizeof(src));
    TEST_CHECK(memcmp(src, dst, sizeof(src)) == 0);
    rig_audio_codec_close(st);
}

void test_none_passthrough_convert_hop(void)
{
    /* NONE treats device bytes as the S16LE pivot; a non-S16 pcm_format
     * exercises the internal rig_stream_convert() hop. */
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_NONE, 1);
    TEST_CHECK(st != NULL);

    int16_t src[3] = { 0, 16384, -16384 };
    float dst[3] = { 0 };
    float expect[3] = { 0 };
    rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S16,
                       expect, RIG_STREAM_FORMAT_PCM_F32, 3, 1);

    size_t out = 0;
    int ret = rig_audio_convert_to_pcm(st, src, sizeof(src),
                                       RIG_STREAM_FORMAT_PCM_F32,
                                       dst, sizeof(dst), &out);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(out == sizeof(dst));
    TEST_CHECK(memcmp(expect, dst, sizeof(dst)) == 0);
    rig_audio_codec_close(st);
}


/* --- mu-law decode to PCM_S16 --- */

void test_mulaw_decode_refpoints(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);
    TEST_CHECK(st != NULL);

    uint8_t src[3] = { 0xFF, 0x80, 0x00 };
    int16_t dst[3] = { 0 };
    size_t out = 0;
    int ret = rig_audio_convert_to_pcm(st, src, sizeof(src),
                                       RIG_STREAM_FORMAT_PCM_S16,
                                       dst, sizeof(dst), &out);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(out == sizeof(dst));
    TEST_CHECK(dst[0] == 0);
    TEST_CHECK(dst[1] == 32124);
    TEST_CHECK(dst[2] == -32124);
    TEST_MSG("got {%d,%d,%d}", dst[0], dst[1], dst[2]);
    rig_audio_codec_close(st);
}

void test_mulaw_decode_null_and_small(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);
    uint8_t src[2] = { 0xFF, 0x80 };
    int16_t dst[2] = { 0 };
    size_t out = 0;

    TEST_CHECK(rig_audio_convert_to_pcm(NULL, src, sizeof(src),
                                        RIG_STREAM_FORMAT_PCM_S16,
                                        dst, sizeof(dst), &out) == -RIG_EINVAL);
    TEST_CHECK(rig_audio_convert_to_pcm(st, NULL, sizeof(src),
                                        RIG_STREAM_FORMAT_PCM_S16,
                                        dst, sizeof(dst), &out) == -RIG_EINVAL);
    TEST_CHECK(rig_audio_convert_to_pcm(st, src, sizeof(src),
                                        RIG_STREAM_FORMAT_PCM_S16,
                                        NULL, sizeof(dst), &out) == -RIG_EINVAL);
    /* buffer too small: 2 samples need 4 bytes, give 2 */
    TEST_CHECK(rig_audio_convert_to_pcm(st, src, sizeof(src),
                                        RIG_STREAM_FORMAT_PCM_S16,
                                        dst, 2, &out) == -RIG_EINVAL);
    rig_audio_codec_close(st);
}


/* --- mu-law decode to F32LE + non-PCM rejection --- */

void test_mulaw_decode_f32(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);
    uint8_t src[2] = { 0x80, 0x00 };  /* +32124, -32124 */
    float dst[2] = { 0 };
    size_t out = 0;
    int ret = rig_audio_convert_to_pcm(st, src, sizeof(src),
                                       RIG_STREAM_FORMAT_PCM_F32,
                                       dst, sizeof(dst), &out);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(out == sizeof(dst));
    TEST_CHECK(dst[0] > 0.97f && dst[0] < 0.99f);   /* 32124/32768 */
    TEST_CHECK(dst[1] < -0.97f && dst[1] > -0.99f);
    rig_audio_codec_close(st);
}

void test_decode_non_pcm_rejected(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);
    uint8_t src[2] = { 0x80, 0x00 };
    uint8_t dst[64] = { 0 };
    size_t out = 0;

    TEST_CHECK(rig_audio_convert_to_pcm(st, src, sizeof(src),
                                        RIG_STREAM_FORMAT_IQ_CS16,
                                        dst, sizeof(dst), &out) == -RIG_EINVAL);
    TEST_CHECK(rig_audio_convert_to_pcm(st, src, sizeof(src),
                                        RIG_STREAM_FORMAT_OPUS,
                                        dst, sizeof(dst), &out) == -RIG_EINVAL);
    rig_audio_codec_close(st);
}


/* --- mu-law encode + round-trip --- */

void test_mulaw_encode_refpoints(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);
    int16_t src[3] = { 0, 32124, -32124 };
    uint8_t dst[3] = { 0 };
    size_t out = 0;
    int ret = rig_audio_convert_from_pcm(st, RIG_STREAM_FORMAT_PCM_S16,
                                         src, sizeof(src),
                                         dst, sizeof(dst), &out);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(out == sizeof(dst));
    TEST_CHECK(dst[0] == 0xFF);
    TEST_CHECK(dst[1] == 0x80);
    TEST_CHECK(dst[2] == 0x00);
    TEST_MSG("got {0x%02X,0x%02X,0x%02X}", dst[0], dst[1], dst[2]);
    rig_audio_codec_close(st);
}

/* Level idempotence: every codeword decodes to a level that re-encodes to a
 * codeword decoding to the same level (handles +/-0 codeword collapse). */
void test_mulaw_roundtrip_levels(void)
{
    struct rig_audio_codec_state *dec =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);
    struct rig_audio_codec_state *enc =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);

    for (int b = 0; b < 256; b++)
    {
        uint8_t code = (uint8_t)b;
        int16_t level = 0;
        size_t out = 0;
        rig_audio_convert_to_pcm(dec, &code, 1, RIG_STREAM_FORMAT_PCM_S16,
                                 &level, sizeof(level), &out);

        uint8_t code2 = 0;
        rig_audio_convert_from_pcm(enc, RIG_STREAM_FORMAT_PCM_S16,
                                   &level, sizeof(level), &code2, 1, &out);

        int16_t level2 = 0;
        rig_audio_convert_to_pcm(dec, &code2, 1, RIG_STREAM_FORMAT_PCM_S16,
                                 &level2, sizeof(level2), &out);
        TEST_CHECK(level2 == level);
        TEST_MSG("b=0x%02X level=%d code2=0x%02X level2=%d",
                 code, level, code2, level2);
    }

    rig_audio_codec_close(dec);
    rig_audio_codec_close(enc);
}


/* --- A-law decode + encode --- */

void test_alaw_decode_refpoints(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ALAW, 1);
    uint8_t src[4] = { 0xD5, 0x55, 0xAA, 0x2A };
    int16_t dst[4] = { 0 };
    size_t out = 0;
    int ret = rig_audio_convert_to_pcm(st, src, sizeof(src),
                                       RIG_STREAM_FORMAT_PCM_S16,
                                       dst, sizeof(dst), &out);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(dst[0] == 8);
    TEST_CHECK(dst[1] == -8);
    TEST_CHECK(dst[2] == 32256);
    TEST_CHECK(dst[3] == -32256);
    TEST_MSG("got {%d,%d,%d,%d}", dst[0], dst[1], dst[2], dst[3]);
    rig_audio_codec_close(st);
}

void test_alaw_encode_refpoints(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ALAW, 1);
    int16_t src[4] = { 8, -8, 32256, -32256 };
    uint8_t dst[4] = { 0 };
    size_t out = 0;
    int ret = rig_audio_convert_from_pcm(st, RIG_STREAM_FORMAT_PCM_S16,
                                         src, sizeof(src),
                                         dst, sizeof(dst), &out);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(dst[0] == 0xD5);
    TEST_CHECK(dst[1] == 0x55);
    TEST_CHECK(dst[2] == 0xAA);
    TEST_CHECK(dst[3] == 0x2A);
    TEST_MSG("got {0x%02X,0x%02X,0x%02X,0x%02X}",
             dst[0], dst[1], dst[2], dst[3]);
    rig_audio_codec_close(st);
}

void test_alaw_roundtrip_levels(void)
{
    struct rig_audio_codec_state *dec =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ALAW, 1);
    struct rig_audio_codec_state *enc =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ALAW, 1);

    for (int b = 0; b < 256; b++)
    {
        uint8_t code = (uint8_t)b;
        int16_t level = 0;
        size_t out = 0;
        rig_audio_convert_to_pcm(dec, &code, 1, RIG_STREAM_FORMAT_PCM_S16,
                                 &level, sizeof(level), &out);
        uint8_t code2 = 0;
        rig_audio_convert_from_pcm(enc, RIG_STREAM_FORMAT_PCM_S16,
                                   &level, sizeof(level), &code2, 1, &out);
        int16_t level2 = 0;
        rig_audio_convert_to_pcm(dec, &code2, 1, RIG_STREAM_FORMAT_PCM_S16,
                                 &level2, sizeof(level2), &out);
        TEST_CHECK(level2 == level);
        TEST_MSG("b=0x%02X level=%d code2=0x%02X level2=%d",
                 code, level, code2, level2);
    }

    rig_audio_codec_close(dec);
    rig_audio_codec_close(enc);
}


/* --- sizing helpers --- */

void test_sizing_helpers(void)
{
    struct rig_audio_codec_state *mu =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);
    struct rig_audio_codec_state *none =
        rig_audio_codec_open(RIG_AUDIO_CODEC_NONE, 1);

    /* mu-law: 1 codec byte -> 1 sample */
    TEST_CHECK(rig_audio_codec_max_pcm_bytes(mu, 10,
               RIG_STREAM_FORMAT_PCM_S16) == 20);
    TEST_CHECK(rig_audio_codec_max_pcm_bytes(mu, 10,
               RIG_STREAM_FORMAT_PCM_F32) == 40);
    /* 20 bytes S16LE = 10 samples -> 10 mu-law bytes */
    TEST_CHECK(rig_audio_codec_max_encoded_bytes(mu, 20,
               RIG_STREAM_FORMAT_PCM_S16) == 10);

    /* NONE: codec byte unit is the 2-byte S16 pivot */
    TEST_CHECK(rig_audio_codec_max_pcm_bytes(none, 8,
               RIG_STREAM_FORMAT_PCM_S16) == 8);
    TEST_CHECK(rig_audio_codec_max_pcm_bytes(none, 8,
               RIG_STREAM_FORMAT_PCM_F32) == 16);

    /* invalid: non-PCM target -> 0 */
    TEST_CHECK(rig_audio_codec_max_pcm_bytes(mu, 10,
               RIG_STREAM_FORMAT_OPUS) == 0);
    TEST_CHECK(rig_audio_codec_max_pcm_bytes(NULL, 10,
               RIG_STREAM_FORMAT_PCM_S16) == 0);

    rig_audio_codec_close(mu);
    rig_audio_codec_close(none);
}


/* --- reset semantics --- */

/* Reset restores the encoder's running ADPCM step index. Only the encode side
 * can show this: every IMA block carries its own predictor and step index in
 * its header, so decoding is independent of the state, and mu-law/A-law keep
 * no state at all — a reset test built on either passes even when reset does
 * nothing. */
void test_reset_semantics(void)
{
    int16_t loud[64], probe[8];
    uint8_t fresh[256], carried[256], after_reset[256];
    size_t nf = 0, nc = 0, nr = 0;
    int i;

    /* A loud block drives the step index well away from its initial value.
     * The probe block is deliberately short: the index adapts downwards on a
     * quiet signal, and over a long block it would decay back to the floor on
     * its own, which would hide whether reset had done anything. Each encoded
     * block starts with the carried index in header byte 2, so that byte is
     * the direct evidence. */
    for (i = 0; i < 64; i++)
    {
        loud[i] = (i % 2) ? 20000 : -20000;
    }

    for (i = 0; i < 8; i++)
    {
        probe[i] = (int16_t)(i * 4);
    }

    struct rig_audio_codec_state *a =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ADPCM_IMA, 1);

    struct rig_audio_codec_state *b =
        rig_audio_codec_open(RIG_AUDIO_CODEC_ADPCM_IMA, 1);
    TEST_ASSERT(a != NULL && b != NULL);

    /* Baseline: the probe encoded from a fresh state, so its header carries
     * the initial index. */
    TEST_CHECK(rig_audio_convert_from_pcm(a, RIG_STREAM_FORMAT_PCM_S16,
                                          probe, sizeof(probe),
                                          fresh, sizeof(fresh), &nf) == RIG_OK);
    TEST_CHECK(nf > 4);

    /* The same probe after a loud block, so a different index is carried in. */
    TEST_CHECK(rig_audio_convert_from_pcm(b, RIG_STREAM_FORMAT_PCM_S16,
                                          loud, sizeof(loud),
                                          carried, sizeof(carried), &nc) == RIG_OK);
    TEST_CHECK(rig_audio_convert_from_pcm(b, RIG_STREAM_FORMAT_PCM_S16,
                                          probe, sizeof(probe),
                                          carried, sizeof(carried), &nc) == RIG_OK);

    /* Guard: if no index were carried, the assertion below would hold whatever
     * reset did, leaving this test unable to fail. */
    TEST_CHECK(carried[2] != fresh[2]);
    TEST_MSG("no index carried between blocks (got %u both times); "
             "reset is untestable here", carried[2]);

    /* After reset the probe must encode exactly as it did from fresh. */
    rig_audio_codec_reset(b);
    TEST_CHECK(rig_audio_convert_from_pcm(b, RIG_STREAM_FORMAT_PCM_S16,
                                          probe, sizeof(probe),
                                          after_reset, sizeof(after_reset),
                                          &nr) == RIG_OK);
    TEST_CHECK(after_reset[2] == fresh[2]);
    TEST_MSG("reset left step index %u, expected the initial %u",
             after_reset[2], fresh[2]);
    TEST_CHECK(nr == nf && memcmp(after_reset, fresh, nf) == 0);
    TEST_MSG("reset did not restore the initial encoder state");

    rig_audio_codec_close(a);
    rig_audio_codec_close(b);
}


/* --- stereo interleave preserved --- */

void test_stereo_mulaw_interleave(void)
{
    struct rig_audio_codec_state *st =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 2);
    TEST_CHECK(st != NULL);

    /* 2 stereo frames: L0 R0 L1 R1 */
    uint8_t src[4] = { 0xFF, 0x80, 0x00, 0xD5 };
    int16_t dst[4] = { 0 };
    size_t out = 0;
    int ret = rig_audio_convert_to_pcm(st, src, sizeof(src),
                                       RIG_STREAM_FORMAT_PCM_S16,
                                       dst, sizeof(dst), &out);
    TEST_CHECK(ret == RIG_OK);
    TEST_CHECK(out == sizeof(dst));

    /* Each codec byte decodes independently; order must be preserved. */
    struct rig_audio_codec_state *mono =
        rig_audio_codec_open(RIG_AUDIO_CODEC_MULAW, 1);
    int16_t ref[4] = { 0 };
    rig_audio_convert_to_pcm(mono, src, sizeof(src),
                             RIG_STREAM_FORMAT_PCM_S16,
                             ref, sizeof(ref), &out);
    TEST_CHECK(memcmp(dst, ref, sizeof(dst)) == 0);

    rig_audio_codec_close(st);
    rig_audio_codec_close(mono);
}


TEST_LIST =
{
    { "open_close_none",              test_open_close_none },
    { "open_invalid_channels",        test_open_invalid_channels },
    { "adpcm_open",                   test_adpcm_open },
    { "adpcm_roundtrip",              test_adpcm_roundtrip },
    { "none_passthrough_s16",         test_none_passthrough_s16 },
    { "none_passthrough_convert_hop", test_none_passthrough_convert_hop },
    { "mulaw_decode_refpoints",       test_mulaw_decode_refpoints },
    { "mulaw_decode_null_and_small",  test_mulaw_decode_null_and_small },
    { "mulaw_decode_f32",             test_mulaw_decode_f32 },
    { "decode_non_pcm_rejected",      test_decode_non_pcm_rejected },
    { "mulaw_encode_refpoints",       test_mulaw_encode_refpoints },
    { "mulaw_roundtrip_levels",       test_mulaw_roundtrip_levels },
    { "alaw_decode_refpoints",        test_alaw_decode_refpoints },
    { "alaw_encode_refpoints",        test_alaw_encode_refpoints },
    { "alaw_roundtrip_levels",        test_alaw_roundtrip_levels },
    { "sizing_helpers",               test_sizing_helpers },
    { "reset_semantics",              test_reset_semantics },
    { "stereo_mulaw_interleave",      test_stereo_mulaw_interleave },
    { NULL, NULL }
};
