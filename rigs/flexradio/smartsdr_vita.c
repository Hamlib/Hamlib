/*
 *  Hamlib SmartSDR VITA-49 codec
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

/* Decoding and encoding of the VITA-49 traffic a SmartSDR radio exchanges: */
/* audio and I/Q sample packets, meter readings and panadapter FFT frames.  */
/* Everything here takes bytes and returns values, touching no socket and   */
/* no rig, so it can be exercised directly rather than only through a live  */
/* stream.                                                                  */

#include <stdint.h>
#include <string.h>

#include "hamlib/rig.h"
#include "smartsdr_vita.h"


/* ------------------------------------------------------------------ */
/* VITA-49 header parser                                               */
/* ------------------------------------------------------------------ */

int vita49_parse_header(const uint8_t *buf, int len,
                        struct vita49_header *hdr)
{
    /* The len >= VITA49_HEADER_BYTES (28) guard covers every fixed word read
     * below: word0, stream_id, class-id hi/lo, and the three timestamp words. */
    if (!buf || !hdr || len < VITA49_HEADER_BYTES)
    {
        return -1;
    }

    uint32_t word0 = ntohl(*(const uint32_t *)buf);

    hdr->packet_type   = (word0 >> 28) & 0x0F;
    hdr->class_id_flag = (word0 >> 27) & 0x01;
    hdr->trailer_flag  = (word0 >> 26) & 0x01;
    /* VITA-49 puts TSI at bits 23-22 and TSF at 21-20. Reading them two bits
     * high made every packet look like TSI_NONE, which is why this backend
     * believed the radio sent no time of day: a FLEX-8400M audio packet with
     * word0 0x38500107 carries TSI=UTC and a timestamp of 0x6A726120, a real
     * Unix epoch. */
    hdr->tsi           = (word0 >> 22) & 0x03;
    hdr->tsf           = (word0 >> 20) & 0x03;
    hdr->packet_count  = (word0 >> 16) & 0x0F;
    /* packet_size is informational only: RX payload sizing uses the actual
     * received nbytes, not this field. */
    hdr->packet_size   = word0 & 0xFFFF;

    hdr->stream_id = ntohl(*(const uint32_t *)(buf + 4));

    /* Class ID: words 2-3 (only valid if C flag set) */
    if (hdr->class_id_flag)
    {
        uint32_t hi = ntohl(*(const uint32_t *)(buf + 8));
        uint32_t lo = ntohl(*(const uint32_t *)(buf + 12));
        hdr->class_id = ((uint64_t)hi << 32) | lo;
        hdr->pcc = (uint16_t)(lo & 0xFFFF);
    }
    else
    {
        hdr->class_id = 0;
        hdr->pcc = 0;
    }

    /* Timestamps: words 4-6 */
    hdr->timestamp_int = ntohl(*(const uint32_t *)(buf + 16));

    uint32_t frac_hi = ntohl(*(const uint32_t *)(buf + 20));
    uint32_t frac_lo = ntohl(*(const uint32_t *)(buf + 24));
    hdr->timestamp_frac = ((uint64_t)frac_hi << 32) | frac_lo;

    return 0;
}


/* ------------------------------------------------------------------ */
/* Payload byte-swap: big-endian float32 → host float32                */
/* ------------------------------------------------------------------ */

void vita49_swap_float32(const uint8_t *src, float *dst, int num_samples)
{
    if (!src || !dst || num_samples <= 0)
    {
        return;
    }

    const uint32_t *in = (const uint32_t *)src;

    for (int i = 0; i < num_samples; i++)
    {
        uint32_t swapped = ntohl(in[i]);
        memcpy(&dst[i], &swapped, sizeof(float));
    }
}


/* The wire word is little-endian; on a big-endian host it needs reversing.
 * Done arithmetically so no <endian.h> is required. */
static uint32_t le32toh_portable(uint32_t v)
{
    const uint16_t probe = 1;

    if (*(const uint8_t *)&probe == 1)
    {
        return v;   /* little-endian host: already correct */
    }

    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8)
           | ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}


/* ------------------------------------------------------------------ */
/* DAX I/Q payload: little-endian float32, full scale 2^15 → ±1.0      */
/* ------------------------------------------------------------------ */

/* Unlike every other payload the radio sends, dax_iq is little-endian, and its
 * samples are counts rather than the ±1.0 the streaming API promises: the
 * radio fills a float with what is really left-justified fixed point, so full
 * scale is 32768. */
void vita49_convert_daxiq_float32(const uint8_t *src, float *dst,
                                  int num_samples)
{
    if (!src || !dst || num_samples <= 0)
    {
        return;
    }

    for (int i = 0; i < num_samples; i++)
    {
        uint32_t raw;
        float v;

        /* memcpy rather than a cast: the payload is not aligned for float. */
        memcpy(&raw, src + (size_t)i * 4, sizeof(raw));
        raw = le32toh_portable(raw);
        memcpy(&v, &raw, sizeof(v));
        dst[i] = v * VITA49_DAXIQ_FULL_SCALE_INV;
    }
}


/* ------------------------------------------------------------------ */
/* PCC 0x0123: big-endian int16 mono → host float32 stereo (L=R)       */
/* ------------------------------------------------------------------ */

