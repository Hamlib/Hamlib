/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft757gx.c - (C) Stephane Fillod 2004
 *             (C) Nate Bargmann 2008
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-757GX/FT-757GXII using the
 * "CAT" interface box (FIF-232C) or similar.
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


/*
 * TODO
 *
 * 1. Allow cached reads
 * 2. set_mem/get_mem, vfo_op, get_channel, set_split/get_split,
 *    set_func/get_func
 *
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>     /* String function definitions */

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "ft757gx.h"

#define FT757GX_VFOS (RIG_VFO_A|RIG_VFO_B)

/* Backend function prototypes.  These map to frontend functions. */
static int ft757_init(RIG *rig);
static int ft757_cleanup(RIG *rig);
static int ft757_open(RIG *rig);

static int ft757gx_get_conf(RIG *rig, token_t token, char *val);
static int ft757gx_set_conf(RIG *rig, token_t token, const char *val);

static int ft757_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft757_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft757gx_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft757_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                          pbwidth_t width); /* select mode */
static int ft757_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                          pbwidth_t *width); /* get mode */

static int ft757_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
static int ft757_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */
static int ft757gx_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

static int ft757_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft757_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);


/* Private helper function prototypes */
static int ft757_get_update_data(RIG *rig);
static int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width);
static int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width);


/*
 * future - private data
 *
 */
struct ft757_priv_data
{
    unsigned char pacing;       /* pacing value */
    unsigned char
    current_vfo;  /* active VFO from last cmd , can be either RIG_VFO_A or RIG_VFO_B only */
    unsigned char
    update_data[FT757GX_STATUS_UPDATE_DATA_LENGTH];   /* returned data */
    double curfreq;         /* for fake get_freq on 757G */
    char fakefreq;          /* 0=no fake, 1=fake get freq */
};

#define TOKEN_BACKEND(t) (t)
#define TOK_FAKEFREQ TOKEN_BACKEND(1)

const struct confparams ft757gx_cfg_params[] =
{
    {
        TOK_FAKEFREQ, "fakefreq", "Fake get-freq", "Fake getting freq",
        "0", RIG_CONF_CHECKBUTTON
    },
    { RIG_CONF_END, NULL, }
};

/*
 * ft757gx rig capabilities.
 * Also this struct is READONLY!
 */
