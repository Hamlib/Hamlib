/*
 *  Hamlib Kenwood backend - TS570 description
 *  Copyright (c) 2001-2005 by Stephane Fillod
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

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <hamlib/rig.h>
#include "kenwood.h"

#define TS570_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS570_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS570_AM_TX_MODES RIG_MODE_AM

#define TS570_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_TSQL|RIG_FUNC_TONE|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_LOCK|RIG_FUNC_BC|RIG_FUNC_TUNER)

#define TS570_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_SLOPE_LOW|RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_KEYSPD)

#define TS570_VFO (RIG_VFO_A|RIG_VFO_B)
#define TS570_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)
#define TS570_SCAN_OPS (RIG_SCAN_VFO)
#define TS570_ANTS (RIG_ANT_1|RIG_ANT_2)

static struct kenwood_priv_caps ts570_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

static int ts570_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[50];
    size_t length;
    int retval;

    retval = kenwood_transaction(rig, "MD", buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    length = strlen(buf);

    if (length != 3 || buf[1] != 'D')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected MD answer, len=%d\n",
                  __func__, (int)length);
        return -RIG_ERJCTED;
    }

    switch (buf[2])
    {
    case MD_CW:         *mode = RIG_MODE_CW; break;

    case MD_CWR:    *mode = RIG_MODE_CWR; break;

    case MD_USB:    *mode = RIG_MODE_USB; break;

    case MD_LSB:    *mode = RIG_MODE_LSB; break;

    case MD_FM:         *mode = RIG_MODE_FM; break;

    case MD_AM:         *mode = RIG_MODE_AM; break;

    case MD_FSK:    *mode = RIG_MODE_RTTY; break;

    case MD_FSKR:   *mode = RIG_MODE_RTTYR; break;

    case MD_NONE:   *mode = RIG_MODE_NONE; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "ts570_get_mode: "
                  "unsupported mode '%c'\n", buf[2]);
        return -RIG_EINVAL;
    }

    /*
     * Use FW (Filter Width) for CW and RTTY,
     * SL (dsp Slope Low cut-off)  for all the other modes.
     * This is how it works on the TS870S, which does not have SL/SH commands.
     * TODO: combine SL and SH to set/read bandwidth....
     */
    switch (*mode)
    {
    case RIG_MODE_CW:
    case RIG_MODE_CWR:
    case RIG_MODE_RTTY:
    case RIG_MODE_RTTYR:
        retval = kenwood_transaction(rig, "FW", buf, sizeof(buf));

        if (retval != RIG_OK) { return retval; }

        length = strlen(buf);

        if (length != 6 || buf[1] != 'W')
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unexpected FW answer, len=%d\n", __func__, (int)length);
            return -RIG_ERJCTED;
        }

        *width = atoi(&buf[2]);
        break;

    case RIG_MODE_USB:
    case RIG_MODE_LSB:
    case RIG_MODE_FM:
    case RIG_MODE_AM:
        retval = kenwood_transaction(rig, "SL", buf, sizeof(buf));

        if (retval != RIG_OK) { return retval; }

        length = strlen(buf);

        if (length != 4 || buf[1] != 'L')
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unexpected SL answer, len=%d\n", __func__, (int)length);
            return -RIG_ERJCTED;
        }

        *width = 50 * atoi(&buf[2]);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static char mode_to_char(rmode_t mode)
{
    switch (mode)
    {
    case RIG_MODE_CW:       return (MD_CW);

    case RIG_MODE_CWR:      return (MD_CWR);

    case RIG_MODE_USB:      return (MD_USB);

    case RIG_MODE_LSB:      return (MD_LSB);

    case RIG_MODE_FM:       return (MD_FM);

    case RIG_MODE_AM:       return (MD_AM);

    case RIG_MODE_RTTY:     return (MD_FSK);

    case RIG_MODE_RTTYR:    return (MD_FSKR);

    default:
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
    }

    return (RIG_MODE_NONE);
}

