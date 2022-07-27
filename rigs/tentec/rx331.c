/*
 *  Hamlib Tentec backend - RX331 description
 *  Copyright (c) 2010 by Berndt Josef Wulf
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
#include <string.h>

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "num_stdio.h"
#include "token.h"

#include "rx331.h"

static const struct confparams rx331_cfg_params[] =
{
    {
        TOK_RIGID, "receiver_id", "receiver ID", "receiver ID",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 99, 1 } }
    },
    { RIG_CONF_END, NULL, }
};

#define RX331_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_DSB|\
            RIG_MODE_AM|RIG_MODE_AMS)

#define RX331_FUNCS (RIG_FUNC_NB)

#define RX331_LEVELS (RIG_LEVEL_STRENGTH| \
            RIG_LEVEL_RF|RIG_LEVEL_IF| \
                    RIG_LEVEL_NOTCHF|RIG_LEVEL_SQL| \
            RIG_LEVEL_CWPITCH|RIG_LEVEL_AGC| \
            RIG_LEVEL_ATT|RIG_LEVEL_PREAMP)

#define RX331_ANTS (RIG_ANT_1)

#define RX331_PARMS (RIG_PARM_NONE)

#define RX331_VFO (RIG_VFO_A)

#define RX331_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

/* TODO: levels.. */
#define RX331_MEM_CAP { \
    .freq = 1,      \
    .mode = 1,      \
    .width = 1,     \
}


static int rx331_init(RIG *rig);
static int rx331_cleanup(RIG *rig);
static int rx331_set_conf(RIG *rig, token_t token, const char *val);
static int rx331_get_conf(RIG *rig, token_t token, char *val);
static int rx331_open(RIG *rig);
static int rx331_close(RIG *rig);
static int rx331_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int rx331_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int rx331_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int rx331_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int rx331_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int rx331_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int rx331_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
static const char *rx331_get_info(RIG *rig);

/*
 * RX331 receiver capabilities.
 *
 * Protocol is documented at
 *      http://radio.tentec.com/downloads/receivers/RX331
 *
 * TODO: from/to memory
 */
