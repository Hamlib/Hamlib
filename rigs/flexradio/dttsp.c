/*
 *  Hamlib DttSP backend - main file
 *  Copyright (c) 2001-2012 by Stephane Fillod
 *
 *  Some code derived from DttSP
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 by Frank Brickle, AB2KT and Bob McGwier, N4HY
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
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "iofunc.h"
#include "misc.h"
#include "token.h"
#include "register.h"
#include "cal.h"
#include "flexradio.h"

/*
 * This backend is a two layer rig control: DttSP core over a mundane tuner
 *
 * 2 interfaces of DttSP are supported: IPC & UDP
 *
 * TODO: Transmit setup
 *  http://openhpsdr.org/wiki/index.php?title=Ghpsdr
 */

#define DEFAULT_DTTSP_CMD_PATH "/dev/shm/SDRcommands"
#define DEFAULT_DTTSP_CMD_NET_ADDR "127.0.0.1:19001"
#define DEFAULT_SAMPLE_RATE 48000

/* DttSP constants */
#define MAXRX 4
#define RXMETERPTS 5
#define TXMETERPTS 9
#define MAXMETERPTS 9

#define DTTSP_PORT_CLIENT_COMMAND 19001
#define DTTSP_PORT_CLIENT_SPECTRUM 19002
#define DTTSP_PORT_CLIENT_METER 19003
#define DTTSP_PORT_CLIENT_BUFSIZE 65536


struct dttsp_priv_data
{
    /* tuner providing IF */
    rig_model_t tuner_model;
    RIG *tuner;
    shortfreq_t IF_center_freq;

    int sample_rate;
    int rx_delta_f;

    hamlib_port_t meter_port;
};

static int dttsp_init(RIG *rig);
static int dttsp_cleanup(RIG *rig);
static int dttsp_open(RIG *rig);
static int dttsp_close(RIG *rig);

static int dttsp_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int dttsp_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int dttsp_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);

static int dttsp_set_conf(RIG *rig, token_t token, const char *val);
static int dttsp_get_conf(RIG *rig, token_t token, char *val);
static int dttsp_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int dttsp_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int dttsp_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int dttsp_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);

static int dttsp_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int dttsp_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);



#define TOK_TUNER_MODEL TOKEN_BACKEND(1)
#define TOK_SAMPLE_RATE TOKEN_BACKEND(2)

const struct confparams dttsp_cfg_params[] =
{
    {
        TOK_TUNER_MODEL, "tuner_model", "Tuner model", "Hamlib rig tuner model number",
        "1" /* RIG_MODEL_DUMMY */, RIG_CONF_NUMERIC, { /* .n = */ { 0, 100000, 1 } }
    },
    {
        TOK_SAMPLE_RATE, "sample_rate", "Sample rate", "DttSP sample rate in Spls/sec",
        "48000", RIG_CONF_NUMERIC, { /* .n = */ { 8000, 192000, 1 } }
    },
    /*
     * TODO: IF_center_freq, etc.
     */
    { RIG_CONF_END, NULL, }
};


/*
 * DttSP dsr-core rig capabilities.
 */

#define DTTSP_FUNC  (RIG_FUNC_NB|RIG_FUNC_ANF|RIG_FUNC_NR|RIG_FUNC_MUTE)
#define DTTSP_LEVEL (RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AGC)
#define DTTSP_PARM   RIG_PARM_NONE
#define DTTSP_VFO_OP RIG_OP_NONE
#define DTTSP_SCAN   RIG_SCAN_NONE

#define DTTSP_VFO (RIG_VFO_A)

#define DTTSP_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_SAM|RIG_MODE_DSB| \
        RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB)

enum dttsp_mode_e
{ LSB, USB, DSB, CWL, CWU, FMN, AM, DIGU, SPEC, DIGL, SAM, DRM };

