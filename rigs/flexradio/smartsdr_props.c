/*
 *  Hamlib SmartSDR tracked radio properties
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

/* The backend's model of the radio. A SmartSDR radio pushes its settings as */
/* status lines and never answers a query for one, so every value of interest */
/* is recorded as it arrives and read back from here. This file names those   */
/* values, absorbs them from a status line, and writes them back out.         */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "smartsdr_priv.h"
#include "smartsdr_props.h"
#include "smartsdr_session.h"


void smartsdr_state_lock(struct smartsdr_priv_data *priv)
{
    pthread_mutex_lock(&priv->state_lock);
}


void smartsdr_state_unlock(struct smartsdr_priv_data *priv)
{
    pthread_mutex_unlock(&priv->state_lock);
}


int64_t smartsdr_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


/* Find "key=" in a status line, requiring a space or '|' before it so that
 * e.g. "pan=" does not match inside "audio_pan=". Returns the value start. */
int smartsdr_status_as_double(const char *v, double *out)
{
    char *endp;
    double d;

    if (v == NULL || out == NULL)
    {
        return 0;
    }

    d = strtod(v, &endp);

    if (endp == v)
    {
        return 0;
    }

    *out = d;
    return 1;
}


int smartsdr_status_as_long(const char *v, long *out)
{
    char *endp;
    long l;

    if (v == NULL || out == NULL)
    {
        return 0;
    }

    l = strtol(v, &endp, 10);

    if (endp == v)
    {
        return 0;
    }

    *out = l;
    return 1;
}


const char *smartsdr_status_find(const char *line, const char *key)
{
    size_t keylen = strlen(key);
    const char *p = line;

    if (line == NULL)
    {
        return NULL;
    }

    while ((p = strstr(p, key)) != NULL)
    {
        int start_ok = (p == line) || (p[-1] == ' ') || (p[-1] == '|');

        if (start_ok && p[keylen] == '=')
        {
            return p + keylen + 1;
        }

        p += keylen;
    }

    return NULL;
}


/* The properties this backend tracks. The radio reports an object across
 * several status lines of differing shape, so each value is recorded when it
 * arrives rather than re-parsed out of retained text on every read. */
