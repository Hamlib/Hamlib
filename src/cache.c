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

#include "cache.h"
#include "hamlib/rig_state.h"
#include "misc.h"

#include <pthread.h>
#include <stdlib.h>

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !STATE(r)->comm_state)
#define CACHE(r) ((r)->cache_addr)

struct rig_cache
{
    pthread_mutex_t mutex;
    int timeout_ms;
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
    int satmode;
};

struct rig_cache *rig_cache_create(void)
{
    struct rig_cache *cache = calloc(1, sizeof(*cache));

    if (!cache)
    {
        return NULL;
    }

    if (pthread_mutex_init(&cache->mutex, NULL) != 0)
    {
        free(cache);
        return NULL;
    }

    cache->timeout_ms = 1000;
    cache->current_vfo = RIG_VFO_CURR;
    cache->rx_vfo = RIG_VFO_CURR;
    cache->tx_vfo = RIG_VFO_CURR;
    return cache;
}

void rig_cache_destroy(struct rig_cache *cache)
{
    if (!cache)
    {
        return;
    }

    pthread_mutex_destroy(&cache->mutex);
    free(cache);
}

static void rig_cache_lock(RIG *rig)
{
    pthread_mutex_lock(&CACHE(rig)->mutex);
}

static void rig_cache_unlock(RIG *rig)
{
    pthread_mutex_unlock(&CACHE(rig)->mutex);
}

static void rig_cache_show_locked(RIG *rig, const char *func, int line);

static void rig_get_cache_routing_snapshot_locked(
    RIG *rig, struct rig_cache_routing_snapshot *snapshot)
{
    struct rig_cache *cachep = CACHE(rig);

    snapshot->current_vfo = cachep->current_vfo;
    snapshot->rx_vfo = cachep->rx_vfo;
    snapshot->tx_vfo = cachep->tx_vfo;
    snapshot->split = cachep->split;
    snapshot->split_vfo = cachep->split_vfo;
    snapshot->satmode = cachep->satmode;
    snapshot->ptt = cachep->ptt;
}

/**
 * \file cache.c
 * \addtogroup rig
 * @{
 */

enum cache_width_action
{
    CACHE_WIDTH_KEEP,
    CACHE_WIDTH_SET,
    CACHE_WIDTH_INVALIDATE
};

struct mode_cache_entry
{
    rmode_t *mode;
    pbwidth_t *width;
    struct timespec *time_mode;
    struct timespec *time_width;
};

static void update_mode_cache_entry(struct mode_cache_entry *entry,
                                    rmode_t mode, pbwidth_t width,
                                    enum cache_width_action width_action,
                                    bool invalidate_mode)
{
    if (invalidate_mode)
    {
        elapsed_ms(entry->time_mode, HAMLIB_ELAPSED_INVALIDATE);
    }
    else
    {
        *entry->mode = mode;
        elapsed_ms(entry->time_mode, HAMLIB_ELAPSED_SET);
    }

    if (width_action == CACHE_WIDTH_SET)
    {
        *entry->width = width;
        elapsed_ms(entry->time_width, HAMLIB_ELAPSED_SET);
    }
    else if (width_action == CACHE_WIDTH_INVALIDATE)
    {
        elapsed_ms(entry->time_width, HAMLIB_ELAPSED_INVALIDATE);
    }
}

