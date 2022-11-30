/*
 *  Hamlib Dummy backend - main file
 *  Copyright (c) 2001-2010 by Stephane Fillod
 *  Copyright (c) 2010 by Nate Bargmann
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

// cppcheck-suppress *
#include <stdio.h>
// cppcheck-suppress *
#include <stdlib.h>
// cppcheck-suppress *
#include <string.h>  /* String function definitions */
// cppcheck-suppress *
#include <unistd.h>  /* UNIX standard function definitions */
// cppcheck-suppress *
#include <time.h>

#include "hamlib/rig.h"
#include "dummy_common.h"
#include "serial.h"
#include "parallel.h"
#include "cm108.h"
#include "gpio.h"
#include "misc.h"
#include "tones.h"
#include "idx_builtin.h"
#include "register.h"

#include "dummy.h"

#define NB_CHAN 22      /* see caps->chan_list */


#define CMDSLEEP 20*1000  /* ms for each command */

struct dummy_priv_data
{
    /* current vfo already in rig_state ? */
    vfo_t curr_vfo;
    vfo_t last_vfo; /* VFO A or VFO B, when in MEM mode */

    split_t split;
    vfo_t tx_vfo;
    ptt_t ptt;
    powerstat_t powerstat;
    int bank;
    value_t parms[RIG_SETTING_MAX];
    int ant_option[4]; /* simulate 4 antennas */
    int trn; /* transceive */

    channel_t *curr;    /* points to vfo_a, vfo_b or mem[] */

    // we're trying to emulate all sorts of vfo possibilities so this looks redundant
    channel_t vfo_a;
    channel_t vfo_b;
    channel_t vfo_c;
    channel_t vfo_maina;
    channel_t vfo_mainb;
    channel_t vfo_suba;
    channel_t vfo_subb;
    channel_t mem[NB_CHAN];

    struct ext_list *ext_funcs;
    struct ext_list *ext_parms;

    char *magic_conf;
    int static_data;

    //freq_t freq_vfoa;
    //freq_t freq_vfob;
};

/* levels pertain to each VFO */
static const struct confparams dummy_ext_levels[] =
{
    {
        TOK_EL_MAGICLEVEL, "MGL", "Magic level", "Magic level, as an example",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
    },
    {
        TOK_EL_MAGICFUNC, "MGF", "Magic func", "Magic function, as an example",
        NULL, RIG_CONF_CHECKBUTTON
    },
    {
        TOK_EL_MAGICOP, "MGO", "Magic Op", "Magic Op, as an example",
        NULL, RIG_CONF_BUTTON
    },
    {
        TOK_EL_MAGICCOMBO, "MGC", "Magic combo", "Magic combo, as an example",
        "VALUE1", RIG_CONF_COMBO, { .c = { .combostr = { "VALUE1", "VALUE2", "NONE", NULL } } }
    },
    { RIG_CONF_END, NULL, }
};

static const struct confparams dummy_ext_funcs[] =
{
    {
        TOK_EL_MAGICEXTFUNC, "MGEF", "Magic ext func", "Magic ext function, as an example",
        NULL, RIG_CONF_CHECKBUTTON
    },
    { RIG_CONF_END, NULL, }
};

/* parms pertain to the whole rig */
static const struct confparams dummy_ext_parms[] =
{
    {
        TOK_EP_MAGICPARM, "MGP", "Magic parm", "Magic parameter, as an example",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
    },
    { RIG_CONF_END, NULL, }
};

/* cfgparams are configuration item generally used by the backend's open() method */
static const struct confparams dummy_cfg_params[] =
{
    {
        TOK_CFG_MAGICCONF, "mcfg", "Magic conf", "Magic parameter, as an example",
        "DX", RIG_CONF_STRING, { }
    },
    {
        TOK_CFG_STATIC_DATA, "static_data", "Static data", "Output only static data, no randomization of meter values",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    { RIG_CONF_END, NULL, }
};

/********************************************************************/

static void init_chan(RIG *rig, vfo_t vfo, channel_t *chan)
{
    chan->channel_num = 0;
    chan->vfo = vfo;
    strcpy(chan->channel_desc, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN_A:
        chan->freq = MHz(145);
        break;

    case RIG_VFO_B:
    case RIG_VFO_MAIN_B:
        chan->freq = MHz(146);
        break;

    case RIG_VFO_SUB_A:
        chan->freq = MHz(147);
        break;

    case RIG_VFO_SUB_B:
        chan->freq = MHz(148);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s(%d) unknown vfo=%s\n", __FILE__, __LINE__,
                  rig_strvfo(vfo));
    }

    chan->mode = RIG_MODE_FM;
    chan->width = rig_passband_normal(rig, RIG_MODE_FM);
    chan->tx_freq = chan->freq;
    chan->tx_mode = chan->mode;
    chan->tx_width = chan->width;
    chan->split = RIG_SPLIT_OFF;
    chan->tx_vfo = vfo;

    chan->rptr_shift = RIG_RPT_SHIFT_NONE;
    chan->rptr_offs = 0;
    chan->ctcss_tone = 0;
    chan->dcs_code = 0;
    chan->ctcss_sql = 0;
    chan->dcs_sql = 0;
    chan->rit = 0;
    chan->xit = 0;
    chan->tuning_step = 0;
    chan->ant = 0;

    chan->funcs = (setting_t)0;
    memset(chan->levels, 0, RIG_SETTING_MAX * sizeof(value_t));
}

static void copy_chan(channel_t *dest, const channel_t *src)
{
    struct ext_list *saved_ext_levels;
    int i;

    /* TODO: ext_levels[] of different sizes */

    for (i = 0; !RIG_IS_EXT_END(src->ext_levels[i]) &&
            !RIG_IS_EXT_END(dest->ext_levels[i]); i++)
    {
        dest->ext_levels[i] = src->ext_levels[i];
    }

    saved_ext_levels = dest->ext_levels;
    memcpy(dest, src, sizeof(channel_t));
    dest->ext_levels = saved_ext_levels;
}

static int dummy_init(RIG *rig)
{
    struct dummy_priv_data *priv;
    int i;

    ENTERFUNC;
    priv = (struct dummy_priv_data *)calloc(1, sizeof(struct dummy_priv_data));

    if (!priv)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    rig->state.priv = (void *)priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig->state.rigport.type.rig = RIG_PORT_NONE;

    priv->split = RIG_SPLIT_OFF;
    priv->ptt = RIG_PTT_OFF;
    priv->powerstat = RIG_POWER_ON;
    rig->state.powerstat = priv->powerstat;
    priv->bank = 0;
    memset(priv->parms, 0, RIG_SETTING_MAX * sizeof(value_t));

    memset(priv->mem, 0, sizeof(priv->mem));

    for (i = 0; i < NB_CHAN; i++)
    {
        priv->mem[i].channel_num = i;
        priv->mem[i].vfo = RIG_VFO_MEM;

        priv->mem[i].ext_levels = alloc_init_ext(dummy_ext_levels);

        if (!priv->mem[i].ext_levels)
        {
            RETURNFUNC(-RIG_ENOMEM);
        }
    }

    priv->vfo_a.ext_levels = alloc_init_ext(dummy_ext_levels);

    if (!priv->vfo_a.ext_levels)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv->vfo_b.ext_levels = alloc_init_ext(dummy_ext_levels);

    if (!priv->vfo_b.ext_levels)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv->ext_funcs = alloc_init_ext(dummy_ext_funcs);

    if (!priv->ext_funcs)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv->ext_parms = alloc_init_ext(dummy_ext_parms);

    if (!priv->ext_parms)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    init_chan(rig, RIG_VFO_A, &priv->vfo_a);
    init_chan(rig, RIG_VFO_B, &priv->vfo_b);
    init_chan(rig, RIG_VFO_MAIN_A, &priv->vfo_maina);
    init_chan(rig, RIG_VFO_MAIN_B, &priv->vfo_mainb);
    init_chan(rig, RIG_VFO_SUB_A, &priv->vfo_suba);
    init_chan(rig, RIG_VFO_SUB_B, &priv->vfo_subb);
    priv->curr = &priv->vfo_a;

    if (rig->caps->rig_model == RIG_MODEL_DUMMY_NOVFO)
    {
        priv->curr_vfo = priv->last_vfo = RIG_VFO_CURR;
    }
    else
    {
        priv->curr_vfo = priv->last_vfo = RIG_VFO_A;
    }

    priv->magic_conf = strdup("DX");

    RETURNFUNC(RIG_OK);
}

