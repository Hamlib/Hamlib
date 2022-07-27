/*
 *  Hamlib Watkins-Johnson backend - main file
 *  Copyright (c) 2004 by Stephane Fillod
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

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"
#include "token.h"

#include "wj.h"


#define CMDSZ 10

const struct confparams wj_cfg_params[] =
{
    {
        TOK_RIGID, "receiver_id", "receiver ID", "receiver ID",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 15, 1 } }
    },
    { RIG_CONF_END, NULL, }
};


/*
 * modes
 */
#define MD_AM   0
#define MD_FM   1
#define MD_CW   2
#define MD_VCW  3   /* BFO variable */
#define MD_ISB  4
#define MD_LSB  5
#define MD_USB  6
#define MD_AMNL 7


/*
 * wj_transaction
 *
 * I'm not sure how monitor protocol works, whether you have
 * to send the full frame, or just the modal byte. --SF
 *
 * TODO: decode the whole reply, and maybe do some caching
 */
static int wj_transaction(RIG *rig, int monitor)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;

    unsigned char buf[CMDSZ] = { 0x8, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned char rxbuf[CMDSZ];
    unsigned char freqbuf[4];
    unsigned wj_agc, wj_mode, wj_width, wj_bfo, wj_rfgain;
    int retval;

    if (monitor)
    {
        buf[1] |= 0x40;    /* Monitor+AGC dump */
    }
    else
    {
        buf[0] |= 0x40;    /* Command */
    }

    buf[0] |= priv->receiver_id & 0x0f;

    /* tuned frequency */
    to_bcd_be(freqbuf, priv->freq / 10, 7);
    buf[1] |= freqbuf[0] & 0x3f;
    buf[2] |= freqbuf[1] >> 1;
    buf[3] |= ((freqbuf[1] & 0x1) << 6) | (freqbuf[2] >> 2);
    buf[4] |= ((freqbuf[2] & 0x2) << 5) | (freqbuf[3] >> 3);

    /* gain mode */
    switch (priv->agc.i)
    {
    case RIG_AGC_SLOW: wj_agc = 0; break;   /* slow, 2s */

    case RIG_AGC_OFF: wj_agc = 1; break;    /* "not used" */

    case RIG_AGC_FAST: wj_agc = 2; break;   /* normal, 0.1s */

    case RIG_AGC_USER: wj_agc = 3; break;   /* manual */

    default: return -RIG_EINVAL;
    }

    buf[4] |= wj_agc & 0x1;
    buf[5] |= (wj_agc & 0x2) << 5;

    /* IF BW */
    switch (priv->width)
    {
    case  200:
    case 1000: wj_width = 0; break; /* spare */

    case  500: wj_width = 1; break;

    case 2000: wj_width = 2; break;

    case 4000: wj_width = 3; break;

    case 8000: wj_width = 4; break;

    case 3000:
    case 6000:
    case 12000:
    case 16000: wj_width = 5; break;    /* spare */

    default:
        return -RIG_EINVAL;
    }

    buf[5] |= (wj_width & 0x7) << 3;

    /* Detection mode */
    switch (priv->mode)
    {
    case RIG_MODE_CW:   wj_mode = (priv->ifshift.i != 0) ? MD_VCW : MD_CW; break;

    case RIG_MODE_USB:  wj_mode = MD_USB; break;

    case RIG_MODE_LSB:  wj_mode = MD_LSB; break;

    case RIG_MODE_FM:   wj_mode = MD_FM; break;

    case RIG_MODE_AM:   wj_mode = MD_AM; break;

    case RIG_MODE_AMS:  wj_mode = MD_ISB; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(priv->mode));
        return -RIG_EINVAL;
    }

    buf[5] |= wj_mode & 0x7;

    /* BFO frequency, not sure though */
    wj_bfo = (priv->ifshift.i / 10) + 0x400; /* LSBit is 10Hz, +455kHz */
    buf[6] |= (wj_bfo >> 5) & 0x3f;
    buf[7] |= (wj_bfo & 0x1f) << 2;

    /* RF gain */
    wj_rfgain = (unsigned)(priv->rfgain.f * 0x7f);
    buf[7] |= (wj_rfgain >> 6) & 0x1;
    buf[8] |= (wj_rfgain & 0x3f) << 1;

    /* buf[9]: not used if command byte, but must be transmitted */

    rig_flush(&rig->state.rigport);

    retval = write_block(&rig->state.rigport, buf, CMDSZ);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (monitor)
    {
        /*
        * Transceiver sends back ">"
        */
        retval = read_block(&rig->state.rigport, rxbuf, CMDSZ);

        if (retval < 0 || retval > CMDSZ)
        {
            return -RIG_ERJCTED;
        }

        /*
         *  TODO: analyze back the reply, and fill in the priv struct
         */
        priv->rawstr.i = rxbuf[9] & 0x7f;
    }

    return retval;
}

