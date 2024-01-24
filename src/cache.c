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
#include "misc.h"

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)

/**
 * \file cache.c
 * \addtogroup rig
 * @{
 */

int rig_set_cache_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct rig_cache *cachep = CACHE(rig);
  
    ENTERFUNC;

    rig_cache_show(rig, __func__, __LINE__);

    if (vfo == RIG_VFO_CURR)
    {
        // if CURR then update this before we figure out the real VFO
        vfo = rig->state.current_vfo;
    }
    else if (vfo == RIG_VFO_TX)
    {
        vfo = rig->state.tx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: TX VFO = %s\n", __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_RX)
    {
        vfo = rig->state.rx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: RX VFO = %s\n", __func__, rig_strvfo(vfo));
    }

    // pick a sane default
    if (vfo == RIG_VFO_NONE || vfo == RIG_VFO_CURR) { vfo = RIG_VFO_A; }

    if (vfo == RIG_VFO_SUB && cachep->satmode) { vfo = RIG_VFO_SUB_A; };

    if (vfo == RIG_VFO_OTHER) { vfo = vfo_fixup(rig, vfo, cachep->split); }

    if (vfo == rig->state.current_vfo)
    {
        cachep->modeCurr = mode;
        if (width > 0)
        {
            cachep->widthCurr = width;
        }
        elapsed_ms(&cachep->time_modeCurr, HAMLIB_ELAPSED_SET);
    }

    switch (vfo)
    {
    case RIG_VFO_ALL: // we'll use NONE to reset all VFO caches
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
        break;

    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
    case RIG_VFO_MAIN_A:
        cachep->modeMainA = mode;

        if (width > 0) { cachep->widthMainA = width; }

        elapsed_ms(&cachep->time_modeMainA, HAMLIB_ELAPSED_SET);
        elapsed_ms(&cachep->time_widthMainA, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
    case RIG_VFO_MAIN_B:
        cachep->modeMainB = mode;

        if (width > 0) { cachep->widthMainB = width; }

        elapsed_ms(&cachep->time_modeMainB, HAMLIB_ELAPSED_SET);
        elapsed_ms(&cachep->time_widthMainB, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_C:
    case RIG_VFO_MAIN_C:
        cachep->modeMainC = mode;

        if (width > 0) { cachep->widthMainC = width; }

        elapsed_ms(&cachep->time_modeMainC, HAMLIB_ELAPSED_SET);
        elapsed_ms(&cachep->time_widthMainC, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_SUB_A:
        cachep->modeSubA = mode;
        elapsed_ms(&cachep->time_modeSubA, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_SUB_B:
        cachep->modeSubB = mode;
        elapsed_ms(&cachep->time_modeSubB, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_SUB_C:
        cachep->modeSubC = mode;
        elapsed_ms(&cachep->time_modeSubC, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_MEM:
        cachep->modeMem = mode;
        elapsed_ms(&cachep->time_modeMem, HAMLIB_ELAPSED_SET);
        break;

    default:
        rig_debug(RIG_DEBUG_WARN, "%s(%d): unknown vfo=%s\n", __func__, __LINE__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINTERNAL);
    }

    rig_cache_show(rig, __func__, __LINE__);
    RETURNFUNC(RIG_OK);
}

int rig_set_cache_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int flag = HAMLIB_ELAPSED_SET;
    struct rig_cache *cachep = CACHE(rig);

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        rig_cache_show(rig, __func__, __LINE__);
    }

    rig_debug(RIG_DEBUG_CACHE, "%s(%d):  vfo=%s, current_vfo=%s\n", __func__,
              __LINE__,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));

    if (vfo == RIG_VFO_CURR)
    {
        // if CURR then update this before we figure out the real VFO
        vfo = rig->state.current_vfo;
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

    if (vfo == rig->state.current_vfo)
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
        return (-RIG_EINVAL);
    }

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        rig_cache_show(rig, __func__, __LINE__);
        return (RIG_OK);
    }

    return (RIG_OK);
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
    struct rig_cache *cachep = CACHE(rig);
  
    if (CHECK_RIG_ARG(rig) || !freq || !cache_ms_freq ||
            !mode || !cache_ms_mode || !width || !cache_ms_width)
    {
        return -RIG_EINVAL;
    }

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        ENTERFUNC2;
    }

    rig_debug(RIG_DEBUG_CACHE, "%s(%d):  vfo=%s, current_vfo=%s\n", __func__,
              __LINE__,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }
    else if (vfo == RIG_VFO_TX)
    {
        vfo = rig->state.tx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: TX VFO = %s\n", __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_RX)
    {
        vfo = rig->state.rx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: RX VFO = %s\n", __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_OTHER)
    {
        switch (rig->state.current_vfo)
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
                      rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));
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
        RETURNFUNC2(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_CACHE, "%s(%d): vfo=%s, freq=%.0f, mode=%s, width=%d\n",
              __func__, __LINE__, rig_strvfo(vfo),
              (double)*freq, rig_strrmode(*mode), (int)*width);

    if (rig_need_debug(RIG_DEBUG_CACHE))
    {
        RETURNFUNC2(RIG_OK);
    }

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


void rig_cache_show(RIG *rig, const char *func, int line)
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

    if (rig->state.vfo_list & RIG_VFO_SUB_A)
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

/*! @} */