static int dummy_cleanup(RIG *rig)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    int i;

    ENTERFUNC;

    for (i = 0; i < NB_CHAN; i++)
    {
        free(priv->mem[i].ext_levels);
    }

    free(priv->vfo_a.ext_levels);
    free(priv->vfo_b.ext_levels);
    free(priv->ext_funcs);
    free(priv->ext_parms);
    free(priv->magic_conf);

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    RETURNFUNC(RIG_OK);
}

static int dummy_open(RIG *rig)
{
    ENTERFUNC;

    if (rig->caps->rig_model == RIG_MODEL_DUMMY_NOVFO)
    {
        // then we emulate a rig without set_vfo or get_vfo
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Emulating rig without get_vfo or set_vfo\n",
                  __func__);
        rig->caps->set_vfo = NULL;
        rig->caps->get_vfo = NULL;
    }

    usleep(CMDSLEEP);

    RETURNFUNC(RIG_OK);
}

static int dummy_close(RIG *rig)
{
    ENTERFUNC;

    usleep(CMDSLEEP);

    RETURNFUNC(RIG_OK);
}

static int dummy_set_conf(RIG *rig, token_t token, const char *val)
{
    struct dummy_priv_data *priv;

    ENTERFUNC;
    priv = (struct dummy_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_CFG_MAGICCONF:
        if (val)
        {
            free(priv->magic_conf);
            priv->magic_conf = strdup(val);
        }

        break;

    case TOK_CFG_STATIC_DATA:
        priv->static_data = atoi(val) ? 1 : 0;
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

static int dummy_get_conf(RIG *rig, token_t token, char *val)
{
    struct dummy_priv_data *priv;

    ENTERFUNC;
    priv = (struct dummy_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_CFG_MAGICCONF:
        strcpy(val, priv->magic_conf);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

static int dummy_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    char fstr[20];

    ENTERFUNC;

    if (rig == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig is NULL!!!\n", __func__);
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) { vfo = priv->curr_vfo; }

    if (vfo == RIG_VFO_CURR || vfo == RIG_VFO_TX) { vfo = vfo_fixup(rig, vfo, rig->state.cache.split); }

// if needed for testing enable this to emulate a rig with 100hz resolution
#if 0
    // we emulate a rig with 100Hz set freq interval limits -- truncation
    freq = freq - fmod(freq, 100);
#endif
    usleep(CMDSLEEP);
    sprintf_freq(fstr, sizeof(fstr), freq);
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              rig_strvfo(vfo), fstr);

    switch (vfo)
    {
    case RIG_VFO_MAIN:
    case RIG_VFO_A: priv->vfo_a.freq = freq; break;

    case RIG_VFO_MAIN_A: priv->vfo_maina.freq = freq; break;

    case RIG_VFO_MAIN_B: priv->vfo_mainb.freq = freq; break;

    case RIG_VFO_SUB:
    case RIG_VFO_B: priv->vfo_b.freq = freq; break;

    case RIG_VFO_SUB_A: priv->vfo_suba.freq = freq; break;

    case RIG_VFO_SUB_B: priv->vfo_subb.freq = freq; break;

    case RIG_VFO_C: priv->vfo_c.freq = freq; break;
    }

    if (priv && !priv->split)
    {
        priv->curr->tx_freq = freq;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: curr->freq=%.0f, curr->tx_freq=%.0f\n",
              __func__,
              priv->curr->freq, priv->curr->tx_freq);
    RETURNFUNC(RIG_OK);
}


static int dummy_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;

    if (vfo == RIG_VFO_CURR && rig->caps->rig_model != RIG_MODEL_DUMMY_NOVFO) { vfo = priv->curr_vfo; }

    if ((vfo == RIG_VFO_SUB && rig->state.uplink == 1)
            || (vfo == RIG_VFO_MAIN && rig->state.uplink == 2))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: uplink=%d, ignoring get_freq\n", __func__,
                  rig->state.uplink);
        RETURNFUNC(RIG_OK);
    }

    usleep(CMDSLEEP);
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_MAIN:
    case RIG_VFO_A:  *freq = priv->vfo_a.freq; break;

    case RIG_VFO_MAIN_A: *freq = priv->vfo_maina.freq; break;

    case RIG_VFO_MAIN_B: *freq = priv->vfo_mainb.freq; break;

    case RIG_VFO_SUB:
    case RIG_VFO_B:  *freq = priv->vfo_b.freq; break;

    case RIG_VFO_SUB_A:  *freq = priv->vfo_suba.freq; break;

    case RIG_VFO_SUB_B:  *freq = priv->vfo_subb.freq; break;

    case RIG_VFO_C:  *freq = priv->vfo_c.freq; break;

    default: RETURNFUNC(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: freq=%.0f\n", __func__, *freq);
    RETURNFUNC(RIG_OK);
}


static int dummy_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;
    char buf[16];

    ENTERFUNC;
    usleep(CMDSLEEP);
    sprintf_freq(buf, sizeof(buf), width);
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s %s\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), buf);

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    if (width == RIG_PASSBAND_NOCHANGE)
    {
        switch (vfo)
        {
        case RIG_VFO_MAIN:
        case RIG_VFO_A: width = priv->vfo_a.width; break;

        case RIG_VFO_SUB:
        case RIG_VFO_B: width = priv->vfo_b.width; break;

        case RIG_VFO_C: width = priv->vfo_c.width; break;
        }
    }

    switch (vfo)
    {
    case RIG_VFO_MAIN:
    case RIG_VFO_A: priv->vfo_a.mode = mode; priv->vfo_a.width = width; break;

    case RIG_VFO_SUB:
    case RIG_VFO_B: priv->vfo_b.mode = mode; priv->vfo_b.width = width; break;

    case RIG_VFO_C: priv->vfo_c.mode = mode; priv->vfo_c.width = width; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown VFO=%s\n", __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    if (RIG_PASSBAND_NOCHANGE == width) { RETURNFUNC(RIG_OK); }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = curr->width = rig_passband_normal(rig, mode);
    }

    switch (vfo)
    {
    case RIG_VFO_A: priv->vfo_a.width = width; break;

    case RIG_VFO_B: priv->vfo_b.width = width; break;

    case RIG_VFO_C: priv->vfo_c.width = width; break;
    }

    RETURNFUNC(RIG_OK);
}


