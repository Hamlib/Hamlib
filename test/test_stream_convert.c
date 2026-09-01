/*
 *  Hamlib streaming format-conversion tests
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

/* Format conversion unit tests for the Hamlib streaming subsystem. */
/* Tests sample size, direct integer, float, fallback, and channel converters. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"
#include "stream_convert.h"
#include <string.h>
#include <math.h>


/* --- Sample size --- */

void test_sample_size(void)
{
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_PCM_S8) == 1);
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_PCM_U8) == 1);
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_PCM_S16) == 2);
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_PCM_F32) == 4);
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_IQ_CU8) == 2);
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_IQ_CS8) == 2);
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_IQ_CS16) == 4);
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_IQ_CF32) == 8);
    TEST_CHECK(rig_stream_format_sample_size(RIG_STREAM_FORMAT_OPUS) == 0);
    /* Unknown format */
    TEST_CHECK(rig_stream_format_sample_size(0) == 0);
}


/* --- Same format memcpy --- */

void test_same_format_memcpy(void)
{
    int16_t src[4] = { 100, -200, 32767, -32768 };
    int16_t dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S16,
                                 dst, RIG_STREAM_FORMAT_PCM_S16,
                                 4, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(memcmp(src, dst, sizeof(src)) == 0);
}


/* --- Direct integer converters --- */

void test_s8_to_s16(void)
{
    int8_t src[3] = { 0, 127, -128 };
    int16_t dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S8,
                                 dst, RIG_STREAM_FORMAT_PCM_S16,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0);
    TEST_CHECK(dst[1] == (127 << 8));      /* 32512 */
    TEST_CHECK(dst[2] == -(128 << 8));     /* -32768 */
    TEST_MSG("dst: %d %d %d", dst[0], dst[1], dst[2]);
}

void test_u8_to_s16(void)
{
    uint8_t src[3] = { 0, 128, 255 };
    int16_t dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_U8,
                                 dst, RIG_STREAM_FORMAT_PCM_S16,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == -32768);          /* (0 - 128) << 8 */
    TEST_CHECK(dst[1] == 0);              /* (128 - 128) << 8 */
    TEST_CHECK(dst[2] == 32512);          /* (255 - 128) << 8 = 127 << 8 */
    TEST_MSG("dst: %d %d %d", dst[0], dst[1], dst[2]);
}

void test_s16_to_s8(void)
{
    int16_t src[3] = { 0, 32767, -32768 };
    int8_t dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S16,
                                 dst, RIG_STREAM_FORMAT_PCM_S8,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0);
    TEST_CHECK(dst[1] == 127);            /* 32767 >> 8 */
    TEST_CHECK(dst[2] == -128);           /* -32768 >> 8 */
    TEST_MSG("dst: %d %d %d", dst[0], dst[1], dst[2]);
}

void test_s16_to_u8(void)
{
    int16_t src[3] = { -32768, 0, 32767 };
    uint8_t dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S16,
                                 dst, RIG_STREAM_FORMAT_PCM_U8,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0);             /* (-128 + 128) */
    TEST_CHECK(dst[1] == 128);           /* (0 + 128) */
    TEST_CHECK(dst[2] == 255);           /* (127 + 128) */
    TEST_MSG("dst: %u %u %u", dst[0], dst[1], dst[2]);
}

void test_cu8_to_cs8(void)
{
    /* CU8: pairs of uint8 (I, Q). Subtract 128 to get signed. */
    uint8_t src[6] = { 128, 128,    /* 0, 0 */
                       0, 0,        /* -128, -128 */
                       255, 255
                     };  /* 127, 127 */
    int8_t dst[6] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CU8,
                                 dst, RIG_STREAM_FORMAT_IQ_CS8,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0 && dst[1] == 0);
    TEST_CHECK(dst[2] == -128 && dst[3] == -128);
    TEST_CHECK(dst[4] == 127 && dst[5] == 127);
}

void test_cs8_to_cu8(void)
{
    int8_t src[4] = { 0, 0, 127, -128 };
    uint8_t dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CS8,
                                 dst, RIG_STREAM_FORMAT_IQ_CU8,
                                 2, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 128 && dst[1] == 128);
    TEST_CHECK(dst[2] == 255 && dst[3] == 0);
}