static int rig_set_cache_mode_worker(RIG *rig, vfo_t vfo, rmode_t mode,
                                     pbwidth_t width,
                                     enum cache_width_action width_action,
                                     bool invalidate_mode)
{
    struct rig_cache *cachep = CACHE(rig);
    struct mode_cache_entry entry;
    struct rig_cache_routing_snapshot routing;

    ENTERFUNC;

    rig_cache_lock(rig);
    rig_cache_show_locked(rig, __func__, __LINE__);

    if (vfo == RIG_VFO_CURR)
    {
        // if CURR then update this before we figure out the real VFO
        vfo = cachep->current_vfo;
    }
    else if (vfo == RIG_VFO_TX)
    {
        vfo = cachep->tx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: TX VFO = %s\n", __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_RX)
    {
        vfo = cachep->rx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: RX VFO = %s\n", __func__, rig_strvfo(vfo));
    }

    // pick a sane default
    if (vfo == RIG_VFO_NONE || vfo == RIG_VFO_CURR) { vfo = RIG_VFO_A; }

    if (vfo == RIG_VFO_SUB && cachep->satmode) { vfo = RIG_VFO_SUB_A; };

    if (vfo == RIG_VFO_OTHER)
    {
        rig_get_cache_routing_snapshot_locked(rig, &routing);
        vfo = vfo_fixup_from_snapshot(rig, vfo, routing.split, &routing,
                                      __func__, __LINE__);
    }

    if (vfo == RIG_VFO_ALL)
    {
        elapsed_ms(&cachep->time_modeCurr, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeOther, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeMainA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeMainB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeMainC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeSubA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeSubB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeSubC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeMem, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthCurr, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthOther, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthMainA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthMainB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthMainC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthSubA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthSubB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthSubC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthMem, HAMLIB_ELAPSED_INVALIDATE);
        rig_cache_show_locked(rig, __func__, __LINE__);
        rig_cache_unlock(rig);
        RETURNFUNC(RIG_OK);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
    case RIG_VFO_MAIN_A:
        entry = (struct mode_cache_entry)
        {
            &cachep->modeMainA,
            &cachep->widthMainA, &cachep->time_modeMainA,
            &cachep->time_widthMainA
        };

        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
    case RIG_VFO_MAIN_B:
        entry = (struct mode_cache_entry)
        {
            &cachep->modeMainB,
            &cachep->widthMainB, &cachep->time_modeMainB,
            &cachep->time_widthMainB
        };

        break;

    case RIG_VFO_C:
    case RIG_VFO_MAIN_C:
        entry = (struct mode_cache_entry)
        {
            &cachep->modeMainC,
            &cachep->widthMainC, &cachep->time_modeMainC,
            &cachep->time_widthMainC
        };

        break;

    case RIG_VFO_SUB_A:
        entry = (struct mode_cache_entry)
        {
            &cachep->modeSubA,
            &cachep->widthSubA, &cachep->time_modeSubA,
            &cachep->time_widthSubA
        };

        break;

    case RIG_VFO_SUB_B:
        entry = (struct mode_cache_entry)
        {
            &cachep->modeSubB,
            &cachep->widthSubB, &cachep->time_modeSubB,
            &cachep->time_widthSubB
        };

        break;

    case RIG_VFO_SUB_C:
        entry = (struct mode_cache_entry)
        {
            &cachep->modeSubC,
            &cachep->widthSubC, &cachep->time_modeSubC,
            &cachep->time_widthSubC
        };

        break;

    case RIG_VFO_MEM:
        entry = (struct mode_cache_entry)
        {
            &cachep->modeMem,
            &cachep->widthMem, &cachep->time_modeMem,
            &cachep->time_widthMem
        };

        break;

    default:
        rig_debug(RIG_DEBUG_WARN, "%s(%d): unknown vfo=%s\n", __func__, __LINE__,
                  rig_strvfo(vfo));
        rig_cache_unlock(rig);
        RETURNFUNC(-RIG_EINTERNAL);
    }

    if (vfo == cachep->current_vfo)
    {
        struct mode_cache_entry current_entry = { &cachep->modeCurr,
            &cachep->widthCurr, &cachep->time_modeCurr,
            &cachep->time_widthCurr
        };
        update_mode_cache_entry(&current_entry, mode, width, width_action,
                                invalidate_mode);
    }

    update_mode_cache_entry(&entry, mode, width, width_action,
                            invalidate_mode);
    rig_cache_show_locked(rig, __func__, __LINE__);
    rig_cache_unlock(rig);
    RETURNFUNC(RIG_OK);
}

int rig_set_cache_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    enum cache_width_action width_action;

    if (width == RIG_PASSBAND_NOCHANGE)
    {
        width_action = CACHE_WIDTH_KEEP;
    }
    else
    {
        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        width_action = width > 0 ? CACHE_WIDTH_SET : CACHE_WIDTH_INVALIDATE;
    }

    return rig_set_cache_mode_worker(rig, vfo, mode, width, width_action,
                                     false);
}

int rig_set_cache_mode_only(RIG *rig, vfo_t vfo, rmode_t mode)
{
    return rig_set_cache_mode_worker(rig, vfo, mode, 0, CACHE_WIDTH_KEEP,
                                     false);
}

int rig_invalidate_cache_mode(RIG *rig, vfo_t vfo)
{
    return rig_set_cache_mode_worker(rig, vfo, RIG_MODE_NONE, 0,
                                     CACHE_WIDTH_INVALIDATE, true);
}