static int dummy_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;
    usleep(CMDSLEEP);
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__, rig_strvfo(vfo));

    if (vfo == RIG_VFO_CURR) { vfo = rig->state.current_vfo; }

    switch (vfo)
    {
    case RIG_VFO_MAIN:
    case RIG_VFO_A: *mode = priv->vfo_a.mode; *width = priv->vfo_a.width; break;

    case RIG_VFO_SUB:
    case RIG_VFO_B: *mode = priv->vfo_b.mode; *width = priv->vfo_b.width; break;

    case RIG_VFO_C: *mode = priv->vfo_c.mode; *width = priv->vfo_c.width; break;
    }

    RETURNFUNC(RIG_OK);
}


static int dummy_set_vfo(RIG *rig, vfo_t vfo)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__, rig_strvfo(vfo));

    if (vfo == RIG_VFO_CURR) { vfo = rig->state.current_vfo; }

    priv->last_vfo = priv->curr_vfo;
    priv->curr_vfo = vfo;

    switch (vfo)
    {
    case RIG_VFO_VFO: /* FIXME */

    case RIG_VFO_RX:
    case RIG_VFO_MAIN: priv->curr = &priv->vfo_a; break;

    case RIG_VFO_MAIN_A: priv->curr = &priv->vfo_maina; break;

    case RIG_VFO_MAIN_B: priv->curr = &priv->vfo_mainb; break;

    case RIG_VFO_A: priv->curr = &priv->vfo_a; break;

    case RIG_VFO_SUB: priv->curr = &priv->vfo_b; break;

    case RIG_VFO_SUB_A: priv->curr = &priv->vfo_suba; break;

    case RIG_VFO_SUB_B: priv->curr = &priv->vfo_subb; break;

    case RIG_VFO_B: priv->curr = &priv->vfo_b; break;

    case RIG_VFO_C: priv->curr = &priv->vfo_c; break;

    case RIG_VFO_MEM:
        if (curr->channel_num >= 0 && curr->channel_num < NB_CHAN)
        {
            priv->curr = &priv->mem[curr->channel_num];
            break;
        }

    case RIG_VFO_TX:
        if (priv->tx_vfo == RIG_VFO_A) { priv->curr = &priv->vfo_a; }
        else if (priv->tx_vfo == RIG_VFO_B) { priv->curr = &priv->vfo_b; }
        else if (priv->tx_vfo == RIG_VFO_MEM) { priv->curr = &priv->mem[curr->channel_num]; }
        else { priv->curr = &priv->vfo_a; }

        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s unknown vfo: %s\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->state.current_vfo = vfo;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;
    usleep(CMDSLEEP);
    *vfo = priv->curr_vfo;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;
    priv->ptt = ptt;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    int rc;
    int status = 0;

    ENTERFUNC;
    usleep(CMDSLEEP);

    // sneak a look at the hardware PTT and OR that in with our result
    // as if it had keyed us
    switch (rig->state.pttport.type.ptt)
    {
    case RIG_PTT_SERIAL_DTR:
        if (rig->state.pttport.fd >= 0)
        {
            if (RIG_OK != (rc = ser_get_dtr(&rig->state.pttport, &status))) { RETURNFUNC(rc); }

            *ptt = status ? RIG_PTT_ON : RIG_PTT_OFF;
        }
        else
        {
            *ptt = RIG_PTT_OFF;
        }

        break;

    case RIG_PTT_SERIAL_RTS:
        if (rig->state.pttport.fd >= 0)
        {
            if (RIG_OK != (rc = ser_get_rts(&rig->state.pttport, &status))) { RETURNFUNC(rc); }

            *ptt = status ? RIG_PTT_ON : RIG_PTT_OFF;
        }
        else
        {
            *ptt = RIG_PTT_OFF;
        }

        break;

    case RIG_PTT_PARALLEL:
        if (rig->state.pttport.fd >= 0)
        {
            if (RIG_OK != (rc = par_ptt_get(&rig->state.pttport, ptt))) { RETURNFUNC(rc); }
        }

        break;

    case RIG_PTT_CM108:
        if (rig->state.pttport.fd >= 0)
        {
            if (RIG_OK != (rc = cm108_ptt_get(&rig->state.pttport, ptt))) { RETURNFUNC(rc); }
        }

        break;

    case RIG_PTT_GPIO:
    case RIG_PTT_GPION:
        if (rig->state.pttport.fd >= 0)
        {
            if (RIG_OK != (rc = gpio_ptt_get(&rig->state.pttport, ptt))) { RETURNFUNC(rc); }
        }

        break;

    default:
        *ptt = priv->ptt;
        break;
    }

    RETURNFUNC(RIG_OK);
}


static int dummy_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    static int twiddle = 0;

    ENTERFUNC;
    usleep(CMDSLEEP);
    *dcd = (twiddle++ & 1) ? RIG_DCD_ON : RIG_DCD_OFF;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    curr->rptr_shift = rptr_shift;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    *rptr_shift = curr->rptr_shift;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    curr->rptr_offs = rptr_offs;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *rptr_offs = curr->rptr_offs;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    curr->ctcss_tone = tone;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    *tone = curr->ctcss_tone;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    curr->dcs_code = code;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    *code = curr->dcs_code;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    usleep(CMDSLEEP);
    curr->ctcss_sql = tone;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *tone = curr->ctcss_sql;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    curr->dcs_sql = code;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *code = curr->dcs_sql;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    int retval;

    ENTERFUNC;

    retval = dummy_set_freq(rig, vfo, tx_freq);
    priv->curr->tx_freq = tx_freq;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: priv->curr->tx_freq = %.0f\n", __func__,
              priv->curr->tx_freq);

    RETURNFUNC(retval);
}


static int dummy_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;

    *tx_freq = priv->curr->tx_freq;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: priv->curr->tx_freq = %.0f\n", __func__,
              priv->curr->tx_freq);

    RETURNFUNC(RIG_OK);
}

static int dummy_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                pbwidth_t tx_width)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;

    curr->tx_mode = tx_mode;

    if (RIG_PASSBAND_NOCHANGE == tx_width) { RETURNFUNC(RIG_OK); }

    curr->tx_width = tx_width;

    RETURNFUNC(RIG_OK);
}

static int dummy_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                pbwidth_t *tx_width)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;

    *tx_mode = curr->tx_mode;
    *tx_width = curr->tx_width;

    RETURNFUNC(RIG_OK);
}

static int dummy_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: split=%d, vfo=%s, tx_vfo=%s\n",
              __func__, split, rig_strvfo(vfo), rig_strvfo(tx_vfo));
    curr->split = split;
    priv->tx_vfo = tx_vfo;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *split = curr->split;

    RETURNFUNC(RIG_OK);
}

static int dummy_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    curr->rit = rit;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *rit = curr->rit;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    curr->xit = xit;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *xit = curr->xit;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    curr->tuning_step = ts;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *ts = curr->tuning_step;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %d\n", __func__,
              rig_strfunc(func), status);

    if (status)
    {
        curr->funcs |=  func;
    }
    else
    {
        curr->funcs &= ~func;
    }

    RETURNFUNC(RIG_OK);
}


static int dummy_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *status = (curr->funcs & func) ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              rig_strfunc(func));

    RETURNFUNC(RIG_OK);
}


static int dummy_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;
    int idx;
    char lstr[32];

    ENTERFUNC;
    idx = rig_setting2idx(level);

    if (idx >= RIG_SETTING_MAX)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    curr->levels[idx] = val;

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              rig_strlevel(level), lstr);

    RETURNFUNC(RIG_OK);
}


