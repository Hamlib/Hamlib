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

/*
 * Coherent copy of the application-facing cache values. All fields are
 * captured while holding the cache mutex, but the rig may change after the
 * snapshot is returned. Freshness timestamps remain private to the cache.
 *
 * current_vfo is Hamlib's working routing assumption. observed_vfo is the
 * last VFO selection confirmed through a rig observation and has separately
 * tracked freshness. The Curr, Other, Main, Sub, A, B, C, and Mem suffixes
 * identify the logical VFO slots used by Hamlib's cache mapping.
 */
struct rig_cache_snapshot
{
    vfo_t current_vfo;
    vfo_t observed_vfo;
    vfo_t rx_vfo;
    vfo_t tx_vfo;
    freq_t freqCurr;
    freq_t freqOther;
    freq_t freqMainA;
    freq_t freqMainB;
    freq_t freqMainC;
    freq_t freqSubA;
    freq_t freqSubB;
    freq_t freqSubC;
    freq_t freqMem;
    rmode_t modeCurr;
    rmode_t modeOther;
    rmode_t modeMainA;
    rmode_t modeMainB;
    rmode_t modeMainC;
    rmode_t modeSubA;
    rmode_t modeSubB;
    rmode_t modeSubC;
    rmode_t modeMem;
    pbwidth_t widthCurr;
    pbwidth_t widthOther;
    pbwidth_t widthMainA;
    pbwidth_t widthMainB;
    pbwidth_t widthMainC;
    pbwidth_t widthSubA;
    pbwidth_t widthSubB;
    pbwidth_t widthSubC;
    pbwidth_t widthMem;
    ptt_t ptt;
    split_t split;
    vfo_t split_vfo;
    int satmode;
};

/*
 * Coherent subset of cache state used for VFO routing decisions. Use this
 * instead of rig_cache_snapshot when frequency, mode, and width values are
 * not needed.
 */
struct rig_cache_routing_snapshot
{
    vfo_t current_vfo;
    vfo_t rx_vfo;
    vfo_t tx_vfo;
    split_t split;
    vfo_t split_vfo;
    int satmode;
    ptt_t ptt;
};

/* Function templates
 * Does not include those marked as part of HAMLIB_API
 */
struct rig_cache *rig_cache_create(void);
void rig_cache_destroy(struct rig_cache *cache);
int rig_set_cache_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int rig_set_cache_mode_only(RIG *rig, vfo_t vfo, rmode_t mode);
int rig_invalidate_cache_mode(RIG *rig, vfo_t vfo);
int rig_set_cache_freq(RIG *rig, vfo_t vfo, freq_t freq);
void rig_invalidate_cache_current_freq(RIG *rig);
void rig_invalidate_cache_vfo(RIG *rig);
void rig_get_cached_vfo(RIG *rig, vfo_t *vfo, int *cache_ms,
                        int *timeout_ms);
void rig_set_cache_ptt(RIG *rig, ptt_t ptt);
void rig_invalidate_cache_ptt(RIG *rig);
void rig_invalidate_cache(RIG *rig);
void rig_get_cache_ptt(RIG *rig, ptt_t *ptt, int *cache_ms,
                       int *timeout_ms);
void rig_set_cache_split(RIG *rig, split_t split, vfo_t split_vfo);
void rig_get_cache_split(RIG *rig, split_t *split, vfo_t *split_vfo,
                         int *cache_ms, int *timeout_ms);
void rig_set_cache_satmode(RIG *rig, int satmode);
int rig_get_cache_satmode(RIG *rig);
void rig_set_current_vfo_state(RIG *rig, vfo_t vfo);
vfo_t rig_get_current_vfo_state(RIG *rig);
void rig_get_cache_routing_snapshot(
    RIG *rig, struct rig_cache_routing_snapshot *snapshot);
void rig_get_cache_snapshot(RIG *rig, struct rig_cache_snapshot *snapshot);
void rig_cache_show(RIG *rig, const char *func, int line);

__END_DECLS

#endif