static const struct
{
    const char *key;
    uint8_t object;     /* SMARTSDR_OBJ_* */
    uint8_t is_string;
    /* A few properties are reported under one name and set under another; the
     * transmit monitor is reported as sb_monitor but only accepts mon. NULL
     * means the radio uses one name for both. */
    const char *set_key;
    /* Not everything is set with "<object> set key=value". The CW and
     * microphone settings are reported on the transmit object but written
     * positionally to their own command, "cw wpm 30" rather than
     * "transmit set speed=30". NULL means the object's own key=value form. */
    const char *set_cmd;
} smartsdr_props[SMARTSDR_PROP_COUNT] =
{
    [SMARTSDR_P_AUDIO_LEVEL]    = { "audio_level",    SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_SQUELCH_LEVEL]  = { "squelch_level",  SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_NR_LEVEL]       = { "nr_level",       SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_NB_LEVEL]       = { "nb_level",       SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_APF_LEVEL]      = { "apf_level",      SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_AUDIO_PAN]      = { "audio_pan",      SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_RFGAIN]         = { "rfgain",         SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_AUDIO_MUTE]     = { "audio_mute",     SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_SQUELCH]        = { "squelch",        SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_NR]             = { "nr",             SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_NB]             = { "nb",             SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_WNB]            = { "wnb",            SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_ANF]            = { "anf",            SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_APF]            = { "apf",            SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_LOCK]           = { "lock",           SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_RIT_ON]         = { "rit_on",         SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_XIT_ON]         = { "xit_on",         SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_DIVERSITY]      = { "diversity",      SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_AGC_MODE]       = { "agc_mode",       SMARTSDR_OBJ_SLICE, 1 },
    [SMARTSDR_P_RXANT]          = { "rxant",          SMARTSDR_OBJ_SLICE, 1 },
    [SMARTSDR_P_TXANT]          = { "txant",          SMARTSDR_OBJ_SLICE, 1 },
    [SMARTSDR_P_TX_ANT_LIST]    = { "tx_ant_list",    SMARTSDR_OBJ_SLICE, 1 },

    [SMARTSDR_P_RFPOWER]        = { "rfpower",        SMARTSDR_OBJ_TRANSMIT, 0 },
    [SMARTSDR_P_MIC_LEVEL]      = { "mic_level",      SMARTSDR_OBJ_TRANSMIT, 0, "miclevel" },
    [SMARTSDR_P_SP_LEVEL]       = { "speech_processor_level",  SMARTSDR_OBJ_TRANSMIT, 0 },
    [SMARTSDR_P_VOX_LEVEL]      = { "vox_level",      SMARTSDR_OBJ_TRANSMIT, 0 },
    [SMARTSDR_P_VOX_DELAY]      = { "vox_delay",      SMARTSDR_OBJ_TRANSMIT, 0 },
    [SMARTSDR_P_SPEED]          = { "speed",          SMARTSDR_OBJ_TRANSMIT, 0, "wpm", "cw" },
    [SMARTSDR_P_PITCH]          = { "pitch",          SMARTSDR_OBJ_TRANSMIT, 0, "pitch", "cw" },
    [SMARTSDR_P_BREAK_IN_DELAY] = { "break_in_delay", SMARTSDR_OBJ_TRANSMIT, 0, "break_in_delay", "cw" },
    [SMARTSDR_P_MON_GAIN_SB]    = { "mon_gain_sb",    SMARTSDR_OBJ_TRANSMIT, 0 },
    [SMARTSDR_P_MON_GAIN_CW]    = { "mon_gain_cw",    SMARTSDR_OBJ_TRANSMIT, 0 },
    [SMARTSDR_P_VOX_ENABLE]     = { "vox_enable",     SMARTSDR_OBJ_TRANSMIT, 0 },
    [SMARTSDR_P_SP_ENABLE]      = { "speech_processor_enable", SMARTSDR_OBJ_TRANSMIT, 0 },
    [SMARTSDR_P_BREAK_IN]       = { "break_in",       SMARTSDR_OBJ_TRANSMIT, 0, "break_in", "cw" },
    [SMARTSDR_P_SB_MONITOR]     = { "sb_monitor",     SMARTSDR_OBJ_TRANSMIT, 0, "mon" },
    [SMARTSDR_P_TUNE]           = { "tune",           SMARTSDR_OBJ_TRANSMIT, 0 },

    [SMARTSDR_P_RF_FREQUENCY]   = { "RF_frequency",   SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_MODE]           = { "mode",           SMARTSDR_OBJ_SLICE, 1 },
    [SMARTSDR_P_FILTER_LO]      = { "filter_lo",      SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_FILTER_HI]      = { "filter_hi",      SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_TX]             = { "tx",             SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_RIT_FREQ]       = { "rit_freq",       SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_XIT_FREQ]       = { "xit_freq",       SMARTSDR_OBJ_SLICE, 0 },
    [SMARTSDR_P_STEP]           = { "step",           SMARTSDR_OBJ_SLICE, 0 },

    [SMARTSDR_P_STATE]          = { "state",          SMARTSDR_OBJ_INTERLOCK, 1 },
};


void smartsdr_props_absorb(struct smartsdr_priv_data *priv, int object,
                           const char *line)
{
    int i;

    if (priv == NULL || line == NULL)
    {
        return;
    }

    for (i = 0; i < SMARTSDR_PROP_COUNT; i++)
    {
        const char *v;

        if (smartsdr_props[i].key == NULL
                || smartsdr_props[i].object != (uint8_t)object)
        {
            continue;
        }

        v = smartsdr_status_find(line, smartsdr_props[i].key);

        if (v == NULL)
        {
            continue;
        }

        if (smartsdr_props[i].is_string)
        {
            size_t n = 0;

            /* An empty value is a truncated line, not a state the radio is
             * in, and overwriting a good reading with it loses the last thing
             * the radio actually said. A value that is present but not one
             * this backend knows is different: that is the radio's state, and
             * a reader is better told the mode is unavailable than told a
             * stale one. */
            if (*v == '\0' || *v == ' ' || *v == '\n')
            {
                continue;
            }

            while (v[n] != '\0' && v[n] != ' ' && v[n] != '\n'
                    && n + 1 < sizeof(priv->props[i].sval))
            {
                priv->props[i].sval[n] = v[n];
                n++;
            }

            priv->props[i].sval[n] = '\0';
        }
        else
        {
            double d;

            /* strtod answers 0 for both an empty value and a non-numeric one,
             * so a truncated "RF_frequency=" would record a radio tuned to
             * 0 Hz -- indistinguishable, to anything reading it back, from a
             * radio really tuned there. Leave the previous reading in place
             * and do not mark the property seen. */
            if (!smartsdr_status_as_double(v, &d))
            {
                continue;
            }

            priv->props[i].dval = d;
            priv->props[i].ival = (int)d;
        }

        priv->props[i].seen = 1;
        priv->props[i].stamp_ms = smartsdr_now_ms();
    }
}