static int dummy_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;
    int idx;

    ENTERFUNC;
    idx = rig_setting2idx(level);

    if (idx >= RIG_SETTING_MAX)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
    case RIG_LEVEL_RAWSTR:
        if (priv->static_data)
        {
            curr->levels[idx].i = -12;
        }
        else
        {
            uint64_t level1, level2;
            /* make S-Meter jiggle */
            int qrm = -56;

            if (curr->freq < MHz(7))
            {
                qrm = -20;
            }
            else if (curr->freq < MHz(21))
            {
                qrm = -30;
            }
            else if (curr->freq < MHz(50))
            {
                qrm = -50;
            }

            // cppcheck-suppress *
            level1 = LVL_ATT;
            level2 = LVL_PREAMP;
            curr->levels[idx].i = qrm + (time(NULL) % 32) + (rand() % 4)
                                  - curr->levels[level1].i + curr->levels[level2].i;
        }

        break;

    case RIG_LEVEL_RFPOWER_METER:
        if (priv->static_data)
        {
            curr->levels[idx].f = 0.5f;
        }
        else
        {
            curr->levels[idx].f = (float)(time(NULL) % 32) / 64.0f + (float)(
                                      rand() % 4) / 8.0f;
        }

        break;

    case RIG_LEVEL_RFPOWER_METER_WATTS:
        if (priv->static_data)
        {
            curr->levels[idx].f = 50.0f;
        }
        else
        {
            curr->levels[idx].f = (float)(time(NULL) % 32) / 64.0f + (float)(
                                      rand() % 4) / 8.0f;
            curr->levels[idx].f *= 100.0f;
        }

        break;

    case RIG_LEVEL_COMP_METER:
        if (priv->static_data)
        {
            curr->levels[idx].f = 3.5f;
        }
        else
        {
            curr->levels[idx].f = 0.5f + (float)(time(NULL) % 32) / 16.0f + (float)(
                                      rand() % 200) / 20.0f;
        }

        break;

    case RIG_LEVEL_VD_METER:
        if (priv->static_data)
        {
            curr->levels[idx].f = 13.82f;
        }
        else
        {
            curr->levels[idx].f = 13.82f + (float)(time(NULL) % 10) / 50.0f - (float)(
                                      rand() % 10) / 40.0f;
        }

        break;

    case RIG_LEVEL_ID_METER:
        if (priv->static_data)
        {
            curr->levels[idx].f = 0.85f;
        }
        else
        {
            curr->levels[idx].f = 2.0f + (float)(time(NULL) % 320) / 16.0f - (float)(
                                      rand() % 40) / 20.0f;
        }

        break;
    }

    memcpy(val, &curr->levels[idx], sizeof(value_t));
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              rig_strlevel(level));

    RETURNFUNC(RIG_OK);
}

static int dummy_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;
    char lstr[64];
    const struct confparams *cfp;
    struct ext_list *elp;

    ENTERFUNC;
    cfp = rig_ext_lookup_tok(rig, token);

    if (!cfp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_EL_MAGICLEVEL:
    case TOK_EL_MAGICFUNC:
    case TOK_EL_MAGICOP:
    case TOK_EL_MAGICCOMBO:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (cfp->type)
    {
    case RIG_CONF_STRING:
        strcpy(lstr, val.s);
        break;

    case RIG_CONF_COMBO:
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
        break;

    case RIG_CONF_NUMERIC:
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
        break;

    case RIG_CONF_CHECKBUTTON:
        SNPRINTF(lstr, sizeof(lstr), "%s", val.i ? "ON" : "OFF");
        break;

    case RIG_CONF_BUTTON:
        lstr[0] = '\0';
        break;

    default:
        RETURNFUNC(-RIG_EINTERNAL);
    }

    elp = find_ext(curr->ext_levels, token);

    if (!elp)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    /* store value */
    elp->val = val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              cfp->name, lstr);

    RETURNFUNC(RIG_OK);
}

static int dummy_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;
    const struct confparams *cfp;
    struct ext_list *elp;

    ENTERFUNC;
    cfp = rig_ext_lookup_tok(rig, token);

    if (!cfp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_EL_MAGICLEVEL:
    case TOK_EL_MAGICFUNC:
    case TOK_EL_MAGICOP:
    case TOK_EL_MAGICCOMBO:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    elp = find_ext(curr->ext_levels, token);

    if (!elp)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    /* load value */
    *val = elp->val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              cfp->name);

    RETURNFUNC(RIG_OK);
}


static int dummy_set_ext_func(RIG *rig, vfo_t vfo, token_t token, int status)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    const struct confparams *cfp;
    struct ext_list *elp;

    ENTERFUNC;
    cfp = rig_ext_lookup_tok(rig, token);

    if (!cfp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_EL_MAGICEXTFUNC:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (cfp->type)
    {
    case RIG_CONF_CHECKBUTTON:
        break;

    case RIG_CONF_BUTTON:
        break;

    default:
        RETURNFUNC(-RIG_EINTERNAL);
    }

    elp = find_ext(priv->ext_funcs, token);

    if (!elp)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    /* store value */
    elp->val.i = status;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %d\n", __func__,
              cfp->name, status);

    RETURNFUNC(RIG_OK);
}


static int dummy_get_ext_func(RIG *rig, vfo_t vfo, token_t token, int *status)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    const struct confparams *cfp;
    struct ext_list *elp;

    ENTERFUNC;
    cfp = rig_ext_lookup_tok(rig, token);

    if (!cfp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_EL_MAGICEXTFUNC:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    elp = find_ext(priv->ext_funcs, token);

    if (!elp)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    /* load value */
    *status = elp->val.i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              cfp->name);

    RETURNFUNC(RIG_OK);
}


static int dummy_set_powerstat(RIG *rig, powerstat_t status)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;
    priv->powerstat = status;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_powerstat(RIG *rig, powerstat_t *status)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;
    *status = priv->powerstat;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_parm(RIG *rig, setting_t parm, value_t val)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    int idx;
    char pstr[32];

    ENTERFUNC;
    idx = rig_setting2idx(parm);

    if (idx >= RIG_SETTING_MAX)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_PARM_IS_FLOAT(parm))
    {
        SNPRINTF(pstr, sizeof(pstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(pstr, sizeof(pstr), "%d", val.i);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              rig_strparm(parm), pstr);
    priv->parms[idx] = val;

    RETURNFUNC(RIG_OK);
}


static int dummy_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    int idx;

    ENTERFUNC;
    idx = rig_setting2idx(parm);

    if (idx >= RIG_SETTING_MAX)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    *val = priv->parms[idx];
    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s\n", __func__,
              rig_strparm(parm));

    RETURNFUNC(RIG_OK);
}

static int dummy_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    char lstr[64];
    const struct confparams *cfp;
    struct ext_list *epp;

    ENTERFUNC;
    cfp = rig_ext_lookup_tok(rig, token);

    if (!cfp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_EP_MAGICPARM:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (cfp->type)
    {
    case RIG_CONF_STRING:
        strcpy(lstr, val.s);
        break;

    case RIG_CONF_COMBO:
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
        break;

    case RIG_CONF_NUMERIC:
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
        break;

    case RIG_CONF_CHECKBUTTON:
        SNPRINTF(lstr, sizeof(lstr), "%s", val.i ? "ON" : "OFF");
        break;

    case RIG_CONF_BUTTON:
        lstr[0] = '\0';
        break;

    default:
        RETURNFUNC(-RIG_EINTERNAL);
    }

    epp = find_ext(priv->ext_parms, token);

    if (!epp)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    /* store value */
    epp->val = val;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              cfp->name, lstr);

    RETURNFUNC(RIG_OK);
}