int rig_set_cache_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int flag = HAMLIB_ELAPSED_SET;
    struct rig_cache *cachep = CACHE(rig);

    rig_cache_lock(rig);

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        rig_cache_show_locked(rig, __func__, __LINE__);
    }

    rig_debug(RIG_DEBUG_CACHE, "%s(%d):  vfo=%s, current_vfo=%s\n", __func__,
              __LINE__,
              rig_strvfo(vfo), rig_strvfo(cachep->current_vfo));

    if (vfo == RIG_VFO_CURR)
    {
        // if CURR then update this before we figure out the real VFO
        vfo = cachep->current_vfo;
    }

    // if freq == 0 then we are asking to invalidate the cache
    if (freq == 0) { flag = HAMLIB_ELAPSED_INVALIDATE; }

    // pick a sane default
    if (vfo == RIG_VFO_NONE || vfo == RIG_VFO_CURR) { vfo = RIG_VFO_A; }

    if (vfo == RIG_VFO_SUB && cachep->satmode) { vfo = RIG_VFO_SUB_A; };

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        rig_debug(RIG_DEBUG_CACHE, "%s(%d): set vfo=%s to freq=%.0f\n", __func__,
                  __LINE__,
                  rig_strvfo(vfo), freq);
    }

    if (vfo == cachep->current_vfo)
    {
        cachep->freqCurr = freq;
        elapsed_ms(&cachep->time_freqCurr, flag);
    }

    switch (vfo)
    {
    case RIG_VFO_ALL: // we'll use NONE to reset all VFO caches
        elapsed_ms(&cachep->time_freqMainA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_freqMainB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_freqMainC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_freqSubA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_freqSubB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_freqSubC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_freqMem, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_vfo, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeMainA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeMainB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeMainC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeSubA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeSubB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_modeSubC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthMainA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthMainB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthMainC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthSubA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthSubB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_widthSubC, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_ptt, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&cachep->time_split, HAMLIB_ELAPSED_INVALIDATE);
        break;

    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
    case RIG_VFO_MAIN_A:
        cachep->freqMainA = freq;
        elapsed_ms(&cachep->time_freqMainA, flag);
        break;

    case RIG_VFO_B:
    case RIG_VFO_MAIN_B:
    case RIG_VFO_SUB:
        cachep->freqMainB = freq;
        elapsed_ms(&cachep->time_freqMainB, flag);
        break;

    case RIG_VFO_C:
    case RIG_VFO_MAIN_C:
        cachep->freqMainC = freq;
        elapsed_ms(&cachep->time_freqMainC, flag);
        break;

    case RIG_VFO_SUB_A:
        cachep->freqSubA = freq;
        elapsed_ms(&cachep->time_freqSubA, flag);
        break;

    case RIG_VFO_SUB_B:
        cachep->freqSubB = freq;
        elapsed_ms(&cachep->time_freqSubB, flag);
        break;

    case RIG_VFO_SUB_C:
        cachep->freqSubC = freq;
        elapsed_ms(&cachep->time_freqSubC, flag);
        break;

    case RIG_VFO_MEM:
        cachep->freqMem = freq;
        elapsed_ms(&cachep->time_freqMem, flag);
        break;

    case RIG_VFO_OTHER:
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): ignoring VFO_OTHER\n", __func__,
                  __LINE__);
        break;

    default:
        rig_debug(RIG_DEBUG_WARN, "%s(%d): unknown vfo?, vfo=%s\n", __func__, __LINE__,
                  rig_strvfo(vfo));
        rig_cache_unlock(rig);
        return (-RIG_EINVAL);
    }

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        rig_cache_show_locked(rig, __func__, __LINE__);
        rig_cache_unlock(rig);
        return (RIG_OK);
    }

    rig_cache_unlock(rig);
    return (RIG_OK);
}

