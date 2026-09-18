/*
 *  Hamlib SmartSDR classic rig controls
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

/* What an operator turns on the front panel: frequency, mode, PTT, the RIT  */
/* and XIT offsets, the tuning step and the keyer. Also the conversion       */
/* between Hamlib modes and the names SmartSDR uses for them, which every    */
/* other layer needs in one direction or the other.                          */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "cache.h"
#include "smartsdr_priv.h"
#include "smartsdr_props.h"
#include "smartsdr_rig.h"
#include "smartsdr_session.h"
#include "smartsdr_slice.h"


int smartsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[64];
    int retval;
    ENTERFUNC;

    /* VFO B addresses the transmit slice, which is how Hamlib reaches the
     * second frequency on a radio whose slices each have only one. */
    if (smartsdr_vfo_is_sub(vfo) == 1)
    {
        RETURNFUNC(smartsdr_set_split_freq(rig, vfo, freq));
    }

    if (smartsdr_vfo_is_sub(vfo) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    snprintf(cmd, sizeof(cmd), "slice tune %d %.6f autopan=1", priv->slicenum,
             freq / 1e6);
    retval = smartsdr_transaction(rig, cmd);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    rig_set_cache_freq(rig, RIG_VFO_A, freq);

    smartsdr_state_lock(priv);
    smartsdr_slice_set_freq(priv, freq);
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}




/* The radio's mode spellings, in one place because they are needed in both
 * directions: parsing what a status line reports, and naming a mode in a
 * command. Keeping two lists let them drift.
 *
 * "emit" marks the spelling to send for a mode. The entries below it are
 * spellings the radio also reports which map onto a mode already named above,
 * so they are accepted but never sent. */
static const struct
{
    rmode_t mode;
    const char *name;
    int emit;
} smartsdr_mode_names[] =
{
    { RIG_MODE_CW,     "CW",   1 },
    { RIG_MODE_USB,    "USB",  1 },
    { RIG_MODE_LSB,    "LSB",  1 },
    { RIG_MODE_PKTUSB, "DIGU", 1 },
    { RIG_MODE_PKTLSB, "DIGL", 1 },
    { RIG_MODE_AM,     "AM",   1 },
    { RIG_MODE_FM,     "FM",   1 },
    { RIG_MODE_FMN,    "FMN",  1 },
    { RIG_MODE_SAM,    "SAM",  1 },
    { RIG_MODE_RTTY,   "RTTY", 1 },
    { RIG_MODE_FMN,    "NFM",  0 },
    /* DFM is the radio's FM for data: it carries its own pre/de-emphasis
     * setting (dfm_pre_de_emphasis) rather than the voice network, which is
     * what Hamlib means by PKTFM -- Yaesu spells it DATA-FM, Icom FM-D. */
    { RIG_MODE_PKTFM,  "DFM",  1 },
};

#define SMARTSDR_MODE_NAME_COUNT \
    (int)(sizeof(smartsdr_mode_names) / sizeof(smartsdr_mode_names[0]))


/* Compare a already-lowercased token against a table spelling. */
static int smartsdr_mode_name_matches(const char *lowered, const char *name)
{
    int i;

    for (i = 0; lowered[i] != '\0' && name[i] != '\0'; i++)
    {
        if (lowered[i] != tolower((unsigned char)name[i]))
        {
            return 0;
        }
    }

    return lowered[i] == '\0' && name[i] == '\0';
}


static int smartsdr_mode_numeric_index(unsigned u, rmode_t *out)
{
    /* Typical Flex slice mode ordering (numeric status lines). */
    switch (u)
    {
    case 0:
        *out = RIG_MODE_LSB;
        return 0;

    case 1:
        *out = RIG_MODE_USB;
        return 0;

    case 3:
    case 4:
        *out = RIG_MODE_CW;
        return 0;

    case 5:
        *out = RIG_MODE_FM;
        return 0;

    case 6:
        *out = RIG_MODE_AM;
        return 0;

    case 7:
        *out = RIG_MODE_PKTLSB;
        return 0;

    case 8:
        *out = RIG_MODE_PKTUSB;
        return 0;

    case 9:
        *out = RIG_MODE_SAM;
        return 0;

    case 11:
        *out = RIG_MODE_FMN;
        return 0;

    case 12:
        *out = RIG_MODE_RTTY;
        return 0;

    default:
        return 1;
    }
}