static int dummy_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    const struct confparams *cfp;
    struct ext_list *epp;

    ENTERFUNC;
    /* TODO: load value from priv->ext_parms */

    cfp = rig_ext_lookup_tok(rig, token);

    if (!cfp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_EP_MAGICPARM:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    epp = find_ext(priv->ext_parms, token);

    if (!epp)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    /* load value */
    *val = epp->val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              cfp->name);

    RETURNFUNC(RIG_OK);
}



static int dummy_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;
    ENTERFUNC;

    switch (ant)
    {
    case RIG_ANT_CURR:
        break;

    case RIG_ANT_1:
    case RIG_ANT_2:
    case RIG_ANT_3:
    case RIG_ANT_4:
        curr->ant = ant;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown antenna requested=0x%02x\n", __func__,
                  ant);
        RETURNFUNC(-RIG_EINVAL);
    }

    priv->ant_option[rig_setting2idx(curr->ant)] = option.i;
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s called ant=0x%02x, option=%d, curr->ant=0x%02x\n", __func__, ant, option.i,
              curr->ant);

    RETURNFUNC(RIG_OK);
}


static int dummy_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                         ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;
    ENTERFUNC;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, ant=0x%02x\n", __func__, ant);

    switch (ant)
    {
    case RIG_ANT_CURR:
        *ant_curr = curr->ant;
        break;

    case RIG_ANT_1:
    case RIG_ANT_2:
    case RIG_ANT_3:
    case RIG_ANT_4:
        *ant_curr = ant;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown antenna requested=0x%02x\n", __func__,
                  ant);
        RETURNFUNC(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: ant_curr=0x%02x, idx=%d\n", __func__, *ant_curr,
              rig_setting2idx(*ant_curr));
    option->i = priv->ant_option[rig_setting2idx(*ant_curr)];

    RETURNFUNC(RIG_OK);
}


static int dummy_set_bank(RIG *rig, vfo_t vfo, int bank)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;
    priv->bank = bank;

    RETURNFUNC(RIG_OK);
}


static int dummy_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;

    if (ch < 0 || ch >= NB_CHAN)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (priv->curr_vfo == RIG_VFO_MEM)
    {
        priv->curr = &priv->mem[ch];
    }
    else
    {
        priv->curr->channel_num = ch;
    }

    RETURNFUNC(RIG_OK);
}


static int dummy_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;

    ENTERFUNC;
    *ch = curr->channel_num;

    RETURNFUNC(RIG_OK);
}

static int dummy_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %d\n", __func__,
              rig_strscan(scan), ch);
    /* TODO: change freq, etc. */
    RETURNFUNC(RIG_OK);
}

static void chan_vfo(channel_t *chan, vfo_t vfo)
{
    chan->vfo = vfo;
    strcpy(chan->channel_desc, rig_strvfo(vfo));
}

static int dummy_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
    channel_t *curr = priv->curr;
    int ret;
    freq_t freq;
    shortfreq_t ts;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              rig_strvfop(op));

    switch (op)
    {
    case RIG_OP_FROM_VFO: /* VFO->MEM */
        if (priv->curr_vfo == RIG_VFO_MEM)
        {
            int ch = curr->channel_num;
            copy_chan(curr, priv->last_vfo == RIG_VFO_A ?
                      &priv->vfo_a : &priv->vfo_b);
            curr->channel_num = ch;
            curr->channel_desc[0] = '\0';
            curr->vfo = RIG_VFO_MEM;
        }
        else
        {
            channel_t *mem_chan = &priv->mem[curr->channel_num];
            copy_chan(mem_chan, curr);
            mem_chan->channel_num = curr->channel_num;
            mem_chan->channel_desc[0] = '\0';
            mem_chan->vfo = RIG_VFO_MEM;
        }

        break;

    case RIG_OP_TO_VFO:         /* MEM->VFO */
        if (priv->curr_vfo == RIG_VFO_MEM)
        {
            channel_t *vfo_chan = (priv->last_vfo == RIG_VFO_A) ?
                                  &priv->vfo_a : &priv->vfo_b;
            copy_chan(vfo_chan, curr);
            chan_vfo(vfo_chan, priv->last_vfo);
        }
        else
        {
            copy_chan(&priv->mem[curr->channel_num], curr);
            chan_vfo(curr, priv->curr_vfo);
        }

        break;

    case RIG_OP_CPY:   /* VFO A = VFO B   or   VFO B = VFO A */
        if (priv->curr_vfo == RIG_VFO_A)
        {
            copy_chan(&priv->vfo_b, &priv->vfo_a);
            chan_vfo(&priv->vfo_b, RIG_VFO_B);
            break;
        }
        else if (priv->curr_vfo == RIG_VFO_B)
        {
            copy_chan(&priv->vfo_a, &priv->vfo_b);
            chan_vfo(&priv->vfo_a, RIG_VFO_A);
            break;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s beep!\n", __func__);
        break;

    case RIG_OP_XCHG: /* Exchange VFO A/B */
    {
        channel_t chan;
        chan.ext_levels = alloc_init_ext(dummy_ext_levels);

        if (!chan.ext_levels)
        {
            RETURNFUNC(-RIG_ENOMEM);
        }

        copy_chan(&chan, &priv->vfo_b);
        copy_chan(&priv->vfo_b, &priv->vfo_a);
        copy_chan(&priv->vfo_a, &chan);
        chan_vfo(&priv->vfo_a, RIG_VFO_A);
        chan_vfo(&priv->vfo_b, RIG_VFO_B);
        free(chan.ext_levels);
        break;
    }

    case RIG_OP_MCL:  /* Memory clear */
        if (priv->curr_vfo == RIG_VFO_MEM)
        {
            struct ext_list *saved_ext_levels = curr->ext_levels;
            int saved_ch = curr->channel_num;
            int i;

            for (i = 0; !RIG_IS_EXT_END(curr->ext_levels[i]); i++)
            {
                curr->ext_levels[i].val.i = 0;
            }

            memset(curr, 0, sizeof(channel_t));
            curr->ext_levels = saved_ext_levels;
            curr->channel_num = saved_ch;
            curr->vfo = RIG_VFO_MEM;
        }
        else
        {
            struct ext_list *saved_ext_levels = curr->ext_levels;
            channel_t *mem_chan = &priv->mem[curr->channel_num];
            int i;

            for (i = 0; !RIG_IS_EXT_END(mem_chan->ext_levels[i]); i++)
            {
                mem_chan->ext_levels[i].val.i = 0;
            }

            memset(mem_chan, 0, sizeof(channel_t));
            mem_chan->ext_levels = saved_ext_levels;
            mem_chan->channel_num = curr->channel_num;
            mem_chan->vfo = RIG_VFO_MEM;
        }

        break;

    case RIG_OP_TOGGLE:
        if (priv->curr_vfo == RIG_VFO_A)
        {
            RETURNFUNC(dummy_set_vfo(rig, RIG_VFO_B));
        }
        else if (priv->curr_vfo == RIG_VFO_B)
        {
            RETURNFUNC(dummy_set_vfo(rig, RIG_VFO_A));
        }
        else
        {
            RETURNFUNC(-RIG_EVFO);
        }

    case RIG_OP_RIGHT:
    case RIG_OP_LEFT:
    case RIG_OP_TUNE:
        /* NOP */
        break;

    case RIG_OP_BAND_UP:
    case RIG_OP_BAND_DOWN:
        RETURNFUNC(-RIG_ENIMPL);

    case RIG_OP_UP:
        ret = dummy_get_freq(rig, vfo, &freq);

        if (!ret) { break; }

        ret = dummy_get_ts(rig, vfo, &ts);

        if (!ret) { break; }

        dummy_set_freq(rig, vfo, freq + ts);  /* up */
        break;

    case RIG_OP_DOWN:
        ret = dummy_get_freq(rig, vfo, &freq);

        if (!ret) { break; }

        ret = dummy_get_ts(rig, vfo, &ts);

        if (!ret) { break; }

        dummy_set_freq(rig, vfo, freq - ts);  /* down */
        break;

    default:
        break;
    }

    RETURNFUNC(RIG_OK);
}