const struct rig_caps ft757gx_caps =
{
    RIG_MODEL(RIG_MODEL_FT757),
    .model_name =       "FT-757GX",
    .mfg_name =     "Yaesu",
    .version =      "20220429.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_MOBILE,
    .ptt_type =     RIG_PTT_NONE,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 2,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      FT757GX_WRITE_DELAY,
    .post_write_delay = FT757GX_POST_WRITE_DELAY,
    .timeout =      FT757GX_DEFAULT_READ_TIMEOUT,
    .retry =        10,
    .has_get_func =     RIG_FUNC_NONE,
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    RIG_LEVEL_BAND_SELECT,
    .has_set_level =    RIG_LEVEL_BAND_SELECT,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =      Hz(9999),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,
    .chan_list =        { RIG_CHAN_END, },  /* FIXME: memory channel list:20 */

    .rx_range_list1 =   { RIG_FRNG_END, },  /* FIXME: enter region 1 setting */

    .tx_range_list1 =   { RIG_FRNG_END, },

    .rx_range_list2 = { {
            .startf = kHz(500), .endf = 29999990,
            .modes = FT757GX_ALL_RX_MODES, .low_power = -1, .high_power = -1
        },
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list2 =   { {kHz(1500), 1999900, FT757GX_ALL_TX_MODES, .low_power = 5000, .high_power = 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = kHz(3500), 3999900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = kHz(7000), 7499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(10), 10499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(14), 14499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(18), 18499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(21), 21499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = kHz(24500), 24999900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(28), 29999900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        RIG_FRNG_END,
    },


    .tuning_steps = {
        {FT757GX_ALL_RX_MODES, 10},
        {FT757GX_ALL_RX_MODES, 100},
        RIG_TS_END,
    },

    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .priv =         NULL,           /* private data */

    .rig_init =     ft757_init,
    .rig_cleanup =      ft757_cleanup,
    .rig_open =     ft757_open,     /* port opened */
    .rig_close =        NULL,           /* port closed */

    .set_freq =     ft757_set_freq,     /* set freq */
    .get_freq =     ft757gx_get_freq,   /* get freq */
    .set_mode =     NULL,           /* set mode */
    .get_mode =     NULL,           /* get mode */
    .get_vfo =      ft757gx_get_vfo,    /* set vfo */
    .set_vfo =      ft757_set_vfo,      /* set vfo */

    .set_ptt =      NULL,           /* set ptt */
    .get_ptt =      NULL,           /* get ptt */

    .cfgparams =        ft757gx_cfg_params,
    .set_conf =     ft757gx_set_conf,
    .get_conf =     ft757gx_get_conf,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * ft757gx2 rig capabilities.
 * Also this struct is READONLY!
 */
const struct rig_caps ft757gx2_caps =
{
    RIG_MODEL(RIG_MODEL_FT757GXII),
    .model_name =       "FT-757GXII",
    .mfg_name =     "Yaesu",
    .version =      "20200325.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_MOBILE,
    .ptt_type =     RIG_PTT_SERIAL_DTR, /* CAT port pin 4 per manual */
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 2,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      FT757GX_WRITE_DELAY,
    .post_write_delay = FT757GX_POST_WRITE_DELAY,
    .timeout =      FT757GX_DEFAULT_READ_TIMEOUT,
    .retry =        10,
    .has_get_func =     RIG_FUNC_LOCK,
    .has_set_func =     RIG_FUNC_LOCK,
    .has_get_level =    RIG_LEVEL_RAWSTR,
    .has_set_level =    RIG_LEVEL_BAND_SELECT,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .vfo_ops =      RIG_OP_CPY | RIG_OP_FROM_VFO | RIG_OP_TO_VFO |
    RIG_OP_UP | RIG_OP_DOWN,
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list = {
        {   0,  9, RIG_MTYPE_MEM, FT757_MEM_CAP },
        RIG_CHAN_END
    },

    .rx_range_list1 =   { RIG_FRNG_END, },  /* FIXME: enter region 1 setting */

    .tx_range_list1 =   { RIG_FRNG_END, },

    .rx_range_list2 = { {
            .startf =   kHz(150),
            .endf =     29999990,
            .modes =    FT757GX_ALL_RX_MODES,
            .low_power =    -1,
            .high_power =   -1
        },
        RIG_FRNG_END,
    }, /* rx range */

    /* FIXME: 10m is "less" and AM is 25W carrier */
    .tx_range_list2 = { {kHz(1500), 1999900, FT757GX_ALL_TX_MODES, .low_power = 5000, .high_power = 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = kHz(3500), 3999900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = kHz(7000), 7499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(10), 10499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(14), 14499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(18), 18499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(21), 21499900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = kHz(24500), 24999900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        {.startf = MHz(28), 29999900, FT757GX_ALL_TX_MODES, 5000, 100000, FT757GX_VFOS, RIG_ANT_1},

        RIG_FRNG_END,
    },


    .tuning_steps = {
        {FT757GX_ALL_RX_MODES, 10},
        RIG_TS_END,
    },

    /* mode/filter list, .remember =  order matters! */
    .filters = {
        {RIG_MODE_SSB | RIG_MODE_CW,  kHz(2.7)},
        {RIG_MODE_CW,  Hz(600)},    /* narrow */
        {RIG_MODE_AM,  kHz(6)},
        {RIG_MODE_FM,  kHz(15)},

        RIG_FLT_END,
    },

    .str_cal =      FT757GXII_STR_CAL,

    .priv =         NULL,       /* private data */

    .rig_init =     ft757_init,
    .rig_cleanup =      ft757_cleanup,
    .rig_open =     ft757_open, /* port opened */
    .rig_close =        NULL,       /* port closed */

    .set_freq =     ft757_set_freq, /* set freq */
    .get_freq =     ft757_get_freq, /* get freq */
    .set_mode =     ft757_set_mode, /* set mode */
    .get_mode =     ft757_get_mode, /* get mode */
    .set_vfo =      ft757_set_vfo,  /* set vfo */
    .get_vfo =      ft757_get_vfo,  /* get vfo */
    .get_level =        ft757_get_level,
    .get_ptt =      ft757_get_ptt,  /* get ptt */
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * _init
 *
 */

static int ft757_init(RIG *rig)
{
    struct ft757_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct ft757_priv_data *) calloc(1,
                      sizeof(struct ft757_priv_data));

    if (!rig->state.priv)     /* whoops! memory shortage! */
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->curfreq = 1e6;

    /* TODO: read pacing from preferences */

    priv->pacing =
        FT757GX_PACING_DEFAULT_VALUE;   /* set pacing to minimum for now */
    priv->current_vfo =  RIG_VFO_A;            /* default to VFO_A ? */

    return RIG_OK;
}


/*
 * ft757_cleanup routine
 * the serial port is closed by the frontend
 */

static int ft757_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

/*
 * ft757_open  routine
 *
 */

static int ft757_open(RIG *rig)
{
    int retval;
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    priv->fakefreq = 1; // turn this on by default

    /* FT757GX has a write-only serial port: don't try to read status data */
    if (rig->caps->rig_model == RIG_MODEL_FT757)
    {
        memset(priv->update_data, 0, FT757GX_STATUS_UPDATE_DATA_LENGTH);
        retval = rig_set_vfo(rig, RIG_VFO_A);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_set_vfo error: %s\n", __func__,
                      rigerror(retval));
        }
    }
    else
    {
        /* read back the 75 status bytes from FT757GXII */
        int retval = ft757_get_update_data(rig);

        if (retval < 0)
        {
            memset(priv->update_data, 0, FT757GX_STATUS_UPDATE_DATA_LENGTH);
            return retval;
        }
    }

    return RIG_OK;
}


/*
 * This command only functions when operating on a vfo.
 * TODO: test Status Update Byte 1
 *
 */

static int ft757_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0a};

    rig_debug(RIG_DEBUG_VERBOSE, "%s called. Freq=%"PRIfreq"\n", __func__, freq);

    /* fill in first four bytes */
    to_bcd(cmd, freq / 10, BCD_LEN);

    priv->curfreq = freq;
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


static int ft757_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0c};

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: mode = %s, width = %d\n",
              __func__, rig_strrmode(mode), (int)width);

    if (mode == RIG_MODE_NONE)
    {
        return -RIG_EINVAL;
    }

    /* fill in p1 */
    cmd[3] = mode2rig(rig, mode, width);

    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


