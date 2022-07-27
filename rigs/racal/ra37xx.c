/*
 *  Hamlib Racal backend - RA37XX main file
 *  Copyright (c) 2004-2010 by Stephane Fillod
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

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"
#include "token.h"

#include "ra37xx.h"


const struct confparams ra37xx_cfg_params[] =
{
    {
        TOK_RIGID, "receiver_id", "receiver ID", "receiver ID, -1 to disable addressing",
        "-1", RIG_CONF_NUMERIC, { .n = { -1, 9, 1 } }
    },
    { RIG_CONF_END, NULL, }
};



/* packet framing, 5-8 */
#define BUFSZ 256

#define SOM "\x0a"  /* LF */
#define EOM "\x0d"  /* CR */

/*
 * modes
 */
#define MD_USB  1
#define MD_LSB  2
#define MD_AM   3
#define MD_FM   4
#define MD_CW   5
#define MD_FSK  6   /* option */
#define MD_ISB_USB  7
#define MD_ISB_LSB  8
#define MD_FSK_NAR  13  /* option */
#define MD_FSK_MED  14  /* option */
#define MD_FSK_WID  15  /* option */


/*
 * retries are handled by ra37xx_transaction()
 */
static int ra37xx_one_transaction(RIG *rig, const char *cmd, char *data,
                                  int *data_len)
{
    struct ra37xx_priv_data *priv = (struct ra37xx_priv_data *)rig->state.priv;
    struct rig_state *rs = &rig->state;
    char cmdbuf[BUFSZ];
    char respbuf[BUFSZ];
    int retval;
    int pkt_header_len;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    /* Packet Framing:
       - no Link Control Character
       - (optional) 1 Address Character
       - no Check Character
     */
    if (priv->receiver_id != -1)
    {
        pkt_header_len = 2;
        SNPRINTF(cmdbuf, sizeof(cmdbuf), SOM "%d%s" EOM, priv->receiver_id, cmd);
    }
    else
    {
        pkt_header_len = 1;
        SNPRINTF(cmdbuf, sizeof(cmdbuf), SOM "%s" EOM, cmd);
    }

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }


    /* forward COMMAND frame? no data expected */
    if (!data || !data_len)
    {
        return retval;
    }

    do
    {
        retval = read_string(&rs->rigport, (unsigned char *) respbuf, BUFSZ, EOM,
                             strlen(EOM), 0, 1);

        if (retval < 0)
        {
            return retval;
        }

        /* drop short/invalid packets */
        if (retval <= pkt_header_len + 1 || respbuf[0] != '\x0a')
        {
            if (!rig_check_cache_timeout(&tv, rs->rigport.timeout))
            {
                continue;
            }
            else
            {
                return -RIG_EPROTO;
            }
        }

        /* drop other receiver id, and "pause" (empty) packets */
        if ((priv->receiver_id != -1 && (respbuf[1] - '0') != priv->receiver_id))
        {
            if (!rig_check_cache_timeout(&tv, rs->rigport.timeout))
            {
                continue;
            }
            else
            {
                return -RIG_ETIMEOUT;
            }
        }

        if (retval >= pkt_header_len + 3 && !memcmp(respbuf + pkt_header_len, "ERR", 3))
        {
            return -RIG_ERJCTED;
        }

        if (retval >= pkt_header_len + 5
                && !memcmp(respbuf + pkt_header_len, "FAULT", 5))
        {
            return -RIG_ERJCTED;
        }

        if (cmd[0] == 'Q' && (retval + pkt_header_len + 1 < strlen(cmd) ||
                              cmd[1] != respbuf[pkt_header_len]))
        {

            rig_debug(RIG_DEBUG_WARN, "%s: unexpected revertive frame\n",
                      __func__);

            if (!rig_check_cache_timeout(&tv, rs->rigport.timeout))
            {
                continue;
            }
            else
            {
                return -RIG_ETIMEOUT;
            }
        }
    }
    while (retval < 0);

    /* Strip starting LF and ending CR */
    memcpy(data, respbuf + pkt_header_len, retval - pkt_header_len - 1);

    *data_len = retval;
    return RIG_OK;
}

