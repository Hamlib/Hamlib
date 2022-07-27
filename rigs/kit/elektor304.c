/*
 *  Hamlib KIT backend - Elektor DRM receiver description
 *  Copyright (c) 2004-2005 by Stephane Fillod
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
#include "serial.h"
#include "token.h"


#define ELEKTOR304_MODES (RIG_MODE_AM)

#define ELEKTOR304_FUNC (RIG_FUNC_NONE)

#define ELEKTOR304_LEVEL_ALL (RIG_LEVEL_NONE)

#define ELEKTOR304_PARM_ALL (RIG_PARM_NONE)

#define ELEKTOR304_VFO (RIG_VFO_A)

/* defaults */
#define OSCFREQ     MHz(50)
#define IFMIXFREQ   kHz(454.3)

struct elektor304_priv_data
{
    freq_t osc_freq;
    freq_t if_mix_freq;
};
#define TOK_OSCFREQ TOKEN_BACKEND(1)
#define TOK_IFMIXFREQ TOKEN_BACKEND(2)

static const struct confparams elektor304_cfg_params[] =
{
    {
        TOK_OSCFREQ, "osc_freq", "Oscillatorfreq", "Oscillator frequency in Hz",
        "50000000", RIG_CONF_NUMERIC, { .n = { 0, GHz(2), 1 } }
    },
    {
        TOK_IFMIXFREQ, "if_mix_freq", "IF", "IF mixing frequency in Hz",
        "454300", RIG_CONF_NUMERIC, { .n = { 0, MHz(100), 1 } }
    },
    { RIG_CONF_END, NULL, }
};

static int elektor304_init(RIG *rig);
static int elektor304_cleanup(RIG *rig);
static int elektor304_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int elektor304_set_conf(RIG *rig, token_t token, const char *val);
static int elektor304_get_conf(RIG *rig, token_t token, char *val);

/*
 * The Elektor DRM Receiver 3/04 COM interface is based on the Visual Basic
 * source code by Burkhard Kainka which can be downloaded from www.b-kainka.de
 * Linux support is based on a code written by Markus März:
 *  http://mitglied.lycos.de/markusmaerz/drm
 * Linux support is available from DRM Dream project.
 *
 * The DDS is a AD9835.
 *
 * The receiver is controlled via the TX, RTS and DTR pins of the serial port.
 */

