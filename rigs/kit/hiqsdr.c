/*
 *  Hamlib HiQSDR backend
 *  Copyright (c) 20012 by Stephane Fillod
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
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */

#include "hamlib/rig.h"
#include "iofunc.h"
#include "misc.h"
#include "token.h"

/*
 *  http://www.hiqsdr.org
 */

/* HiQSDR constants */

#define REFCLOCK 122880000
#define DEFAULT_SAMPLE_RATE 48000

/* V1.1 */
#define CTRL_FRAME_LEN 22

struct hiqsdr_priv_data
{
    split_t split;
    int sample_rate;
    double ref_clock;
    unsigned char control_frame[CTRL_FRAME_LEN];
};

static int hiqsdr_init(RIG *rig);
static int hiqsdr_cleanup(RIG *rig);
static int hiqsdr_open(RIG *rig);
static int hiqsdr_close(RIG *rig);

static int hiqsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int hiqsdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int hiqsdr_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                vfo_t tx_vfo);
static int hiqsdr_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int hiqsdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int hiqsdr_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int hiqsdr_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);

static int hiqsdr_set_conf(RIG *rig, token_t token, const char *val);
static int hiqsdr_get_conf(RIG *rig, token_t token, char *val);
static int hiqsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int hiqsdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);


#define TOK_OSCFREQ TOKEN_BACKEND(1)
#define TOK_SAMPLE_RATE TOKEN_BACKEND(2)

const struct confparams hiqsdr_cfg_params[] =
{
    {
        TOK_OSCFREQ, "osc_freq", "Oscillator freq", "Oscillator frequency of reference clock in Hz",
        "122880000", RIG_CONF_NUMERIC, { .n = { 0, MHz(256), 1 } }
    },
    {
        TOK_SAMPLE_RATE, "sample_rate", "Sample rate", "Sample rate",
        "48000", RIG_CONF_NUMERIC, { /* .n = */ { 48000, 1920000, 1 } }
    },
    { RIG_CONF_END, NULL, }
};


/*
 * HiQSDR rig capabilities.
 */

#define HIQSDR_FUNC  RIG_FUNC_NONE
#define HIQSDR_LEVEL (RIG_LEVEL_RFPOWER|RIG_LEVEL_PREAMP|RIG_LEVEL_ATT)
#define HIQSDR_PARM   RIG_PARM_NONE
#define HIQSDR_VFO_OP RIG_OP_NONE
#define HIQSDR_SCAN   RIG_SCAN_NONE

#define HIQSDR_VFO (RIG_VFO_A)
#define HIQSDR_ANT (RIG_ANT_1|RIG_ANT_2)

#define HIQSDR_MODES (RIG_MODE_CW|RIG_MODE_DSB)