void rig_invalidate_cache_current_freq(RIG *rig)
{
    struct rig_cache *cachep = CACHE(rig);
    vfo_t vfo;

    rig_cache_lock(rig);
    vfo = cachep->current_vfo;
    elapsed_ms(&cachep->time_freqCurr, HAMLIB_ELAPSED_INVALIDATE);

    if (vfo == RIG_VFO_SUB && cachep->satmode)
    {
        vfo = RIG_VFO_SUB_A;
    }

    switch (vfo)
    {
    case RIG_VFO_NONE:
    case RIG_VFO_CURR:
    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
    case RIG_VFO_MAIN_A:
        elapsed_ms(&cachep->time_freqMainA, HAMLIB_ELAPSED_INVALIDATE);
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
    case RIG_VFO_MAIN_B:
        elapsed_ms(&cachep->time_freqMainB, HAMLIB_ELAPSED_INVALIDATE);
        break;

    case RIG_VFO_C:
    case RIG_VFO_MAIN_C:
        elapsed_ms(&cachep->time_freqMainC, HAMLIB_ELAPSED_INVALIDATE);
        break;

    case RIG_VFO_SUB_A:
        elapsed_ms(&cachep->time_freqSubA, HAMLIB_ELAPSED_INVALIDATE);
        break;

    case RIG_VFO_SUB_B:
        elapsed_ms(&cachep->time_freqSubB, HAMLIB_ELAPSED_INVALIDATE);
        break;

    case RIG_VFO_SUB_C:
        elapsed_ms(&cachep->time_freqSubC, HAMLIB_ELAPSED_INVALIDATE);
        break;

    case RIG_VFO_MEM:
        elapsed_ms(&cachep->time_freqMem, HAMLIB_ELAPSED_INVALIDATE);
        break;

    case RIG_VFO_OTHER:
        break;

    default:
        rig_debug(RIG_DEBUG_WARN, "%s: unknown current VFO %s\n", __func__,
                  rig_strvfo(vfo));
        break;
    }

    rig_cache_unlock(rig);
}