const struct rig_caps rx331_caps =
{
    RIG_MODEL(RIG_MODEL_RX331),
    .model_name = "RX-331",
    .mfg_name =  "Ten-Tec",
    .version =  "20200911.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  75,
    .serial_rate_max =  38400,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  400,
    .retry =  3,

    .has_get_func =  RX331_FUNCS,
    .has_set_func =  RX331_FUNCS,
    .has_get_level =  RX331_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(RX331_LEVELS),
    .has_get_parm =  RX331_PARMS,
    .has_set_parm =  RX331_PARMS,
    .level_gran =  {},      /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END },
    .attenuator =   { 15, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  kHz(2),
    .targetable_vfo =  RIG_TARGETABLE_NONE,
    .transceive =  RIG_TRN_OFF,
    .vfo_ops =  RX331_VFO_OPS,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list = {
        { 1, 100, RIG_MTYPE_MEM, RX331_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(0), MHz(30), RX331_MODES, -1, -1, RX331_VFO, RX331_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list1 =  {
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(0), MHz(30), RX331_MODES, -1, -1, RX331_VFO, RX331_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {RX331_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RX331_MODES, kHz(3.2)},
        {RX331_MODES, Hz(100)},
        {RX331_MODES, kHz(16)},
        {RX331_MODES, 0},
        RIG_FLT_END,
    },

    .priv = (void *)NULL,

    .cfgparams = rx331_cfg_params,

    .rig_init =   rx331_init,
    .rig_cleanup = rx331_cleanup,
    .rig_open =   rx331_open,
    .rig_close =  rx331_close,
    .set_conf =   rx331_set_conf,
    .get_conf =   rx331_get_conf,
    .set_freq =   rx331_set_freq,
    .get_freq =   rx331_get_freq,
    .set_mode =   rx331_set_mode,
    .get_mode =   rx331_get_mode,
    .set_level =  rx331_set_level,
    .get_level =  rx331_get_level,
    .vfo_op =     rx331_vfo_op,
    .get_info =   rx331_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

#define BUFSZ 128

#define EOM "\015"  /* CR */

#define RX331_AM  '1'
#define RX331_FM  '2'
#define RX331_CW  '3'
#define RX331_CW1 '4'
#define RX331_ISB '5'
#define RX331_LSB '6'
#define RX331_USB '7'
#define RX331_SAM '8'

#define RX331_PREAMP_OFF    0x1
#define RX331_PREAMP_ON     0x2
#define RX331_ATT_OFF       0x1
#define RX331_ATT_ON        0x3

#define RX331_AGC_FAST      0x1
#define RX331_AGC_MEDIUM    0x2
#define RX331_AGC_SLOW      0x3
#define RX331_AGC_PROG      0x4

#define REPORT_FREQ     "TF"EOM
#define REPORT_MODEFILTER   "TDI"EOM
#define REPORT_FIRM     "V"EOM
#define REPORT_STRENGTH     "X"EOM
#define REPORT_AGC      "TM"EOM
#define REPORT_ATT      "TK"EOM
#define REPORT_PREAMP       "TK"EOM
#define REPORT_RF       "TA"EOM
#define REPORT_IF       "TP"EOM
#define REPORT_SQL      "TQ"EOM
#define REPORT_CWPITCH      "TB"EOM
#define REPORT_NOTCHF       "TN"EOM

/*
 * rx331_transaction
 * read exactly data_len bytes
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 */
static int rx331_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                             int *data_len)
{
    int rig_id;
    int retval;
    char str[BUFSZ];
    char fmt[16];
    struct rig_state *rs;
    struct rx331_priv_data *priv = (struct rx331_priv_data *)rig->state.priv;

    rs = &rig->state;

    rig_flush(&rs->rigport);

    num_snprintf(str, BUFSZ, "$%u%s", priv->receiver_id, cmd);
    retval = write_block(&rs->rigport, (unsigned char *) str, strlen(str));

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* no data expected, TODO: flush input? */
    if (!data || !data_len)
    {
        return RIG_OK;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, EOM, 1, 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    SNPRINTF(fmt, sizeof(fmt) - 1, "%%i%%%ds", BUFSZ);
    sscanf(data + 1, fmt, &rig_id, data);

    if (rig_id != priv->receiver_id)
    {
        return -RIG_EPROTO;
    }

    *data_len = retval;

    return RIG_OK;
}

/*
 * rx331_init:
 * Basically, it just sets up *priv
 */
int rx331_init(RIG *rig)
{
    struct rx331_priv_data *priv;

    rig->state.priv = (struct rx331_priv_data *)calloc(1, sizeof(
                          struct rx331_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct rx331_priv_data));

    return RIG_OK;
}

/*
 * Tentec generic rx331_cleanup routine
 * the serial port is closed by the frontend
 */
int rx331_cleanup(RIG *rig)
{
    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

int rx331_set_conf(RIG *rig, token_t token, const char *val)
{
    struct rx331_priv_data *priv = (struct rx331_priv_data *)rig->state.priv;

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

int rx331_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct rx331_priv_data *priv = (struct rx331_priv_data *)rig->state.priv;

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

int rx331_get_conf(RIG *rig, token_t token, char *val)
{
    return rx331_get_conf2(rig, token, val, 128);
}

int rx331_open(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * rig_close
 */
int rx331_close(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int rx331_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct rx331_priv_data *priv = (struct rx331_priv_data *)rig->state.priv;

    struct rig_state *rs = &rig->state;
    int freq_len, retval;
    char freqbuf[16];

    freq_len = num_snprintf(freqbuf, sizeof(freqbuf), "$%uF%.6f" EOM,
                            priv->receiver_id, freq / 1e6);

    retval = write_block(&rs->rigport, (unsigned char *) freqbuf, freq_len);

    return retval;
}

/*
 * rx331_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int rx331_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char buf[BUFSZ];
    int buf_len;
    int retval;
    double f;

    retval = rx331_transaction(rig, REPORT_FREQ, strlen(REPORT_FREQ),
                               buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    if (buf_len < 2 || buf[0] != 'F' || num_sscanf(buf + 1, "%lf", &f) != 1)
    {
        return -RIG_EPROTO;
    }

    *freq = f * 1e6;

    return RIG_OK;
}

/*
 * rx331_set_mode
 * Assumes rig!=NULL
 */
int rx331_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct rx331_priv_data *priv = (struct rx331_priv_data *)rig->state.priv;
    struct rig_state *rs = &rig->state;
    char dmode;
    int mdbuf_len, retval;
    char mdbuf[32];

    switch (mode)
    {
    case RIG_MODE_USB:      dmode = RX331_USB; break;

    case RIG_MODE_LSB:      dmode = RX331_LSB; break;

    case RIG_MODE_CW:       dmode = RX331_CW; break;

    case RIG_MODE_FM:       dmode = RX331_FM; break;

    case RIG_MODE_AM:       dmode = RX331_AM; break;

    case RIG_MODE_AMS:      dmode = RX331_SAM; break;

    case RIG_MODE_DSB:      dmode = RX331_ISB; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        /*
         * Set DETECTION MODE and IF FILTER
         */
        mdbuf_len = num_snprintf(mdbuf, sizeof(mdbuf),  "$%uD%cI%.02f" EOM,
                                 priv->receiver_id,
                                 dmode, (float)width / 1e3);
    }
    else
    {
        /*
         * Set DETECTION MODE
         */
        mdbuf_len = num_snprintf(mdbuf, sizeof(mdbuf),  "$%uD%c" EOM, priv->receiver_id,
                                 dmode);
    }

    retval = write_block(&rs->rigport, (unsigned char *) mdbuf, mdbuf_len);

    return retval;
}

/*
 * rx331_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int rx331_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[BUFSZ];
    int buf_len;
    int retval;
    double f;

    retval = rx331_transaction(rig, REPORT_MODEFILTER,
                               strlen(REPORT_MODEFILTER), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    if (buf_len < 4 || buf[0] != 'D' || buf[2] != 'I')
    {
        return -RIG_EPROTO;
    }

    switch (buf[1])
    {
    case RX331_USB: *mode = RIG_MODE_USB; break;

    case RX331_LSB: *mode = RIG_MODE_LSB; break;

    case RX331_CW1:
    case RX331_CW:  *mode = RIG_MODE_CW; break;

    case RX331_FM:  *mode = RIG_MODE_FM; break;

    case RX331_AM:  *mode = RIG_MODE_AM; break;

    case RX331_SAM: *mode = RIG_MODE_AMS; break;

    case RX331_ISB: *mode = RIG_MODE_DSB; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unknown mode '%c'\n", __func__, buf[1]);
        return -RIG_EPROTO;
    }

    if (num_sscanf(buf + 3, "%lf", &f) != 1)
    {
        return -RIG_EPROTO;
    }

    *width = f * 1e3;

    return RIG_OK;
}


/*
 * rx331_set_level
 * Assumes rig!=NULL
 * cannot support PREAMP and ATT both at same time (make sense though)
 */
int rx331_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct rx331_priv_data *priv = (struct rx331_priv_data *)rig->state.priv;
    struct rig_state *rs = &rig->state;
    int retval = RIG_OK;
    char cmdbuf[32];

    switch (level)
    {
    case RIG_LEVEL_ATT:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "$%uK%i" EOM,
                 priv->receiver_id,
                 val.i ? RX331_ATT_ON : RX331_ATT_OFF);
        break;

    case RIG_LEVEL_PREAMP:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "$%uK%i" EOM,
                 priv->receiver_id,
                 val.i ? RX331_PREAMP_ON : RX331_PREAMP_OFF);
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_FAST: val.i = RX331_AGC_FAST; break;

        case RIG_AGC_MEDIUM:  val.i = RX331_AGC_MEDIUM; break;

        case RIG_AGC_SLOW:  val.i = RX331_AGC_SLOW; break;

        case RIG_AGC_USER:  val.i = RX331_AGC_PROG; break;

        default:
            rig_debug(RIG_DEBUG_ERR,
                      "%s: Unsupported set_level %d\n",
                      __func__, val.i);
            return -RIG_EINVAL;
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "$%uM%i" EOM,
                 priv->receiver_id, val.i);
        break;

    case RIG_LEVEL_RF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "$%uA%d" EOM, priv->receiver_id,
                 120 - (int)(val.f * 120));
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "$%uQ%d" EOM, priv->receiver_id,
                 120 - (int)(val.f * 120));
        break;

    case RIG_LEVEL_NOTCHF:
        num_snprintf(cmdbuf, sizeof(cmdbuf), "$%uN%f" EOM, priv->receiver_id,
                     ((float)val.i) / 1e3);
        break;

    case RIG_LEVEL_IF:
        num_snprintf(cmdbuf, sizeof(cmdbuf), "$%uP%f" EOM, priv->receiver_id,
                     ((float)val.i) / 1e3);
        break;

    case RIG_LEVEL_CWPITCH:
        /* only in CW mode */
        num_snprintf(cmdbuf, sizeof(cmdbuf), "$%uB%f" EOM, priv->receiver_id,
                     ((float)val.i) / 1e3);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported set_level %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));
    return retval;
}