/* A property is usable once the radio has reported it and, when an expiry is
 * configured, has done so recently enough. */
int smartsdr_prop_valid(const struct smartsdr_priv_data *priv, int idx)
{
    if (!priv->props[idx].seen)
    {
        return 0;
    }

    if (priv->status_timeout_ms > 0
            && (smartsdr_now_ms() - priv->props[idx].stamp_ms)
            > priv->status_timeout_ms)
    {
        return 0;
    }

    return 1;
}


/* The command that assigns a property, written into out. A property reported
 * under one name may be set under another, and the CW and microphone settings
 * are written positionally to their own command rather than as the transmit
 * object's key=value, so the command cannot be derived from the status key.
 * Returns 0, or -1 when the index names no property or the command does not
 * fit, which would otherwise send a truncated command meaning something
 * else. */
int smartsdr_prop_build_set_cmd(int idx, int slicenum, const char *value,
                                char *out, size_t out_len)
{
    const char *key;
    int n;

    if (idx < 0 || idx >= SMARTSDR_PROP_COUNT || value == NULL
            || out == NULL || out_len == 0
            || smartsdr_props[idx].key == NULL)
    {
        return -1;
    }

    /* The interlock reports what the radio is doing; there is no command that
     * sets it. Building one would produce a plausible-looking "slice set N
     * state=..." that the radio rejects. */
    if (idx == SMARTSDR_P_STATE)
    {
        return -1;
    }

    key = smartsdr_props[idx].set_key != NULL
          ? smartsdr_props[idx].set_key
          : smartsdr_props[idx].key;

    if (smartsdr_props[idx].set_cmd != NULL)
    {
        n = snprintf(out, out_len, "%s %s %s", smartsdr_props[idx].set_cmd,
                     key, value);
    }
    else if (smartsdr_props[idx].object == SMARTSDR_OBJ_TRANSMIT)
    {
        n = snprintf(out, out_len, "transmit set %s=%s", key, value);
    }
    else
    {
        n = snprintf(out, out_len, "slice set %d %s=%s", slicenum, key, value);
    }

    if (n < 0 || (size_t)n >= out_len)
    {
        out[0] = '\0';
        return -1;
    }

    return 0;
}


/* Send a property assignment and record the accepted value. The radio does not
 * broadcast a status line for these changes, and re-subscribing to force one
 * disturbs audio, so an accepted write updates the tracked value directly. */
int smartsdr_prop_set(RIG *rig, int idx, const char *value)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[160];
    int retval;

    if (smartsdr_prop_build_set_cmd(idx, priv->slicenum, value, cmd,
                                    sizeof(cmd)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no command for property %d\n", __func__,
                  idx);
        return -RIG_EINVAL;
    }

    retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

    if (retval == RIG_OK)
    {
        smartsdr_state_lock(priv);

        if (smartsdr_props[idx].is_string)
        {
            strncpy(priv->props[idx].sval, value,
                    sizeof(priv->props[idx].sval) - 1);
            priv->props[idx].sval[sizeof(priv->props[idx].sval) - 1] = '\0';
        }
        else
        {
            priv->props[idx].dval = strtod(value, NULL);
            priv->props[idx].ival = (int)priv->props[idx].dval;
        }

        priv->props[idx].seen = 1;
        priv->props[idx].stamp_ms = smartsdr_now_ms();
        smartsdr_state_unlock(priv);
    }

    return retval;
}