/**
 * \brief get cached values for a VFO
 * \param rig           The rig handle
 * \param vfo           The VFO to get information from
 * \param freq          The frequency is stored here
 * \param cache_ms_freq The age of the last frequency update in ms
 * \param mode          The mode is stored here
 * \param cache_ms_mode The age of the last mode update in ms
 * \param width         The width is stored here
 * \param cache_ms_width The age of the last width update in ms
 *
 * Use this to query the cache and then determine to actually fetch data from
 * the rig.
 *
 * \note All pointers must be given. No pointer can be left at NULL
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int rig_get_cache(RIG *rig, vfo_t vfo, freq_t *freq, int *cache_ms_freq,
                  rmode_t *mode, int *cache_ms_mode, pbwidth_t *width, int *cache_ms_width)
{
    struct rig_cache *cachep;

    if (CHECK_RIG_ARG(rig) || !freq || !cache_ms_freq ||
            !mode || !cache_ms_mode || !width || !cache_ms_width)
    {
        return -RIG_EINVAL;
    }

    cachep = CACHE(rig);
    rig_cache_lock(rig);

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        ENTERFUNC2;
    }

    rig_debug(RIG_DEBUG_CACHE, "%s(%d):  vfo=%s, current_vfo=%s\n", __func__,
              __LINE__,
              rig_strvfo(vfo), rig_strvfo(cachep->current_vfo));

    if (vfo == RIG_VFO_CURR)
    {
        vfo = cachep->current_vfo;
    }
    else if (vfo == RIG_VFO_TX)
    {
        vfo = cachep->tx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: TX VFO = %s\n", __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_RX)
    {
        vfo = cachep->rx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: RX VFO = %s\n", __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_OTHER)
    {
        switch (cachep->current_vfo)
        {
        case RIG_VFO_CURR:
            break;  // no change

        case RIG_VFO_OTHER:
            vfo = RIG_VFO_OTHER;
            break;

        case RIG_VFO_A:
            vfo = RIG_VFO_B;
            break;

        case RIG_VFO_MAIN_A:
            vfo = RIG_VFO_MAIN_B;
            break;

        case RIG_VFO_MAIN:
            vfo = RIG_VFO_SUB;
            break;

        case RIG_VFO_B:
            vfo = RIG_VFO_A;
            break;

        case RIG_VFO_MAIN_B:
            vfo = RIG_VFO_MAIN_A;
            break;

        case RIG_VFO_SUB_A:
            vfo = RIG_VFO_SUB_B;
            break;

        case RIG_VFO_SUB_B:
            vfo = RIG_VFO_SUB_A;
            break;

        case RIG_VFO_NONE:
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): ignoring VFO_NONE\n", __func__,
                      __LINE__);
            break;

        default:
            rig_debug(RIG_DEBUG_WARN, "%s(%d): unknown vfo=%s, curr_vfo=%s\n", __func__,
                      __LINE__,
                      rig_strvfo(vfo), rig_strvfo(cachep->current_vfo));
        }
    }

    // pick a sane default
    if (vfo == RIG_VFO_CURR || vfo == RIG_VFO_NONE) { vfo = RIG_VFO_A; }

    // If we're in satmode we map SUB to SUB_A
    if (vfo == RIG_VFO_SUB && cachep->satmode) { vfo = RIG_VFO_SUB_A; };

    switch (vfo)
    {
    case RIG_VFO_CURR:
        *freq = cachep->freqCurr;
        *mode = cachep->modeCurr;
        *width = cachep->widthCurr;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqCurr,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeCurr,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthCurr,
                                     HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_OTHER:
        *freq = cachep->freqOther;
        *mode = cachep->modeOther;
        *width = cachep->widthOther;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqOther,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeOther,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthOther,
                                     HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
    case RIG_VFO_MAIN_A:
        *freq = cachep->freqMainA;
        *mode = cachep->modeMainA;
        *width = cachep->widthMainA;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqMainA,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeMainA,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthMainA,
                                     HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
    case RIG_VFO_MAIN_B:
        *freq = cachep->freqMainB;
        *mode = cachep->modeMainB;
        *width = cachep->widthMainB;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqMainB,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeMainB,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthMainB,
                                     HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_SUB_A:
        *freq = cachep->freqSubA;
        *mode = cachep->modeSubA;
        *width = cachep->widthSubA;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqSubA,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeSubA,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthSubA,
                                     HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_SUB_B:
        *freq = cachep->freqSubB;
        *mode = cachep->modeSubB;
        *width = cachep->widthSubB;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqSubB,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeSubB,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthSubB,
                                     HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_C:
        //case RIG_VFO_MAINC: // not used by any rig yet
        *freq = cachep->freqMainC;
        *mode = cachep->modeMainC;
        *width = cachep->widthMainC;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqMainC,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeMainC,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthMainC,
                                     HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_SUB_C:
        *freq = cachep->freqSubC;
        *mode = cachep->modeSubC;
        *width = cachep->widthSubC;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqSubC,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeSubC,
                                    HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthSubC,
                                     HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_MEM:
        *freq = cachep->freqMem;
        *mode = cachep->modeMem;
        *width = cachep->widthMem;
        *cache_ms_freq = elapsed_ms(&cachep->time_freqMem, HAMLIB_ELAPSED_GET);
        *cache_ms_mode = elapsed_ms(&cachep->time_modeMem, HAMLIB_ELAPSED_GET);
        *cache_ms_width = elapsed_ms(&cachep->time_widthMem,
                                     HAMLIB_ELAPSED_GET);
        break;

    default:
        rig_debug(RIG_DEBUG_WARN, "%s(%d): unknown vfo?, vfo=%s\n", __func__, __LINE__,
                  rig_strvfo(vfo));
        rig_cache_unlock(rig);
        RETURNFUNC2(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_CACHE, "%s(%d): vfo=%s, freq=%.0f, mode=%s, width=%d\n",
              __func__, __LINE__, rig_strvfo(vfo),
              (double)*freq, rig_strrmode(*mode), (int)*width);

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        rig_cache_unlock(rig);
        RETURNFUNC2(RIG_OK);
    }

    rig_cache_unlock(rig);
    return RIG_OK;
}

/**
 * \brief get cached values for a VFO
 * \param rig           The rig handle
 * \param vfo           The VFO to get information from
 * \param freq          The frequency is stored here
 * \param cache_ms_freq The age of the last frequency update in ms -- NULL if you don't want it

 * Use this to query the frequency cache and then determine to actually fetch data from
 * the rig.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int rig_get_cache_freq(RIG *rig, vfo_t vfo, freq_t *freq, int *cache_ms_freq_p)
{
    rmode_t mode;
    int cache_ms_freq;
    int cache_ms_mode;
    pbwidth_t width;
    int cache_ms_width;
    int retval;
    retval = rig_get_cache(rig, vfo, freq, &cache_ms_freq, &mode, &cache_ms_mode,
                           &width, &cache_ms_width);

    if (retval == RIG_OK)
    {
        if (cache_ms_freq_p) { *cache_ms_freq_p = cache_ms_freq; }
    }

    return retval;
}

/* Get cache timeout period
 * Returns value in msec, -1 if error
 */
