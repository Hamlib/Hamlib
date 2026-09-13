/*
 *  Hamlib SmartSDR levels, functions and antenna selection
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

/* Maps Hamlib levels, functions and antenna selection onto SmartSDR slice and */
/* transmit properties, tracking each value as the radio reports it. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "cache.h"
#include "smartsdr_level.h"
#include "smartsdr_props.h"
#include "smartsdr_session.h"
#include "smartsdr_slice.h"
#include "smartsdr_stream.h"


/* Levels. Hamlib float levels are 0.0..1.0 while the radio uses 0..100 for the
 * same controls, so full_scale is 100 there. Integer levels pass through. */
static const struct
{
    setting_t id;
    int prop;
    double full_scale;
} smartsdr_level_map[] =
{
    { RIG_LEVEL_AF,         SMARTSDR_P_AUDIO_LEVEL,    100.0 },
    { RIG_LEVEL_SQL,        SMARTSDR_P_SQUELCH_LEVEL,  100.0 },
    { RIG_LEVEL_NR,         SMARTSDR_P_NR_LEVEL,       100.0 },
    { RIG_LEVEL_NB,         SMARTSDR_P_NB_LEVEL,       100.0 },
    { RIG_LEVEL_APF,        SMARTSDR_P_APF_LEVEL,      100.0 },
    { RIG_LEVEL_BALANCE,    SMARTSDR_P_AUDIO_PAN,      100.0 },
    { RIG_LEVEL_RF,         SMARTSDR_P_RFGAIN,         100.0 },
    { RIG_LEVEL_RFPOWER,    SMARTSDR_P_RFPOWER,        100.0 },
    { RIG_LEVEL_MICGAIN,    SMARTSDR_P_MIC_LEVEL,      100.0 },
    { RIG_LEVEL_COMP,       SMARTSDR_P_SP_LEVEL,       100.0 },
    { RIG_LEVEL_VOXGAIN,    SMARTSDR_P_VOX_LEVEL,      100.0 },
    { RIG_LEVEL_VOXDELAY,   SMARTSDR_P_VOX_DELAY,      0.0 },
    { RIG_LEVEL_KEYSPD,     SMARTSDR_P_SPEED,          0.0 },
    { RIG_LEVEL_CWPITCH,    SMARTSDR_P_PITCH,          0.0 },
    { RIG_LEVEL_BKIN_DLYMS, SMARTSDR_P_BREAK_IN_DELAY, 0.0 },
    { 0, 0, 0.0 }
};


/* Functions. All are 0/1 on the wire. */
static const struct
{
    setting_t id;
    int prop;
} smartsdr_func_map[] =
{
    { RIG_FUNC_MUTE,      SMARTSDR_P_AUDIO_MUTE },
    { RIG_FUNC_SQL,       SMARTSDR_P_SQUELCH },
    { RIG_FUNC_NR,        SMARTSDR_P_NR },
    { RIG_FUNC_NB,        SMARTSDR_P_NB },
    { RIG_FUNC_ANL,       SMARTSDR_P_WNB },
    { RIG_FUNC_ANF,       SMARTSDR_P_ANF },
    { RIG_FUNC_APF,       SMARTSDR_P_APF },
    { RIG_FUNC_LOCK,      SMARTSDR_P_LOCK },
    { RIG_FUNC_RIT,       SMARTSDR_P_RIT_ON },
    { RIG_FUNC_XIT,       SMARTSDR_P_XIT_ON },
    { RIG_FUNC_DIVERSITY, SMARTSDR_P_DIVERSITY },
    { RIG_FUNC_VOX,       SMARTSDR_P_VOX_ENABLE },
    { RIG_FUNC_COMP,      SMARTSDR_P_SP_ENABLE },
    { RIG_FUNC_FBKIN,     SMARTSDR_P_BREAK_IN },
    { RIG_FUNC_MON,       SMARTSDR_P_SB_MONITOR },
    { 0, 0 }
};


static int level_index(setting_t level, double *full_scale)
{
    int i;

    for (i = 0; smartsdr_level_map[i].id != 0; i++)
    {
        if (smartsdr_level_map[i].id == level)
        {
            *full_scale = smartsdr_level_map[i].full_scale;
            return smartsdr_level_map[i].prop;
        }
    }

    return -1;
}


static int func_index(setting_t func)
{
    int i;

    for (i = 0; smartsdr_func_map[i].id != 0; i++)
    {
        if (smartsdr_func_map[i].id == func)
        {
            return smartsdr_func_map[i].prop;
        }
    }

    return -1;
}


/* AGC is an enumeration on both sides rather than a scaled number. */
static const struct
{
    int hamlib;
    const char *flex;
} smartsdr_agc_modes[] =
{
    { RIG_AGC_OFF,    "off"  },
    { RIG_AGC_SLOW,   "slow" },
    { RIG_AGC_MEDIUM, "med"  },
    { RIG_AGC_FAST,   "fast" },
};