void test_cu8_to_cs16(void)
{
    uint8_t src[4] = { 128, 0,     /* I=0, Q=-128 after offset */
                       255, 1
                     };   /* I=127, Q=-127 after offset */
    int16_t dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CU8,
                                 dst, RIG_STREAM_FORMAT_IQ_CS16,
                                 2, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0);              /* (128-128) << 8 */
    TEST_CHECK(dst[1] == -(128 << 8));    /* (0-128) << 8 = -32768 */
    TEST_CHECK(dst[2] == (127 << 8));     /* (255-128) << 8 = 32512 */
    TEST_CHECK(dst[3] == -(127 << 8));    /* (1-128) << 8 = -32512 */
    TEST_MSG("dst: %d %d %d %d", dst[0], dst[1], dst[2], dst[3]);
}

void test_cs8_to_cs16(void)
{
    int8_t src[4] = { 0, 127, -128, -1 };
    int16_t dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CS8,
                                 dst, RIG_STREAM_FORMAT_IQ_CS16,
                                 2, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0);
    TEST_CHECK(dst[1] == (127 << 8));     /* 32512 */
    TEST_CHECK(dst[2] == -(128 << 8));    /* -32768 */
    TEST_CHECK(dst[3] == -(1 << 8));      /* -256 */
}

void test_cs16_to_cs8(void)
{
    int16_t src[4] = { 0, 32767, -32768, -256 };
    int8_t dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CS16,
                                 dst, RIG_STREAM_FORMAT_IQ_CS8,
                                 2, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0);
    TEST_CHECK(dst[1] == 127);            /* 32767 >> 8 */
    TEST_CHECK(dst[2] == -128);           /* -32768 >> 8 */
    TEST_CHECK(dst[3] == -1);             /* -256 >> 8 */
}

void test_cs16_to_cu8(void)
{
    int16_t src[4] = { 0, 32512, -32768, -32512 };
    uint8_t dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CS16,
                                 dst, RIG_STREAM_FORMAT_IQ_CU8,
                                 2, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 128);            /* (0>>8) + 128 */
    TEST_CHECK(dst[1] == 255);            /* (32512>>8=127) + 128 */
    TEST_CHECK(dst[2] == 0);              /* (-32768>>8=-128) + 128 */
    TEST_CHECK(dst[3] == 1);              /* (-32512>>8=-127) + 128 */
    TEST_MSG("dst: %u %u %u %u", dst[0], dst[1], dst[2], dst[3]);
}


/* --- Direct float converters --- */

void test_s16_to_f32(void)
{
    int16_t src[3] = { 0, 32767, -32768 };
    float dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S16,
                                 dst, RIG_STREAM_FORMAT_PCM_F32,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(fabsf(dst[0] - 0.0f) < 1e-6f);
    TEST_CHECK(fabsf(dst[1] - (32767.0f / 32768.0f)) < 1e-4f);
    TEST_CHECK(fabsf(dst[2] - (-1.0f)) < 1e-6f);
    TEST_MSG("dst: %f %f %f", dst[0], dst[1], dst[2]);
}

void test_f32_to_s16(void)
{
    float src[4] = { 0.0f, 1.0f, -1.0f, 0.5f };
    int16_t dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_F32,
                                 dst, RIG_STREAM_FORMAT_PCM_S16,
                                 4, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0);
    TEST_CHECK(dst[1] == 32767);   /* 1.0 * 32768 = 32768, clamped */
    TEST_CHECK(dst[2] == -32768);  /* -1.0 * 32768 = full-scale negative */
    TEST_CHECK(dst[3] == 16384);   /* 0.5 * 32768, exact */
    TEST_MSG("dst: %d %d %d %d", dst[0], dst[1], dst[2], dst[3]);
}

