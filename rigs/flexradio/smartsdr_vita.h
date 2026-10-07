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

/* Wire format of the VITA-49 traffic a SmartSDR radio exchanges, and the
 * functions that decode and encode it. Everything declared here takes bytes
 * and returns values, touching no socket and no rig. */

#ifndef SMARTSDR_VITA_H
#define SMARTSDR_VITA_H

#include <stdint.h>

#include "smartsdr_priv.h"

/* VITA-49 header: 7 words = 28 bytes. */
#define VITA49_HEADER_BYTES  28
#define VITA49_HEADER_WORDS  7

/* VITA-49 packet types. */
#define VITA49_PKT_TYPE_IF_DATA   0x1   /* IF Data with Stream ID */
#define VITA49_PKT_TYPE_EXT_DATA  0x3   /* Extension Data with Stream ID */

/* SmartSDR Packet Class Codes (lower 16 bits of class_id). */
#define VITA49_PCC_AUDIO_F32   0x03E3   /* Float32 stereo, big-endian */
#define VITA49_PCC_AUDIO_S16   0x0123   /* Int16 mono, big-endian (reduced BW) */
/* DAX I/Q, one class code per rate. These are the exception on the wire: the
 * payload is LITTLE-endian float32, where every other class code above is
 * big-endian, and its floats carry counts against a full scale of 32768.
 * Byte-swapping them, as the others need, yields denormals that read as zero. */
#define VITA49_PCC_IQ_24K      0x02E3
#define VITA49_PCC_IQ_48K      0x02E4
#define VITA49_PCC_IQ_96K      0x02E5
#define VITA49_PCC_IQ_192K     0x02E6
#define VITA49_PCC_METER       0x8002   /* meter values, (uint16 id, int16 raw)* */
#define VITA49_PCC_PAN_FFT     0x8003   /* panadapter FFT bins */

/* TX frame size: exactly 128 stereo pairs per packet. */
#define SMARTSDR_TX_STEREO_PAIRS   128
#define SMARTSDR_TX_SAMPLES        (SMARTSDR_TX_STEREO_PAIRS * 2)
#define SMARTSDR_TX_PAYLOAD_BYTES  (SMARTSDR_TX_SAMPLES * (int)sizeof(float))
#define SMARTSDR_TX_PACKET_BYTES   (VITA49_HEADER_BYTES + SMARTSDR_TX_PAYLOAD_BYTES)


/* Header of a panadapter FFT payload. */
struct smartsdr_fft_header
{
    unsigned start_bin;
    unsigned num_bins;
    unsigned bin_size;
    unsigned total_bins;
    uint32_t frame;
};

/* Parsed VITA-49 header. */
struct vita49_header
{
    uint8_t  packet_type;       /* 4 bits: 0x1 = IF_DATA, 0x3 = EXT_DATA */
    uint8_t  class_id_flag;     /* 1 bit: class ID present */
    uint8_t  trailer_flag;      /* 1 bit: trailer present */
    uint8_t  tsi;               /* 2 bits: timestamp integer type */
    uint8_t  tsf;               /* 2 bits: timestamp fractional type */
    uint8_t  packet_count;      /* 4 bits: rolling counter (unreliable) */
    uint16_t packet_size;       /* Total packet size in 32-bit words */
    uint32_t stream_id;
    uint64_t class_id;          /* Full 64-bit class ID (OUI + PCC) */
    uint16_t pcc;               /* Packet Class Code (lower 16 bits) */
    uint32_t timestamp_int;     /* Integer timestamp (seconds) */
    uint64_t timestamp_frac;    /* Fractional timestamp (sample count) */
};


/* Parse a VITA-49 header from a raw UDP packet.
 * Returns 0 on success, -1 on error (packet too short or invalid). */
extern int vita49_parse_header(const uint8_t *buf, int len,
                               struct vita49_header *hdr);

/* DAX I/Q arrives as little-endian float32 holding left-justified fixed point,
 * so full scale is 32768 rather than the 1.0 the streaming API promises. */
#define VITA49_DAXIQ_FULL_SCALE_INV (1.0f / 32768.0f)

/* Little-endian float32 DAX I/Q → host float32 normalised to ±1.0. */
extern void vita49_convert_daxiq_float32(const uint8_t *src, float *dst,
                                         int num_samples);

/* Byte-swap big-endian float32 samples to host order.
 * src and dst may not overlap. */
extern void vita49_swap_float32(const uint8_t *src, float *dst,
                                int num_samples);

/* Convert big-endian int16 mono samples to host float32 stereo (L=R).
 * Writes num_mono_samples * 2 floats to dst. */
extern void vita49_convert_s16_to_f32_stereo(const uint8_t *src, float *dst,
        int num_mono_samples);

/* Build a VITA-49 TX packet (type 0x1, IF_DATA_WITH_STREAM_ID).
 * samples[] is host-native float32, num_samples is total float count.
 * Returns total packet size in bytes, or -1 on error. */
extern int vita49_build_tx_packet(uint8_t *buf, int buf_len,
                                  uint32_t stream_id, uint64_t class_id,
                                  uint8_t packet_count,
                                  const float *samples, int num_samples);

/* How many packets went missing between two 4-bit VITA counters. */
extern unsigned smartsdr_gap_from_counter(uint8_t prev, uint8_t cur);

/* Split a panadapter FFT payload into its header fields. Returns 0 when the
 * payload is long enough to hold what the header claims. */
extern int smartsdr_parse_fft_header(const uint8_t *payload, int payload_bytes,
                                     struct smartsdr_fft_header *out);

/* One FFT bin, narrowed to the 8 bits Hamlib reports. */
extern unsigned char smartsdr_fft_bin_to_level(unsigned value, unsigned height);

/* The divisor that turns a meter's fixed-point reading into its unit. */
extern double smartsdr_meter_divisor(int slot);

#endif /* SMARTSDR_VITA_H */
