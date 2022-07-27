/*
 *  Hamlib KIT backend - DDS-60 description
 *  Copyright (c) 2007 by Stephane Fillod
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

#include <stdlib.h>
#include <stdio.h>
#include "hamlib/rig.h"

#include "kit.h"
#include "parallel.h"
#include "token.h"


#define DDS60_MODES (RIG_MODE_AM)

#define DDS60_FUNC (RIG_FUNC_NONE)

#define DDS60_LEVEL_ALL (RIG_LEVEL_NONE)

#define DDS60_PARM_ALL (RIG_PARM_NONE)

#define DDS60_VFO (RIG_VFO_A)

/* defaults */
#define OSCFREQ     MHz(30)
#define IFMIXFREQ   kHz(0)

#define PHASE_INCR  11.25

struct dds60_priv_data
{
    freq_t osc_freq;
    freq_t if_mix_freq;
    int multiplier;
    unsigned phase_step;    /* as 11.25 deg steps */
};
#define TOK_OSCFREQ TOKEN_BACKEND(1)
#define TOK_IFMIXFREQ TOKEN_BACKEND(2)
#define TOK_MULTIPLIER TOKEN_BACKEND(3)
#define TOK_PHASE_MOD TOKEN_BACKEND(4)

static const struct confparams dds60_cfg_params[] =
{
    {
        TOK_OSCFREQ, "osc_freq", "Oscillator freq", "Oscillator frequency in Hz",
        "30000000", RIG_CONF_NUMERIC, { .n = { 0, MHz(180), 1 } }
    },
    {
        TOK_IFMIXFREQ, "if_mix_freq", "IF", "IF mixing frequency in Hz",
        "0", RIG_CONF_NUMERIC, { .n = { 0, MHz(180), 1 } }
    },
    {
        TOK_MULTIPLIER, "multiplier", "Multiplier", "Optional X6 multiplier",
        "1", RIG_CONF_CHECKBUTTON
    },
    {
        TOK_IFMIXFREQ, "phase_mod", "Phase Modulation", "Phase modulation in degrees",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 360, PHASE_INCR } }
    },
    { RIG_CONF_END, NULL, }
};

static int dds60_init(RIG *rig);
static int dds60_cleanup(RIG *rig);
static int dds60_open(RIG *rig);
static int dds60_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int dds60_set_conf(RIG *rig, token_t token, const char *val);
static int dds60_get_conf(RIG *rig, token_t token, char *val);

/*
 * The DDS-60 kit exists with a AD9851 chip (60 MHz),
 * as well as with the AD9850 chip (30 MHz) (no multiplier).
 * There is an option to enable/disable the AD9851 X6 multiplier.
 * http://www.amqrp.org/kits/dds60/
 * http://www.analog.com/en/prod/0,2877,AD9851,00.html
 *
 * The receiver is controlled via the parallel port (D0,D1,D2).
 */