void test_f32_to_s16_clipping(void)
{
    /* Values beyond +/-1.0 must clamp to the int16 range. The converter
     * uses the SYMMETRIC power-of-two scale (v * 32768, the inverse of
     * the s16 -> f32 direction) with round-to-nearest, so integer ->
     * float -> integer round-trips are value-exact:
     *   0.0  -> 0
     *   1.0  -> 32768 clamped to 32767
     *  -1.0  -> -32768 (exact full-scale)
     *   2.0  -> 65536 clamped to 32767
     *  -2.0  -> -65536 clamped to -32768
     *   0.5  -> 16384 (exact)
     */
    float src[6] = { 0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 0.5f };
    int16_t dst[6] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_F32,
                                 dst, RIG_STREAM_FORMAT_PCM_S16,
                                 6, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0);
    TEST_CHECK(dst[1] == 32767);
    TEST_CHECK(dst[2] == -32768);
    TEST_CHECK(dst[3] == 32767);     /* clipped from 65536 */
    TEST_CHECK(dst[4] == -32768);    /* clipped from -65536 */
    TEST_CHECK(dst[5] == 16384);
    TEST_MSG("dst: %d %d %d %d %d %d",
             dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
}

void test_roundtrip_s16_f32(void)
{
    int16_t src[4] = { 0, 1000, -1000, 32767 };
    float mid[4] = { 0 };
    int16_t dst[4] = { 0 };

    rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S16,
                       mid, RIG_STREAM_FORMAT_PCM_F32, 4, 1);
    rig_stream_convert(mid, RIG_STREAM_FORMAT_PCM_F32,
                       dst, RIG_STREAM_FORMAT_PCM_S16, 4, 1);

    for (int i = 0; i < 4; i++)
    {
        TEST_CHECK(abs(src[i] - dst[i]) <= 1);
        TEST_MSG("sample %d: src=%d dst=%d", i, src[i], dst[i]);
    }
}

void test_cs16_to_cf32(void)
{
    int16_t src[4] = { 0, 32767, -32768, -1 };
    float dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CS16,
                                 dst, RIG_STREAM_FORMAT_IQ_CF32,
                                 2, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(fabsf(dst[0] - 0.0f) < 1e-6f);
    TEST_CHECK(fabsf(dst[1] - (32767.0f / 32768.0f)) < 1e-4f);
    TEST_CHECK(fabsf(dst[2] - (-1.0f)) < 1e-6f);
    TEST_CHECK(fabsf(dst[3] - (-1.0f / 32768.0f)) < 1e-4f);
}

void test_roundtrip_cs16_cf32(void)
{
    int16_t src[4] = { 100, -100, 32000, -32000 };
    float mid[4] = { 0 };
    int16_t dst[4] = { 0 };

    rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CS16,
                       mid, RIG_STREAM_FORMAT_IQ_CF32, 2, 1);
    rig_stream_convert(mid, RIG_STREAM_FORMAT_IQ_CF32,
                       dst, RIG_STREAM_FORMAT_IQ_CS16, 2, 1);

    for (int i = 0; i < 4; i++)
    {
        TEST_CHECK(abs(src[i] - dst[i]) <= 1);
        TEST_MSG("component %d: src=%d dst=%d", i, src[i], dst[i]);
    }
}

void test_cu8_to_cf32(void)
{
    uint8_t src[6] = { 128, 128,   /* 0.0, 0.0 */
                       0, 0,       /* -1.0, -1.0 */
                       255, 255
                     }; /* ~1.0, ~1.0 */
    float dst[6] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CU8,
                                 dst, RIG_STREAM_FORMAT_IQ_CF32,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(fabsf(dst[0] - 0.0f) < 0.01f);
    TEST_CHECK(fabsf(dst[1] - 0.0f) < 0.01f);
    TEST_CHECK(fabsf(dst[2] - (-1.0f)) < 0.01f);
    TEST_CHECK(fabsf(dst[3] - (-1.0f)) < 0.01f);
    TEST_CHECK(fabsf(dst[4] - (127.0f / 128.0f)) < 0.01f);
    TEST_CHECK(fabsf(dst[5] - (127.0f / 128.0f)) < 0.01f);
}

