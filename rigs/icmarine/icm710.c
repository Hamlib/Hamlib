/*
 *  Hamlib ICOM Marine backend - description of IC-M710 caps
 *  Copyright (c) 2015 by Stephane Fillod
 *  Copyright (c) 2017 by Michael Black W9MDB
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
#include <string.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include <token.h>
#include <register.h>

#include "idx_builtin.h"
#include "bandplan.h"

#include "icm710.h"
#include "icmarine.h"

#define ICM710_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY)
#define ICM710_RX_MODES (ICM710_MODES|RIG_MODE_AM)

#define ICM710_FUNC_ALL (RIG_FUNC_NB)

#define ICM710_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR)

#define ICM710_VFO_ALL (RIG_VFO_A)

#define ICM710_VFO_OPS (RIG_OP_TUNE)

#define ICM710_SCAN_OPS (RIG_SCAN_NONE)

/*
 * TODO calibrate the real values
 */
#define ICM710_STR_CAL { 2, {{ 0, -60}, { 8, 60}} }


static const struct icm710_priv_caps icm710_priv_caps =
{
    .default_remote_id = 0x01,  /* default address */
};


const struct rig_caps icm710_caps =
{
    RIG_MODEL(RIG_MODEL_IC_M710),
    .model_name = "IC-M710",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =   RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  0,
    .has_get_func =  ICM710_FUNC_ALL,
    .has_set_func =  ICM710_FUNC_ALL,
    .has_get_level =  ICM710_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ICM710_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 8 } },
    },
    .parm_gran =  {},
    .str_cal = ICM710_STR_CAL,
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICM710_VFO_OPS,
//.scan_ops =  ICM710_SCAN_OPS,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(500), MHz(30) - 100, ICM710_RX_MODES, -1, -1, ICM710_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        {kHz(1600), MHz(3) - 100, ICM710_MODES, W(60), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(4),   MHz(5) - 100, ICM710_MODES, W(60), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(6),   MHz(7) - 100, ICM710_MODES, W(60), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(8),   MHz(9) - 100, ICM710_MODES, W(60), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(12), MHz(14) - 100, ICM710_MODES, W(60), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(16), MHz(18) - 100, ICM710_MODES, W(60), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(18), MHz(20) - 100, ICM710_MODES, W(60), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(22), MHz(23) - 100, ICM710_MODES, W(60), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(25), MHz(27.500), ICM710_MODES, W(60), W(60), ICM710_VFO_ALL, RIG_ANT_1},
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(500), MHz(30) - 100, ICM710_RX_MODES, -1, -1, ICM710_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   {
        {kHz(1600), MHz(3) - 100, ICM710_MODES, W(20), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(4),   MHz(5) - 100, ICM710_MODES, W(20), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(6),   MHz(7) - 100, ICM710_MODES, W(20), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(8),   MHz(9) - 100, ICM710_MODES, W(20), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(12), MHz(14) - 100, ICM710_MODES, W(20), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(16), MHz(18) - 100, ICM710_MODES, W(20), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(18), MHz(20) - 100, ICM710_MODES, W(20), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(22), MHz(23) - 100, ICM710_MODES, W(20), W(150), ICM710_VFO_ALL, RIG_ANT_1},
        {MHz(25), MHz(27.500), ICM710_MODES, W(20), W(60), ICM710_VFO_ALL, RIG_ANT_1},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {ICM710_RX_MODES, Hz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.3)},
        {RIG_MODE_AM, kHz(14)},
        RIG_FLT_END,
    },

    .cfgparams = icm710_cfg_params,
    .set_conf = icm710_set_conf,
    .get_conf = icm710_get_conf,
    .get_conf2 = icm710_get_conf2,

    .priv = (void *)& icm710_priv_caps,
    .rig_init =  icm710_init,
    .rig_cleanup =  icm710_cleanup,
    .rig_open =  icm710_open,
    .rig_close =  icm710_close,

    .set_freq = icm710_set_freq,
    .get_freq = icm710_get_freq,
    .set_split_freq = icm710_set_tx_freq,
    .get_split_freq = icm710_get_tx_freq,
    .set_split_vfo = icm710_set_split_vfo,
    .get_split_vfo = icm710_get_split_vfo,
    .set_mode = icm710_set_mode,
    .get_mode = icm710_get_mode,

    .set_ptt = icm710_set_ptt,
    .get_ptt = icm710_get_ptt,
    .vfo_op = icm710_vfo_op,

    .set_level = icm710_set_level,
    .get_level = icm710_get_level,
    .set_func = icm710_set_func,
    .get_func = icm710_get_func,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * NMEA 0183 protocol
 *
 * Total message is maximum 82 characters, including '$' and CR+LF.
 *
 * Serial setup is 8N1, msb always 0, -> ASCII protocol
 *
 * Proprietary Extension Message format:
 * Byte pos Length  Value   Description
 * 0    1   0x24    '$'     Start character
 * 1    1   0x50    'P'     Type: Proprietary
 * 2    3           'ICO'   Manufacturer ID
 * 5                        Message Data
 */