static int ra37xx_transaction(RIG *rig, const char *cmd, char *data,
                              int *data_len)
{
    int retval, retry;

    retry = rig->state.rigport.retry;

    do
    {
        retval = ra37xx_one_transaction(rig, cmd, data, data_len);

        if (retval == RIG_OK)
        {
            break;
        }
    }
    while (retry-- > 0);

    return retval;
}


int ra37xx_init(RIG *rig)
{
    struct ra37xx_priv_data *priv;

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct ra37xx_priv_data *)calloc(1, sizeof(
                          struct ra37xx_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->receiver_id = -1;

    return RIG_OK;
}

/*
 */
int ra37xx_cleanup(RIG *rig)
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
int ra37xx_set_conf2(RIG *rig, token_t token, const char *val, int val_len)
{
    struct ra37xx_priv_data *priv = (struct ra37xx_priv_data *)rig->state.priv;
    int receiver_id;

    switch (token)
    {
    case TOK_RIGID:
        receiver_id = atoi(val);

        if (receiver_id < -1 || receiver_id > 9)
        {
            return -RIG_EINVAL;
        }

        priv->receiver_id = receiver_id;
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int ra37xx_set_conf(RIG *rig, token_t token, const char *val)
{
    return ra37xx_set_conf2(rig, token, val, 128);
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int ra37xx_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct ra37xx_priv_data *priv = (struct ra37xx_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_RIGID:
        SNPRINTF(val, val_len, "%d", priv->receiver_id);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int ra37xx_get_conf(RIG *rig, token_t token, char *val)
{
    return ra37xx_get_conf2(rig, token, val, 128);
}

/*
 * ra37xx_open
 * Assumes rig!=NULL
 */
int ra37xx_open(RIG *rig)
{
    /* Set Receiver to remote control
     */
    return ra37xx_transaction(rig, "REM1", NULL, NULL);
}


/*
 * ra37xx_close
 * Assumes rig!=NULL
 */
int ra37xx_close(RIG *rig)
{
    /* Set Receiver to local control */
    return ra37xx_transaction(rig, "REM0", NULL, NULL);
}

/*
 * ra37xx_set_freq
 * Assumes rig!=NULL
 */
int ra37xx_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];

    SNPRINTF(freqbuf, sizeof(freqbuf), "F%lu", (unsigned long)freq);

    return ra37xx_transaction(rig, freqbuf, NULL, NULL);
}

int ra37xx_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char freqbuf[BUFSZ];
    int retval, len;
    double f;

    retval = ra37xx_transaction(rig, "QF", freqbuf, &len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(freqbuf + 1, "%lf", &f);
    *freq = (freq_t)f;

    return RIG_OK;
}

/*
 * ra37xx_set_mode
 * Assumes rig!=NULL
 */
int ra37xx_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    //struct ra37xx_priv_data *priv = (struct ra37xx_priv_data*)rig->state.priv;
    int ra_mode, widthtype, widthnum = 0;
    char buf[BUFSZ];

    switch (mode)
    {
    case RIG_MODE_CW:   widthtype = 1; ra_mode = MD_CW; break;

    case RIG_MODE_CWR:  widthtype = 2; ra_mode = MD_CW; break;

    case RIG_MODE_USB:  widthtype = 1; ra_mode = MD_USB; break;

    case RIG_MODE_LSB:  widthtype = 2; ra_mode = MD_LSB; break;

    case RIG_MODE_AM:   widthtype = 3; ra_mode = MD_AM; break;

    case RIG_MODE_FM:   widthtype = 3; ra_mode = MD_FM; break;

    case RIG_MODE_RTTY: widthtype = 3; ra_mode = MD_FSK; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (width == RIG_PASSBAND_NORMAL)
    {
        rig_passband_normal(rig, mode);
    }

    rig_debug(RIG_DEBUG_TRACE,
              "%s: widthtype = %i, widthnum = %i not implemented\n", __func__, widthtype,
              widthnum);
#ifdef XXREMOVEDXX
    widthtype = 0; /* FIXME: no bandwidth for now */
    widthnum = 0;
    /* width set using 'B', QBCON must be queried firsthand */
#endif

#ifdef XXREMOVEDXX
    SNPRINTF(buf, sizeof(buf), "M%d;B%d,%d", ra_mode, widthtype, widthnum);
#else
    SNPRINTF(buf, sizeof(buf), "M%d", ra_mode);
#endif

    return ra37xx_transaction(rig, buf, NULL, NULL);
}

int ra37xx_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[BUFSZ], resbuf[BUFSZ];
    int retval, len, ra_mode, widthtype, widthnum;

    retval = ra37xx_transaction(rig, "QM", resbuf, &len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(resbuf + 1, "%d", &ra_mode);

    switch (ra_mode)
    {
    case MD_CW:  widthtype = 1; *mode = RIG_MODE_CW; break;

    case MD_ISB_LSB:
    case MD_LSB: widthtype = 2; *mode = RIG_MODE_LSB; break;

    case MD_ISB_USB:
    case MD_USB: widthtype = 1; *mode = RIG_MODE_USB; break;

    case MD_FSK_NAR:
    case MD_FSK_MED:
    case MD_FSK_WID:
    case MD_FSK: widthtype = 3; *mode = RIG_MODE_RTTY; break;

    case MD_FM:  widthtype = 3; *mode = RIG_MODE_FM; break;

    case MD_AM:  widthtype = 3; *mode = RIG_MODE_AM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(*mode));
        return -RIG_EPROTO;
    }

    retval = ra37xx_transaction(rig, "QB", resbuf, &len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* FIXME */
    widthnum = 0;
    SNPRINTF(buf, sizeof(buf), "QBCON%d,%d", widthtype, widthnum);
    retval = ra37xx_transaction(rig, buf, resbuf, &len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* TODO: width */
    *width = 0;

    return RIG_OK;
}

int ra37xx_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmdbuf[BUFSZ];

    switch (func)
    {
    case RIG_FUNC_MUTE:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "MUTE%d", status ? 1 : 0);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported %s\n",
                  __func__, rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return ra37xx_transaction(rig, cmdbuf, NULL, NULL);
}

int ra37xx_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char resbuf[BUFSZ];
    int retval, len, i;

    switch (func)
    {
    case RIG_FUNC_MUTE:
        retval = ra37xx_transaction(rig, "QMUTE", resbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(resbuf + 4, "%d", &i);
        *status = i == 0 ? 0 : 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported %s\n", __func__, rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * ra37xx_set_level
 * Assumes rig!=NULL
 */
int ra37xx_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char cmdbuf[BUFSZ];
    int agc;

    switch (level)
    {
    case RIG_LEVEL_AF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "AFL%d", (int)(val.f * 255));
        break;

    case RIG_LEVEL_PREAMP:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "RFAMP%d", val.i ? 1 : 0);
        break;

    case RIG_LEVEL_CWPITCH: /* BFO */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "BFO%d", val.i);
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "CORL%d", (int)(val.f * 255));
        break;

    case RIG_LEVEL_RF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "G%d", (int)(val.f * 255));
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_FAST: agc = 0; break;

        case RIG_AGC_MEDIUM: agc = 1; break;

        case RIG_AGC_SLOW: agc = 2; break;

        case RIG_AGC_USER: agc = 0; break;

        default: return -RIG_EINVAL;
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "AGC%d,%d", val.i == RIG_AGC_USER ? 1 : 0,
                 agc);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return ra37xx_transaction(rig, cmdbuf, NULL, NULL);
}