int smartsdr_parse_mode_status_value(const char *value, rmode_t *out_mode)
{
    char low[32];
    size_t j;
    static int warned_unknown;

    if (value == NULL || out_mode == NULL)
    {
        return -1;
    }

    while (*value == ' ' || *value == '\t')
    {
        value++;
    }

    if (*value == '\0')
    {
        return -1;
    }

    j = 0;

    while (value[j] != '\0' && j < sizeof(low) - 1)
    {
        low[j] = (char)tolower((unsigned char)value[j]);
        j++;
    }

    low[j] = '\0';

    {
        int i;

        for (i = 0; i < SMARTSDR_MODE_NAME_COUNT; i++)
        {
            if (smartsdr_mode_name_matches(low, smartsdr_mode_names[i].name))
            {
                *out_mode = smartsdr_mode_names[i].mode;
                return 0;
            }
        }
    }

    {
        const char *q = value;
        int all_digit = 1;

        if (*q == '\0')
        {
            all_digit = 0;
        }

        while (*q != '\0')
        {
            if (!isdigit((unsigned char)*q))
            {
                all_digit = 0;
                break;
            }

            q++;
        }

        if (all_digit)
        {
            unsigned long u = strtoul(value, NULL, 10);
            rmode_t m;

            if (smartsdr_mode_numeric_index((unsigned)u, &m) == 0)
            {
                *out_mode = m;
                return 0;
            }
        }
    }

    if (!warned_unknown)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: unknown mode token \"%s\" (ignored)\n", __func__, value);
        warned_unknown = 1;
    }

    return 1;
}


int smartsdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    ENTERFUNC;

    if (smartsdr_vfo_is_sub(vfo) == 1)
    {
        RETURNFUNC(smartsdr_get_split_freq(rig, vfo, freq));
    }

    if (smartsdr_vfo_is_sub(vfo) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    smartsdr_state_lock(priv);
    *freq = smartsdr_slice_freq(priv);
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}

int smartsdr_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[64];
    static const char slicechar[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H' };
    int has_tx;
    int retval, xmit_retval;
    ENTERFUNC;

    smartsdr_state_lock(priv);
    has_tx = priv->props[SMARTSDR_P_TX].ival;
    smartsdr_state_unlock(priv);

    if (!has_tx)
    {
        /* tx= comes from this slice's status and says whether it holds the
         * transmitter; without it the radio would key a different slice. */
        rig_debug(RIG_DEBUG_ERR,
                  "%s: slice %c does not have PTT control\n", __func__,
                  slicechar[priv->slicenum]);
        RETURNFUNC(-RIG_ENTARGET);
    }

    if (ptt)
    {
        /* slice= is not optional: the radio rejects the command outright
         * unless it accompanies tx=, so both operands are needed to route
         * this slice's DAX audio to the transmitter. DAX channels are
         * 1-based, matching the channel the TX stream binds. */
        SNPRINTF(cmd, sizeof(cmd), "dax audio set %d slice=%d tx=1",
                 priv->slicenum + 1, priv->slicenum);
        retval = smartsdr_transaction(rig, cmd);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    SNPRINTF(cmd, sizeof(cmd), "slice set %d tx=1", priv->slicenum);
    retval = smartsdr_transaction(rig, cmd);

    /* Claiming the transmitter can fail, but an unkey still has to reach the
     * radio or it stays on the air, so only a keying attempt gives up here. */
    if (retval != RIG_OK && ptt)
    {
        RETURNFUNC(retval);
    }

    SNPRINTF(cmd, sizeof(cmd), "xmit %d", ptt);
    xmit_retval = smartsdr_transaction(rig, cmd);

    RETURNFUNC(xmit_retval != RIG_OK ? xmit_retval : retval);
}

int smartsdr_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    ENTERFUNC;

    smartsdr_state_lock(priv);
    *ptt = smartsdr_slice_ptt(priv) ? RIG_PTT_ON : RIG_PTT_OFF;
    smartsdr_state_unlock(priv);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __func__, *ptt);
    RETURNFUNC(RIG_OK);
}

const char *smartsdr_mode_name(rmode_t mode)
{
    int i;

    for (i = 0; i < SMARTSDR_MODE_NAME_COUNT; i++)
    {
        if (smartsdr_mode_names[i].emit && smartsdr_mode_names[i].mode == mode)
        {
            return smartsdr_mode_names[i].name;
        }
    }

    return NULL;
}


int smartsdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[64];
    const char *rmode = smartsdr_mode_name(mode);
    int retval;
    ENTERFUNC;

    if (smartsdr_vfo_is_sub(vfo) == 1)
    {
        RETURNFUNC(smartsdr_set_split_mode(rig, vfo, mode, width));
    }

    if (rmode == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode=%s\n", __func__,
                  rig_strrmode(mode));
        RETURNFUNC(-RIG_EINVAL);
    }

    snprintf(cmd, sizeof(cmd), "slice set %d mode=%s", priv->slicenum, rmode);
    retval = smartsdr_transaction(rig, cmd);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (width != RIG_PASSBAND_NOCHANGE && width != RIG_PASSBAND_NORMAL)
    {
        snprintf(cmd, sizeof(cmd), "filt %d 0 %ld", priv->slicenum, (long)width);
        retval = smartsdr_transaction(rig, cmd);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    RETURNFUNC(RIG_OK);
}

int smartsdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    ENTERFUNC;

    if (smartsdr_vfo_is_sub(vfo) == 1)
    {
        RETURNFUNC(smartsdr_get_split_mode(rig, vfo, mode, width));
    }

    if (smartsdr_vfo_is_sub(vfo) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    smartsdr_state_lock(priv);
    *mode = smartsdr_slice_mode(priv);
    *width = smartsdr_slice_width(priv);
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}