int HAMLIB_API rig_get_cache_timeout_ms(RIG *rig, hamlib_cache_t selection)
{
    int timeout_ms;

    rig_debug(RIG_DEBUG_TRACE, "%s: called selection=%d\n", __func__, selection);

    if (!rig) {return -1;}

    rig_cache_lock(rig);
    timeout_ms = CACHE(rig)->timeout_ms;
    rig_cache_unlock(rig);
    return timeout_ms;
}

int HAMLIB_API rig_set_cache_timeout_ms(RIG *rig, hamlib_cache_t selection,
                                        int ms)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called selection=%d, ms=%d\n", __func__,
              selection, ms);

    if (!rig) {return -RIG_EINVAL;}

    rig_cache_lock(rig);
    CACHE(rig)->timeout_ms = ms;
    rig_cache_unlock(rig);
    return RIG_OK;
}

enum routing_state_field
{
    ROUTING_CURRENT = 1u << 0,
    ROUTING_RX = 1u << 1,
    ROUTING_TX = 1u << 2,
    ROUTING_SPLIT = 1u << 3,
    ROUTING_OBSERVED = 1u << 4
};

static void rig_publish_routing_state_locked(
    RIG *rig, unsigned int fields,
    const struct rig_cache_routing_snapshot *routing)
{
    struct rig_cache *cachep = CACHE(rig);

    if (fields & ROUTING_CURRENT)
    {
        STATE(rig)->current_vfo = routing->current_vfo;
        cachep->current_vfo = routing->current_vfo;
    }

    if (fields & ROUTING_RX)
    {
        STATE(rig)->rx_vfo = routing->rx_vfo;
        cachep->rx_vfo = routing->rx_vfo;
    }

    if (fields & ROUTING_TX)
    {
        STATE(rig)->tx_vfo = routing->tx_vfo;
        cachep->tx_vfo = routing->tx_vfo;
    }

    if (fields & ROUTING_SPLIT)
    {
        cachep->split = routing->split;
        cachep->split_vfo = routing->split_vfo;
        elapsed_ms(&cachep->time_split, HAMLIB_ELAPSED_SET);
    }

    if (fields & ROUTING_OBSERVED)
    {
        cachep->observed_vfo = routing->current_vfo;
        elapsed_ms(&cachep->time_vfo, HAMLIB_ELAPSED_SET);
    }
}

void rig_set_current_vfo_state(RIG *rig, vfo_t vfo)
{
    struct rig_cache_routing_snapshot routing = { .current_vfo = vfo };

    rig_cache_lock(rig);
    rig_publish_routing_state_locked(rig, ROUTING_CURRENT, &routing);
    rig_cache_unlock(rig);
}

vfo_t rig_get_current_vfo_state(RIG *rig)
{
    vfo_t vfo;

    rig_cache_lock(rig);
    vfo = CACHE(rig)->current_vfo;
    rig_cache_unlock(rig);
    return vfo;
}

void rig_set_rx_vfo_state(RIG *rig, vfo_t rx_vfo)
{
    struct rig_cache_routing_snapshot routing = { .rx_vfo = rx_vfo };

    rig_cache_lock(rig);
    rig_publish_routing_state_locked(rig, ROUTING_RX, &routing);
    rig_cache_unlock(rig);
}

void rig_set_tx_vfo_state(RIG *rig, vfo_t tx_vfo)
{
    struct rig_cache_routing_snapshot routing = { .tx_vfo = tx_vfo };

    rig_cache_lock(rig);
    rig_publish_routing_state_locked(rig, ROUTING_TX, &routing);
    rig_cache_unlock(rig);
}

void rig_set_vfo_state(RIG *rig, vfo_t current_vfo, vfo_t tx_vfo)
{
    struct rig_cache_routing_snapshot routing =
    {
        .current_vfo = current_vfo,
        .tx_vfo = tx_vfo
    };

    rig_cache_lock(rig);
    rig_publish_routing_state_locked(rig, ROUTING_CURRENT | ROUTING_TX,
                                     &routing);
    rig_cache_unlock(rig);
}

void rig_observe_current_vfo(RIG *rig, vfo_t vfo)
{
    struct rig_cache_routing_snapshot routing = { .current_vfo = vfo };

    rig_cache_lock(rig);
    rig_publish_routing_state_locked(
        rig, ROUTING_CURRENT | ROUTING_OBSERVED, &routing);
    rig_cache_unlock(rig);
}