static const struct hamlib_vs_dttsp
{
    rmode_t hamlib_mode;
    enum dttsp_mode_e dttsp_mode;
} hamlib_vs_dttsp_modes[] =
{
    { RIG_MODE_USB, USB },
    { RIG_MODE_LSB, LSB },
    { RIG_MODE_CW, CWU },
    { RIG_MODE_CWR, CWL },
    { RIG_MODE_AM, AM },
    { RIG_MODE_SAM, SAM },
    { RIG_MODE_FM, FMN },
    { RIG_MODE_DSB, DSB },
};

#define HAMLIB_VS_DTTSP_MODES_COUNT (sizeof(hamlib_vs_dttsp_modes)/sizeof(struct hamlib_vs_dttsp))

#define DTTSP_REAL_STR_CAL { 16, \
    { \
            {  -73, -73 }, /* S0 */ \
            {  50, 40 }, /* +40 */ \
    } }

const struct rig_caps dttsp_rig_caps =
{
    RIG_MODEL(RIG_MODEL_DTTSP),
    .model_name =     "DttSP IPC",
    .mfg_name =       "DTTS Microwave Society",
    .version =        "20200319.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rig_type =       RIG_TYPE_COMPUTER,
    .targetable_vfo =      RIG_TARGETABLE_ALL,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_RIG,
    .port_type =      RIG_PORT_DEVICE,
    .has_get_func =   DTTSP_FUNC,
    .has_set_func =   DTTSP_FUNC,
    .has_get_level =  DTTSP_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(DTTSP_LEVEL),
    .has_get_parm =    DTTSP_PARM,
    .has_set_parm =    RIG_PARM_SET(DTTSP_PARM),
    .ctcss_list =      NULL,
    .dcs_list =        NULL,
    .chan_list =   {
        RIG_CHAN_END,
    },
    .scan_ops =    DTTSP_SCAN,
    .vfo_ops =     DTTSP_VFO_OP,
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { RIG_DBLST_END, },
    .preamp =      { RIG_DBLST_END, },
    /* In fact, RX and TX ranges are dependent on the tuner */
    .rx_range_list1 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DTTSP_MODES,
            .low_power = -1, .high_power = -1, DTTSP_VFO
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, }, /* TODO */
    .rx_range_list2 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DTTSP_MODES,
            .low_power = -1, .high_power = -1, DTTSP_VFO
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, }, /* TODO */
    .tuning_steps =  { {DTTSP_MODES, 1}, {DTTSP_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .filters =      {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_DSB | RIG_MODE_SAM, kHz(8)},
        {RIG_MODE_FM, kHz(15)},
        {DTTSP_MODES, RIG_FLT_ANY},
        RIG_FLT_END,
    },

    .str_cal = DTTSP_REAL_STR_CAL,

    .priv =  NULL,

    .rig_init =     dttsp_init,
    .rig_cleanup =  dttsp_cleanup,
    .rig_open =     dttsp_open,
    .rig_close =    dttsp_close,

    .cfgparams =    dttsp_cfg_params,
    .set_conf =     dttsp_set_conf,
    .get_conf =     dttsp_get_conf,

    .set_freq =     dttsp_set_freq,
    .get_freq =     dttsp_get_freq,

    .set_mode =     dttsp_set_mode,

    .set_level =    dttsp_set_level,
    .get_level =    dttsp_get_level,

    .set_func =     dttsp_set_func,

    .set_rit =      dttsp_set_rit,
    .get_rit =      dttsp_get_rit,

    .set_ant =      dttsp_set_ant,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * The same as the previous IPC, but of type RIG_PORT_UDP_NETWORK
 */
const struct rig_caps dttsp_udp_rig_caps =
{
    RIG_MODEL(RIG_MODEL_DTTSP_UDP),
    .model_name =     "DttSP UDP",
    .mfg_name =       "DTTS Microwave Society",
    .version =        "20200319.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rig_type =       RIG_TYPE_COMPUTER,
    .targetable_vfo =      RIG_TARGETABLE_ALL,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_RIG,
    .port_type =      RIG_PORT_UDP_NETWORK,
    .timeout =        500,
    .has_get_func =   DTTSP_FUNC,
    .has_set_func =   DTTSP_FUNC,
    .has_get_level =  DTTSP_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(DTTSP_LEVEL),
    .has_get_parm =    DTTSP_PARM,
    .has_set_parm =    RIG_PARM_SET(DTTSP_PARM),
    .ctcss_list =      NULL,
    .dcs_list =        NULL,
    .chan_list =   { RIG_CHAN_END, },
    .scan_ops =    DTTSP_SCAN,
    .vfo_ops =     DTTSP_VFO_OP,
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { RIG_DBLST_END, },
    .preamp =      { RIG_DBLST_END, },
    /* In fact, RX and TX ranges are dependent on the tuner */
    .rx_range_list1 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DTTSP_MODES,
            .low_power = -1, .high_power = -1, DTTSP_VFO
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, }, /* TODO */
    .rx_range_list2 =  { {
            .startf = kHz(150), .endf = MHz(1500), .modes = DTTSP_MODES,
            .low_power = -1, .high_power = -1, DTTSP_VFO
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, }, /* TODO */
    .tuning_steps =  { {DTTSP_MODES, 1}, {DTTSP_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .filters =      {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_DSB | RIG_MODE_SAM, kHz(8)},
        {RIG_MODE_FM, kHz(15)},
        {DTTSP_MODES, RIG_FLT_ANY},
        RIG_FLT_END,
    },

    .str_cal = DTTSP_REAL_STR_CAL,

    .priv =  NULL,

    .rig_init =     dttsp_init,
    .rig_cleanup =  dttsp_cleanup,
    .rig_open =     dttsp_open,
    .rig_close =    dttsp_close,

    .cfgparams =    dttsp_cfg_params,
    .set_conf =     dttsp_set_conf,
    .get_conf =     dttsp_get_conf,

    .set_freq =     dttsp_set_freq,
    .get_freq =     dttsp_get_freq,

    .set_mode =     dttsp_set_mode,

    .set_level =    dttsp_set_level,
    .get_level =    dttsp_get_level,

    .set_func =     dttsp_set_func,

    .set_rit =      dttsp_set_rit,
    .get_rit =      dttsp_get_rit,

    .set_ant =      dttsp_set_ant,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


static int send_command(RIG *rig, const char *cmdstr, size_t buflen)
{
    int ret;

    ret = write_block(&rig->state.rigport, (unsigned char *) cmdstr, buflen);

    return ret;
}

static int fetch_meter(RIG *rig, int *label, float *data, int npts)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;
    int ret, buf_len;

    if (priv->meter_port.type.rig == RIG_PORT_UDP_NETWORK)
    {
        char buf[sizeof(float)*MAXMETERPTS * MAXRX];
        buf_len = sizeof(buf);

        ret = read_block(&priv->meter_port, (unsigned char *) buf, buf_len);

        if (ret != buf_len)
        {
            ret = -RIG_EIO;
        }

        /* copy payload back to client space */
        memcpy((void *) label, buf, sizeof(int));
        memcpy((void *) data,  buf + sizeof(int), npts * sizeof(float));

    }
    else
    {
        /* IPC */
        buf_len = sizeof(int);
        ret = read_block(&priv->meter_port, (unsigned char *) label, buf_len);

        if (ret != buf_len)
        {
            ret = -RIG_EIO;
        }

        if (ret < 0)
        {
            return ret;
        }

        buf_len = (int) sizeof(float) * npts;

        if (sizeof(float) != 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: sizeof(float)!=4, instead = %d\n", __func__,
                      (int)sizeof(float));
            return -RIG_EINTERNAL;
        }

        ret = read_block(&priv->meter_port, (unsigned char *) data, buf_len);

        if (ret != buf_len)
        {
            ret = -RIG_EIO;
        }

        if (ret < 0)
        {
            return ret;
        }
    }

    return ret;
}


/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int dttsp_set_conf(RIG *rig, token_t token, const char *val)
{
    struct dttsp_priv_data *priv;
    struct rig_state *rs;

    rs = &rig->state;
    priv = (struct dttsp_priv_data *)rs->priv;

    switch (token)
    {
    case TOK_TUNER_MODEL:
        priv->tuner_model = atoi(val);
        break;

    case TOK_SAMPLE_RATE:
        priv->sample_rate = atoi(val);
        break;

    default:

        /* if it's not for the dttsp backend, maybe it's for the tuner */
        if (priv->tuner)
        {
            return rig_set_conf(priv->tuner, token, val);
        }
        else
        {
            return -RIG_EINVAL;
        }
    }

    return RIG_OK;
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int dttsp_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct dttsp_priv_data *priv;
    struct rig_state *rs;

    rs = &rig->state;
    priv = (struct dttsp_priv_data *)rs->priv;

    switch (token)
    {
    case TOK_TUNER_MODEL:
        SNPRINTF(val, val_len, "%u", priv->tuner_model);
        break;

    case TOK_SAMPLE_RATE:
        SNPRINTF(val, val_len, "%d", priv->sample_rate);
        break;

    default:

        /* if it's not for the dttsp backend, maybe it's for the tuner */
        if (priv->tuner)
        {
            return rig_get_conf(priv->tuner, token, val);
        }
        else
        {
            return -RIG_EINVAL;
        }
    }

    return RIG_OK;
}

int dttsp_get_conf(RIG *rig, token_t token, char *val)
{
    return dttsp_get_conf2(rig, token, val, 128);
}

int dttsp_init(RIG *rig)
{
    struct dttsp_priv_data *priv;
    const char *cmdpath;
    char *p;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig->state.priv = (struct dttsp_priv_data *)calloc(1,
                      sizeof(struct dttsp_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->tuner = NULL;
    priv->tuner_model = RIG_MODEL_DUMMY;
    priv->IF_center_freq = 0;


    p = getenv("SDR_DEFRATE");

    if (p)
    {
        priv->sample_rate = atoi(p);
    }
    else
    {
        priv->sample_rate = DEFAULT_SAMPLE_RATE;
    }

    cmdpath = getenv("SDR_PARMPATH");

    if (!cmdpath)
        cmdpath = rig->state.rigport.type.rig == RIG_PORT_UDP_NETWORK ?
                  DEFAULT_DTTSP_CMD_NET_ADDR : DEFAULT_DTTSP_CMD_PATH;

    strncpy(rig->state.rigport.pathname, cmdpath, HAMLIB_FILPATHLEN - 1);

    return RIG_OK;
}


int dttsp_open(RIG *rig)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;
    int ret;
    char *p;
    char *meterpath;


    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /*
     * prevent l8ps
     */
    if (priv->tuner_model == RIG_MODEL_DTTSP ||
            priv->tuner_model == RIG_MODEL_DTTSP_UDP)
    {
        return -RIG_ECONF;
    }

    priv->tuner = rig_init(priv->tuner_model);

    if (!priv->tuner)
    {
        /* FIXME: wrong rig model? */
        return -RIG_ENOMEM;
    }

    ret = rig_open(priv->tuner);

    if (ret != RIG_OK)
    {
        rig_cleanup(priv->tuner);
        priv->tuner = NULL;
        return ret;
    }

    /* open DttSP meter pipe */
    priv->meter_port.post_write_delay = rig->state.rigport.post_write_delay;
    priv->meter_port.timeout = rig->state.rigport.timeout;
    priv->meter_port.retry = rig->state.rigport.retry;

    p = getenv("SDR_METERPATH");

    if (!p)
    {
        meterpath = priv->meter_port.pathname;
        SNPRINTF(meterpath, HAMLIB_FILPATHLEN, "%s", rig->state.rigport.pathname);

        if (rig->state.rigport.type.rig == RIG_PORT_UDP_NETWORK)
        {
            p = strrchr(meterpath, ':');

            if (p)
            {
                strcpy(p + 1, "19003");
            }
            else
            {
                strcat(meterpath, ":19003");
            }

            p = meterpath;
        }
        else
        {
            p = strrchr(meterpath, '/');

            if (p)
            {
                strcpy(p + 1, "SDRmeter");
            }
        }
    }

    if (!p)
    {
        /* disabled */
        priv->meter_port.fd = -1;
    }
    else
    {
        priv->meter_port.type.rig = rig->state.rigport.type.rig;
        ret = port_open(&priv->meter_port);

        if (ret < 0)
        {
            return ret;
        }
    }


    /* TODO:
     *    copy priv->tuner->rx_range/tx_range to rig->state
     */

#if 1
    rig->state.has_set_func  |= priv->tuner->state.has_set_func;
    rig->state.has_get_func  |= priv->tuner->state.has_get_func;
    rig->state.has_set_level |= priv->tuner->state.has_set_level;
    rig->state.has_get_level |= priv->tuner->state.has_get_level;
    rig->state.has_set_parm  |= priv->tuner->state.has_set_parm;
    rig->state.has_get_parm  |= priv->tuner->state.has_get_parm;
#endif


    /* Because model dummy has funky init value */
    if (priv->tuner_model == RIG_MODEL_DUMMY)
    {
        dttsp_set_freq(rig, RIG_VFO_CURR, priv->IF_center_freq);
    }

    dttsp_set_func(rig, RIG_VFO_CURR, RIG_FUNC_MUTE, 0);

    return RIG_OK;
}


int dttsp_close(RIG *rig)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    port_close(&priv->meter_port, priv->meter_port.type.rig);
    rig_close(priv->tuner);

    return RIG_OK;
}

int dttsp_cleanup(RIG *rig)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (priv && priv->tuner)
    {
        rig_cleanup(priv->tuner);
    }

    priv->tuner = NULL;

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}


