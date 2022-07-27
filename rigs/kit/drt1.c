/*
 *  Hamlib KIT backend - Sat-Schneider DRT1/SAD1 DRM receiver description
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


#define DRT1_MODES (RIG_MODE_AM)

#define DRT1_FUNC (RIG_FUNC_NONE)

#define DRT1_LEVEL_ALL (RIG_LEVEL_NONE)

#define DRT1_PARM_ALL (RIG_PARM_NONE)

#define DRT1_VFO (RIG_VFO_A)

/* defaults */
#define OSCFREQ     MHz(45.012)
#define IFMIXFREQ   MHz(45)
#define REFMULT     8
#define CHARGE_PUMP_CURRENT 150

struct drt1_priv_data
{
    freq_t osc_freq;
    freq_t if_mix_freq;
    unsigned ref_mult;
    unsigned pump_crrnt;
};
#define TOK_OSCFREQ TOKEN_BACKEND(1)
#define TOK_IFMIXFREQ TOKEN_BACKEND(2)
#define TOK_REFMULT TOKEN_BACKEND(3)
#define TOK_PUMPCRNT TOKEN_BACKEND(4)

static const struct confparams drt1_cfg_params[] =
{
    {
        TOK_OSCFREQ, "osc_freq", "Oscillatorfreq", "Oscillator frequency in Hz",
        "45012000", RIG_CONF_NUMERIC, { .n = { 0, MHz(400), 1 } }
    },
    {
        TOK_IFMIXFREQ, "if_mix_freq", "IF", "IF mixing frequency in Hz",
        "45000000", RIG_CONF_NUMERIC, { .n = { 0, MHz(400), 1 } }
    },
    {
        TOK_REFMULT, "ref_mult", "REFCLK Multiplier", "REFCLK Multiplier",
        "8", RIG_CONF_NUMERIC, { .n = { 4, 20, 1 } }
    },
    {
        TOK_PUMPCRNT, "pump_current", "Charge pump current", "Charge pump current in uA",
        "150", RIG_CONF_NUMERIC, { .n = { 75, 150, 25 } }
    },
    { RIG_CONF_END, NULL, }
};

static int drt1_init(RIG *rig);
static int drt1_cleanup(RIG *rig);
static int drt1_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int drt1_set_conf(RIG *rig, token_t token, const char *val);
static int drt1_get_conf(RIG *rig, token_t token, char *val);

/*
 * SAT-Service Schneider DRM tuner.
 *
 * The receiver is controlled via the TX, RTS and DTR pins of the serial port.
 */