void test_cs8_to_cf32(void)
{
    int8_t src[4] = { 0, 127, -128, -1 };
    float dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_IQ_CS8,
                                 dst, RIG_STREAM_FORMAT_IQ_CF32,
                                 2, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(fabsf(dst[0] - 0.0f) < 0.01f);
    TEST_CHECK(fabsf(dst[1] - (127.0f / 128.0f)) < 0.01f);
    TEST_CHECK(fabsf(dst[2] - (-1.0f)) < 0.01f);
    TEST_CHECK(fabsf(dst[3] - (-1.0f / 128.0f)) < 0.01f);
}

void test_s8_to_f32(void)
{
    int8_t src[3] = { 0, 127, -128 };
    float dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S8,
                                 dst, RIG_STREAM_FORMAT_PCM_F32,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(fabsf(dst[0] - 0.0f) < 0.01f);
    TEST_CHECK(fabsf(dst[1] - (127.0f / 128.0f)) < 0.01f);
    TEST_CHECK(fabsf(dst[2] - (-1.0f)) < 0.01f);
}

void test_u8_to_f32(void)
{
    uint8_t src[3] = { 0, 128, 255 };
    float dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_U8,
                                 dst, RIG_STREAM_FORMAT_PCM_F32,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(fabsf(dst[0] - (-1.0f)) < 0.01f);
    TEST_CHECK(fabsf(dst[1] - 0.0f) < 0.01f);
    TEST_CHECK(fabsf(dst[2] - (127.0f / 128.0f)) < 0.01f);
}


/* --- Direct converters: S8<->U8 --- */

void test_s8_to_u8(void)
{
    int8_t src[3] = { 0, 127, -128 };
    uint8_t dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S8,
                                 dst, RIG_STREAM_FORMAT_PCM_U8,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 128);           /* 0 + 128 */
    TEST_CHECK(dst[1] == 255);           /* 127 + 128 */
    TEST_CHECK(dst[2] == 0);             /* -128 + 128 */
    TEST_MSG("dst: %u %u %u", dst[0], dst[1], dst[2]);
}

void test_u8_to_s8(void)
{
    uint8_t src[3] = { 0, 128, 255 };
    int8_t dst[3] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_U8,
                                 dst, RIG_STREAM_FORMAT_PCM_S8,
                                 3, 1);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == -128);          /* 0 - 128 */
    TEST_CHECK(dst[1] == 0);             /* 128 - 128 */
    TEST_CHECK(dst[2] == 127);           /* 255 - 128 */
    TEST_MSG("dst: %d %d %d", dst[0], dst[1], dst[2]);
}

void test_s8_u8_roundtrip(void)
{
    int8_t src[4] = { 0, 1, 127, -128 };
    uint8_t mid[4] = { 0 };
    int8_t dst[4] = { 0 };

    rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S8,
                       mid, RIG_STREAM_FORMAT_PCM_U8, 4, 1);
    rig_stream_convert(mid, RIG_STREAM_FORMAT_PCM_U8,
                       dst, RIG_STREAM_FORMAT_PCM_S8, 4, 1);
    TEST_CHECK(memcmp(src, dst, sizeof(src)) == 0);
    TEST_MSG("Roundtrip S8->U8->S8 failed");
}


/* --- Multichannel --- */

void test_multichannel(void)
{
    /* Stereo S16 -> F32 */
    int16_t src[4] = { 0, 32767, -32768, 16384 };
    float dst[4] = { 0 };
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S16,
                                 dst, RIG_STREAM_FORMAT_PCM_F32,
                                 2, 2);  /* 2 samples, 2 channels */
    TEST_CHECK(ret == 0);
    TEST_CHECK(fabsf(dst[0] - 0.0f) < 1e-6f);
    TEST_CHECK(fabsf(dst[1] - (32767.0f / 32768.0f)) < 1e-4f);
    TEST_CHECK(fabsf(dst[2] - (-1.0f)) < 1e-6f);
    TEST_CHECK(fabsf(dst[3] - (16384.0f / 32768.0f)) < 1e-4f);
}


/* --- Unsupported conversion --- */

void test_unsupported_conversion(void)
{
    int16_t src[2] = { 0, 0 };
    float dst[4] = { 0 };
    /* Audio -> I/Q cross-conversion not supported */
    int ret = rig_stream_convert(src, RIG_STREAM_FORMAT_PCM_S16,
                                 dst, RIG_STREAM_FORMAT_IQ_CF32,
                                 1, 1);
    TEST_CHECK(ret == -1);
}