static int ts570_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char buf[16];
    int kmode, retval;

    if ((kmode = mode_to_char(mode)) == RIG_MODE_NONE)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "MD%c", kmode);
    retval = kenwood_transaction(rig, buf, NULL, 0);

    if (retval != RIG_OK) { return retval; }

    if (RIG_PASSBAND_NOCHANGE == width) { return retval; }

    switch (mode)
    {
    case RIG_MODE_CW:
    case RIG_MODE_CWR:
    case RIG_MODE_RTTY:
    case RIG_MODE_RTTYR:
        SNPRINTF(buf, sizeof(buf), "FW%04d", (int)width);
        retval = kenwood_transaction(rig, buf, NULL, 0);

        if (retval != RIG_OK) { return retval; }

        break;

    case RIG_MODE_USB:
    case RIG_MODE_LSB:
    case RIG_MODE_FM:
    case RIG_MODE_AM:
        SNPRINTF(buf, sizeof(buf), "SL%02d", (int)width / 50);
        retval = kenwood_transaction(rig, buf, NULL, 0);

        if (retval != RIG_OK) { return retval; }

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * extends kenwood_set_func
 * Assumes rig!=NULL, val!=NULL
 */
int ts570_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char fctbuf[6];

    /* Filter unimplemented RIG_FUNC_TUNER and allow settings 0..2 for
     * RIG_FUNC_NR.
     * Send all other requests to kenwood_set_func()
     */
    switch (func)
    {

    case RIG_FUNC_NR:
        if ((status < 0) || (status > 2))
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(fctbuf, sizeof(fctbuf), "NR%01d", status);
        return kenwood_transaction(rig, fctbuf, NULL, 0);

    case RIG_FUNC_TUNER:
        SNPRINTF(fctbuf, sizeof(fctbuf), "AC %c0", (0 == status) ? '0' : '1');
        return kenwood_transaction(rig, fctbuf, NULL, 0);

    default:
        return kenwood_set_func(rig, vfo, func, status);
    }

    return RIG_OK;
}


/*
 * extends kenwood_get_func
 * Assumes rig!=NULL, val!=NULL
 */
int ts570_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char fctbuf[50];
    size_t fct_len;
    int retval;

    fct_len = 50;

    /* filter unimplemented RIG_FUNC_TUNER
     * and send all other requests to kenwood_get_func()
     */
    switch (func)
    {
    case RIG_FUNC_NR:
        retval = kenwood_transaction(rig, "NR", fctbuf, sizeof(fctbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        fct_len = strlen(fctbuf);

        if (fct_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n", __func__, (int)fct_len);
            return -RIG_ERJCTED;
        }

        *status = atoi(&fctbuf[2]);
        break;

    case RIG_FUNC_TUNER:
        retval = kenwood_transaction(rig, "AC", fctbuf, sizeof(fctbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        fct_len = strlen(fctbuf);

        if (fct_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n", __func__, (int)fct_len);
            return -RIG_ERJCTED;
        }

        *status = fctbuf[3] == '0' ? 0 : 1;
        break;

    default:
        return kenwood_get_func(rig, vfo, func, status);
    }

    return RIG_OK;
}

/*
 * ts570_set_level
 * Assumes rig!=NULL
 *
 * set levels of most functions
 */
int
ts570_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int kenwood_val;

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        kenwood_val = val.i;

        /* set the preamplifier if a correct value is entered */
        if (kenwood_val == 0)
        {
            SNPRINTF(levelbuf, sizeof(levelbuf), "PA0");
        }
        else
        {
            int i;

            for (i = 0; i < HAMLIB_MAXDBLSTSIZ; i++)
                if (kenwood_val == rig->state.preamp[i])
                {
                    SNPRINTF(levelbuf, sizeof(levelbuf), "PA%01d", i + 1);
                    break;  /* found - stop searching */
                }
                else
                {
                    return -RIG_EINVAL;
                }
        }

        return kenwood_transaction(rig, levelbuf, NULL, 0);

    case RIG_LEVEL_RFPOWER:
        /* level for TS570D is from 0.. 100W in SSB and CW */
        kenwood_val = val.f * 100;
        SNPRINTF(levelbuf, sizeof(levelbuf), "PC%03d", kenwood_val);
        return kenwood_transaction(rig, levelbuf, NULL, 0);

    case RIG_LEVEL_MICGAIN:
        /* level is from 0..100 */
        kenwood_val = val.f * 100;
        SNPRINTF(levelbuf, sizeof(levelbuf), "MG%03d", kenwood_val);
        return kenwood_transaction(rig, levelbuf, NULL, 0);

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    return RIG_OK;        /* never reached */
}


/*
 * ts570_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int
ts570_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char ackbuf[50];
    size_t ack_len;
    int levelint;
    int retval;

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        /* ts570d returns 5..100 measured in watt */
        retval = kenwood_transaction(rig, "PC", ackbuf, sizeof(ackbuf));

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (5 != ack_len)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[2], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        val->f = (float) levelint / 100.;
        return RIG_OK;

    case RIG_LEVEL_MICGAIN:
        /* reads from 0..100 */
        retval = kenwood_transaction(rig, "MG", ackbuf, sizeof(ackbuf));

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (5 != ack_len)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[2], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        val->f = (float) levelint / 100.;
        return RIG_OK;

    case RIG_LEVEL_PREAMP:
        retval = kenwood_transaction(rig, "PA", ackbuf, sizeof(ackbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (3  != ack_len)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)ack_len);
            return -RIG_ERJCTED;
        }

        sscanf(ackbuf + 2, "%d", &levelint);

        if (levelint == 0)
        {
            val->i = 0;
        }
        else
        {
            int i;

            for (i = 0; i < levelint && i < HAMLIB_MAXDBLSTSIZ; i++)
            {
                if (rig->state.preamp[i] == 0)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: unexpected att level %d\n", __func__,
                              (int)levelint);
                    return -RIG_EPROTO;
                }
            }

            if (i != levelint)
            {
                return -RIG_EINTERNAL;
            }

            val->i = rig->state.preamp[i - 1];
        }

        break;

    default:
        return kenwood_get_level(rig, vfo, level, val);
    }

    return RIG_OK;        /* never reached */
}