const struct rig_caps drt1_caps =
{
    RIG_MODEL(RIG_MODEL_DRT1),
    .model_name = "DRT1",
    .mfg_name =  "SAT-Schneider",
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

    .has_get_func =  DRT1_FUNC,
    .has_set_func =  DRT1_FUNC,
    .has_get_level =  DRT1_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(DRT1_LEVEL_ALL),
    .has_get_parm =  DRT1_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(DRT1_PARM_ALL),
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

    .chan_list =  { RIG_CHAN_END, },

    .rx_range_list1 =  {
        {kHz(50), MHz(30), DRT1_MODES, -1, -1, DRT1_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(50), MHz(30), DRT1_MODES, -1, -1, DRT1_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  {
        {DRT1_MODES, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {DRT1_MODES, kHz(10)},  /* opt. 20 kHz */
        RIG_FLT_END,
    },
    .cfgparams =  drt1_cfg_params,

    .rig_init =  drt1_init,
    .rig_cleanup =  drt1_cleanup,
    .set_conf =  drt1_set_conf,
    .get_conf =  drt1_get_conf,

    .set_freq =  drt1_set_freq,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


int drt1_init(RIG *rig)
{
    struct drt1_priv_data *priv;

    rig->state.priv = (struct drt1_priv_data *)calloc(1, sizeof(
                          struct drt1_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->osc_freq = OSCFREQ;
    priv->ref_mult = REFMULT;
    priv->if_mix_freq = IFMIXFREQ;
    priv->pump_crrnt = CHARGE_PUMP_CURRENT;

    return RIG_OK;
}

int drt1_cleanup(RIG *rig)
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
int drt1_set_conf(RIG *rig, token_t token, const char *val)
{
    struct drt1_priv_data *priv;

    priv = (struct drt1_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        sscanf(val, "%"SCNfreq, &priv->osc_freq);
        break;

    case TOK_REFMULT:
        sscanf(val, "%u", &priv->ref_mult);
        break;

    case TOK_IFMIXFREQ:
        sscanf(val, "%"SCNfreq, &priv->if_mix_freq);
        break;

    case TOK_PUMPCRNT:
        sscanf(val, "%u", &priv->pump_crrnt);
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
int drt1_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct drt1_priv_data *priv;

    priv = (struct drt1_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        SNPRINTF(val, val_len, "%"PRIfreq, priv->osc_freq);
        break;

    case TOK_REFMULT:
        SNPRINTF(val, val_len, "%u", priv->ref_mult);
        break;

    case TOK_IFMIXFREQ:
        SNPRINTF(val, val_len, "%"PRIfreq, priv->if_mix_freq);
        break;

    case TOK_PUMPCRNT:
        SNPRINTF(val, val_len, "%u", priv->pump_crrnt);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int drt1_get_conf(RIG *rig, token_t token, char *val)
{
    return drt1_get_conf2(rig, token, val, 128);
}


/*

 DDS is AD9951.

 The clock input is 45,012 MHz (also 2nd LO frequencie at the same time).

The clock multiplier should be set to 8x at start value (possible, that this
will change to lower clock multiplier).

The charge pump current to 75 µA at start value (possible will change).

The VCO gain bit has to be set.

The IF offset for LO to the receiving frequency is  + 45,000 MHz  (fLO =
frec + 45,000 MHz)

Don't make the data clock too high, there are 1 KOhms decoupling resistors at
the input pins of the DDS.


Inputs (SDI(O)); SCLK und I/O UPDATE haves 5V TTL level, so that a
 level converter from RS232 is needed.
 I will use the attached motorola IC MC1489 as converter for amateur
 application. This IC inverts the signals !

*/

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

static int ad_sdio(hamlib_port_t *port, int i)
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

static int ad_ioupd(hamlib_port_t *port, int i)
{
    int ret;

    ret = ser_set_dtr(port, i);
    ad_delay(AD_DELAY);

    if (ret != RIG_OK)
        rig_debug(RIG_DEBUG_ERR, "%s: unable to set statusbits\n",
                  __func__);

    return ret;
}

static int ad_write_reg(hamlib_port_t *port, unsigned addr, unsigned nb_bytes,
                        unsigned data)
{
    int i;

    ad_sclk(port, 0);   /* TXD 0 */
    ad_ioupd(port, 1);  /* DTR 1, CE */

    /* Instruction byte, MSB Logic 0 = write */
    addr &= 0x1f;

    for (i = 7; i >= 0; i--)
    {
        ad_sdio(port, (addr & (1U << i)) ? 0 : 1); /* RTS 0 or 1 */
        ad_sclk(port, 1);   /* TXD 1, clock */
        ad_sclk(port, 0);   /* TXD 0 */
    }

    /* Data transfer */
    for (i = nb_bytes * 8 - 1; i >= 0; i--)
    {

        ad_sdio(port, (data & (1U << i)) ? 0 : 1); /* RTS 0 or 1 */
        ad_sclk(port, 1);   /* TXD 1, clock */
        ad_sclk(port, 0);   /* TXD 0 */
    }

    ad_ioupd(port, 0);  /* DTR 0 */

    return RIG_OK;
}

/* Register serial adresses */
#define CFR1    0x0
#define CFR2    0x1
#define ASF 0x2
#define ARR 0x3
#define FTW0    0x4
#define POW0    0x5

int drt1_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned long frg;
    unsigned cfr2;

    struct drt1_priv_data *priv;
    hamlib_port_t *port = &rig->state.rigport;

    priv = (struct drt1_priv_data *)rig->state.priv;

    rig_flush(port);

    /* Initialization */
    ad_ioupd(port, 0);
    ad_sdio(port, 0);
    ad_sclk(port, 0);

    /*
     * CFR2:
     *  clock multiplier set to 8x, charge pump current to 75 µA
     *  VCO gain bit has to be set
     */
    cfr2 = ((priv->ref_mult << 3) & 0xf8) | 0x4 |
           (((priv->pump_crrnt - 75) / 25) & 0x3);

    ad_write_reg(port, CFR2, 3, cfr2);



    /* all frequencies are in Hz */
    frg = (unsigned long)(((double)freq + priv->if_mix_freq) /
                          (priv->osc_freq * priv->ref_mult)
                          * 4294967296.0);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: [%#lx]\n", __func__, frg);

    ad_write_reg(port, FTW0, 4, frg);

    return RIG_OK;
}