void vita49_convert_s16_to_f32_stereo(const uint8_t *src, float *dst,
                                      int num_mono_samples)
{
    if (!src || !dst || num_mono_samples <= 0)
    {
        return;
    }

    for (int i = 0; i < num_mono_samples; i++)
    {
        uint16_t raw = ((uint16_t)src[2 * i] << 8) | src[2 * i + 1];
        int16_t s16;
        memcpy(&s16, &raw, sizeof(int16_t));
        float val = (float)s16 / 32768.0f;
        dst[2 * i]     = val;   /* Left */
        dst[2 * i + 1] = val;   /* Right (duplicate mono) */
    }
}


/* ------------------------------------------------------------------ */
/* TX packet builder: host float32 → VITA-49 big-endian                */
/* ------------------------------------------------------------------ */

int vita49_build_tx_packet(uint8_t *buf, int buf_len,
                           uint32_t stream_id, uint64_t class_id,
                           uint8_t packet_count,
                           const float *samples, int num_samples)
{
    if (!buf || !samples || num_samples <= 0)
    {
        return -1;
    }

    int payload_words = num_samples;
    int packet_words = VITA49_HEADER_WORDS + payload_words;
    int packet_bytes = packet_words * 4;

    if (packet_bytes > buf_len)
    {
        return -1;
    }

    uint32_t *words = (uint32_t *)buf;

    /* Word 0: type=0x1, C=1, T=0, TSI=0, TSF=0 */
    uint32_t word0 = ((uint32_t)VITA49_PKT_TYPE_IF_DATA << 28)
                     | (1u << 27)                            /* C = 1 */
                     | ((uint32_t)(packet_count & 0x0F) << 16)
                     | ((uint32_t)packet_words & 0xFFFF);
    words[0] = htonl(word0);

    /* Word 1: stream ID */
    words[1] = htonl(stream_id);

    /* Words 2-3: class ID */
    words[2] = htonl((uint32_t)(class_id >> 32));
    words[3] = htonl((uint32_t)(class_id & 0xFFFFFFFF));

    /* Words 4-6: timestamps (zeros — radio ignores on TX) */
    words[4] = 0;
    words[5] = 0;
    words[6] = 0;

    /* Payload: host float32 → big-endian */
    uint32_t *payload = &words[VITA49_HEADER_WORDS];

    for (int i = 0; i < num_samples; i++)
    {
        uint32_t raw;
        memcpy(&raw, &samples[i], sizeof(float));
        payload[i] = htonl(raw);
    }

    return packet_bytes;
}


/* ------------------------------------------------------------------ */
/* Forward declarations for internal functions                         */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Decode helpers. These are pure: they take bytes and return values,    */
/* touching no socket and no rig, so they can be tested directly rather  */
/* than only through a live stream.                                      */
/* ------------------------------------------------------------------ */

/* How many packets went missing between two 4-bit VITA counters. Zero means
 * the sequence is intact. A loss of an exact multiple of 16 is invisible --
 * the counter has wrapped to the same value -- which is a property of the
 * field, not of this function. */
unsigned smartsdr_gap_from_counter(uint8_t prev, uint8_t cur)
{
    return (unsigned)((cur - prev - 1) & 0x0F);
}


/* Header of a panadapter FFT payload: four 16-bit fields and a 32-bit frame
 * index, big-endian, followed by num_bins 16-bit bins. Returns 0 when the
 * payload is long enough to hold what the header claims. */
int smartsdr_parse_fft_header(const uint8_t *payload, int payload_bytes,
                              struct smartsdr_fft_header *out)
{
    if (payload == NULL || out == NULL || payload_bytes < 12)
    {
        return -1;
    }

    out->start_bin = (unsigned)((payload[0] << 8) | payload[1]);
    out->num_bins  = (unsigned)((payload[2] << 8) | payload[3]);
    out->bin_size  = (unsigned)((payload[4] << 8) | payload[5]);
    out->total_bins = (unsigned)((payload[6] << 8) | payload[7]);
    out->frame = ((uint32_t)payload[8] << 24) | ((uint32_t)payload[9] << 16)
                 | ((uint32_t)payload[10] << 8) | (uint32_t)payload[11];

    if (out->num_bins == 0 || out->total_bins == 0
            || out->total_bins > SMARTSDR_MAX_SPECTRUM_BINS
            || out->start_bin + out->num_bins > out->total_bins
            || payload_bytes < 12 + (int)out->num_bins * 2)
    {
        return -1;
    }

    return 0;
}


/* One FFT bin, narrowed to the 8 bits Hamlib reports. Bin values are heights
 * within the panadapter -- 0 at the top, the strongest signal -- so they are
 * inverted here, because Hamlib reports larger as stronger. */
unsigned char smartsdr_fft_bin_to_level(unsigned value, unsigned height)
{
    if (height == 0)
    {
        height = SMARTSDR_SPECTRUM_YPIXELS;
    }

    if (value > height)
    {
        value = height;
    }

    return (unsigned char)(((height - value) * 255u) / height);
}


/* The divisor that turns a meter's fixed-point reading into its unit. Derived
 * from known-good readings on hardware rather than assumed: an SWR of exactly
 * 1.00 with no carrier, a 14.03 V supply, an idle PA at 27.6 degC. */
double smartsdr_meter_divisor(int slot)
{
    switch (slot)
    {
    case SMARTSDR_MTR_PATEMP:                       return 64.0;   /* degC */

    case SMARTSDR_MTR_VOLTS: case SMARTSDR_MTR_AMPS: return 256.0;  /* V, A */

    default:                                        return 128.0;  /* dBm, dBFS, SWR */
    }
}