/*
 * rig_set_freq is a good candidate for the GNUradio GUI setFrequency callback?
 */
int dttsp_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;
    freq_t tuner_freq;
    int ret;
    char fstr[20];
    char buf[32];
    shortfreq_t max_delta;
    freq_t freq_offset;

    max_delta = priv->sample_rate / 2 - kHz(2);

    sprintf_freq(fstr, sizeof(fstr), freq);
    rig_debug(RIG_DEBUG_TRACE, "%s called: %s %s\n",
              __func__, rig_strvfo(vfo), fstr);

    ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);

    if (ret != RIG_OK)
    {
        return ret;
    }

    freq_offset = freq - tuner_freq;

    /* TODO: take into account the mode width and the IF dead zone */
    if (fabs(freq_offset) > max_delta)
    {
        tuner_freq = priv->IF_center_freq + freq - kHz(6);
        ret = rig_set_freq(priv->tuner, RIG_VFO_CURR, tuner_freq);

        if (ret != RIG_OK)
        {
            return ret;
        }

        /* reread the tuner freq because some rigs can't tune to exact frequency.
         * The rx_delta_f will correct that */
        ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    priv->rx_delta_f = freq - tuner_freq;

    sprintf_freq(fstr, sizeof(fstr), tuner_freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: tuner=%s, rx_delta=%d Hz\n",
              __func__, fstr, priv->rx_delta_f);

    /* setRxFrequenc */
    SNPRINTF(buf, sizeof(buf), "setOsc %d\n", priv->rx_delta_f);
    ret = send_command(rig, buf, strlen(buf));

    return ret;
}