const struct rig_caps elektor304_caps =
{
    RIG_MODEL(RIG_MODEL_ELEKTOR304),
    .model_name = "Elektor 3/04",
    .mfg_name =  "Elektor",
    .version =  "20200112.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TUNER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,  /* bit banging */
    .serial_rate_min =  9600,   /* don't care */
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry = 0,

    .has_get_func =  ELEKTOR304_FUNC,
    .has_set_func =  ELEKTOR304_FUNC,
    .has_get_level =  ELEKTOR304_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ELEKTOR304_LEVEL_ALL),
    .has_get_parm =  ELEKTOR304_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(ELEKTOR304_PARM_ALL),
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
        {kHz(500), MHz(22), ELEKTOR304_MODES, -1, -1, ELEKTOR304_VFO}, /* TBC */
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(500), MHz(22), ELEKTOR304_MODES, -1, -1, ELEKTOR304_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  {
        {ELEKTOR304_MODES, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {ELEKTOR304_MODES, kHz(12)},
        RIG_FLT_END,
    },
    .cfgparams =  elektor304_cfg_params,

    .rig_init =  elektor304_init,
    .rig_cleanup =  elektor304_cleanup,
    .set_conf =  elektor304_set_conf,
    .get_conf =  elektor304_get_conf,

    .set_freq =  elektor304_set_freq,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


int elektor304_init(RIG *rig)
{
    struct elektor304_priv_data *priv;

    rig->state.priv = (struct elektor304_priv_data *)calloc(1, sizeof(struct
                      elektor304_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->osc_freq = OSCFREQ;
    priv->if_mix_freq = IFMIXFREQ;

    return RIG_OK;
}

int elektor304_cleanup(RIG *rig)
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
int elektor304_set_conf(RIG *rig, token_t token, const char *val)
{
    struct elektor304_priv_data *priv;

    priv = (struct elektor304_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        sscanf(val, "%"SCNfreq, &priv->osc_freq);
        break;

    case TOK_IFMIXFREQ:
        sscanf(val, "%"SCNfreq, &priv->if_mix_freq);
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
int elektor304_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct elektor304_priv_data *priv;

    priv = (struct elektor304_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        SNPRINTF(val, val_len, "%"PRIfreq, priv->osc_freq);
        break;

    case TOK_IFMIXFREQ:
        SNPRINTF(val, val_len, "%"PRIfreq, priv->if_mix_freq);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int elektor304_get_conf(RIG *rig, token_t token, char *val)
{
    return elektor304_get_conf2(rig, token, val, 128);
}


#define AD_DELAY 4000
/*
 * Introduce delay after changing the bit state
 * FIXME: This implementation may not work for very fast computers,
 *      or smart compilers. However, nanosleep can have
 *      granularity > 10ms!
 */
static int ad_delay(int m)
{
    long j;

    for (j = 0; j <= m; j++) {}

    return j;
}

static int ad_sdata(hamlib_port_t *port, int i)
{
    int ret;

    ret = ser_set_rts(port, i);
    ad_delay(AD_DELAY);

    if (ret != RIG_OK)
        rig_debug(RIG_DEBUG_ERR, "%s: unable to set statusbits\n",
                  __func__);

    return ret;
}

static int ad_sclk(hamlib_port_t *port, int i)
{
    int ret;

    ret = ser_set_brk(port, i);
    ad_delay(AD_DELAY);

    if (ret != RIG_OK)
        rig_debug(RIG_DEBUG_ERR, "%s: unable to set statusbits\n",
                  __func__);

    return ret;
}

static int ad_fsync(hamlib_port_t *port, int i)
{
    int ret;

    ret = ser_set_dtr(port, i);
    ad_delay(AD_DELAY);

    if (ret != RIG_OK)
        rig_debug(RIG_DEBUG_ERR, "%s: unable to set statusbits\n",
                  __func__);

    return ret;
}

static int ad_write(hamlib_port_t *port, unsigned data)
{
    unsigned mask = 0x8000;
    int i;

    ad_sclk(port, 0);   /* TXD 0 */
    ad_fsync(port, 1);  /* DTR 1, CE */

    for (i = 0; i < 16; i++)
    {

        ad_sdata(port, (data & mask) ? 0 : 1);    /* RTS 0 or 1 */
        ad_sclk(port, 1);   /* TXD 1, clock */
        ad_sclk(port, 0);   /* TXD 0 */
        mask >>= 1; /* Next bit for masking */
    }

    ad_fsync(port, 0);  /* DTR 0 */

    return RIG_OK;
}


int elektor304_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned long frg;
    unsigned fhl, fhh, fll, flh;

    struct elektor304_priv_data *priv;
    hamlib_port_t *port = &rig->state.rigport;

    priv = (struct elektor304_priv_data *)rig->state.priv;

    rig_flush(port);

    /* Initialization */
    ad_fsync(port, 0);
    ad_sdata(port, 0);
    ad_sclk(port, 0);

    /* all frequencies are in Hz */
    frg = (unsigned long)(((double)freq + priv->if_mix_freq) /
                          priv->osc_freq * 4294967296.0 + 0.5);

    fll = frg & 0xff;
    flh = (frg >> 8) & 0xff;
    fhl = (frg >> 16) & 0xff;
    fhh = (frg >> 24) & 0xff;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %lu=[%02x.%02x.%02x.%02x]\n",
              __func__, frg, fll, flh, fhl, fhh);

    ad_write(port, 0xF800);  /* Reset */

    ad_write(port, 0x3000 | fll); /* 4 Bytes to FREQ0 */
    ad_write(port, 0x2100 | flh);
    ad_write(port, 0x3200 | fhl);
    ad_write(port, 0x2300 | fhh);

    ad_write(port, 0x8000); /* Sync */
    ad_write(port, 0xC000); /* Reset end */

    return RIG_OK;
}

