/*
 *  Hamlib Interface - rig state cache routines
 *  Copyright (c) 2000-2012 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

#ifndef _CACHE_H
#define _CACHE_H

#include "hamlib/rig.h"

__BEGIN_DECLS

/* It should be noted that there are two distinct cache implementations
 * in Hamlib.  This one is for the app-facing side, providing cached status
 * (freq, mode, band, etc) for application queries. The other is used by
 * backends for rig status probes, watchdog timers, and other hardware related
 * tasks. Also note that they use different times - timespec vs timeval.
 *      - n3gb 2025-05-14
 */

/**
 * \brief Rig cache data
 *
 * This struct contains all the items we cache at the highest level
 * Replaces cache structure(s) in state
 */
struct rig_cache {
    int timeout_ms;  // the cache timeout for invalidating itself
    vfo_t vfo;
    //freq_t freq; // to be deprecated in 4.1 when full Main/Sub/A/B caching is implemented in 4.1
    // other abstraction here is based on dual vfo rigs and mapped to all others
    // So we have four possible states of rig
    // MainA, MainB, SubA, SubB
    // Main is the Main VFO and Sub is for the 2nd VFO
    // Most rigs have MainA and MainB
    // Dual VFO rigs can have SubA and SubB too
    // For dual VFO rigs simplex operations are all done on MainA/MainB -- ergo this abstraction
    freq_t freqCurr; // Other VFO
    freq_t freqOther; // Other VFO
    freq_t freqMainA; // VFO_A, VFO_MAIN, and VFO_MAINA
    freq_t freqMainB; // VFO_B, VFO_SUB, and VFO_MAINB
    freq_t freqMainC; // VFO_C, VFO_MAINC
    freq_t freqSubA;  // VFO_SUBA -- only for rigs with dual Sub VFOs
    freq_t freqSubB;  // VFO_SUBB -- only for rigs with dual Sub VFOs
    freq_t freqSubC;  // VFO_SUBC -- only for rigs with 3 Sub VFOs
    freq_t freqMem;   // VFO_MEM -- last MEM channel
    rmode_t modeCurr;
    rmode_t modeOther;
    rmode_t modeMainA;
    rmode_t modeMainB;
    rmode_t modeMainC;
    rmode_t modeSubA;
    rmode_t modeSubB;
    rmode_t modeSubC;
    rmode_t modeMem;
    pbwidth_t widthCurr; // if non-zero then rig has separate width for MainA
    pbwidth_t widthOther; // if non-zero then rig has separate width for MainA
    pbwidth_t widthMainA; // if non-zero then rig has separate width for MainA
    pbwidth_t widthMainB; // if non-zero then rig has separate width for MainB
    pbwidth_t widthMainC; // if non-zero then rig has separate width for MainC
    pbwidth_t widthSubA;  // if non-zero then rig has separate width for SubA
    pbwidth_t widthSubB;  // if non-zero then rig has separate width for SubB
    pbwidth_t widthSubC;  // if non-zero then rig has separate width for SubC
    pbwidth_t widthMem;  // if non-zero then rig has separate width for Mem
    ptt_t ptt;
    split_t split;
    vfo_t split_vfo;  // split caches two values
    struct timespec time_freqCurr;
    struct timespec time_freqOther;
    struct timespec time_freqMainA;
    struct timespec time_freqMainB;
    struct timespec time_freqMainC;
    struct timespec time_freqSubA;
    struct timespec time_freqSubB;
    struct timespec time_freqSubC;
    struct timespec time_freqMem;
    struct timespec time_vfo;
    struct timespec time_modeCurr;
    struct timespec time_modeOther;
    struct timespec time_modeMainA;
    struct timespec time_modeMainB;
    struct timespec time_modeMainC;
    struct timespec time_modeSubA;
    struct timespec time_modeSubB;
    struct timespec time_modeSubC;
    struct timespec time_modeMem;
    struct timespec time_widthCurr;
    struct timespec time_widthOther;
    struct timespec time_widthMainA;
    struct timespec time_widthMainB;
    struct timespec time_widthMainC;
    struct timespec time_widthSubA;
    struct timespec time_widthSubB;
    struct timespec time_widthSubC;
    struct timespec time_widthMem;
    struct timespec time_ptt;
    struct timespec time_split;
    int satmode; // if rig is in satellite mode
};

/* Access macros */
#define CACHE(r) ((r)->cache_addr)
//#define HAMLIB_CACHE(r) ((struct rig_cache *)rig_data_pointer(r, RIG_PTRX_CACHE))

/* Function templates
 * Does not include those marked as part of HAMLIB_API
 */
int rig_set_cache_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int rig_set_cache_freq(RIG *rig, vfo_t vfo, freq_t freq);
void rig_cache_show(RIG *rig, const char *func, int line);

__END_DECLS

#endif