/*
 * rx331_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int rx331_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval, lvl_len;
    double f;
    char lvlbuf[BUFSZ];

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        retval = rx331_transaction(rig, REPORT_STRENGTH,
                                   strlen(REPORT_STRENGTH), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 2 || lvlbuf[0] != 'X')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        // Range is 0-120 covering the 120dB range of the receiver

        if (num_sscanf(lvlbuf + 1, "%d", &val->i) != 1)
        {
            return -RIG_EPROTO;
        }

        val->i = val->i - 120;
        break;

    case RIG_LEVEL_AGC:
        retval = rx331_transaction(rig, REPORT_AGC,
                                   strlen(REPORT_AGC), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 0 || lvlbuf[0] != 'M')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        switch (atoi(lvlbuf + 1))
        {
        case RX331_AGC_FAST:  val->i = RIG_AGC_FAST; break;

        case RX331_AGC_MEDIUM: val->i = RIG_AGC_MEDIUM; break;

        case RX331_AGC_SLOW: val->i = RIG_AGC_SLOW; break;

        case RX331_AGC_PROG: val->i = RIG_AGC_USER; break;

        default:
            rig_debug(RIG_DEBUG_ERR,
                      "%s:Unsupported get_level %s\n",
                      __func__, rig_strlevel(level));
            return -RIG_EINVAL;
        }

        break;

    case RIG_LEVEL_ATT:
        retval = rx331_transaction(rig, REPORT_ATT,
                                   strlen(REPORT_ATT), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 0 || lvlbuf[0] != 'K')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        if (num_sscanf(lvlbuf + 1, "%i", &val->i) != 1)
        {
            return -RIG_EPROTO;
        }

        val->i = (val->i == RX331_ATT_ON);
        break;

    case RIG_LEVEL_PREAMP:
        retval = rx331_transaction(rig, REPORT_PREAMP,
                                   strlen(REPORT_PREAMP), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 0 || lvlbuf[0] != 'K')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        if (num_sscanf(lvlbuf + 1, "%i", &val->i) != 1)
        {
            return -RIG_EPROTO;
        }

        val->i = (val->i == RX331_PREAMP_ON);
        break;

    case RIG_LEVEL_RF:
        retval = rx331_transaction(rig, REPORT_RF,
                                   strlen(REPORT_RF), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 0 || lvlbuf[0] != 'A')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        if (num_sscanf(lvlbuf + 1, "%d", &val->i) != 1)
        {
            return -RIG_EPROTO;
        }

        f = val->i / 120.0;
        val->f = 1.0 - f;

        break;

    case RIG_LEVEL_IF:
        retval = rx331_transaction(rig, REPORT_IF,
                                   strlen(REPORT_IF), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 0 || lvlbuf[0] != 'P')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        if (num_sscanf(lvlbuf + 1, "%f", &val->f) != 1)
        {
            return -RIG_EPROTO;
        }

        f = val->f * 1000.0;
        val->i = (int)f;

        break;

    case RIG_LEVEL_SQL:
        retval = rx331_transaction(rig, REPORT_SQL,
                                   strlen(REPORT_SQL), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 0 || lvlbuf[0] != 'Q')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        if (num_sscanf(lvlbuf + 1, "%d", &val->i) != 1)
        {
            return -RIG_EPROTO;
        }

        f = val->i / 120.0;
        val->f = 1.0 - f;

        break;

    case RIG_LEVEL_CWPITCH:
        retval = rx331_transaction(rig, REPORT_CWPITCH,
                                   strlen(REPORT_CWPITCH), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 0 || lvlbuf[0] != 'B')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        if (num_sscanf(lvlbuf + 1, "%f", &val->f) != 1)
        {
            return -RIG_EPROTO;
        }

        f = val->f * 1000.0;
        val->i = f;

        break;

    case RIG_LEVEL_NOTCHF:
        retval = rx331_transaction(rig, REPORT_NOTCHF,
                                   strlen(REPORT_NOTCHF), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 0 || lvlbuf[0] != 'N')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        if (num_sscanf(lvlbuf + 1, "%f", &val->f) != 1)
        {
            return -RIG_EPROTO;
        }

        f = val->f * 1000.0;
        val->i = f;

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported get_level %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * rx331_vfo_op
 * Assumes rig!=NULL
 */
int rx331_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    return -RIG_ENIMPL;
}

/*
 * rx331_get_info
 * Assumes rig!=NULL
 */
const char *rx331_get_info(RIG *rig)
{
    static char buf[BUFSZ]; /* FIXME: reentrancy */
    int firmware_len = sizeof(buf), retval;

    retval = rx331_transaction(rig, REPORT_FIRM, strlen(REPORT_FIRM),
                               buf, &firmware_len);

    if ((retval != RIG_OK) || (firmware_len > 10))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG, len=%d\n",
                  __func__, firmware_len);
        return NULL;
    }

    return buf;
}