int smartsdr_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    ENTERFUNC;

    int retval;
    size_t msg_len = strlen(msg);
    size_t buf_len = msg_len + 20;

    char *newmsg = malloc(msg_len + 1);
    char *cmd;

    if (!newmsg)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    memcpy(newmsg, msg, msg_len + 1);

    /* The radio's CWX buffer uses 0x7f as the word separator. */
    for (size_t i = 0; newmsg[i] != '\0'; i++)
    {
        if (newmsg[i] == ' ')
        {
            newmsg[i] = 0x7f;
        }
    }

    cmd = malloc(buf_len);

    if (!cmd)
    {
        free(newmsg);
        RETURNFUNC(-RIG_ENOMEM);
    }

    snprintf(cmd, buf_len, "cwx send \"%s\"", newmsg);

    free(newmsg);

    retval = smartsdr_transaction(rig, cmd);

    free(cmd);

    RETURNFUNC(retval);
}


int smartsdr_stop_morse(RIG *rig, vfo_t vfo)
{
    int retval;
    ENTERFUNC;

    retval = smartsdr_transaction(rig, "cwx clear");

    RETURNFUNC(retval);

}


/* Ask the radio what it is. "info" reports model and serial, "version" the
 * firmware; both answer in the R-line body. Called once at open. */
void smartsdr_query_info(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char resp[SMARTSDR_RESP_LEN];
    char model[64] = "";
    char serial[64] = "";
    char fw[64] = "";
    const char *p;

    if (smartsdr_transaction_resp(rig, "info", resp, sizeof(resp)) == RIG_OK)
    {
        p = strstr(resp, "model=\"");

        if (p != NULL)
        {
            sscanf(p + 7, "%63[^\"]", model);
        }

        p = strstr(resp, "chassis_serial=\"");

        if (p != NULL)
        {
            sscanf(p + 16, "%63[^\"]", serial);
        }
    }

    if (smartsdr_transaction_resp(rig, "version", resp, sizeof(resp)) == RIG_OK)
    {
        p = strstr(resp, "SmartSDR-MB=");

        if (p != NULL)
        {
            sscanf(p + 12, "%63[^#\r\n]", fw);
        }
    }

    snprintf(priv->info, sizeof(priv->info), "%s%s%s%s%s%s",
             model[0] ? model : "FlexRadio",
             serial[0] ? " s/n " : "", serial,
             fw[0] ? " firmware " : "", fw,
             "");
}


const char *smartsdr_get_info(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    return priv->info[0] ? priv->info : NULL;
}


/* ------------------------------------------------------------------ */
/* RIT / XIT / tuning step                                             */
/* ------------------------------------------------------------------ */

/* The radio keeps the offset and its enable separately, so a zero offset also
 * clears the enable to match Hamlib, where zero means the offset is off. */
static int offset_set(RIG *rig, int freq_prop, int on_prop, shortfreq_t offset)
{
    char value[32];
    int retval;

    snprintf(value, sizeof(value), "%ld", (long)offset);
    retval = smartsdr_prop_set(rig, freq_prop, value);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return smartsdr_prop_set(rig, on_prop, offset != 0 ? "1" : "0");
}


int smartsdr_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    ENTERFUNC;
    RETURNFUNC(offset_set(rig, SMARTSDR_P_RIT_FREQ, SMARTSDR_P_RIT_ON, rit));
}


int smartsdr_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    ENTERFUNC;

    smartsdr_state_lock(priv);

    if (!smartsdr_prop_valid(priv, SMARTSDR_P_RIT_FREQ))
    {
        smartsdr_state_unlock(priv);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *rit = priv->props[SMARTSDR_P_RIT_ON].ival
           ? (shortfreq_t)priv->props[SMARTSDR_P_RIT_FREQ].ival : 0;
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}


int smartsdr_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    ENTERFUNC;
    RETURNFUNC(offset_set(rig, SMARTSDR_P_XIT_FREQ, SMARTSDR_P_XIT_ON, xit));
}


int smartsdr_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    ENTERFUNC;

    smartsdr_state_lock(priv);

    if (!smartsdr_prop_valid(priv, SMARTSDR_P_XIT_FREQ))
    {
        smartsdr_state_unlock(priv);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *xit = priv->props[SMARTSDR_P_XIT_ON].ival
           ? (shortfreq_t)priv->props[SMARTSDR_P_XIT_FREQ].ival : 0;
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}


int smartsdr_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    char value[32];

    ENTERFUNC;

    snprintf(value, sizeof(value), "%ld", (long)ts);
    RETURNFUNC(smartsdr_prop_set(rig, SMARTSDR_P_STEP, value));
}


int smartsdr_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    ENTERFUNC;

    smartsdr_state_lock(priv);

    if (!smartsdr_prop_valid(priv, SMARTSDR_P_STEP))
    {
        smartsdr_state_unlock(priv);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *ts = (shortfreq_t)priv->props[SMARTSDR_P_STEP].ival;
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}