static const char *agc_to_flex(int hamlib)
{
    size_t i;

    for (i = 0; i < sizeof(smartsdr_agc_modes) / sizeof(smartsdr_agc_modes[0]); i++)
    {
        if (smartsdr_agc_modes[i].hamlib == hamlib)
        {
            return smartsdr_agc_modes[i].flex;
        }
    }

    return NULL;
}


static int agc_from_flex(const char *flex)
{
    size_t i;

    for (i = 0; i < sizeof(smartsdr_agc_modes) / sizeof(smartsdr_agc_modes[0]); i++)
    {
        if (strcmp(smartsdr_agc_modes[i].flex, flex) == 0)
        {
            return smartsdr_agc_modes[i].hamlib;
        }
    }

    return RIG_AGC_OFF;
}


int smartsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char value[32];
    double full_scale = 0.0;
    int idx;

    ENTERFUNC;

    if (level == RIG_LEVEL_AGC)
    {
        const char *mode = agc_to_flex(val.i);

        if (mode == NULL)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        RETURNFUNC(smartsdr_prop_set(rig, SMARTSDR_P_AGC_MODE, mode));
    }

    if (level == RIG_LEVEL_MONITOR_GAIN)
    {
        rmode_t mode;

        smartsdr_state_lock(priv);
        mode = smartsdr_slice_mode(priv);
        smartsdr_state_unlock(priv);

        /* The radio keeps separate monitor gains for CW and voice. */
        idx = (mode == RIG_MODE_CW) ? SMARTSDR_P_MON_GAIN_CW
              : SMARTSDR_P_MON_GAIN_SB;
        snprintf(value, sizeof(value), "%d", (int)(val.f * 100.0 + 0.5));
        RETURNFUNC(smartsdr_prop_set(rig, idx, value));
    }

    idx = level_index(level, &full_scale);

    if (idx < 0)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (full_scale > 0.0)
    {
        snprintf(value, sizeof(value), "%d", (int)(val.f * full_scale + 0.5));
    }
    else
    {
        snprintf(value, sizeof(value), "%d", val.i);
    }

    RETURNFUNC(smartsdr_prop_set(rig, idx, value));
}



/* Meters the radio streams, as opposed to the settings it reports in status.
 * Returns 1 when it handled the level. */
static int smartsdr_get_meter_level(RIG *rig, setting_t level, value_t *val,
                                    int *retval)
{
    double raw;
    int slot;

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:            slot = SMARTSDR_MTR_LEVEL;  break;

    case RIG_LEVEL_SWR:                 slot = SMARTSDR_MTR_SWR;    break;

    case RIG_LEVEL_ALC:                 slot = SMARTSDR_MTR_ALC;    break;

    case RIG_LEVEL_RFPOWER_METER:       /* fall through */
    case RIG_LEVEL_RFPOWER_METER_WATTS: slot = SMARTSDR_MTR_FWDPWR; break;

    case RIG_LEVEL_TEMP_METER:          slot = SMARTSDR_MTR_PATEMP; break;

    case RIG_LEVEL_VD_METER:            slot = SMARTSDR_MTR_VOLTS;  break;

    case RIG_LEVEL_ID_METER:            slot = SMARTSDR_MTR_AMPS;   break;

    default:
        return 0;
    }

    *retval = smartsdr_meter_read(rig, slot, &raw);

    if (*retval != RIG_OK)
    {
        return 1;
    }

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        /* Hamlib wants dB relative to S9, and S9 is -73 dBm. */
        val->i = (int)(raw + 73.0 + (raw < 0 ? -0.5 : 0.5));
        break;

    case RIG_LEVEL_RFPOWER_METER_WATTS:
        val->f = (float)pow(10.0, (raw - 30.0) / 10.0);
        break;

    case RIG_LEVEL_RFPOWER_METER:
    {
        /* A fraction of rated power. The rating belongs to the transmit
         * range the rig declares, so let the core scale against that rather
         * than repeating a figure here. */
        struct smartsdr_priv_data *priv =
            (struct smartsdr_priv_data *)STATE(rig)->priv;
        double watts = pow(10.0, (raw - 30.0) / 10.0);
        unsigned int mw = (unsigned int)(watts * 1000.0 + 0.5);
        float frac = 0.0f;
        freq_t freq;
        rmode_t mode;

        smartsdr_state_lock(priv);
        freq = smartsdr_slice_freq(priv);
        mode = smartsdr_slice_mode(priv);
        smartsdr_state_unlock(priv);

        if (mw > 0
                && rig_mW2power(rig, &frac, mw, freq, mode) != RIG_OK)
        {
            frac = 0.0f;
        }

        val->f = frac;
        break;
    }

    case RIG_LEVEL_ALC:
        /* dBFS is a level below full scale, so report the linear fraction of
         * full scale it stands for. */
        val->f = (float)pow(10.0, raw / 20.0);
        break;

    default:
        val->f = (float)raw;
        break;
    }

    return 1;
}


int smartsdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    double full_scale = 0.0;
    int idx;

    ENTERFUNC;

    {
        int meter_rc = RIG_OK;

        if (smartsdr_get_meter_level(rig, level, val, &meter_rc))
        {
            RETURNFUNC(meter_rc);
        }
    }

    if (level == RIG_LEVEL_AGC)
    {
        smartsdr_state_lock(priv);

        if (!smartsdr_prop_valid(priv, SMARTSDR_P_AGC_MODE))
        {
            smartsdr_state_unlock(priv);
            RETURNFUNC(-RIG_ENAVAIL);
        }

        val->i = agc_from_flex(priv->props[SMARTSDR_P_AGC_MODE].sval);
        smartsdr_state_unlock(priv);
        RETURNFUNC(RIG_OK);
    }

    if (level == RIG_LEVEL_MONITOR_GAIN)
    {
        smartsdr_state_lock(priv);
        idx = (smartsdr_slice_mode(priv) == RIG_MODE_CW) ? SMARTSDR_P_MON_GAIN_CW
              : SMARTSDR_P_MON_GAIN_SB;

        if (!smartsdr_prop_valid(priv, idx))
        {
            smartsdr_state_unlock(priv);
            RETURNFUNC(-RIG_ENAVAIL);
        }

        val->f = (float)(priv->props[idx].ival / 100.0);
        smartsdr_state_unlock(priv);
        RETURNFUNC(RIG_OK);
    }

    idx = level_index(level, &full_scale);

    if (idx < 0)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    smartsdr_state_lock(priv);

    if (!smartsdr_prop_valid(priv, idx))
    {
        smartsdr_state_unlock(priv);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (full_scale > 0.0)
    {
        val->f = (float)(priv->props[idx].ival / full_scale);
    }
    else
    {
        val->i = priv->props[idx].ival;
    }

    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}


int smartsdr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int idx = func_index(func);

    ENTERFUNC;

    if (idx < 0)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(smartsdr_prop_set(rig, idx, status ? "1" : "0"));
}


int smartsdr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int idx = func_index(func);

    ENTERFUNC;

    if (idx < 0)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    smartsdr_state_lock(priv);

    if (!smartsdr_prop_valid(priv, idx))
    {
        smartsdr_state_unlock(priv);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *status = (priv->props[idx].ival != 0);
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}


/* Antenna ports, in RIG_ANT_1..4 order. RX_A is receive-only, which the radio
 * enforces by omitting it from tx_ant_list. */
static const char *const smartsdr_ant_names[] = { "ANT1", "ANT2", "RX_A", "XVTA" };
#define SMARTSDR_ANT_COUNT \
    ((int)(sizeof(smartsdr_ant_names) / sizeof(smartsdr_ant_names[0])))


static ant_t ant_from_name(const char *name)
{
    int i;

    for (i = 0; i < SMARTSDR_ANT_COUNT; i++)
    {
        if (strcmp(smartsdr_ant_names[i], name) == 0)
        {
            return RIG_ANT_N(i);
        }
    }

    return RIG_ANT_NONE;
}


int smartsdr_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    const char *name = NULL;
    int tx_capable;
    int i, retval;

    ENTERFUNC;

    for (i = 0; i < SMARTSDR_ANT_COUNT; i++)
    {
        if (ant == RIG_ANT_N(i))
        {
            name = smartsdr_ant_names[i];
            break;
        }
    }

    if (name == NULL)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = smartsdr_prop_set(rig, SMARTSDR_P_RXANT, name);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* Only move the transmit antenna if the radio lists this port as
     * transmit-capable, so a receive-only port cannot land on txant. */
    smartsdr_state_lock(priv);
    tx_capable = smartsdr_prop_valid(priv, SMARTSDR_P_TX_ANT_LIST)
                 && strstr(priv->props[SMARTSDR_P_TX_ANT_LIST].sval, name)
                 != NULL;
    smartsdr_state_unlock(priv);

    if (tx_capable)
    {
        retval = smartsdr_prop_set(rig, SMARTSDR_P_TXANT, name);
    }

    RETURNFUNC(retval);
}


int smartsdr_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                     ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    ENTERFUNC;

    *ant_curr = RIG_ANT_NONE;
    *ant_tx = RIG_ANT_NONE;
    *ant_rx = RIG_ANT_NONE;

    smartsdr_state_lock(priv);

    if (smartsdr_prop_valid(priv, SMARTSDR_P_RXANT))
    {
        *ant_rx = ant_from_name(priv->props[SMARTSDR_P_RXANT].sval);
        *ant_curr = *ant_rx;
    }

    if (smartsdr_prop_valid(priv, SMARTSDR_P_TXANT))
    {
        *ant_tx = ant_from_name(priv->props[SMARTSDR_P_TXANT].sval);
    }

    smartsdr_state_unlock(priv);

    if (*ant_curr == RIG_ANT_NONE)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(RIG_OK);
}