/*
 * ts570_get_split_vfo
 */
int ts570_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    char ack[10];
    char ack2[10];
    int retval;

    retval = kenwood_transaction(rig, "FR", ack, sizeof(ack));

    if (retval != RIG_OK)
    {
        return retval;
    }


    retval = kenwood_transaction(rig, "FT", ack2, sizeof(ack2));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack[2] != ack2[2])
    {
        *split = RIG_SPLIT_ON;

        switch (ack2[2])
        {
        case '0': *tx_vfo = RIG_VFO_A; break;

        case '1': *tx_vfo = RIG_VFO_B; break;

        case '2': *tx_vfo = RIG_VFO_MEM; break;

        default: rig_debug(RIG_DEBUG_ERR, "ts570_get_split_vfo: unknown tx vfo: %d\n",
                               ack2[2]);
            return -RIG_EINVAL;
        }

    }
    else
    {
        *split = RIG_SPLIT_OFF;
        *tx_vfo = RIG_VFO_CURR;
    }

    return RIG_OK;
}

#define cmd_trm(rig) ((struct kenwood_priv_caps *)(rig)->caps->priv)->cmdtrm

/*
 * ts570_set_split_vfo
 */
int ts570_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    char cmdbuf[16], ackbuf[20];
    int retval;
    unsigned char vfo_function;

    if (vfo != RIG_VFO_CURR)
    {
        switch (vfo)
        {
        case RIG_VFO_VFO:
        case RIG_VFO_A: vfo_function = '0'; break;

        case RIG_VFO_B: vfo_function = '1'; break;

        case RIG_VFO_MEM: vfo_function = '2'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "ts570_set_split_vfo: unsupported VFO %s\n",
                      rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        /* set RX VFO */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "FR%c%c", vfo_function, cmd_trm(rig));

        retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (split == RIG_SPLIT_ON)
    {
        switch (txvfo)
        {
        case RIG_VFO_VFO:
        case RIG_VFO_A: vfo_function = '0'; break;

        case RIG_VFO_B: vfo_function = '1'; break;

        case RIG_VFO_MEM: vfo_function = '2'; break;

        case RIG_VFO_TX:
            if (vfo == RIG_VFO_A) { vfo_function = '0'; }
            else if (vfo == RIG_VFO_B) { vfo_function = '1'; }
            else if (vfo == RIG_VFO_MEM) { vfo_function = '2'; }
            else
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: unsupported vfo/txvfo combination vfo=%s, txvfo=%s\n", __func__,
                          rig_strvfo(vfo), rig_strvfo(txvfo));
                return -RIG_EINVAL;
            }

            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "ts570_set_split_vfo: unsupported VFO %s\n",
                      rig_strvfo(txvfo));
            return -RIG_EINVAL;
        }

        /* set TX VFO */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "FT%c%c", vfo_function, cmd_trm(rig));

        retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }

    }
    else   /* RIG_SPLIT_OFF */
        if (vfo == RIG_VFO_CURR)
        {
            /* switch to current RX VFO */
            /* first ask for it */
            retval = kenwood_transaction(rig, "FR", ackbuf, sizeof(ackbuf));

            if (retval != RIG_OK)
            {
                return retval;
            }

            /* and then set it to both vfo's */
            vfo_function = ackbuf[2];
            SNPRINTF(cmdbuf, sizeof(cmdbuf), "FT%c%c", vfo_function, cmd_trm(rig));

            retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

            if (retval != RIG_OK)
            {
                return retval;
            }
        }

    return RIG_OK;
}