int dttsp_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;
    freq_t tuner_freq;
    int ret;

    ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);

    if (ret != RIG_OK)
    {
        return ret;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    *freq = tuner_freq - priv->rx_delta_f;

    return RIG_OK;
}

static enum dttsp_mode_e rmode2dttsp(rmode_t mode)
{
    int i;

    for (i = 0; i < HAMLIB_VS_DTTSP_MODES_COUNT; i++)
    {
        if (hamlib_vs_dttsp_modes[i].hamlib_mode == mode)
        {
            return hamlib_vs_dttsp_modes[i].dttsp_mode;
        }
    }

    return 0;
}

/*
 * WIP
 */
int dttsp_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char buf[32];
    int ret = RIG_OK;
    int filter_l, filter_h;

    /* DttSP set mode */

    SNPRINTF(buf, sizeof(buf), "setMode %d\n", rmode2dttsp(mode));
    ret = send_command(rig, buf, strlen(buf));

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n",
              __func__, buf);

    if (ret != RIG_OK || RIG_PASSBAND_NOCHANGE == width) { return ret; }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    sprintf_freq(buf, sizeof(buf), width);
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n",
              __func__, rig_strrmode(mode), buf);

    switch (mode)
    {
    case RIG_MODE_USB:
    case RIG_MODE_CW:
        filter_l = 10;
        filter_h = width;
        break;

    case RIG_MODE_LSB:
    case RIG_MODE_CWR:
        filter_l = -width;
        filter_h = -10;
        break;

    case RIG_MODE_AM:
    case RIG_MODE_SAM:
    case RIG_MODE_FM:
    case RIG_MODE_DSB:
        filter_l = -width / 2;
        filter_h = width / 2;
        break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "setFilter %d %d\n", filter_l, filter_h);
    ret = send_command(rig, buf, strlen(buf));

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n",
              __func__, buf);

    return ret;
}