/* --- Channel conversion --- */

void test_mono_to_stereo_s16(void)
{
    int16_t src[3] = { 100, -200, 32767 };
    int16_t dst[6] = { 0 };
    int ret = rig_stream_convert_channels(src, 1, dst, 2, 3,
                                          RIG_STREAM_FORMAT_PCM_S16);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 100 && dst[1] == 100);
    TEST_CHECK(dst[2] == -200 && dst[3] == -200);
    TEST_CHECK(dst[4] == 32767 && dst[5] == 32767);
}

void test_stereo_to_mono_s16(void)
{
    int16_t src[6] = { 100, 200, 32767, 32767, -32768, -32768 };
    int16_t dst[3] = { 0 };
    int ret = rig_stream_convert_channels(src, 2, dst, 1, 3,
                                          RIG_STREAM_FORMAT_PCM_S16);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 150);           /* (100+200)/2 */
    TEST_CHECK(dst[1] == 32767);         /* (32767+32767)/2 = 32767 */
    TEST_CHECK(dst[2] == -32768);        /* (-32768+-32768)/2 = -32768 */
}

void test_mono_to_stereo_f32(void)
{
    float src[2] = { 0.5f, -0.5f };
    float dst[4] = { 0 };
    int ret = rig_stream_convert_channels(src, 1, dst, 2, 2,
                                          RIG_STREAM_FORMAT_PCM_F32);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 0.5f && dst[1] == 0.5f);
    TEST_CHECK(dst[2] == -0.5f && dst[3] == -0.5f);
}

void test_stereo_to_mono_f32(void)
{
    float src[4] = { 0.5f, 0.3f, -1.0f, 1.0f };
    float dst[2] = { 0 };
    int ret = rig_stream_convert_channels(src, 2, dst, 1, 2,
                                          RIG_STREAM_FORMAT_PCM_F32);
    TEST_CHECK(ret == 0);
    TEST_CHECK(fabsf(dst[0] - 0.4f) < 1e-6f);   /* (0.5+0.3)/2 */
    TEST_CHECK(fabsf(dst[1] - 0.0f) < 1e-6f);   /* (-1.0+1.0)/2 */
}

void test_stereo_to_mono_u8(void)
{
    uint8_t src[4] = { 100, 200, 0, 0 };
    uint8_t dst[2] = { 0 };
    int ret = rig_stream_convert_channels(src, 2, dst, 1, 2,
                                          RIG_STREAM_FORMAT_PCM_U8);
    TEST_CHECK(ret == 0);
    TEST_CHECK(dst[0] == 150);           /* (100+200)/2 */
    TEST_CHECK(dst[1] == 0);             /* (0+0)/2 */
}

void test_channel_convert_noop(void)
{
    int16_t src[2] = { 100, 200 };
    int16_t dst[2] = { 0 };
    int ret = rig_stream_convert_channels(src, 2, dst, 2, 1,
                                          RIG_STREAM_FORMAT_PCM_S16);
    TEST_CHECK(ret == -1);  /* Same channel count is a no-op error */
}

void test_channel_convert_unsupported(void)
{
    int16_t src[6] = { 0 };
    int16_t dst[2] = { 0 };
    int ret = rig_stream_convert_channels(src, 3, dst, 1, 2,
                                          RIG_STREAM_FORMAT_PCM_S16);
    TEST_CHECK(ret == -1);  /* 3-channel not supported */
}


/* NULL src/dst must be rejected, not dereferenced (the same-format path
 * memcpy's them, so an unguarded call would crash). */
void test_convert_null_args(void)
{
    int16_t buf[4] = { 1, 2, 3, 4 };

    TEST_CHECK(rig_stream_convert(NULL, RIG_STREAM_FORMAT_PCM_S16,
                                  buf, RIG_STREAM_FORMAT_PCM_S16,
                                  4, 1) == -1);
    TEST_CHECK(rig_stream_convert(buf, RIG_STREAM_FORMAT_PCM_S16,
                                  NULL, RIG_STREAM_FORMAT_PCM_S16,
                                  4, 1) == -1);
}