/* CR LF */
#define EOM "\x0d\x0a"

#define BUFSZ 96

/*
 * Protocol stuff
 */

#define CONTROLLER_ID 90

#define MD_LSB  "LSB"
#define MD_USB  "USB"
#define MD_CW   "CW"
#define MD_AM   "AM"
#define MD_FSK  "AFS"

#define CMD_TXFREQ  "TXF"   /* Transmit frequency */
#define CMD_RXFREQ  "RXF"   /* Receive frequency */
#define CMD_MODE    "MODE"  /* Mode */
#define CMD_REMOTE  "REMOTE"    /* Remote */
#define CMD_PTT     "TRX"   /* PTT */
#define CMD_AFGAIN  "AFG"
#define CMD_RFGAIN  "RFG"
#define CMD_RFPWR   "TXP"
#define CMD_NB      "NB"
#define CMD_AGC     "AGC"
#define CMD_TUNER   "TUNER"

/* Data Output Commands */
#define CMD_SMETER  "SIGM"  /* S-meter read */
#define CMD_SQLS    "SQLS"  /* Squelch status */


/* Tokens */
#define TOK_REMOTEID TOKEN_BACKEND(1)

const struct confparams icm710_cfg_params[] =
{
    {
        TOK_REMOTEID, "remoteid", "Remote ID", "Transceiver's remote ID",
        "1", RIG_CONF_NUMERIC, { .n = { 1, 99, 1 } }
    },
    { RIG_CONF_END, NULL, }
};

/*
 * Basically, set up *priv
 */
int icm710_init(RIG *rig)
{
    struct icm710_priv_data *priv;
    const struct icm710_priv_caps *priv_caps;
    const struct rig_caps *caps;

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (!caps->priv)
    {
        return -RIG_ECONF;
    }

    priv_caps = (const struct icm710_priv_caps *) caps->priv;

    rig->state.priv = (struct icm710_priv_data *)calloc(1,
                      sizeof(struct icm710_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->remote_id = priv_caps->default_remote_id;
    priv->split = RIG_SPLIT_OFF;
    return RIG_OK;
}

int icm710_open(RIG *rig)
{
    int retval = icmarine_transaction(rig, "REMOTE", "ON", NULL);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: rig not responding? %s\n", __func__,
                  rigerror(retval));

    }

    return RIG_OK;
}

int icm710_close(RIG *rig)
{
    int retval = icmarine_transaction(rig, "REMOTE", "OFF", NULL);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: rig not responding? %s\n", __func__,
                  rigerror(retval));

    }

    return RIG_OK;
}
int icm710_cleanup(RIG *rig)
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

int icm710_set_conf(RIG *rig, token_t token, const char *val)
{
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_REMOTEID:
        priv->remote_id = atoi(val);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int icm710_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_REMOTEID:
        SNPRINTF(val, val_len, "%u", priv->remote_id);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int icm710_get_conf(RIG *rig, token_t token, char *val)
{
    return icm710_get_conf2(rig, token, val, 128);
}

int icm710_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];
    struct icm710_priv_data *priv;
    int retval;

    priv = (struct icm710_priv_data *)rig->state.priv;

    SNPRINTF(freqbuf, sizeof(freqbuf), "%.6f", freq / MHz(1));

    /* no error reporting upon TXFREQ failure */
    if (RIG_SPLIT_OFF == priv->split)
    {
        retval = icmarine_transaction(rig, CMD_TXFREQ, freqbuf, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        priv->txfreq = freq;
    }

    retval = icmarine_transaction(rig, CMD_RXFREQ, freqbuf, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv->rxfreq = freq;

    return RIG_OK;
}

/*
 * icm710_get_freq
 * Assumes rig!=NULL, freq!=NULL
 * The M710 does not respond to queries so we keep our own copy of things as a virtual rig response
 */
int icm710_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    *freq = ((struct icm710_priv_data *)rig->state.priv)->rxfreq;

    return RIG_OK;
}

int icm710_set_tx_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];
    int retval;
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;

    SNPRINTF(freqbuf, sizeof(freqbuf), "%.6f", freq / MHz(1));

    retval = icmarine_transaction(rig, CMD_TXFREQ, freqbuf, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv->txfreq = freq;
    return RIG_OK;
}

int icm710_get_tx_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;

    *freq = priv->txfreq;
    return RIG_OK;
}