static int dummy_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;

    if (!chan->ext_levels)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (chan->channel_num < 0 || chan->channel_num >= NB_CHAN)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* TODO:
     * - check ext_levels is the right length
     */
    switch (chan->vfo)
    {
    case RIG_VFO_MEM:
        copy_chan(&priv->mem[chan->channel_num], chan);
        break;

    case RIG_VFO_A:
        copy_chan(&priv->vfo_a, chan);
        break;

    case RIG_VFO_B:
        copy_chan(&priv->vfo_b, chan);
        break;

    case RIG_VFO_CURR:
        copy_chan(priv->curr, chan);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}


static int dummy_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                             int read_only)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    ENTERFUNC;

    if (chan->channel_num < 0 || chan->channel_num >= NB_CHAN)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!chan->ext_levels)
    {
        chan->ext_levels = alloc_init_ext(dummy_ext_levels);

        if (!chan->ext_levels)
        {
            RETURNFUNC(-RIG_ENOMEM);
        }
    }

    /* TODO:
     * - check ext_levels is the right length
     */
    switch (chan->vfo)
    {
    case RIG_VFO_MEM:
        copy_chan(chan, &priv->mem[chan->channel_num]);
        break;

    case RIG_VFO_A:
        copy_chan(chan, &priv->vfo_a);
        break;

    case RIG_VFO_B:
        copy_chan(chan, &priv->vfo_b);
        break;

    case RIG_VFO_CURR:
        copy_chan(chan, priv->curr);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}


static int dummy_set_trn(RIG *rig, int trn)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    priv->trn = trn;

    RETURNFUNC2(RIG_OK);
}


static int dummy_get_trn(RIG *rig, int *trn)
{
    struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

    *trn = priv->trn;

    RETURNFUNC2(RIG_OK);
}

static const char *dummy_get_info(RIG *rig)
{
    return "Nothing much (dummy)";
}


static int dummy_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__, digits);

    RETURNFUNC(RIG_OK);
}

static int dummy_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
    ENTERFUNC;
    strcpy(digits, "0123456789ABCDEF");
    *length = 16;

    RETURNFUNC(RIG_OK);
}

static int dummy_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    ENTERFUNC;

    RETURNFUNC(RIG_OK);
}

static int dummy_send_voice_mem(RIG *rig, vfo_t vfo, int ch)
{
    ENTERFUNC;

    RETURNFUNC(RIG_OK);
}

static int dummy_power2mW(RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode)
{
    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: passed power = %f\n", __func__, power);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));

    /* Pretend this is a 100W radio */
    *mwpower = (power * 100000);

    RETURNFUNC(RIG_OK);
}


static int dummy_mW2power(RIG *rig, float *power, unsigned int mwpower,
                          freq_t freq, rmode_t mode)
{
    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mwpower = %u\n", __func__, mwpower);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));

    /* Pretend this is a 100W radio */
    if (mwpower > 100000)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    *power = ((float)mwpower / 100000);

    RETURNFUNC(RIG_OK);
}

static int m_year, m_month, m_day, m_hour, m_min, m_sec, m_utc_offset;
static double m_msec;

int dummy_set_clock(RIG *rig, int year, int month, int day, int hour, int min,
                    int sec, double msec, int utc_offset)
{
    int retval = RIG_OK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %04d-%02d-%02dT%02d:%02d:%02d.%.03f%s%02d\n",
              __func__, year,
              month, day, hour, min, sec, msec, utc_offset >= 0 ? "+" : "-",
              (unsigned)(abs(utc_offset)));
    m_year = year;
    m_month = month;
    m_day = day;

    if (hour >= 0)
    {
        m_hour = hour;
        m_min = min;
        m_sec = sec;
        m_msec = msec;
        m_utc_offset = utc_offset;
    }

    return retval;
}

int dummy_get_clock(RIG *rig, int *year, int *month, int *day, int *hour,
                    int *min, int *sec, double *msec, int *utc_offset)
{
    int retval = RIG_OK;

    *year = m_year;
    *month = m_month;
    *day = m_day;
    *hour = m_hour;
    *min = m_min;
    *sec = m_sec;
    *msec = m_msec;
    *utc_offset = m_utc_offset;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: %02d-%02d-%02dT%02d:%02d:%02d:%0.3lf%s%02d\n'",
              __func__, *year, *month, *day, *hour, *min, *sec, *msec,
              *utc_offset >= 0 ? "+" : "-", (unsigned)abs(*utc_offset));
    return retval;
}


/*
 * Dummy rig capabilities.
 */

/*
 * The following macros set bitmasks for the various funcs, levels, parms,
 * etc.  This dummy backend claims support for almost all of them.
 */
#define DUMMY_FUNC  ((setting_t)-1ULL) /* All possible functions */
#define DUMMY_LEVEL (((setting_t)-1ULL)&~(1ULL<<27)) /* All levels except SQLSTAT */
#define DUMMY_PARM  ((setting_t)-1ULL) /* All possible parms */

#define DUMMY_VFO_OP  0x7ffffffUL /* All possible VFO OPs */
#define DUMMY_SCAN    0x7ffffffUL /* All possible scan OPs */

#define DUMMY_VFOS (RIG_VFO_TX|RIG_VFO_TX|RIG_VFO_A|RIG_VFO_B|RIG_VFO_C|RIG_VFO_MEM|RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MAIN_A|RIG_VFO_MAIN_B|RIG_VFO_SUB_A|RIG_VFO_SUB_B)

#define DUMMY_MODES (RIG_MODE_AM | RIG_MODE_CW | RIG_MODE_RTTY | \
                     RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_WFM | \
                     RIG_MODE_CWR | RIG_MODE_RTTYR)

#define DUMMY_MEM_CAP {    \
    .bank_num = 1,  \
    .vfo = 1,   \
    .ant = 1,   \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .tx_freq = 1,   \
    .tx_mode = 1,   \
    .tx_width = 1,  \
    .split = 1, \
    .rptr_shift = 1,    \
    .rptr_offs = 1, \
    .tuning_step = 1,   \
    .rit = 1,   \
    .xit = 1,   \
    .funcs = DUMMY_FUNC,    \
    .levels = RIG_LEVEL_SET(DUMMY_LEVEL),   \
    .ctcss_tone = 1,    \
    .ctcss_sql = 1, \
    .dcs_code = 1,  \
    .dcs_sql = 1,   \
    .scan_group = 1,    \
    .flags = 1, \
    .channel_desc = 1,  \
    .ext_levels = 1,    \
    }