static int agc_level2dttsp(enum agc_level_e agc)
{
    switch (agc)
    {
    case RIG_AGC_OFF: return 0;

    case RIG_AGC_SLOW: return 2;

    case RIG_AGC_MEDIUM: return 3;

    case RIG_AGC_FAST: return 4;

    default:
        return 0;
    }

    return 0;
}
/*
 */
int dttsp_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;
    int ret = RIG_OK;
    char buf[32];

    switch (level)
    {
    case RIG_LEVEL_AGC:
        SNPRINTF(buf, sizeof(buf), "setRXAGC %d\n", agc_level2dttsp(val.i));
        ret = send_command(rig, buf, strlen(buf));
        break;

    default:
        rig_debug(RIG_DEBUG_TRACE, "%s: level %s, try tuner\n",
                  __func__, rig_strlevel(level));
        return rig_set_level(priv->tuner, vfo, level, val);
    }

    return ret;
}


int dttsp_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;
    int ret = RIG_OK;
    char buf[32];
    float rxm[MAXRX][RXMETERPTS];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              rig_strlevel(level));

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
    case RIG_LEVEL_STRENGTH:
        SNPRINTF(buf, sizeof(buf), "reqRXMeter %d\n", getpid());
        ret = send_command(rig, buf, strlen(buf));

        if (ret < 0)
        {
            return ret;
        }

        ret = fetch_meter(rig, (int *)buf, (float *)rxm, MAXRX * RXMETERPTS);

        if (ret < 0)
        {
            return ret;
        }

        val->i = (int)rxm[0][0];

        if (level == RIG_LEVEL_STRENGTH)
        {
            val->i = (int)rig_raw2val(val->i, &rig->state.str_cal);
        }

        ret = RIG_OK;
        break;

    default:
        rig_debug(RIG_DEBUG_TRACE, "%s: level %s, try tuner\n",
                  __func__, rig_strlevel(level));
        return rig_get_level(priv->tuner, vfo, level, val);
    }

    return ret;
}

int dttsp_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;
    char buf[32];
    const char *cmd;
    int ret;

    status = status ? 1 : 0;

    switch (func)
    {
    case RIG_FUNC_MUTE:
        cmd = "setRunState";
        status = status ? 0 : 2;
        break;

    case RIG_FUNC_NB:
        cmd = "setNB";
        break;

    case RIG_FUNC_ANF:
        cmd = "setANF";
        break;

    case RIG_FUNC_NR:
        cmd = "setNR";
        break;

    default:
        rig_debug(RIG_DEBUG_TRACE, "%s: func %s, try tuner\n",
                  __func__, rig_strfunc(func));
        return rig_set_func(priv->tuner, vfo, func, status);
    }

    SNPRINTF(buf, sizeof(buf), "%s %d\n", cmd, status);
    ret = send_command(rig, buf, strlen(buf));

    return ret;
}


/*
 * TODO
 */
int dttsp_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    return -RIG_ENIMPL;
}


int dttsp_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENIMPL;
}


int dttsp_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    struct dttsp_priv_data *priv = (struct dttsp_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: ant %u, try tuner\n",
              __func__, ant);

    return rig_set_ant(priv->tuner, vfo, ant, option);
}

