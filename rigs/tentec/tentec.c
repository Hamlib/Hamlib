/*
 *  Hamlib Tentec backend - main file
 *  Copyright (c) 2001-2009 by Stephane Fillod
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
#include <string.h>  /* String function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"

#include "tentec.h"

static void tentec_tuning_factor_calc(RIG *rig);

#define EOM "\015"  /* CR */

#define TT_AM  '0'
#define TT_USB '1'
#define TT_LSB '2'
#define TT_CW  '3'
#define TT_FM  '4'

static int tentec_filters[] =
{
    6000, 5700, 5400, 5100, 4800, 4500, 4200, 3900, 3600, 3300, 3000, 2850, 2700, 2550, 2400,
    2250, 2100, 1950, 1800,
    1650, 1500, 1350, 1200, 1050, 900, 750, 675, 600, 525, 450, 375, 330, 300, 8000
};



/*
 * tentec_transaction
 * read exactly data_len bytes
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 */
int tentec_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                       int *data_len)
{
    int retval;
    struct rig_state *rs;

    rs = &rig->state;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* no data expected, TODO: flush input? */
    if (!data || !data_len)
    {
        return 0;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, *data_len, NULL, 0,
                         0, 1);

    if (retval == -RIG_ETIMEOUT)
    {
        retval = 0;
    }

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}


/*
 * tentec_init:
 * Basically, it just sets up *priv
 */
int tentec_init(RIG *rig)
{
    struct tentec_priv_data *priv;

    rig->state.priv = (struct tentec_priv_data *)calloc(1, sizeof(
                          struct tentec_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct tentec_priv_data));

    /*
     * set arbitrary initial status
     */
    priv->freq = MHz(10);
    priv->mode = RIG_MODE_AM;
    priv->width = kHz(6);
    priv->pbt = 0;
    priv->cwbfo = 1000;
    priv->agc = RIG_AGC_MEDIUM; /* medium */
    priv->lnvol = priv->spkvol = 0.0;   /* mute */

    /* tentec_tuning_factor_calc needs rig->state.priv */
    tentec_tuning_factor_calc(rig);

    return RIG_OK;
}

/*
 * Tentec generic tentec_cleanup routine
 * the serial port is closed by the frontend
 */
int tentec_cleanup(RIG *rig)
{
    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

/*
 * Tentec transceiver only open routine
 * Restart and set program to execute.
 */
int tentec_trx_open(RIG *rig)
{
    int retval;

    /*
     * be kind: use XX first, and do 'Dsp Program Execute' only
     * in " DSP START" state.
     */
    retval = tentec_transaction(rig, "P1" EOM, 3, NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}


/*
 * Tuning Factor Calculations
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in supported modes.
 */
static void tentec_tuning_factor_calc(RIG *rig)
{
    struct tentec_priv_data *priv;
    freq_t tfreq;
    int adjtfreq, mcor, fcor, cwbfo;

    priv = (struct tentec_priv_data *)rig->state.priv;
    cwbfo = 0;

    /* computed fcor only used if mode is not CW */
    fcor = (int)floor((double)priv->width / 2.0) + 200;

    switch (priv->mode)
    {
    case RIG_MODE_AM:
    case RIG_MODE_FM:
        mcor = 0; break;

    case RIG_MODE_CW:
        mcor = -1; cwbfo = priv->cwbfo; fcor = 0; break;

    case RIG_MODE_LSB:
        mcor = -1; break;

    case RIG_MODE_USB:
        mcor = 1; break;

    default:
        rig_debug(RIG_DEBUG_BUG, "%s: invalid mode %s\n", __func__,
                  rig_strrmode(priv->mode));
        mcor = 1; break;
    }

    tfreq = priv->freq / (freq_t)Hz(1);

    adjtfreq = (int)tfreq - 1250 + (int)(mcor * (fcor + priv->pbt));

    priv->ctf = (adjtfreq / 2500) + 18000;
    priv->ftf = (int)floor((double)(adjtfreq % 2500) * 5.46);
    priv->btf = (int)floor((double)(fcor + priv->pbt + cwbfo + 8000) * 2.73);
}

/*
 * tentec_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */
int tentec_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct tentec_priv_data *priv;
    struct rig_state *rs = &rig->state;
    int retval;
    char freqbuf[16];
    freq_t old_freq;

    priv = (struct tentec_priv_data *)rig->state.priv;

    old_freq = priv->freq;
    priv->freq = freq;
    tentec_tuning_factor_calc(rig);

    SNPRINTF(freqbuf, sizeof(freqbuf), "N%c%c%c%c%c%c" EOM,
             priv->ctf >> 8, priv->ctf & 0xff,
             priv->ftf >> 8, priv->ftf & 0xff,
             priv->btf >> 8, priv->btf & 0xff);

    retval = write_block(&rs->rigport, (unsigned char *) freqbuf, strlen(freqbuf));

    if (retval != RIG_OK)
    {
        priv->freq = old_freq;
        return retval;
    }

    return RIG_OK;
}

/*
 * tentec_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tentec_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct tentec_priv_data *priv = (struct tentec_priv_data *)rig->state.priv;

    *freq = priv->freq;

    return RIG_OK;
}

/*
 * tentec_set_mode
 * Assumes rig!=NULL
 */
int tentec_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct tentec_priv_data *priv = (struct tentec_priv_data *)rig->state.priv;
    struct rig_state *rs = &rig->state;
    char ttmode;
    rmode_t saved_mode;
    pbwidth_t saved_width;
    int ttfilter = -1, retval;
    char mdbuf[32];

    switch (mode)
    {
    case RIG_MODE_USB:      ttmode = TT_USB; break;

    case RIG_MODE_LSB:      ttmode = TT_LSB; break;

    case RIG_MODE_CW:       ttmode = TT_CW; break;

    case RIG_MODE_AM:       ttmode = TT_AM; break;

    case RIG_MODE_FM:       ttmode = TT_FM; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported mode %s\n", __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    /* backup current values
     * in case we fail to write to port
     */
    saved_mode = priv->mode;
    saved_width = priv->width;

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        for (ttfilter = 0; tentec_filters[ttfilter] != 0; ttfilter++)
        {
            if (tentec_filters[ttfilter] == width)
            {
                break;
            }
        }

        if (tentec_filters[ttfilter] != width)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unsupported width %d\n", __func__, (int)width);
            return -RIG_EINVAL;
        }

        priv->width = width;
    }

    priv->mode = mode;

    tentec_tuning_factor_calc(rig);

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        SNPRINTF(mdbuf, sizeof(mdbuf),  "W%c" EOM
                 "N%c%c%c%c%c%c" EOM
                 "M%c" EOM,
                 ttfilter,
                 priv->ctf >> 8, priv->ctf & 0xff,
                 priv->ftf >> 8, priv->ftf & 0xff,
                 priv->btf >> 8, priv->btf & 0xff,
                 ttmode);
        retval = write_block(&rs->rigport, (unsigned char *) mdbuf, strlen(mdbuf));

        if (retval != RIG_OK)
        {
            priv->mode = saved_mode;
            priv->width = saved_width;
            return retval;
        }
    }
    else
    {
        SNPRINTF(mdbuf, sizeof(mdbuf),
                 "N%c%c%c%c%c%c" EOM
                 "M%c" EOM,
                 priv->ctf >> 8, priv->ctf & 0xff,
                 priv->ftf >> 8, priv->ftf & 0xff,
                 priv->btf >> 8, priv->btf & 0xff,
                 ttmode);
        retval = write_block(&rs->rigport, (unsigned char *) mdbuf, strlen(mdbuf));

        if (retval != RIG_OK)
        {
            priv->mode = saved_mode;
            return retval;
        }
    }

    return RIG_OK;
}

