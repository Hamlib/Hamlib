/*
 *  Hamlib GUOHETEC backend - common header file
 *  Copyright (c) 2024 by GUOHETEC
 *
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

// rigs/guohetec/guohetec.h
#ifndef _guohetec_H_
#define _guohetec_H_

#include "hamlib/rig.h"
#include "cache.h"

#define GUOHE_STATUS_CMD_LENGTH 8
#define GUOHE_MAX_FRAME_LENGTH 260

#define GUOHE_MODE_TABLE_MAX 8

static inline void guohetec_get_cached_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    rig_get_cache_freq(rig, vfo, freq, NULL);
}

static inline void guohetec_get_cached_mode(RIG *rig, vfo_t vfo,
        rmode_t *mode)
{
    freq_t freq;
    pbwidth_t width;
    int cache_ms_freq;
    int cache_ms_mode;
    int cache_ms_width;

    rig_get_cache(rig, vfo, &freq, &cache_ms_freq, mode, &cache_ms_mode,
                  &width, &cache_ms_width);
}

static inline void guohetec_get_cached_vfo(RIG *rig, vfo_t *vfo)
{
    int cache_ms;
    int timeout_ms;

    rig_get_cached_vfo(rig, vfo, &cache_ms, &timeout_ms);
}

static inline void guohetec_get_cached_ptt(RIG *rig, ptt_t *ptt)
{
    int cache_ms;
    int timeout_ms;

    rig_get_cache_ptt(rig, ptt, &cache_ms, &timeout_ms);
}

// Common error handling macros for cached values
#define RETURN_CACHED_FREQ(rig, vfo, freq) do { \
    guohetec_get_cached_freq((rig), (vfo), (freq)); \
    return RIG_OK; \
} while (0)

#define RETURN_CACHED_MODE(rig, vfo, mode, width, p) do { \
    guohetec_get_cached_mode((rig), (vfo), (mode)); \
    *(width) = (p)->filterBW; \
    return RIG_OK; \
} while (0)

#define RETURN_CACHED_VFO(rig, vfo) do { \
    guohetec_get_cached_vfo((rig), (vfo)); \
    return RIG_OK; \
} while (0)

#define RETURN_CACHED_PTT(rig, ptt) do { \
    guohetec_get_cached_ptt((rig), (ptt)); \
    return RIG_OK; \
} while (0)

extern struct rig_caps pmr171_caps;
extern struct rig_caps q900_caps;

struct guohetec_status
{
    unsigned char ptt;
    unsigned char mode_a;
    unsigned char mode_b;
    uint32_t freq_a;
    uint32_t freq_b;
    vfo_t vfo;
};

uint16_t CRC16Check(const unsigned char *buf, int len);
rmode_t guohe2rmode(unsigned char mode, const rmode_t mode_table[]);
unsigned char rmode2guohe(rmode_t mode, const rmode_t mode_table[]);
unsigned char *to_be(unsigned char data[], unsigned long long freq, unsigned int byte_len);
unsigned long long from_be(const unsigned char data[],unsigned int byte_len);

int guohetec_decode_status(const unsigned char *reply, size_t reply_size,
                           struct guohetec_status *status,
                           const char *func_name);
int guohetec_read_response(RIG *rig, unsigned char *reply, size_t reply_size,
                           const char *func_name);
int guohetec_get_status(RIG *rig, struct guohetec_status *status,
                        const char *func_name);

#endif // _guohetec_H_
