/*
 *  Hamlib ICOM Marine backend - main file
 *  Copyright (c) 2014-2015 by Stephane Fillod
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
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include <token.h>
#include <register.h>

#include "icmarine.h"


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
#define LF "\x0a"

#define BUFSZ 96

#define OFFSET_CMD 13

/*
 * Protocol stuff
 */

#define CONTROLLER_ID 90

#if 0
#define MD_LSB  "J3E"
#define MD_USB  "J3E"
#define MD_CW   "A1A"
#define MD_AM   "A3E"
#else
#define MD_LSB  "LSB"
#define MD_USB  "USB"
#define MD_CW   "CW"
#define MD_AM   "AM"
#endif
#define MD_FSK  "J2B"

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

int icmarine_transaction(RIG *rig, const char *cmd, const char *param,
                         char *response);

const struct confparams icmarine_cfg_params[] =
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
int icmarine_init(RIG *rig)
{
    struct icmarine_priv_data *priv;
    const struct icmarine_priv_caps *priv_caps;
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

    priv_caps = (const struct icmarine_priv_caps *) caps->priv;

    rig->state.priv = (struct icmarine_priv_data *)calloc(1, sizeof(
                          struct icmarine_priv_data));

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

int icmarine_cleanup(RIG *rig)
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

#ifdef XXREMOVEDXX
// This function not referenced -- doesn't do anything really
int icmarine_open(RIG *rig)
{
    char respbuf[BUFSZ + 1];
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    int retval = icmarine_transaction(rig, "REMOTE", "ON", respbuf);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: rig not responding? %s\n", __func__,
                  rigerror(retval));

    }

    return RIG_OK;
}
#endif


int icmarine_set_conf(RIG *rig, token_t token, const char *val)
{
    struct icmarine_priv_data *priv;

    priv = (struct icmarine_priv_data *)rig->state.priv;

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

int icmarine_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct icmarine_priv_data *priv;

    priv = (struct icmarine_priv_data *)rig->state.priv;

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

int icmarine_get_conf(RIG *rig, token_t token, char *val)
{
    return icmarine_get_conf2(rig, token, val, 128);
}


/*
 * icmarine_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 *
 * cmd: mandatory
 * param: only 1 optional NMEA parameter, NULL for none (=query)
 * response: optional (holding BUFSZ bytes)
 */
int icmarine_transaction(RIG *rig, const char *cmd, const char *param,
                         char *response)
{
    struct icmarine_priv_data *priv;
    int i, retval;
    struct rig_state *rs;
    char cmdbuf[BUFSZ + 1];
    char respbuf[BUFSZ + 1];
    char *p;
    char *strip;
    int cmd_len;
    unsigned csum = 0;

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd='%s', param=%s\n", __func__, cmd,
              param == NULL ? "NULL" : param);

    rs = &rig->state;
    priv = (struct icmarine_priv_data *)rs->priv;

    rig_flush(&rs->rigport);

    /* command formatting */
    SNPRINTF(cmdbuf, BUFSZ, "$PICOA,%02d,%02u,%s",
             CONTROLLER_ID,
             priv->remote_id,
             cmd);
    cmd_len = strlen(cmdbuf);

    if (param)
    {
        cmd_len += snprintf(cmdbuf + cmd_len, BUFSZ - cmd_len, ",%s", param);
    }

    /* NMEA checksum, betwwen '$' and '*' */
    for (i = 1; i < cmd_len; i++)
    {
        csum = csum ^ (unsigned)cmdbuf[i];
    }

    cmd_len += snprintf(cmdbuf + cmd_len, BUFSZ - cmd_len, "*%02X" EOM, csum);

    /* I/O */
    retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * Transceiver sends an echo of cmd followed by a CR/LF
     */
    retval = read_string(&rs->rigport, (unsigned char *) respbuf, BUFSZ, LF,
                         strlen(LF), 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    /* Minimal length */
    if (retval < OFFSET_CMD + 5)
    {
        return -RIG_EPROTO;
    }

    /* check response */
    if (memcmp(respbuf, "$PICOA,", strlen("$PICOA,")) != 0)
    {
        return -RIG_EPROTO;
    }

    /* TODO: check ID's */

    /* if not a query, check for correct as acknowledge */
    if (param)
    {
        if (memcmp(cmdbuf + OFFSET_CMD, respbuf + OFFSET_CMD,
                   cmd_len - OFFSET_CMD - 5) == 0)
        {
            return RIG_OK;
        }
        else
        {
            return -RIG_ERJCTED;
        }
    }

    /* strip from *checksum and after */
    strip = strrchr(respbuf, '*');

    if (strip)
    {
        *strip = 0;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: checksum not in response? response='%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    p = strrchr(respbuf, ',');

    if (p)
    {
        strncpy(response, p + 1, BUFSZ);
    }
    else
    {
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: returning response='%s'\n", __func__,
              response == NULL ? "NULL" : response);
    return RIG_OK;
}


int icmarine_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];
    struct icmarine_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    priv = (struct icmarine_priv_data *)rig->state.priv;

    SNPRINTF(freqbuf, sizeof(freqbuf), "%.6f", freq / MHz(1));

    /* no error reporting upon TXFREQ failure */
    if (RIG_SPLIT_OFF == priv->split)
    {
        icmarine_transaction(rig, CMD_TXFREQ, freqbuf, NULL);
    }

    return icmarine_transaction(rig, CMD_RXFREQ, freqbuf, NULL);
}