void test_channel_convert_null_args(void)
{
    int16_t buf[4] = { 1, 2, 3, 4 };

    TEST_CHECK(rig_stream_convert_channels(NULL, 2, buf, 1, 2,
                                           RIG_STREAM_FORMAT_PCM_S16) == -1);
    TEST_CHECK(rig_stream_convert_channels(buf, 2, NULL, 1, 2,
                                           RIG_STREAM_FORMAT_PCM_S16) == -1);
}


/* --- Resample --- */

void test_resample_upsample(void)
{
    /* 10 samples at 8kHz -> ~60 samples at 48kHz (6:1 ratio) */
    float src[10];
    float dst[120] = { 0 };
    size_t dst_samples = 120;

    /* Fill source with a simple ramp */
    for (int i = 0; i < 10; i++)
    {
        src[i] = (float)i / 9.0f;  /* 0.0 to 1.0 */
    }

    int ret = rig_stream_resample(src, 8000, dst, 48000,
                                  10, &dst_samples, 1,
                                  RIG_RESAMPLE_FAST);
#ifdef HAVE_SAMPLERATE
    TEST_CHECK(ret == 0);
    TEST_MSG("upsample returned %d", ret);
    /* Output should be ~60 samples */
    TEST_CHECK(dst_samples >= 55 && dst_samples <= 65);
    TEST_MSG("dst_samples = %zu (expected ~60)", dst_samples);
    /* Mid-signal value should be near 0.5 (middle of input ramp) */
    size_t mid = dst_samples / 2;
    TEST_CHECK(fabsf(dst[mid] - 0.5f) < 0.1f);
    TEST_MSG("dst[mid=%zu]=%f (expected ~0.5)", mid, dst[mid]);
#else
    TEST_CHECK(ret < 0);
    TEST_MSG("libsamplerate not available, stub returns error");
#endif
}

void test_resample_downsample(void)
{
    /* 60 samples at 48kHz -> ~10 samples at 8kHz (1:6 ratio) */
    float src[60];
    float dst[30] = { 0 };
    size_t dst_samples = 30;

    /* Fill source with a ramp */
    for (int i = 0; i < 60; i++)
    {
        src[i] = (float)i / 59.0f;  /* 0.0 to 1.0 */
    }

    int ret = rig_stream_resample(src, 48000, dst, 8000,
                                  60, &dst_samples, 1,
                                  RIG_RESAMPLE_FAST);
#ifdef HAVE_SAMPLERATE
    TEST_CHECK(ret == 0);
    TEST_MSG("downsample returned %d", ret);
    /* Output should be ~10 samples */
    TEST_CHECK(dst_samples >= 8 && dst_samples <= 12);
    TEST_MSG("dst_samples = %zu (expected ~10)", dst_samples);
    /* Endpoints should approximate source endpoints */
    TEST_CHECK(fabsf(dst[0] - 0.0f) < 0.15f);
#else
    TEST_CHECK(ret < 0);
    TEST_MSG("libsamplerate not available, stub returns error");
#endif
}

void test_resample_identity(void)
{
    /* Same rate: output should closely match input.
     * Use enough samples for the SINC filter to settle. */
    float src[256];
    float dst[512] = { 0 };
    size_t dst_samples = 512;

    for (int i = 0; i < 256; i++)
    {
        src[i] = (float)i / 255.0f;
    }

    int ret = rig_stream_resample(src, 48000, dst, 48000,
                                  256, &dst_samples, 1,
                                  RIG_RESAMPLE_FAST);
#ifdef HAVE_SAMPLERATE
    TEST_CHECK(ret == 0);
    TEST_MSG("identity resample returned %d", ret);
    TEST_CHECK(dst_samples == 256);
    TEST_MSG("dst_samples = %zu (expected 256)", dst_samples);

    /* Check interior samples (skip first/last 8 for filter edge effects) */
    for (int i = 8; i < 248; i++)
    {
        TEST_CHECK(fabsf(src[i] - dst[i]) < 0.01f);
        TEST_MSG("sample %d: src=%f dst=%f", i, src[i], dst[i]);
    }

#else
    TEST_CHECK(ret < 0);
    TEST_MSG("libsamplerate not available, stub returns error");
#endif
}