/*
 * ra37xx_get_level
 * Assumes rig!=NULL
 */
int ra37xx_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char resbuf[BUFSZ];
    int retval, len, i;

    switch (level)
    {
    case RIG_LEVEL_AF:
        retval = ra37xx_transaction(rig, "QAFL", resbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(resbuf + 3, "%d", &i);
        val->f = ((float)i) / 255;
        break;

    case RIG_LEVEL_CWPITCH:
        retval = ra37xx_transaction(rig, "QBFO", resbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(resbuf + 3, "%d", &val->i);
        break;

    case RIG_LEVEL_PREAMP:
        retval = ra37xx_transaction(rig, "QRFAMP", resbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(resbuf + 5, "%d", &i);
        val->i = i ? rig->state.preamp[0] : 0;
        break;

    case RIG_LEVEL_RAWSTR:
        retval = ra37xx_transaction(rig, "QRFL", resbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(resbuf + 3, "%d", &val->i);
        break;

    case RIG_LEVEL_SQL:
        retval = ra37xx_transaction(rig, "QCORL", resbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(resbuf + 4, "%d", &i);
        val->f = ((float)i) / 255;
        break;

    case RIG_LEVEL_RF:
        retval = ra37xx_transaction(rig, "QG", resbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(resbuf + 1, "%d", &i);
        val->f = ((float)i) / 255;
        break;

    case RIG_LEVEL_AGC:
        retval = ra37xx_transaction(rig, "QAGC", resbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (resbuf[3] != '0')
        {
            val->i = RIG_AGC_USER;
            break;
        }

        switch (resbuf[5])
        {
        case '0': val->i = RIG_AGC_FAST; break;

        case '1': val->i = RIG_AGC_MEDIUM; break;

        case '2': val->i = RIG_AGC_SLOW; break;

        default: return -RIG_EPROTO;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported %s\n", __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

const char *ra37xx_get_info(RIG *rig)
{
    static char infobuf[BUFSZ];
    int res_len, retval;

    retval = ra37xx_transaction(rig, "QID", infobuf, &res_len);

    if (retval != RIG_OK || res_len < 2 || res_len >= BUFSZ)
    {
        return NULL;
    }

    infobuf[res_len] = '\0';

    /* TODO: "QSW"? c.f. 5-43 */

    /* skip "ID" */
    return infobuf + 2;
}

int ra37xx_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    char buf[BUFSZ];
    int i_ant;

    switch (ant)
    {
    case RIG_ANT_1: i_ant = 1 << 0; break;

    case RIG_ANT_2: i_ant = 1 << 1; break;

    case RIG_ANT_3: i_ant = 1 << 2; break;

    case RIG_ANT_4: i_ant = 1 << 3; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported ant %#x", ant);
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "ANT%d", i_ant);

    return ra37xx_transaction(rig, buf, NULL, NULL);
}

int ra37xx_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                   ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    char buf[BUFSZ];
    int retval, buflen, ra_ant;

    retval = ra37xx_transaction(rig, "QANT", buf, &buflen);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(buf + 3, "%d", &ra_ant);

    if (ra_ant < 0 || ra_ant > 15)
    {
        return -RIG_EPROTO;
    }

    *ant_curr = ((ra_ant & (1 << 0)) ? RIG_ANT_1 : 0) |
                ((ra_ant & (1 << 1)) ? RIG_ANT_2 : 0) |
                ((ra_ant & (1 << 2)) ? RIG_ANT_3 : 0) |
                ((ra_ant & (1 << 3)) ? RIG_ANT_4 : 0);

    return RIG_OK;
}

int ra37xx_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char buf[BUFSZ];

    /* NB: does a RIG_OP_TO_VFO!*/

    SNPRINTF(buf, sizeof(buf), "CHAN%d", ch);

    return ra37xx_transaction(rig, buf, NULL, NULL);
}

int ra37xx_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char buf[BUFSZ];
    int retval, buflen;

    retval = ra37xx_transaction(rig, "QCHAN", buf, &buflen);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ch = atoi(buf + 4);

    return RIG_OK;
}

int ra37xx_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    char buf[BUFSZ];
    int scantype;

    switch (scan)
    {
    case RIG_SCAN_STOP: scantype = 0; break;

    case RIG_SCAN_VFO: scantype = 1; break;

    case RIG_SCAN_MEM: scantype = 2; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported scan %#x", scan);
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "SCAN%d,0", scantype);

    return ra37xx_transaction(rig, buf, NULL, NULL);
}

int ra37xx_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    char buf[BUFSZ];
    int ret, ch;

    switch (op)
    {
    case RIG_OP_FROM_VFO:
        ret = rig_get_mem(rig, vfo, &ch);

        if (ret < 0)
        {
            return ret;
        }

        SNPRINTF(buf, sizeof(buf), "STRE%d", ch);
        return ra37xx_transaction(rig, buf, NULL, NULL);

    case RIG_OP_TO_VFO:
        ret = rig_get_mem(rig, vfo, &ch);

        if (ret < 0)
        {
            return ret;
        }

        SNPRINTF(buf, sizeof(buf), "CHAN%d", ch);
        return ra37xx_transaction(rig, buf, NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported op %#x", op);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