static int ft757gx_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called. fakefreq=%d\n", __func__,
              priv->fakefreq);

    if (priv->fakefreq)   // only return last freq set when fakeit is turned on
    {
        *freq = priv->curfreq;
        return RIG_OK;
    }

    return RIG_ENAVAIL;
}

/*
 * Return Freq
 */

static int ft757_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    retval = ft757_get_update_data(rig);    /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    /* grab freq (little endian format) and convert */
    switch (vfo)
    {
    case RIG_VFO_CURR:
        *freq = 10 * from_bcd(priv->update_data + STATUS_CURR_FREQ, BCD_LEN);
        break;

    case RIG_VFO_A:
        *freq = 10 * from_bcd(priv->update_data + STATUS_VFOA_FREQ, BCD_LEN);
        break;

    case RIG_VFO_B:
        *freq = 10 * from_bcd(priv->update_data + STATUS_VFOB_FREQ, BCD_LEN);
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong VFO */
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s returning: Freq=%"PRIfreq"\n", __func__,
              *freq);
    return RIG_OK;
}


static int ft757_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    retval = ft757_get_update_data(rig);    /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    switch (vfo)
    {
    case RIG_VFO_CURR:
        retval = rig2mode(rig, priv->update_data[STATUS_CURR_MODE], mode, width);
        break;

    case RIG_VFO_A:
        retval = rig2mode(rig, priv->update_data[STATUS_VFOA_MODE], mode, width);
        break;

    case RIG_VFO_B:
        retval = rig2mode(rig, priv->update_data[STATUS_VFOB_MODE], mode, width);
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong VFO */
    }

    return retval;
}


/*
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */

static int ft757_set_vfo(RIG *rig, vfo_t vfo)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x05};
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;

    ENTERFUNC;

    switch (vfo)
    {
    case RIG_VFO_CURR:
        RETURNFUNC(RIG_OK);

    case RIG_VFO_A:
        cmd[3] = 0x00;          /* VFO A */
        break;

    case RIG_VFO_B:
        cmd[3] = 0x01;          /* VFO B */
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);     /* sorry, wrong VFO */
    }

    priv->current_vfo = vfo;

    RETURNFUNC(write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH));
}

static int ft757gx_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;
    // we'll just use the cached vfo for the 757GX since we can't read it
    *vfo = priv->current_vfo;
    return RIG_OK;
}

static int ft757_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    retval = ft757_get_update_data(rig);    /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    if (priv->update_data[0] & 0x10)
    {
        (*vfo) = RIG_VFO_MEM;
    }
    else if (priv->update_data[0] & 0x08)
    {
        (*vfo) = RIG_VFO_B;
    }
    else
    {
        (*vfo) = RIG_VFO_A;
    }

    return RIG_OK;
}


static int ft757_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    retval = ft757_get_update_data(rig);    /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    *ptt = (priv->update_data[0] & 0x20) ?  RIG_PTT_ON : RIG_PTT_OFF;
    return RIG_OK;
}