void test_resample_quality_levels(void)
{
    /* Verify all three quality levels produce valid output */
    float src[64];
    size_t dst_samples;
    int ret;

    for (int i = 0; i < 64; i++)
    {
        src[i] = (float)i / 63.0f;
    }

    int qualities[] = { RIG_RESAMPLE_BEST, RIG_RESAMPLE_MEDIUM,
                        RIG_RESAMPLE_FAST
                      };

    for (int q = 0; q < 3; q++)
    {
        float dst[256] = { 0 };
        dst_samples = 256;
        ret = rig_stream_resample(src, 16000, dst, 48000,
                                  64, &dst_samples, 1, qualities[q]);
#ifdef HAVE_SAMPLERATE
        TEST_CHECK(ret == 0);
        TEST_MSG("quality %d returned %d", qualities[q], ret);
        /* 3:1 ratio, expect ~192 samples */
        TEST_CHECK(dst_samples >= 180 && dst_samples <= 200);
        TEST_MSG("quality %d: dst_samples=%zu (expected ~192)",
                 qualities[q], dst_samples);
#else
        TEST_CHECK(ret < 0);
#endif
    }
}


TEST_LIST =
{
    { "sample_size",              test_sample_size },
    { "same_format_memcpy",       test_same_format_memcpy },
    /* Direct integer converters */
    { "s8_to_s16",                test_s8_to_s16 },
    { "u8_to_s16",                test_u8_to_s16 },
    { "s16_to_s8",                test_s16_to_s8 },
    { "s16_to_u8",                test_s16_to_u8 },
    { "cu8_to_cs8",               test_cu8_to_cs8 },
    { "cs8_to_cu8",               test_cs8_to_cu8 },
    { "cu8_to_cs16",              test_cu8_to_cs16 },
    { "cs8_to_cs16",              test_cs8_to_cs16 },
    { "cs16_to_cs8",              test_cs16_to_cs8 },
    { "cs16_to_cu8",              test_cs16_to_cu8 },
    /* Direct float */
    { "s16_to_f32",               test_s16_to_f32 },
    { "f32_to_s16",               test_f32_to_s16 },
    { "f32_to_s16_clipping",      test_f32_to_s16_clipping },
    { "roundtrip_s16_f32",        test_roundtrip_s16_f32 },
    { "cs16_to_cf32",             test_cs16_to_cf32 },
    { "roundtrip_cs16_cf32",      test_roundtrip_cs16_cf32 },
    { "cu8_to_cf32",              test_cu8_to_cf32 },
    { "cs8_to_cf32",              test_cs8_to_cf32 },
    { "s8_to_f32",                test_s8_to_f32 },
    { "u8_to_f32",                test_u8_to_f32 },
    /* Direct converters: S8<->U8 */
    { "s8_to_u8",                 test_s8_to_u8 },
    { "u8_to_s8",                 test_u8_to_s8 },
    { "s8_u8_roundtrip",          test_s8_u8_roundtrip },
    /* Multichannel and dispatch */
    { "multichannel",             test_multichannel },
    { "unsupported_conversion",   test_unsupported_conversion },
    /* Channel conversion */
    { "mono_to_stereo_s16",       test_mono_to_stereo_s16 },
    { "stereo_to_mono_s16",       test_stereo_to_mono_s16 },
    { "mono_to_stereo_f32",       test_mono_to_stereo_f32 },
    { "stereo_to_mono_f32",       test_stereo_to_mono_f32 },
    { "stereo_to_mono_u8",        test_stereo_to_mono_u8 },
    { "channel_convert_noop",     test_channel_convert_noop },
    { "channel_convert_unsupported", test_channel_convert_unsupported },
    { "convert_null_args",        test_convert_null_args },
    { "channel_convert_null_args", test_channel_convert_null_args },
    /* Resample */
    { "resample_upsample",        test_resample_upsample },
    { "resample_downsample",      test_resample_downsample },
    { "resample_identity",        test_resample_identity },
    { "resample_quality_levels",  test_resample_quality_levels },
    { NULL, NULL }
};