/* memory capabilities */
#define TS570_MEM_CAP {     \
    .freq = 1,      \
    .mode = 1,      \
    .tx_freq=1,     \
    .tx_mode=1,     \
    .split=1,       \
    .ctcss_tone=1       \
}

int ts570_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char cmdbuf[30];
    int retval;
    int num, freq, tx_freq, tone;
    char mode, tx_mode, tones;

    num = chan->channel_num;
    freq = (int)chan->freq;
    mode = mode_to_char(chan->mode);

    if (chan->split == RIG_SPLIT_ON)
    {
        tx_freq = (int)chan->tx_freq;
        tx_mode = mode_to_char(chan->tx_mode);
    }
    else
    {
        tx_freq = 0;
        tx_mode = '\0';
    }

    for (tone = 1; rig->caps->ctcss_list[tone - 1] != 0 && tone < 39; tone++)
    {
        if (rig->caps->ctcss_list[tone - 1] == chan->ctcss_tone)
        {
            break;
        }
    }

    if (chan->ctcss_tone != 0)
    {
        tones = '1';
    }
    else
    {
        tones = '0';
        tone = 0;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "MW0 %02d%011d%c0%c%02d ",
             num, freq, mode, tones, tone);

    retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "MW1 %02d%011d%c0%c%02d ",
             num, tx_freq, tx_mode, tones, tone);

    retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

int ts570_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    char infobuf[50];
    int retval;
    size_t info_len;

    retval = kenwood_transaction(rig, "IF", infobuf, sizeof(infobuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    info_len = strlen(infobuf);

    if (info_len != 37 || infobuf[1] != 'F')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n", __func__, (int)info_len);
        return -RIG_ERJCTED;
    }

    if (infobuf[24] == '0')     /* RIT off? */
    {
        *rit = 0;
    }
    else
    {
        infobuf[23] = '\0';
        *rit = atoi(&infobuf[18]);
    }

    return RIG_OK;
}