static int ft757_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x01, 0x10};
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (level != RIG_LEVEL_RAWSTR)
    {
        return -RIG_EINVAL;
    }

    rig_flush(&rig->state.rigport);

    /* send READ STATUS(Meter only) cmd to rig  */
    retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    /* read back the 1 byte */
    retval = read_block(&rig->state.rigport, cmd, 1);

    if (retval != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: read meter failed %d.\n",
                  __func__, retval);

        return retval < 0 ? retval : -RIG_EIO;
    }

    val->i = cmd[0];

    return RIG_OK;
}


/*
 * private helper function. Retrieves update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 *
 * need to use this when doing ft757_get_* stuff
 */

static int ft757_get_update_data(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x10};
    struct ft757_priv_data *priv = (struct ft757_priv_data *)rig->state.priv;
    int retval = 0;
    long nbtries;
    /* Maximum number of attempts to ask/read the data. */
    int maxtries = rig->state.rigport.retry ;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called Timeout=%d ms, Retry=%d\n",
              __func__, rig->state.rigport.timeout, maxtries);

    /* At least on one model, returns erraticaly a timeout. Increasing the timeout
    does not fix things. So we restart the read from scratch, it works most of times. */
    for (nbtries = 0 ; nbtries < maxtries ; nbtries++)
    {
        rig_flush(&rig->state.rigport);

        /* send READ STATUS cmd to rig  */
        retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

        if (retval < 0)
        {
            return retval;
        }

        /* read back the 75 status bytes */
        retval = read_block(&rig->state.rigport,
                            priv->update_data,
                            FT757GX_STATUS_UPDATE_DATA_LENGTH);

        if (retval == FT757GX_STATUS_UPDATE_DATA_LENGTH)
        {
            break ;
        }

        rig_debug(RIG_DEBUG_ERR,
                  "%s: read update_data failed, %d octets of %d read, retry %ld out of %d\n",
                  __func__, retval, FT757GX_STATUS_UPDATE_DATA_LENGTH,
                  nbtries, maxtries);
        /* The delay is quadratic. */
        hl_usleep(nbtries * nbtries * 1000000);
    }

    if (retval != FT757GX_STATUS_UPDATE_DATA_LENGTH)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: read update_data failed, %d octets of %d read.\n",
                  __func__, retval, FT757GX_STATUS_UPDATE_DATA_LENGTH);

        return retval < 0 ? retval : -RIG_EIO;
    }

    return RIG_OK;
}


static int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width)
{
    int md;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    /*
     * translate mode from generic to ft757 specific
     */
    switch (mode)
    {
    case RIG_MODE_AM:
        md = MODE_AM;
        break;

    case RIG_MODE_USB:
        md = MODE_USB;
        break;

    case RIG_MODE_LSB:
        md = MODE_LSB;
        break;

    case RIG_MODE_FM:
        md = MODE_FM;
        break;

    case RIG_MODE_CW:
        if (RIG_PASSBAND_NOCHANGE == width
                || width == RIG_PASSBAND_NORMAL
                || width >= rig_passband_normal(rig, mode))
        {
            md = MODE_CWW;
        }
        else
        {
            md = MODE_CWN;
        }

        break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    return md;
}


static int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    /*
     * translate mode from ft757 specific to generic
     */
    switch (md)
    {
    case MODE_AM:
        *mode = RIG_MODE_AM;
        break;

    case MODE_USB:
        *mode = RIG_MODE_USB;
        break;

    case MODE_LSB:
        *mode = RIG_MODE_LSB;
        break;

    case MODE_FM:
        *mode = RIG_MODE_FM;
        break;

    case MODE_CWW:
    case MODE_CWN:
        *mode = RIG_MODE_CW;
        break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    if (md == MODE_CWN)
    {
        *width = rig_passband_narrow(rig, *mode);
    }
    else
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return RIG_OK;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int ft757gx_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct ft757_priv_data *priv;
    struct rig_state *rs;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called.\n", __func__);

    rs = &rig->state;
    priv = (struct ft757_priv_data *)rs->priv;


    switch (token)
    {
    case TOK_FAKEFREQ:
        SNPRINTF(val, val_len, "%d", priv->fakefreq);
        break;

    default:
        *val = 0;
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int ft757gx_get_conf(RIG *rig, token_t token, char *val)
{
    return ft757gx_get_conf2(rig, token, val, 128);
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int ft757gx_set_conf(RIG *rig, token_t token, const char *val)
{
    struct ft757_priv_data *priv;
    struct rig_state *rs;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called. val=%s\n", __func__, val);

    rs = &rig->state;
    priv = (struct ft757_priv_data *)rs->priv;

    switch (token)
    {
    case TOK_FAKEFREQ:
        priv->fakefreq = 0; // should only be 0 or 1 -- default to 0

        if (val[0] != '0')
        {
            priv->fakefreq = 1;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: fakefreq=%d\n", __func__, priv->fakefreq);

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}