int icm710_set_split_vfo(RIG *rig, vfo_t rx_vfo, split_t split, vfo_t tx_vfo)
{
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;


    /* when disabling split mode */
    if (RIG_SPLIT_ON == priv->split && RIG_SPLIT_OFF == split)
    {
        int retval = icm710_set_tx_freq(rig, rx_vfo, priv->rxfreq);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    priv->split = split;

    return RIG_OK;
}

int icm710_get_split_vfo(RIG *rig, vfo_t rx_vfo, split_t *split, vfo_t *tx_vfo)
{
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;

    *split = priv->split;
    *tx_vfo = rx_vfo;

    return RIG_OK;
}

/* REM: no way to change passband width ? */
int icm710_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    const char *pmode;

    switch (mode)
    {
    case RIG_MODE_CW:
        pmode = MD_CW;
        break;

    case RIG_MODE_USB:
        pmode = MD_USB;
        break;

    case RIG_MODE_LSB:
        pmode = MD_LSB;
        break;

    case RIG_MODE_AM:
        pmode = MD_AM;
        break;

    case RIG_MODE_RTTY:
        pmode = MD_FSK;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    return icmarine_transaction(rig, CMD_MODE, pmode, NULL);
}

int icm710_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    *mode = ((struct icm710_priv_data *)rig->state.priv)->mode;
    *width = 2200;

    return RIG_OK;
}

/*
 * Rem: The "TX" command will fail on invalid frequencies.
 */
int icm710_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;
    retval = icmarine_transaction(rig, CMD_PTT,
                                  ptt == RIG_PTT_ON ? "TX" : "RX", NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv->ptt = ptt;
    return RIG_OK;
}


int icm710_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    *ptt = ((struct icm710_priv_data *)rig->state.priv)->ptt;

    return RIG_OK;
}

int icm710_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    if (RIG_OP_TUNE != op && RIG_OP_NONE != op)
    {
        return -RIG_EINVAL;
    }

    return icmarine_transaction(rig, CMD_TUNER,
                                RIG_OP_TUNE == op ? "ON" : "OFF", NULL);
}

int icm710_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int retval;

    switch (func)
    {
    case RIG_FUNC_NB:
        retval = icmarine_transaction(rig, CMD_NB, status ? "ON" : "OFF", NULL);
        break;

    default:
        return -RIG_EINVAL;
    }

    return retval;
}

int icm710_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char funcbuf[BUFSZ];
    int retval;

    switch (func)
    {
    case RIG_FUNC_NB:
        retval = icmarine_transaction(rig, CMD_NB, NULL, funcbuf);
        break;

    default:
        return -RIG_EINVAL;
    }

    *status = !strcmp(funcbuf, "ON");

    return retval;
}


int icm710_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char lvlbuf[BUFSZ];
    int retval;
    unsigned value;
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;


    switch (level)
    {
    case RIG_LEVEL_AF:
        value = (unsigned)(val.f * 255);
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "%u", value);
        retval = icmarine_transaction(rig, CMD_AFGAIN, lvlbuf, NULL);

        if (retval == RIG_OK)
        {
            priv->afgain = value;
        }

        break;

    case RIG_LEVEL_RF:
        value = (unsigned)(val.f * 9);
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "%u", value);
        retval = icmarine_transaction(rig, CMD_RFGAIN, lvlbuf, NULL);

        if (retval == RIG_OK)
        {
            priv->rfgain = value;
        }

        break;

    case RIG_LEVEL_RFPOWER:
        value = (unsigned)(val.f * 2);
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "%u", value);
        retval = icmarine_transaction(rig, CMD_RFPWR, lvlbuf, NULL);

        if (retval == RIG_OK)
        {
            priv->rfpwr = value;
        }

        break;

    case RIG_LEVEL_AGC:
        retval = icmarine_transaction(rig, CMD_AGC,
                                      RIG_AGC_OFF == val.i ? "OFF" : "ON", NULL);

        if (retval == RIG_OK)
        {
            priv->afgain = val.i;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return retval;
}

int icm710_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct icm710_priv_data *priv;

    priv = (struct icm710_priv_data *)rig->state.priv;


    switch (level)
    {

    case RIG_LEVEL_AF:
        val->f = priv->afgain / 255.;
        break;

    case RIG_LEVEL_RF:
        val->f = priv->rfgain / 9.;
        break;

    case RIG_LEVEL_RFPOWER:
        val->f = priv->rfpwr / 3.;
        break;

    case RIG_LEVEL_AGC:
        val->i = priv->agc;
        break;

    default:
        return -RIG_EINVAL;
    }

    return -RIG_OK;
}



/*
 * initrigs_icm710 is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(icm710)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: icm710_init called\n", __func__);

    rig_register(&icm700pro_caps);
    rig_register(&icm710_caps);
    rig_register(&icm802_caps);

    return RIG_OK;
}