struct rig_caps dummy_caps =
{
    RIG_MODEL(RIG_MODEL_DUMMY),
    .model_name =     "Dummy",
    .mfg_name =       "Hamlib",
    .version =        "20221128.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_OTHER,
    .targetable_vfo = RIG_TARGETABLE_PTT | RIG_TARGETABLE_RITXIT | RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE | RIG_TARGETABLE_SPECTRUM,
    .ptt_type =       RIG_PTT_NONE,
    .dcd_type =       RIG_DCD_RIG,
    .port_type =      RIG_PORT_NONE,
    .has_get_func =   DUMMY_FUNC,
    .has_set_func =   DUMMY_FUNC,
    .has_get_level =  DUMMY_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(DUMMY_LEVEL),
    .has_get_parm =    DUMMY_PARM,
    .has_set_parm =    RIG_PARM_SET(DUMMY_PARM),
    .level_gran =      {
        [LVL_CWPITCH] = { .step = { .i = 10 } },
        [LVL_SPECTRUM_SPEED] = {.min = {.i = 0}, .max = {.i = 2}, .step = {.i = 1}},
        [LVL_SPECTRUM_REF] = {.min = {.f = -30.0f}, .max = {.f = 10.0f}, .step = {.f = 0.5f}},
        [LVL_SPECTRUM_AVG] = {.min = {.i = 0}, .max = {.i = 3}, .step = {.i = 1}},
    },
    .ctcss_list =      common_ctcss_list,
    .dcs_list =        full_dcs_list,
    .chan_list =   {
        {   0,  18, RIG_MTYPE_MEM, DUMMY_MEM_CAP },
        {  19,  19, RIG_MTYPE_CALL },
        {  20,  NB_CHAN - 1, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },
    .scan_ops =    DUMMY_SCAN,
    .vfo_ops =     DUMMY_VFO_OP,
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { 10, 20, 30, RIG_DBLST_END, },
    .preamp =          { 10, RIG_DBLST_END, },
    .agc_level_count = 7,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SUPERFAST, RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW, RIG_AGC_AUTO, RIG_AGC_USER },
    .rx_range_list1 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DUMMY_MODES,
            .low_power = -1, .high_power = -1, DUMMY_VFOS, RIG_ANT_1 | RIG_ANT_2 | RIG_ANT_3 | RIG_ANT_4,
            .label = "Dummy#1"
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DUMMY_MODES,
            .low_power = W(5), .high_power = W(100), DUMMY_VFOS, RIG_ANT_1 | RIG_ANT_2 | RIG_ANT_3 | RIG_ANT_4,
            .label = "Dummy#1"
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DUMMY_MODES,
            .low_power = -1, .high_power = -1, DUMMY_VFOS, RIG_ANT_1 | RIG_ANT_2 | RIG_ANT_3 | RIG_ANT_4,
            .label = "Dummy#2"
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  { {DUMMY_MODES, 1}, {DUMMY_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(3.0)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_CW, kHz(2.4)},
        {RIG_MODE_CW, Hz(50)},
        {RIG_MODE_CW, RIG_FLT_ANY},
        {RIG_MODE_RTTY, Hz(300)},
        {RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_RTTY, Hz(50)},
        {RIG_MODE_RTTY, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(8)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_AM, kHz(10)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(8)},
        {RIG_MODE_WFM, kHz(230)},
        RIG_FLT_END,
    },
    .max_rit = 9990,
    .max_xit = 9990,
    .max_ifshift = 10000,

    .spectrum_scopes = {
        {
            .id = 0,
            .name = "Main",
        },
        {
            .id = 1,
            .name = "Sub",
        },
        {
            .id = -1,
            .name = NULL,
        },
    },
    .spectrum_modes = {
        RIG_SPECTRUM_MODE_CENTER,
        RIG_SPECTRUM_MODE_FIXED,
        RIG_SPECTRUM_MODE_CENTER_SCROLL,
        RIG_SPECTRUM_MODE_FIXED_SCROLL,
        RIG_SPECTRUM_MODE_NONE,
    },
    .spectrum_spans = {
        5000,
        10000,
        20000,
        50000,
        100000,
        200000,
        500000,
        1000000,
        2000000,
        5000000,
        0,
    },
    .spectrum_avg_modes = {
        {
            .id = 0,
            .name = "OFF",
        },
        {
            .id = 1,
            .name = "2",
        },
        {
            .id = 2,
            .name = "3",
        },
        {
            .id = 3,
            .name = "4",
        },
    },
    .spectrum_attenuator = { 10, 20, 30, RIG_DBLST_END, },

    .priv =  NULL,    /* priv */

    .extlevels =    dummy_ext_levels,
    .extfuncs =     dummy_ext_funcs,
    .extparms =     dummy_ext_parms,
    .cfgparams =    dummy_cfg_params,

    .rig_init =     dummy_init,
    .rig_cleanup =  dummy_cleanup,
    .rig_open =     dummy_open,
    .rig_close =    dummy_close,

    .set_conf =     dummy_set_conf,
    .get_conf =     dummy_get_conf,

    .set_freq =     dummy_set_freq,
    .get_freq =     dummy_get_freq,
    .set_mode =     dummy_set_mode,
    .get_mode =     dummy_get_mode,
    .set_vfo =      dummy_set_vfo,
    .get_vfo =      dummy_get_vfo,

    .set_powerstat =  dummy_set_powerstat,
    .get_powerstat =  dummy_get_powerstat,
    .set_level =     dummy_set_level,
    .get_level =     dummy_get_level,
    .set_func =      dummy_set_func,
    .get_func =      dummy_get_func,
    .set_parm =      dummy_set_parm,
    .get_parm =      dummy_get_parm,
    .set_ext_level = dummy_set_ext_level,
    .get_ext_level = dummy_get_ext_level,
    .set_ext_func =  dummy_set_ext_func,
    .get_ext_func =  dummy_get_ext_func,
    .set_ext_parm =  dummy_set_ext_parm,
    .get_ext_parm =  dummy_get_ext_parm,

    .get_info =      dummy_get_info,