const struct rig_caps hiqsdr_caps =
{
    RIG_MODEL(RIG_MODEL_HIQSDR),
    .model_name =     "HiQSDR",
    .mfg_name =       "N2ADR",
    .version =        "20200323.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_ALPHA,
    .rig_type =       RIG_TYPE_TUNER,
    .targetable_vfo =  RIG_TARGETABLE_NONE,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_NONE,
    .port_type =      RIG_PORT_UDP_NETWORK,
    .timeout =        500,
    .has_get_func =   HIQSDR_FUNC,
    .has_set_func =   HIQSDR_FUNC,
    .has_get_level =  HIQSDR_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(HIQSDR_LEVEL),
    .has_get_parm =    HIQSDR_PARM,
    .has_set_parm =    RIG_PARM_SET(HIQSDR_PARM),
    .ctcss_list =      NULL,
    .dcs_list =        NULL,
    .chan_list =   { RIG_CHAN_END, },
    .scan_ops =    HIQSDR_SCAN,
    .vfo_ops =     HIQSDR_VFO_OP,
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { 2, 4, 6, 10, 20, 30, 44, RIG_DBLST_END }, // -2dB steps in fact
    .preamp =      { 10, RIG_DBLST_END, },    // TODO

    .rx_range_list1 =  { {
            .startf = kHz(100), .endf = MHz(66), .modes = HIQSDR_MODES,
            .low_power = -1, .high_power = -1, HIQSDR_VFO, HIQSDR_ANT
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { {
            .startf = kHz(100), .endf = MHz(66), .modes = HIQSDR_MODES,
            .low_power = mW(1), .high_power = mW(50), HIQSDR_VFO, HIQSDR_ANT
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 =  { {
            .startf = kHz(100), .endf = MHz(66), .modes = HIQSDR_MODES,
            .low_power = -1, .high_power = -1, HIQSDR_VFO, HIQSDR_ANT
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { {
            .startf = kHz(100), .endf = MHz(66), .modes = HIQSDR_MODES,
            .low_power = mW(1), .high_power = mW(50), HIQSDR_VFO, HIQSDR_ANT
        },
        RIG_FRNG_END,
    },
    .tuning_steps =  { {HIQSDR_MODES, 1}, RIG_TS_END, },
    .filters =      {
        {RIG_MODE_CW, kHz(2.4)},
        {HIQSDR_MODES, RIG_FLT_ANY},
        RIG_FLT_END,
    },

    .priv =  NULL,

    .rig_init =     hiqsdr_init,
    .rig_cleanup =  hiqsdr_cleanup,
    .rig_open =     hiqsdr_open,
    .rig_close =    hiqsdr_close,

    .cfgparams =    hiqsdr_cfg_params,
    .set_conf =     hiqsdr_set_conf,
    .get_conf =     hiqsdr_get_conf,

    .set_freq =     hiqsdr_set_freq,
    .get_freq =     hiqsdr_get_freq,

    .set_split_freq = hiqsdr_set_split_freq,
    .set_split_vfo = hiqsdr_set_split_vfo,

    .set_mode =     hiqsdr_set_mode,

    .set_ptt =      hiqsdr_set_ptt,
    .set_ant =      hiqsdr_set_ant,

    .set_level =    hiqsdr_set_level,
    .get_level =    hiqsdr_get_level,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


static int send_command(RIG *rig)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;
    int ret;

    ret = write_block(&rig->state.rigport, (unsigned char *) priv->control_frame,
                      CTRL_FRAME_LEN);
#if 0
    ret = read_block(&rig->state.rigport, (unsigned char *) priv->control_frame,
                     CTRL_FRAME_LEN);

    if (ret != CTRL_FRAME_LEN)
    {
        ret = ret < 0 ? ret : -RIG_EPROTO;
    }

#endif

    return ret;
}

static unsigned compute_sample_rate(const struct hiqsdr_priv_data *priv)
{
    unsigned rx_control;

    rx_control = (unsigned)(priv->ref_clock / (8. * 8. * priv->sample_rate)) - 1;

    if (rx_control > 39)
    {
        rx_control = 39;
    }

    return rx_control;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int hiqsdr_set_conf(RIG *rig, token_t token, const char *val)
{
    struct hiqsdr_priv_data *priv;
    struct rig_state *rs;

    rs = &rig->state;
    priv = (struct hiqsdr_priv_data *)rs->priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        priv->ref_clock = atof(val);
        priv->control_frame[12] = compute_sample_rate(priv);
        break;

    case TOK_SAMPLE_RATE:
        priv->sample_rate = atoi(val);
        priv->control_frame[12] = compute_sample_rate(priv);
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
int hiqsdr_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct hiqsdr_priv_data *priv;
    struct rig_state *rs;

    rs = &rig->state;
    priv = (struct hiqsdr_priv_data *)rs->priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        SNPRINTF(val, val_len, "%f", priv->ref_clock);
        break;

    case TOK_SAMPLE_RATE:
        SNPRINTF(val, val_len, "%d", priv->sample_rate);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int hiqsdr_get_conf(RIG *rig, token_t token, char *val)
{
    return hiqsdr_get_conf2(rig, token, val, 128);
}

int hiqsdr_init(RIG *rig)
{
    struct hiqsdr_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig->state.priv = (struct hiqsdr_priv_data *)calloc(1, sizeof(
                          struct hiqsdr_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->split = RIG_SPLIT_OFF;
    priv->ref_clock = REFCLOCK;
    priv->sample_rate = DEFAULT_SAMPLE_RATE;
    strncpy(rig->state.rigport.pathname, "192.168.2.196:48248",
            HAMLIB_FILPATHLEN - 1);

    return RIG_OK;
}

int hiqsdr_open(RIG *rig)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;
#if 0
    const char buf_send_to_me[] = { 0x72, 0x72 };
    int ret;
#endif

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /* magic value */
    priv->control_frame[0] = 'S';
    priv->control_frame[1] = 't';
    /* zero tune phase */
    memset(priv->control_frame + 2, 0, 8);
    /* TX output level */
    priv->control_frame[10] = 120;
    /* Tx control: non-CW */
    priv->control_frame[11] = 0x02;
    /* decimation: 48 kSpls */
    priv->control_frame[12] = compute_sample_rate(priv);

    /* firmware version */
    priv->control_frame[13] = 0x00;
    /* X1 connector */
    priv->control_frame[14] = 0x00;
    /* Attenuator */
    priv->control_frame[15] = 0x00;
    /* AntSwitch */
    priv->control_frame[16] = 0x00;
    /* RFU */
    memset(priv->control_frame + 17, 0, 5);

#if 0
    /* Send the samples to me. FIXME: send to port 48247 */
    ret = write_block(&rig->state.rigport, buf_send_to_me, sizeof(buf_send_to_me));

    if (ret != RIG_OK)
    {
        return RIG_OK;
    }

#endif

    return RIG_OK;
}


int hiqsdr_close(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

int hiqsdr_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}


/*
 */
int hiqsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;
    int ret;
    double rxphase;
    uint32_t rxphase32;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    rxphase = (freq / priv->ref_clock) * (1ULL << 32) + 0.5;
    rxphase32 = (uint32_t)rxphase;

    priv->control_frame[2] = rxphase32 & 0xff;
    priv->control_frame[3] = (rxphase32 >> 8) & 0xff;
    priv->control_frame[4] = (rxphase32 >> 16) & 0xff;
    priv->control_frame[5] = (rxphase32 >> 24) & 0xff;

    if (priv->split == RIG_SPLIT_OFF)
    {
        priv->control_frame[6] = priv->control_frame[2];
        priv->control_frame[7] = priv->control_frame[3];
        priv->control_frame[8] = priv->control_frame[4];
        priv->control_frame[9] = priv->control_frame[5];
    }

    ret = send_command(rig);

    return ret;
}

static int hiqsdr_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                vfo_t tx_vfo)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;

    priv->split = split;

    return RIG_OK;
}

int hiqsdr_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;
    int ret;
    double rxphase;
    uint32_t rxphase32;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    rxphase = (tx_freq / priv->ref_clock) * (1ULL << 32) + 0.5;
    rxphase32 = (uint32_t)rxphase;

    priv->control_frame[6] = rxphase32 & 0xff;
    priv->control_frame[7] = (rxphase32 >> 8) & 0xff;
    priv->control_frame[8] = (rxphase32 >> 16) & 0xff;
    priv->control_frame[9] = (rxphase32 >> 24) & 0xff;

    ret = send_command(rig);

    return ret;
}


int hiqsdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    return -RIG_ENIMPL;
}

int hiqsdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;
    int ret = RIG_OK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n",
              __func__, rig_strrmode(mode));


    if (mode == RIG_MODE_CW)
    {
        priv->control_frame[11] = 0x01;
    }
    else
    {
        priv->control_frame[11] = 0x02;
    }

    ret = send_command(rig);

    return ret;
}

int hiqsdr_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;
    int ret = RIG_OK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %d\n",
              __func__, ptt);

    /* not allowed in CW mode */
    if (priv->control_frame[11] & 0x01)
    {
        return -RIG_ERJCTED;
    }

    if (ptt == RIG_PTT_ON)
    {
        priv->control_frame[11] |= 0x08;
    }
    else
    {
        priv->control_frame[11] &= ~0x08;
    }

    ret = send_command(rig);

    return ret;
}

int hiqsdr_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;
    int ret = RIG_OK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %u\n",
              __func__, ant);

    if (ant == RIG_ANT_2)
    {
        priv->control_frame[16] |= 0x01;
    }
    else
    {
        priv->control_frame[16] &= ~0x01;
    }

    ret = send_command(rig);

    return ret;
}


int hiqsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct hiqsdr_priv_data *priv = (struct hiqsdr_priv_data *)rig->state.priv;
    int ret = RIG_OK;

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        if (val.i)
        {
            priv->control_frame[14] |= 0x02;
        }
        else
        {
            priv->control_frame[14] &= ~0x02;
        }

        break;

    case RIG_LEVEL_ATT:
        /* FIXME: val->i should be looked up from the att list */
        priv->control_frame[14] = val.i & 0x1f;
        break;

    case RIG_LEVEL_RFPOWER:
        /* TX output level */
        priv->control_frame[10] = 0xff & (unsigned)(255 * val.f);
        break;

    default:
        return -RIG_EINVAL;
    }

    ret = send_command(rig);

    return ret;
}

static int hiqsdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    return -RIG_ENIMPL;
}