/*
 * tentec_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tentec_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct tentec_priv_data *priv = (struct tentec_priv_data *)rig->state.priv;

    *mode = priv->mode;
    *width = priv->width;

    return RIG_OK;
}


/*
 * tentec_set_level
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sens though)
 */
int tentec_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct tentec_priv_data *priv = (struct tentec_priv_data *)rig->state.priv;
    struct rig_state *rs = &rig->state;
    int retval = RIG_OK;
    char cmdbuf[32];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level)
    {
    case RIG_LEVEL_AGC:
        /* default to MEDIUM */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "G%c" EOM,
                 val.i == RIG_AGC_SLOW ? '1' : (
                     val.i == RIG_AGC_FAST ? '3' : '2'));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->agc = val.i;
        }

        return retval;

    case RIG_LEVEL_AF:
        /* FIXME: support also separate Lineout setting
         * -> need to create RIG_LEVEL_LINEOUT ?
         */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "C\x7f%c" EOM, (int)((1.0 - val.f) * 63.0));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->lnvol = priv->spkvol = val.f;
        }

        return retval;

    case RIG_LEVEL_IF:
        priv->pbt = val.i;
        retval = tentec_set_freq(rig, vfo, priv->freq);
        return retval;

    case RIG_LEVEL_CWPITCH:
        priv->cwbfo = val.i;

        if (priv->mode == RIG_MODE_CW)
        {
            retval = tentec_set_freq(rig, vfo, priv->freq);
        }

        return retval;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * tentec_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int tentec_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct tentec_priv_data *priv = (struct tentec_priv_data *)rig->state.priv;
    int retval, lvl_len;
    unsigned char lvlbuf[32];


    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        /* read A/D converted value */
        lvl_len = 4;
        retval = tentec_transaction(rig, "X" EOM, 2, (char *) lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "tentec_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        lvlbuf[3] = '\0';
        rig_debug(RIG_DEBUG_VERBOSE, "tentec_get_level: cmd=%c,hi=%d,lo=%d\n",
                  lvlbuf[0], lvlbuf[1], lvlbuf[2]);
        val->i = (lvlbuf[1] << 8) + lvlbuf[2];
        break;

    case RIG_LEVEL_AGC:
        val->i = priv->agc;
        break;

    case RIG_LEVEL_AF:
        val->f = priv->spkvol;
        break;

    case RIG_LEVEL_IF:
        val->i = priv->pbt;
        break;

    case RIG_LEVEL_CWPITCH:
        val->i = priv->cwbfo;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * tentec_get_info
 * Assumes rig!=NULL
 */
const char *tentec_get_info(RIG *rig)
{
    static char buf[100];   /* FIXME: reentrancy */
    int firmware_len, retval;

    /*
     * protocol version
     */
    firmware_len = 10;
    retval = tentec_transaction(rig, "?" EOM, 2, buf, &firmware_len);

    if ((retval != RIG_OK) || (firmware_len > 10))
    {
        rig_debug(RIG_DEBUG_ERR, "tentec_get_info: ack NG, len=%d\n",
                  firmware_len);
        return NULL;
    }

    return buf;
}


/*
 * initrigs_tentec is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(tentec)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&tt550_caps);
    rig_register(&tt516_caps);
    rig_register(&tt565_caps);
    rig_register(&tt538_caps);
    rig_register(&tt585_caps);
    rig_register(&tt588_caps);
    rig_register(&tt599_caps);
    rig_register(&rx320_caps);
    rig_register(&rx331_caps);
    rig_register(&rx340_caps);
    rig_register(&rx350_caps);

    return RIG_OK;
}