/*
 * icmarine_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int icmarine_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    char freqbuf[BUFSZ] = "";
    double d;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    retval = icmarine_transaction(rig, CMD_RXFREQ, NULL, freqbuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (freqbuf[0] == '\0')
    {
        *freq = 0;
    }
    else
    {
        if (sscanf(freqbuf, "%lf", &d) != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: sscanf('%s') failed\n", __func__, freqbuf);
            return -RIG_EPROTO;
        }

        *freq = (freq_t)(d * MHz(1));
    }

    return RIG_OK;
}

int icmarine_set_tx_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    SNPRINTF(freqbuf, sizeof(freqbuf), "%.6f", freq / MHz(1));

    return icmarine_transaction(rig, CMD_TXFREQ, freqbuf, NULL);
}

int icmarine_get_tx_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    char freqbuf[BUFSZ] = "";
    double d;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    retval = icmarine_transaction(rig, CMD_TXFREQ, NULL, freqbuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (freqbuf[0] == '\0')
    {
        *freq = 0;
    }
    else
    {
        if (sscanf(freqbuf, "%lf", &d) != 1)
        {
            return -RIG_EPROTO;
        }

        *freq = (freq_t)(d * MHz(1));
    }

    return RIG_OK;
}

int icmarine_set_split_vfo(RIG *rig, vfo_t rx_vfo, split_t split, vfo_t tx_vfo)
{
    struct icmarine_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    priv = (struct icmarine_priv_data *)rig->state.priv;

    /* when disabling split mode */
    if (RIG_SPLIT_ON == priv->split &&
            RIG_SPLIT_OFF == split)
    {
        freq_t freq;

        if (RIG_OK == icmarine_get_freq(rig, rx_vfo, &freq))
        {
            icmarine_set_tx_freq(rig, rx_vfo, freq);
        }
    }

    priv->split = split;

    return RIG_OK;
}

int icmarine_get_split_vfo(RIG *rig, vfo_t rx_vfo, split_t *split,
                           vfo_t *tx_vfo)
{
    struct icmarine_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    priv = (struct icmarine_priv_data *)rig->state.priv;

    *split = priv->split;
    *tx_vfo = rx_vfo;

    return RIG_OK;
}

/* REM: no way to change passband width ? */
int icmarine_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    const char *pmode;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    switch (mode)
    {
    case RIG_MODE_CW:       pmode = MD_CW; break;

    case RIG_MODE_USB:      pmode = MD_USB; break;

    case RIG_MODE_LSB:      pmode = MD_LSB; break;

    case RIG_MODE_AM:       pmode = MD_AM; break;

    case RIG_MODE_RTTY:     pmode = MD_FSK; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    return icmarine_transaction(rig, CMD_MODE, pmode, NULL);
}

int icmarine_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    char modebuf[BUFSZ];

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    retval = icmarine_transaction(rig, CMD_MODE, NULL, modebuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!memcmp(modebuf, MD_LSB, strlen(MD_LSB)))
    {
        *mode = RIG_MODE_LSB;
    }
    else if (!memcmp(modebuf, MD_USB, strlen(MD_USB)))
    {
        *mode = RIG_MODE_USB;
    }
    else if (!memcmp(modebuf, MD_CW, strlen(MD_CW)))
    {
        *mode = RIG_MODE_CW;
    }
    else if (!memcmp(modebuf, MD_AM, strlen(MD_AM)))
    {
        *mode = RIG_MODE_AM;
    }
    else if (!memcmp(modebuf, MD_FSK, strlen(MD_FSK)))
    {
        *mode = RIG_MODE_RTTY;
    }
    else
    {
        retval = -RIG_EPROTO;
    }

    if (retval == RIG_OK)
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return retval;
}