    .set_ptt =    dummy_set_ptt,
    .get_ptt =    dummy_get_ptt,
    .get_dcd =    dummy_get_dcd,
    .set_rptr_shift =     dummy_set_rptr_shift,
    .get_rptr_shift =     dummy_get_rptr_shift,
    .set_rptr_offs =  dummy_set_rptr_offs,
    .get_rptr_offs =  dummy_get_rptr_offs,
    .set_ctcss_tone =     dummy_set_ctcss_tone,
    .get_ctcss_tone =     dummy_get_ctcss_tone,
    .set_dcs_code =   dummy_set_dcs_code,
    .get_dcs_code =   dummy_get_dcs_code,
    .set_ctcss_sql =  dummy_set_ctcss_sql,
    .get_ctcss_sql =  dummy_get_ctcss_sql,
    .set_dcs_sql =    dummy_set_dcs_sql,
    .get_dcs_sql =    dummy_get_dcs_sql,
    .set_split_freq =     dummy_set_split_freq,
    .get_split_freq =     dummy_get_split_freq,
    .set_split_mode =     dummy_set_split_mode,
    .get_split_mode =     dummy_get_split_mode,
    .set_split_vfo =  dummy_set_split_vfo,
    .get_split_vfo =  dummy_get_split_vfo,
    .set_rit =    dummy_set_rit,
    .get_rit =    dummy_get_rit,
    .set_xit =    dummy_set_xit,
    .get_xit =    dummy_get_xit,
    .set_ts =     dummy_set_ts,
    .get_ts =     dummy_get_ts,
    .set_ant =    dummy_set_ant,
    .get_ant =    dummy_get_ant,
    .set_bank =   dummy_set_bank,
    .set_mem =    dummy_set_mem,
    .get_mem =    dummy_get_mem,
    .vfo_op =     dummy_vfo_op,
    .scan =       dummy_scan,
    .send_dtmf =  dummy_send_dtmf,
    .recv_dtmf =  dummy_recv_dtmf,
    .send_morse =  dummy_send_morse,
    .send_voice_mem =  dummy_send_voice_mem,
    .set_channel =    dummy_set_channel,
    .get_channel =    dummy_get_channel,
    .set_trn =    dummy_set_trn,
    .get_trn =    dummy_get_trn,
    .power2mW =   dummy_power2mW,
    .mW2power =   dummy_mW2power,
    .set_clock = dummy_set_clock,
    .get_clock = dummy_get_clock,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

struct rig_caps dummy_no_vfo_caps =
{
    RIG_MODEL(RIG_MODEL_DUMMY_NOVFO),
    .model_name =     "Dummy No VFO",
    .mfg_name =       "Hamlib",
    .version =        "20220510.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_OTHER,
    .targetable_vfo =      RIG_TARGETABLE_PTT | RIG_TARGETABLE_RITXIT | RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_RIG,
    .port_type =      RIG_PORT_NONE,
    .has_get_func =   DUMMY_FUNC,
    .has_set_func =   DUMMY_FUNC,
    .has_get_level =  DUMMY_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(DUMMY_LEVEL),
    .has_get_parm =    DUMMY_PARM,
    .has_set_parm =    RIG_PARM_SET(DUMMY_PARM),
    .level_gran =      { [LVL_CWPITCH] = { .step = { .i = 10 } } },
    .ctcss_list =      common_ctcss_list,
    .dcs_list =        full_dcs_list,
    .chan_list =   {
        {   0,  18, RIG_MTYPE_MEM, DUMMY_MEM_CAP },
        {  19,  19, RIG_MTYPE_CALL },
        {  20,  NB_CHAN - 1, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },
    .scan_ops =    DUMMY_SCAN,
    .vfo_ops =     DUMMY_VFO_OP,
    .transceive =     RIG_TRN_RIG,
    .attenuator =     { 10, 20, 30, RIG_DBLST_END, },
    .preamp =          { 10, RIG_DBLST_END, },
    .rx_range_list1 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DUMMY_MODES,
            .low_power = -1, .high_power = -1, DUMMY_VFOS, RIG_ANT_1 | RIG_ANT_2 | RIG_ANT_3 | RIG_ANT_4,
            .label = "Dummy#1"
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DUMMY_MODES,
            .low_power = W(5), .high_power = W(100), DUMMY_VFOS, RIG_ANT_1 | RIG_ANT_2 | RIG_ANT_3 | RIG_ANT_4,
            .label = "Dummy#1"
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DUMMY_MODES,
            .low_power = -1, .high_power = -1, DUMMY_VFOS, RIG_ANT_1 | RIG_ANT_2 | RIG_ANT_3 | RIG_ANT_4,
            .label = "Dummy#2"
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  { {DUMMY_MODES, 1}, {DUMMY_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(3.0)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_CW, kHz(2.4)},
        {RIG_MODE_CW, Hz(50)},
        {RIG_MODE_CW, RIG_FLT_ANY},
        {RIG_MODE_RTTY, Hz(300)},
        {RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_RTTY, Hz(50)},
        {RIG_MODE_RTTY, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(8)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_AM, kHz(10)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(8)},
        {RIG_MODE_WFM, kHz(230)},
        RIG_FLT_END,
    },
    .max_rit = 9990,
    .max_xit = 9990,
    .max_ifshift = 10000,
    .priv =  NULL,    /* priv */

    .extlevels =    dummy_ext_levels,
    .extfuncs =     dummy_ext_funcs,
    .extparms =     dummy_ext_parms,
    .cfgparams =    dummy_cfg_params,

    .rig_init =     dummy_init,
    .rig_cleanup =  dummy_cleanup,
    .rig_open =     dummy_open,
    .rig_close =    dummy_close,

    .set_conf =     dummy_set_conf,
    .get_conf =     dummy_get_conf,

    .set_freq =     dummy_set_freq,
    .get_freq =     dummy_get_freq,
    .set_mode =     dummy_set_mode,
    .get_mode =     dummy_get_mode,
    .set_vfo =      dummy_set_vfo,
    .get_vfo =      dummy_get_vfo,

    .set_powerstat =  dummy_set_powerstat,
    .get_powerstat =  dummy_get_powerstat,
    .set_level =     dummy_set_level,
    .get_level =     dummy_get_level,
    .set_func =      dummy_set_func,
    .get_func =      dummy_get_func,
    .set_parm =      dummy_set_parm,
    .get_parm =      dummy_get_parm,
    .set_ext_level = dummy_set_ext_level,
    .get_ext_level = dummy_get_ext_level,
    .set_ext_func =  dummy_set_ext_func,
    .get_ext_func =  dummy_get_ext_func,
    .set_ext_parm =  dummy_set_ext_parm,
    .get_ext_parm =  dummy_get_ext_parm,

    .get_info =      dummy_get_info,


    .set_ptt =    dummy_set_ptt,
    .get_ptt =    dummy_get_ptt,
    .get_dcd =    dummy_get_dcd,
    .set_rptr_shift =     dummy_set_rptr_shift,
    .get_rptr_shift =     dummy_get_rptr_shift,
    .set_rptr_offs =  dummy_set_rptr_offs,
    .get_rptr_offs =  dummy_get_rptr_offs,
    .set_ctcss_tone =     dummy_set_ctcss_tone,
    .get_ctcss_tone =     dummy_get_ctcss_tone,
    .set_dcs_code =   dummy_set_dcs_code,
    .get_dcs_code =   dummy_get_dcs_code,
    .set_ctcss_sql =  dummy_set_ctcss_sql,
    .get_ctcss_sql =  dummy_get_ctcss_sql,
    .set_dcs_sql =    dummy_set_dcs_sql,
    .get_dcs_sql =    dummy_get_dcs_sql,
    .set_split_freq =     dummy_set_split_freq,
    .get_split_freq =     dummy_get_split_freq,
    .set_split_mode =     dummy_set_split_mode,
    .get_split_mode =     dummy_get_split_mode,
    .set_split_vfo =  dummy_set_split_vfo,
    .get_split_vfo =  dummy_get_split_vfo,
    .set_rit =    dummy_set_rit,
    .get_rit =    dummy_get_rit,
    .set_xit =    dummy_set_xit,
    .get_xit =    dummy_get_xit,
    .set_ts =     dummy_set_ts,
    .get_ts =     dummy_get_ts,
    .set_ant =    dummy_set_ant,
    .get_ant =    dummy_get_ant,
    .set_bank =   dummy_set_bank,
    .set_mem =    dummy_set_mem,
    .get_mem =    dummy_get_mem,
    .vfo_op =     dummy_vfo_op,
    .scan =       dummy_scan,
    .send_dtmf =  dummy_send_dtmf,
    .recv_dtmf =  dummy_recv_dtmf,
    .send_morse =  dummy_send_morse,
    .send_voice_mem =  dummy_send_voice_mem,
    .set_channel =    dummy_set_channel,
    .get_channel =    dummy_get_channel,
    .set_trn =    dummy_set_trn,
    .get_trn =    dummy_get_trn,
    .power2mW =   dummy_power2mW,
    .mW2power =   dummy_mW2power,
    .set_clock = dummy_set_clock,
    .get_clock = dummy_get_clock,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

DECLARE_INITRIG_BACKEND(dummy)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&dummy_caps);
    rig_register(&netrigctl_caps);
    rig_register(&flrig_caps);
    rig_register(&trxmanager_caps);
    rig_register(&dummy_no_vfo_caps);
//    rig_register(&tci1x_caps);
    return RIG_OK;
}