void rig_get_cached_vfo(RIG *rig, vfo_t *vfo, int *cache_ms,
                        int *timeout_ms)
{
    struct rig_cache *cachep = CACHE(rig);

    rig_cache_lock(rig);
    *vfo = cachep->observed_vfo;
    *cache_ms = elapsed_ms(&cachep->time_vfo, HAMLIB_ELAPSED_GET);
    *timeout_ms = cachep->timeout_ms;
    rig_cache_unlock(rig);
}

void rig_invalidate_cache_vfo(RIG *rig)
{
    rig_cache_lock(rig);
    elapsed_ms(&CACHE(rig)->time_vfo, HAMLIB_ELAPSED_INVALIDATE);
    rig_cache_unlock(rig);
}

void rig_set_cache_ptt(RIG *rig, ptt_t ptt)
{
    struct rig_cache *cachep = CACHE(rig);

    rig_cache_lock(rig);
    cachep->ptt = ptt;
    elapsed_ms(&cachep->time_ptt, HAMLIB_ELAPSED_SET);
    rig_cache_unlock(rig);
}

void rig_invalidate_cache_ptt(RIG *rig)
{
    struct rig_cache *cachep = CACHE(rig);

    rig_cache_lock(rig);
    elapsed_ms(&cachep->time_ptt, HAMLIB_ELAPSED_INVALIDATE);
    rig_cache_unlock(rig);
}

void rig_invalidate_cache(RIG *rig)
{
    struct rig_cache *cachep = CACHE(rig);

    rig_cache_lock(rig);
    elapsed_ms(&cachep->time_freqMainA, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_freqMainB, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_freqSubA, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_freqSubB, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_vfo, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_modeMainA, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_modeMainB, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_modeMainC, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_modeSubA, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_modeSubB, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_modeSubC, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_widthMainA, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_widthMainB, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_widthMainC, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_widthSubA, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_widthSubB, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_widthSubC, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_ptt, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&cachep->time_split, HAMLIB_ELAPSED_INVALIDATE);
    rig_cache_unlock(rig);
}

void rig_get_cache_ptt(RIG *rig, ptt_t *ptt, int *cache_ms,
                       int *timeout_ms)
{
    struct rig_cache *cachep = CACHE(rig);

    rig_cache_lock(rig);
    *ptt = cachep->ptt;
    *cache_ms = elapsed_ms(&cachep->time_ptt, HAMLIB_ELAPSED_GET);
    *timeout_ms = cachep->timeout_ms;
    rig_cache_unlock(rig);
}

void rig_set_cache_split(RIG *rig, split_t split, vfo_t split_vfo)
{
    struct rig_cache_routing_snapshot routing;

    rig_cache_lock(rig);
    rig_get_cache_routing_snapshot_locked(rig, &routing);
    routing.tx_vfo = split_vfo;
    routing.split = split;
    routing.split_vfo = split_vfo;
    rig_publish_routing_state_locked(rig, ROUTING_TX | ROUTING_SPLIT,
                                     &routing);
    rig_cache_unlock(rig);
}

void rig_set_split_routing_state(RIG *rig, split_t split, vfo_t rx_vfo,
                                 vfo_t tx_vfo)
{
    struct rig_cache_routing_snapshot routing =
    {
        .rx_vfo = rx_vfo,
        .tx_vfo = tx_vfo,
        .split = split,
        .split_vfo = tx_vfo
    };

    rig_cache_lock(rig);
    rig_publish_routing_state_locked(
        rig, ROUTING_RX | ROUTING_TX | ROUTING_SPLIT, &routing);
    rig_cache_unlock(rig);
}

void rig_get_cache_split(RIG *rig, split_t *split, vfo_t *split_vfo,
                         int *cache_ms, int *timeout_ms)
{
    struct rig_cache *cachep = CACHE(rig);

    rig_cache_lock(rig);
    *split = cachep->split;
    *split_vfo = cachep->split_vfo;
    *cache_ms = elapsed_ms(&cachep->time_split, HAMLIB_ELAPSED_GET);
    *timeout_ms = cachep->timeout_ms;
    rig_cache_unlock(rig);
}

void rig_set_cache_satmode(RIG *rig, int satmode)
{
    rig_cache_lock(rig);
    CACHE(rig)->satmode = satmode;
    rig_cache_unlock(rig);
}