int ts570_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    char buf[50];
    unsigned char c;
    int retval, i;

    if (rit == 0)
    {
        retval = kenwood_transaction(rig, "RT0", NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
        else
        {
            return RIG_OK;
        }
    }
    else
    {
        retval = kenwood_transaction(rig, "RT1", NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (rit > 0)
    {
        c = 'U';
    }
    else
    {
        c = 'D';
    }

    SNPRINTF(buf, sizeof(buf), "R%c", c);

    retval = kenwood_transaction(rig, "RC", NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    for (i = 0; i < labs(lrint(rit / 10)); i++)
    {
        retval = kenwood_transaction(rig, buf, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return RIG_OK;
}

int ts570_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    char buf[50];
    unsigned char c;
    int retval, i;

    if (rit == 0)
    {
        retval = kenwood_transaction(rig, "XT0", NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
        else
        {
            return RIG_OK;
        }
    }
    else
    {
        retval = kenwood_transaction(rig, "XT1", NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (rit > 0)
    {
        c = 'U';
    }
    else
    {
        c = 'D';
    }

    SNPRINTF(buf, sizeof(buf), "R%c", c);

    retval = kenwood_transaction(rig, "RC", NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    for (i = 0; i < labs(lrint(rit / 10)); i++)
    {
        retval = kenwood_transaction(rig, buf, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return RIG_OK;
}



/*
 * ts570 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts570s_caps =
{
    RIG_MODEL(RIG_MODEL_TS570S),
    .model_name = "TS-570S",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".2",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  30,
    .timeout = 500,
    .retry =  10,

    .has_get_func =  TS570_FUNC_ALL,
    .has_set_func =  TS570_FUNC_ALL,
    .has_get_level =  TS570_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS570_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 12, RIG_DBLST_END, },
    .attenuator =   { 18, RIG_DBLST_END, },
    .max_rit =  Hz(9990),
    .max_xit =  Hz(9990),
    .max_ifshift =  Hz(0),
    .vfo_ops =  TS570_VFO_OP,
    .scan_ops =  TS570_SCAN_OPS,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .agc_level_count = 2,
    .agc_levels = { RIG_AGC_FAST, RIG_AGC_SLOW },
    .bank_qty =   0,
    .chan_desc_sz =  0,


    .chan_list =  {
        {  0, 89, RIG_MTYPE_MEM,  TS570_MEM_CAP },
        { 90, 99, RIG_MTYPE_EDGE, TS570_MEM_CAP },
        RIG_CHAN_END,
    },
    .rx_range_list1 =  {
        {kHz(500), MHz(60), TS570_ALL_MODES, -1, -1, TS570_VFO, TS570_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {kHz(1810), kHz(1850) - 1, TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS}, /* 25W class */
        {kHz(3500), kHz(3800) - 1, TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(3500), kHz(3800) - 1, TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(7), kHz(7100), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(7), kHz(7100), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(10100), kHz(10150), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(10100), kHz(10150), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(14), kHz(14350), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(14), kHz(14350), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(18068), kHz(18168), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(18068), kHz(18168), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(21), kHz(21450), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(21), kHz(21450), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(24890), kHz(24990), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(24890), kHz(24990), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(28), kHz(29700), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(28), kHz(29700), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(50), MHz(54), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(50), MHz(54), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(500), MHz(60), TS570_ALL_MODES, -1, -1, TS570_VFO, TS570_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2) - 1, TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS}, /* 25W class */
        {kHz(3500), MHz(4) - 1, TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(3500), MHz(4) - 1, TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(7), kHz(7300), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(7), kHz(7300), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(10100), kHz(10150), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(10100), kHz(10150), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(14), kHz(14350), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(14), kHz(14350), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(18068), kHz(18168), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(18068), kHz(18168), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(21), kHz(21450), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(21), kHz(21450), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(24890), kHz(24990), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(24890), kHz(24990), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(28), kHz(29700), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(28), kHz(29700), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(50), MHz(54), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(50), MHz(54), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {TS570_ALL_MODES, 50},
        {TS570_ALL_MODES, 100},
        {TS570_ALL_MODES, kHz(1)},
        {TS570_ALL_MODES, kHz(5)},
        {TS570_ALL_MODES, kHz(9)},
        {TS570_ALL_MODES, kHz(10)},
        {TS570_ALL_MODES, 12500},
        {TS570_ALL_MODES, kHz(20)},
        {TS570_ALL_MODES, kHz(25)},
        {TS570_ALL_MODES, kHz(100)},
        {TS570_ALL_MODES, MHz(1)},
        {TS570_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_CW, Hz(200)},
        {RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_FM, kHz(14)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts570_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  ts570_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  ts570_set_xit,
    .get_xit =  ts570_get_xit,
    .set_mode =  ts570_set_mode,
    .get_mode =  ts570_get_mode,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_split_vfo = ts570_set_split_vfo,
    .get_split_vfo = ts570_get_split_vfo,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  ts570_set_func,
    .get_func =  ts570_get_func,
    .set_ant = kenwood_set_ant_no_ack,
    .get_ant = kenwood_get_ant,
    .set_level =  ts570_set_level,
    .get_level =  ts570_get_level,
    .send_morse =  kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .get_channel = kenwood_get_channel,
    .set_channel = ts570_set_channel,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .set_powerstat =  kenwood_set_powerstat,
    .get_powerstat =  kenwood_get_powerstat,
    .scan =  kenwood_scan,
    .reset =  kenwood_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * ts570d rig capabilities, which is basically the ts570s without 6m.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts570d_caps =
{
    RIG_MODEL(RIG_MODEL_TS570D),
    .model_name = "TS-570D",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  30,
    .timeout = 500,
    .retry =  10,

    .has_get_func =  TS570_FUNC_ALL,
    .has_set_func =  TS570_FUNC_ALL,
    .has_get_level =  TS570_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS570_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 12, RIG_DBLST_END, },
    .attenuator =   { 18, RIG_DBLST_END, },
    .max_rit =  Hz(9990),
    .max_xit =  Hz(9990),
    .max_ifshift =  Hz(0),
    .vfo_ops =  TS570_VFO_OP,
    .scan_ops =  TS570_SCAN_OPS,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {  0, 89, RIG_MTYPE_MEM,  TS570_MEM_CAP  },
        { 90, 99, RIG_MTYPE_EDGE, TS570_MEM_CAP  },
        RIG_CHAN_END,
    },
    .rx_range_list1 =  {
        {kHz(500), MHz(30), TS570_ALL_MODES, -1, -1, TS570_VFO, TS570_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {kHz(1810), kHz(1850) - 1, TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS}, /* 25W class */
        {kHz(3500), kHz(3800) - 1, TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(3500), kHz(3800) - 1, TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(7), kHz(7100), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(7), kHz(7100), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(10100), kHz(10150), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(10100), kHz(10150), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(14), kHz(14350), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(14), kHz(14350), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(18068), kHz(18168), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(18068), kHz(18168), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(21), kHz(21450), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(21), kHz(21450), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(24890), kHz(24990), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(24890), kHz(24990), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(28), kHz(29700), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(28), kHz(29700), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(500), MHz(30), TS570_ALL_MODES, -1, -1, TS570_VFO, TS570_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2) - 1, TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS}, /* 25W class */
        {kHz(3500), MHz(4) - 1, TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(3500), MHz(4) - 1, TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(7), kHz(7300), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(7), kHz(7300), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(10100), kHz(10150), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(10100), kHz(10150), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(14), kHz(14350), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(14), kHz(14350), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(18068), kHz(18168), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(18068), kHz(18168), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(21), kHz(21450), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(21), kHz(21450), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {kHz(24890), kHz(24990), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {kHz(24890), kHz(24990), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        {MHz(28), kHz(29700), TS570_OTHER_TX_MODES, 5000, 100000, TS570_VFO, TS570_ANTS},
        {MHz(28), kHz(29700), TS570_AM_TX_MODES, 5000, 25000, TS570_VFO, TS570_ANTS},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {TS570_ALL_MODES, 50},
        {TS570_ALL_MODES, 100},
        {TS570_ALL_MODES, kHz(1)},
        {TS570_ALL_MODES, kHz(5)},
        {TS570_ALL_MODES, kHz(9)},
        {TS570_ALL_MODES, kHz(10)},
        {TS570_ALL_MODES, 12500},
        {TS570_ALL_MODES, kHz(20)},
        {TS570_ALL_MODES, kHz(25)},
        {TS570_ALL_MODES, kHz(100)},
        {TS570_ALL_MODES, MHz(1)},
        {TS570_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, Hz(500)},
        {RIG_MODE_SSB, Hz(0)},
        {RIG_MODE_SSB, kHz(1)},
        {RIG_MODE_CW, Hz(400)},
        {RIG_MODE_CW, Hz(100)},
        {RIG_MODE_CW, Hz(1000)},
        {RIG_MODE_RTTY, Hz(1000)},
        {RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_RTTY, Hz(1500)},
        {RIG_MODE_AM, Hz(500)},
        {RIG_MODE_AM, Hz(0)},
        {RIG_MODE_AM, kHz(1)},
        {RIG_MODE_FM, Hz(500)},
        {RIG_MODE_FM, Hz(0)},
        {RIG_MODE_FM, kHz(1)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts570_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  ts570_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  ts570_set_xit,
    .get_xit =  ts570_get_xit,
    .set_mode =  ts570_set_mode,
    .get_mode =  ts570_get_mode,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_split_vfo = ts570_set_split_vfo,
    .get_split_vfo = ts570_get_split_vfo,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  ts570_set_func,
    .get_func =  ts570_get_func,
    .set_ant = kenwood_set_ant_no_ack,
    .get_ant = kenwood_get_ant,
    .set_level =  ts570_set_level,
    .get_level =  ts570_get_level,
    .send_morse =  kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .get_channel = kenwood_get_channel,
    .set_channel = ts570_set_channel,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .set_powerstat =  kenwood_set_powerstat,
    .get_powerstat =  kenwood_get_powerstat,
    .scan =  kenwood_scan,
    .reset =  kenwood_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