int wj_init(RIG *rig)
{
    struct wj_priv_data *priv;

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct wj_priv_data *)calloc(1, sizeof(struct wj_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->receiver_id = 0;
    priv->freq = kHz(500);
    priv->mode = RIG_MODE_AM;
    priv->width = kHz(8);
    priv->agc.i = RIG_AGC_SLOW;
    priv->rfgain.f = 1;
    priv->ifshift.i = 0;

    return RIG_OK;
}

/*
 */
int wj_cleanup(RIG *rig)
{
    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}



/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int wj_set_conf(RIG *rig, token_t token, const char *val)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_RIGID:
        priv->receiver_id = atoi(val);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int wj_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_RIGID:
        SNPRINTF(val, val_len, "%u", priv->receiver_id);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int wj_get_conf(RIG *rig, token_t token, char *val)
{
    return wj_get_conf2(rig, token, val, 128);
}

/*
 * wj_set_freq
 * Assumes rig!=NULL
 */
int wj_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;

    priv->freq = freq;

    return wj_transaction(rig, 0);
}

/*
 * wj_get_freq
 * Assumes rig!=NULL
 */
int wj_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;
    int retval;

    retval =  wj_transaction(rig, 1);

    if (retval == RIG_OK)
    {
        return retval;
    }

    *freq = priv->freq;

    return RIG_OK;
}

/*
 * wj_set_mode
 * Assumes rig!=NULL
 */
int wj_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;

    priv->mode = mode;

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        priv->width = width;
    }

    return wj_transaction(rig, 0);
}

/*
 * wj_get_mode
 * Assumes rig!=NULL
 */
int wj_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;
    int retval;

    retval =  wj_transaction(rig, 1);

    if (retval == RIG_OK)
    {
        return retval;
    }

    *mode = priv->mode;
    *width = priv->width;

    return RIG_OK;
}



/*
 * wj_set_level
 * Assumes rig!=NULL
 */
int wj_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;

    switch (level)
    {
    case RIG_LEVEL_IF:
        priv->ifshift.i = val.i;
        break;

    case RIG_LEVEL_RF:
        priv->rfgain.f = val.f;
        break;

    case RIG_LEVEL_AGC:
        priv->agc.i = val.i;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported %s\n", __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return wj_transaction(rig, 0);
}

/*
 * wj_get_level
 * Assumes rig!=NULL
 */
int wj_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct wj_priv_data *priv = (struct wj_priv_data *)rig->state.priv;
    int retval = RIG_OK;

    retval =  wj_transaction(rig, 1);

    if (retval == RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        val->i = priv->rawstr.i;
        break;

    case RIG_LEVEL_IF:
        val->i = priv->ifshift.i;
        break;

    case RIG_LEVEL_RF:
        val->f = priv->rfgain.f;
        break;

    case RIG_LEVEL_AGC:
        val->i = priv->agc.i;
        break;


    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported %s\n", __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return retval;
}


/*
 * initrigs_wj is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(wj)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&wj8888_caps);

    return RIG_OK;
}