int rig_get_cache_satmode(RIG *rig)
{
    int satmode;

    rig_cache_lock(rig);
    satmode = CACHE(rig)->satmode;
    rig_cache_unlock(rig);
    return satmode;
}

void rig_get_cache_routing_snapshot(
    RIG *rig, struct rig_cache_routing_snapshot *snapshot)
{
    rig_cache_lock(rig);
    rig_get_cache_routing_snapshot_locked(rig, snapshot);
    rig_cache_unlock(rig);
}

void rig_get_cache_snapshot(RIG *rig, struct rig_cache_snapshot *snapshot)
{
    struct rig_cache *cachep = CACHE(rig);

    rig_cache_lock(rig);
    snapshot->current_vfo = cachep->current_vfo;
    snapshot->observed_vfo = cachep->observed_vfo;
    snapshot->rx_vfo = cachep->rx_vfo;
    snapshot->tx_vfo = cachep->tx_vfo;
    snapshot->freqCurr = cachep->freqCurr;
    snapshot->freqOther = cachep->freqOther;
    snapshot->freqMainA = cachep->freqMainA;
    snapshot->freqMainB = cachep->freqMainB;
    snapshot->freqMainC = cachep->freqMainC;
    snapshot->freqSubA = cachep->freqSubA;
    snapshot->freqSubB = cachep->freqSubB;
    snapshot->freqSubC = cachep->freqSubC;
    snapshot->freqMem = cachep->freqMem;
    snapshot->modeCurr = cachep->modeCurr;
    snapshot->modeOther = cachep->modeOther;
    snapshot->modeMainA = cachep->modeMainA;
    snapshot->modeMainB = cachep->modeMainB;
    snapshot->modeMainC = cachep->modeMainC;
    snapshot->modeSubA = cachep->modeSubA;
    snapshot->modeSubB = cachep->modeSubB;
    snapshot->modeSubC = cachep->modeSubC;
    snapshot->modeMem = cachep->modeMem;
    snapshot->widthCurr = cachep->widthCurr;
    snapshot->widthOther = cachep->widthOther;
    snapshot->widthMainA = cachep->widthMainA;
    snapshot->widthMainB = cachep->widthMainB;
    snapshot->widthMainC = cachep->widthMainC;
    snapshot->widthSubA = cachep->widthSubA;
    snapshot->widthSubB = cachep->widthSubB;
    snapshot->widthSubC = cachep->widthSubC;
    snapshot->widthMem = cachep->widthMem;
    snapshot->ptt = cachep->ptt;
    snapshot->split = cachep->split;
    snapshot->split_vfo = cachep->split_vfo;
    snapshot->satmode = cachep->satmode;
    rig_cache_unlock(rig);
}

static void rig_cache_show_locked(RIG *rig, const char *func, int line)
{
    struct rig_cache *cachep = CACHE(rig);

    rig_debug(RIG_DEBUG_CACHE,
              "%s(%d): freqMainA=%.0f, modeMainA=%s, widthMainA=%d\n", func, line,
              cachep->freqMainA, rig_strrmode(cachep->modeMainA),
              (int)cachep->widthMainA);
    rig_debug(RIG_DEBUG_CACHE,
              "%s(%d): freqMainB=%.0f, modeMainB=%s, widthMainB=%d\n", func, line,
              cachep->freqMainB, rig_strrmode(cachep->modeMainB),
              (int)cachep->widthMainB);

    if (STATE(rig)->vfo_list & RIG_VFO_SUB_A)
    {
        rig_debug(RIG_DEBUG_CACHE,
                  "%s(%d): freqSubA=%.0f, modeSubA=%s, widthSubA=%d\n", func, line,
                  cachep->freqSubA, rig_strrmode(cachep->modeSubA),
                  (int)cachep->widthSubA);
        rig_debug(RIG_DEBUG_CACHE,
                  "%s(%d): freqSubB=%.0f, modeSubB=%s, widthSubB=%d\n", func, line,
                  cachep->freqSubB, rig_strrmode(cachep->modeSubB),
                  (int)cachep->widthSubB);
    }
}

void rig_cache_show(RIG *rig, const char *func, int line)
{
    rig_cache_lock(rig);
    rig_cache_show_locked(rig, func, line);
    rig_cache_unlock(rig);
}

/*! @} */
