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

#define PMR171_CMD_LENGTH 8
#define PMR171_REPLY_LENGTH 24

#define GUOHE_MODE_TABLE_MAX 8  

// Common error handling macros for cached values
#define RETURN_CACHED_FREQ(rig, vfo, freq) do { \
    *(freq) = (vfo == RIG_VFO_A) ? CACHE(rig)->freqMainA : CACHE(rig)->freqMainB; \
    return RIG_OK; \
} while(0)

#define RETURN_CACHED_MODE(rig, vfo, mode, width, cachep, p) do { \
    *(mode) = (vfo == RIG_VFO_A) ? (cachep)->modeMainA : (cachep)->modeMainB; \
    *(width) = (p)->filterBW; \
    return RIG_OK; \
} while(0)

#define RETURN_CACHED_VFO(rig, vfo) do { \
    *(vfo) = CACHE(rig)->vfo; \
    return RIG_OK; \
} while(0)

#define RETURN_CACHED_PTT(rig, ptt, cachep) do { \
    *(ptt) = (cachep)->ptt; \
    return RIG_OK; \
} while(0)

// Common response validation function declarations
int validate_packet_header(const unsigned char *reply, const char *func_name);
int validate_data_length(const unsigned char *reply, int reply_size, const char *func_name);

// Keep the macro for backward compatibility
#define VALIDATE_PACKET_HEADER(reply, func_name) validate_packet_header(reply, func_name)
#define VALIDATE_DATA_LENGTH(reply, reply_size, func_name) validate_data_length(reply, reply_size, func_name)

#define VALIDATE_READ_RESULT(ret, expected, func_name) do { \
    if (ret < 0) { \
        rig_debug(RIG_DEBUG_ERR, "%s: Failed to read data, using cached values\n", func_name); \
        return -1; \
    } \
    if (ret != expected) { \
        rig_debug(RIG_DEBUG_ERR, "%s: Data read mismatch: expected %d, got %d, using cached values\n", \
                 func_name, expected, ret); \
        return -1; \
    } \
} while(0)

extern struct rig_caps pmr171_caps;
extern struct rig_caps q900_caps;

uint16_t CRC16Check(const unsigned char *buf, int len);
rmode_t guohe2rmode(unsigned char mode, const rmode_t mode_table[]);
unsigned char rmode2guohe(rmode_t mode, const rmode_t mode_table[]);
unsigned char *to_be(unsigned char data[], unsigned long long freq, unsigned int byte_len);
unsigned long long from_be(const unsigned char data[],unsigned int byte_len);

// Common response validation functions
int validate_rig_response(RIG *rig, const unsigned char *reply, int reply_size, 
                         const char *func_name);
int read_rig_response(RIG *rig, unsigned char *reply, int reply_size, 
                     const char *func_name);
int validate_freq_response(RIG *rig, const unsigned char *reply, int reply_size, 
                          const char *func_name);
int validate_mode_response(RIG *rig, const unsigned char *reply, int reply_size, 
                          const char *func_name, int min_length);

#endif // _guohetec_H_