const struct rig_caps dds60_caps =
{
    RIG_MODEL(RIG_MODEL_DDS60),
    .model_name = "DDS-60",
    .mfg_name =  "AmQRP",
    .version =  "20200112.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_TUNER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_PARALLEL,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry = 0,

    .has_get_func =  DDS60_FUNC,
    .has_set_func =  DDS60_FUNC,
    .has_get_level =  DDS60_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(DDS60_LEVEL_ALL),
    .has_get_parm =  DDS60_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(DDS60_PARM_ALL),
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { RIG_CHAN_END },

    .rx_range_list1 =  {
        {MHz(1), MHz(60), DDS60_MODES, -1, -1, DDS60_VFO}, /* TBC */
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {MHz(1), MHz(60), DDS60_MODES, -1, -1, DDS60_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  {
        {DDS60_MODES, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {DDS60_MODES, kHz(12)},
        RIG_FLT_END,
    },
    .cfgparams =  dds60_cfg_params,

    .rig_init =  dds60_init,
    .rig_cleanup =  dds60_cleanup,
    .rig_open =  dds60_open,
    .set_conf =  dds60_set_conf,
    .get_conf =  dds60_get_conf,

    .set_freq =  dds60_set_freq,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


int dds60_init(RIG *rig)
{
    struct dds60_priv_data *priv;

    rig->state.priv = (struct dds60_priv_data *)calloc(1, sizeof(
                          struct dds60_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->osc_freq = OSCFREQ;
    priv->if_mix_freq = IFMIXFREQ;
    priv->multiplier = 1;
    priv->phase_step = 0;

    return RIG_OK;
}

int dds60_cleanup(RIG *rig)
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
int dds60_set_conf(RIG *rig, token_t token, const char *val)
{
    struct dds60_priv_data *priv;
    float phase;

    priv = (struct dds60_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        sscanf(val, "%"SCNfreq, &priv->osc_freq);
        break;

    case TOK_IFMIXFREQ:
        sscanf(val, "%"SCNfreq, &priv->if_mix_freq);
        break;

    case TOK_MULTIPLIER:
        sscanf(val, "%d", &priv->multiplier);
        break;

    case TOK_PHASE_MOD:
        sscanf(val, "%f", &phase);
        priv->phase_step = ((unsigned)((phase + PHASE_INCR / 2) / PHASE_INCR)) % 32;
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
int dds60_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct dds60_priv_data *priv;

    priv = (struct dds60_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        SNPRINTF(val, val_len, "%"PRIfreq, priv->osc_freq);
        break;

    case TOK_IFMIXFREQ:
        SNPRINTF(val, val_len, "%"PRIfreq, priv->if_mix_freq);
        break;

    case TOK_MULTIPLIER:
        SNPRINTF(val, val_len, "%d", priv->multiplier);
        break;

    case TOK_PHASE_MOD:
        SNPRINTF(val, val_len, "%f", priv->phase_step * PHASE_INCR);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int dds60_get_conf(RIG *rig, token_t token, char *val)
{
    return dds60_get_conf2(rig, token, val, 128);
}


#define DATA  0x01  /* d0 */
#define CLOCK 0x02  /* d1 */
#define LOAD  0x03  /* d2 */


static void ad_delay(int delay)
{
    /* none needed, I/O bus should be slow enough */
}

static void ad_bit(hamlib_port_t *port, unsigned char bit)
{
    bit &= DATA;

    par_write_data(port, bit);
    ad_delay(1);

    par_write_data(port, bit | CLOCK);
    ad_delay(1);

    par_write_data(port, bit);
    ad_delay(1);
}

static void ad_write(hamlib_port_t *port, unsigned long word,
                     unsigned char control)
{
    int i;

    /* lock the parallel port */
    par_lock(port);

    /* shift out the least significant 32 bits of the word */
    for (i = 0; i < 32; i++)
    {

        ad_bit(port, word & DATA);

        word >>= 1;
    }

    /* write out the control byte */
    for (i = 0; i < 8; i++)
    {

        ad_bit(port, control & DATA);

        control >>= 1;
    }

    /* load the register */
    par_write_data(port, LOAD);
    ad_delay(1);
    par_write_data(port, 0);

    /* unlock the parallel port */
    par_unlock(port);
}


int dds60_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned long frg;
    unsigned char control;
    struct dds60_priv_data *priv;
    hamlib_port_t *port = &rig->state.rigport;
    freq_t osc_ref;

    priv = (struct dds60_priv_data *)rig->state.priv;

    if (priv->multiplier)
    {
        osc_ref = priv->osc_freq * 6;
    }
    else
    {
        osc_ref = priv->osc_freq;
    }

    /* all frequencies are in Hz */
    frg = (unsigned long)(((double)freq + priv->if_mix_freq) /
                          osc_ref * 4294967296.0 + 0.5);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: word %lu, X6 multiplier %d, phase %.2f\n",
              __func__, frg, priv->multiplier, priv->phase_step * PHASE_INCR);

    control = priv->multiplier ? 0x01 : 0x00;
    control |= (priv->phase_step & 0x1f) << 3;

    ad_write(port, frg, control);

    return RIG_OK;
}

int dds60_open(RIG *rig)
{
    hamlib_port_t *port = &rig->state.rigport;

    /* lock the parallel port */
    par_lock(port);

    /* Serial load enable sequence W_CLK */
    par_write_data(port, 0);
    ad_delay(1);

    par_write_data(port, CLOCK);
    ad_delay(1);

    par_write_data(port, 0);
    ad_delay(1);

    /* Serial load enable sequence FQ_UD */
    par_write_data(port, LOAD);
    ad_delay(1);
    par_write_data(port, 0);

    /* unlock the parallel port */
    par_unlock(port);

    return RIG_OK;
}