/*
 * Rem: The "TX" command will fail on invalid frequencies.
 */
int icmarine_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    retval = icmarine_transaction(rig, CMD_PTT,
                                  ptt == RIG_PTT_ON ? "TX" : "RX", NULL);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: transaction failed\n", __func__);
        return retval;
    }

    return RIG_OK;
}

int icmarine_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char pttbuf[BUFSZ];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    retval = icmarine_transaction(rig, CMD_PTT, NULL, pttbuf);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: transaction failed\n", __func__);
        return retval;
    }

    if (strncmp(pttbuf, "TX", 2) == 0)
    {
        *ptt = RIG_PTT_ON;
    }
    else if (strncmp(pttbuf, "RX", 2) == 0)
    {
        *ptt = RIG_PTT_OFF;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid pttbuf='%s'\n", __func__, pttbuf);
        retval = -RIG_EPROTO;
    }

    return retval;
}

int icmarine_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char dcdbuf[BUFSZ];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    retval = icmarine_transaction(rig, CMD_SQLS, NULL, dcdbuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!strcmp(dcdbuf, "OPEN"))
    {
        *dcd = RIG_DCD_ON;
    }
    else if (!strcmp(dcdbuf, "CLOSE"))
    {
        *dcd = RIG_DCD_OFF;
    }
    else
    {
        retval = -RIG_EPROTO;
    }

    return retval;
}

int icmarine_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    if (RIG_OP_TUNE != op && RIG_OP_NONE != op)
    {
        return -RIG_EINVAL;
    }

    return icmarine_transaction(rig, CMD_TUNER,
                                RIG_OP_TUNE == op ? "ON" : "OFF", NULL);
}

int icmarine_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

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

int icmarine_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char funcbuf[BUFSZ];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

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


int icmarine_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char lvlbuf[BUFSZ];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_AF:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "%u", (unsigned)(val.f * 255));
        retval = icmarine_transaction(rig, CMD_AFGAIN, lvlbuf, NULL);
        break;

    case RIG_LEVEL_RF:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "%u", (unsigned)(val.f * 9));
        retval = icmarine_transaction(rig, CMD_RFGAIN, lvlbuf, NULL);
        break;

    case RIG_LEVEL_RFPOWER:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "%u", 1 + (unsigned)(val.f * 2));
        retval = icmarine_transaction(rig, CMD_RFPWR, lvlbuf, NULL);
        break;

    case RIG_LEVEL_AGC:
        retval = icmarine_transaction(rig, CMD_AGC,
                                      RIG_AGC_OFF == val.i ? "OFF" : "ON", NULL);
        break;

    default:
        return -RIG_EINVAL;
    }

    return retval;
}

int icmarine_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[BUFSZ];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        retval = icmarine_transaction(rig, CMD_SMETER, NULL, lvlbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] < '0' || lvlbuf[0] > '9')
        {
            return -RIG_EPROTO;
        }

        val->i = lvlbuf[0] - '0';
        break;

    case RIG_LEVEL_AF:
        retval = icmarine_transaction(rig, CMD_AFGAIN, NULL, lvlbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = atof(lvlbuf) / 255.;
        break;

    case RIG_LEVEL_RF:
        retval = icmarine_transaction(rig, CMD_RFGAIN, NULL, lvlbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] < '0' || lvlbuf[0] > '9')
        {
            return -RIG_EPROTO;
        }

        val->f = (float)(lvlbuf[0] - '0') / 9.;
        break;

    case RIG_LEVEL_RFPOWER:
        retval = icmarine_transaction(rig, CMD_RFPWR, NULL, lvlbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] < '1' || lvlbuf[0] > '3')
        {
            return -RIG_EPROTO;
        }

        val->f = (float)(lvlbuf[0] - '1') / 3.;
        break;

    case RIG_LEVEL_AGC:
        retval = icmarine_transaction(rig, CMD_AGC, NULL, lvlbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->i = !strcmp(lvlbuf, "ON") ? RIG_AGC_SLOW : RIG_AGC_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    return retval;
}



/*
 * initrigs_icmarine is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(icmarine)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&icm700pro_caps);
    rig_register(&icm710_caps);
    rig_register(&icm802_caps);
    rig_register(&icm803_caps);

    return RIG_OK;
}

