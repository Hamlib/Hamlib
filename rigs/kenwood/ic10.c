/*
 *  Hamlib Kenwood backend - IC-10 interface for:
 *    TS-940, TS-811, TS-711, TS-440, and R-5000
 *
 *  Copyright (c) 2000-2010 by Stephane Fillod and others
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
#include <string.h>  /* String function definitions */
#include <ctype.h>   /* character class tests */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "kenwood.h"
#include "ic10.h"


/****
 * ic10_cmd_trim
 * gobbles up additional spaces at the end of a line
 *
****/
int ic10_cmd_trim(char *data, int data_len)
{

    int i;
    rig_debug(RIG_DEBUG_TRACE, "%s: incoming data_len is '%d'\n",
              __func__, data_len);

    /* suck up additional spaces at end of the data buffer */
    for (i = data_len; !isdigit((int)data[i - 1]); i--)
    {
        data_len = data_len - 1;
        rig_debug(RIG_DEBUG_TRACE, "%s: data['%d'] is '%c'\n",
                  __func__, i - 1, data[i - 1]);
        rig_debug(RIG_DEBUG_TRACE, "%s: For i='%d' data_len is now '%d'\n",
                  __func__, i, data_len);
    }

    rig_debug(RIG_DEBUG_TRACE,
              "%s: finished loop.. i='%d' data_len='%d' data[i-1]='%c'\n",
              __func__, i, data_len, data[i - 1]);

    return data_len;
}


/**
 * ic10_transaction
 * Assumes rig!=NULL rig->state!=NULL rig->caps!=NULL
**/
int ic10_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                     int *data_len)
{
    int retval;
    int retry_cmd = 0;
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_TRACE,
              "%s: called cmd='%s', len=%d, data=%p, data_len=%p\n", __func__, cmd, cmd_len,
              data, data_len);

    rs = &rig->state;

transaction:
    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!data)
    {
        char buffer[50];
        struct kenwood_priv_data *priv = rig->state.priv;

        if (RIG_OK != (retval = write_block(&rs->rigport,
                                            (unsigned char *) priv->verify_cmd, strlen(priv->verify_cmd))))
        {
            return retval;
        }

        // this should be the ID response
        retval = read_string(&rs->rigport, (unsigned char *) buffer, sizeof(buffer),
                             ";", 1, 0, 1);

        // might be ?; too
        if (buffer[0] == '?' && retry_cmd++ < rs->rigport.retry)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: retrying cmd #%d\n", __func__, retry_cmd);
            goto transaction;
        }

        if (strncmp("ID", buffer, 2) != 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: expected ID response and got %s\n", __func__,
                      buffer);
            return retval;
        }

        return RIG_OK;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, 50, ";", 1, 0, 1);

    if (retval == -RIG_ETIMEOUT)
    {
        retval = 0;
    }

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}

/*
 * Get the answer of IF command, with retry handling
 */
static int get_ic10_if(RIG *rig, char *data)
{
    struct kenwood_priv_caps *priv = (struct kenwood_priv_caps *)rig->caps->priv;
    int i, data_len, retval = RIG_EINVAL;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    for (i = 0; retval != RIG_OK && i < rig->state.rigport.retry; i++)
    {
        data_len = 37;
        retval = ic10_transaction(rig, "IF;", 3, data, &data_len);

        if (retval != RIG_OK)
        {
            continue;
        }

        if (retval == RIG_OK &&
                (data_len < priv->if_len ||
                 data[0] != 'I' || data[1] != 'F'))
        {
            rig_debug(RIG_DEBUG_WARN, "%s: unexpected answer %s, len=%d\n",
                      __func__, data, data_len);
            retval = -RIG_ERJCTED;
        }
    }

    return retval;
}


/*
 * ic10_set_vfo
 * Assumes rig!=NULL
 */
int ic10_set_vfo(RIG *rig, vfo_t vfo)
{
    char cmdbuf[6];
    int retval;
    char vfo_function;

    switch (vfo)
    {
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
    case RIG_VFO_A: vfo_function = '0'; break;

    case RIG_VFO_SUB:
    case RIG_VFO_B: vfo_function = '1'; break;

    case RIG_VFO_MEM: vfo_function = '2'; break;

    case RIG_VFO_CURR: return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "FN%c;", vfo_function);

    retval = ic10_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, 0);
    return retval;
}


/*
 * ic10_get_vfo
 * Assumes rig!=NULL, !vfo
 */
int ic10_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct kenwood_priv_caps *priv = (struct kenwood_priv_caps *)rig->caps->priv;
    char vfobuf[50];
    unsigned char c;
    int retval, iflen;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    /* query RX VFO */
    retval = get_ic10_if(rig, vfobuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* trim extra spaces */
    iflen = ic10_cmd_trim(vfobuf, priv->if_len);

    /* IFggmmmkkkhhh snnnzrx yytdfcp */
    /* IFggmmmkkkhhhxxxxxrrrrrssxcctmfcp */

    c = vfobuf[iflen - 3];

    switch (c)
    {
    case '0': *vfo = RIG_VFO_A; break;

    case '1': *vfo = RIG_VFO_B; break;

    case '2': *vfo = RIG_VFO_MEM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %c\n",
                  __func__, c);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: returning VFO=%s\n", __func__,
              rig_strvfo(*vfo));

    return RIG_OK;
}


int ic10_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called vfo=%s tx_freq=%.0f\n", __func__,
              rig_strvfo(vfo), tx_freq);
    return ic10_set_freq(rig, RIG_VFO_B, tx_freq);
}

int ic10_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called vfo=%s\n", __func__, rig_strvfo(vfo));
    return ic10_get_freq(rig, RIG_VFO_B, tx_freq);
}

int ic10_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    return ic10_transaction(rig, split == RIG_SPLIT_ON ? "SP1;" : "SP0;", 4,
                            NULL, 0);
}


int ic10_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *txvfo)
{
    struct kenwood_priv_caps *priv = (struct kenwood_priv_caps *)rig->caps->priv;
    char infobuf[50];
    int retval, iflen;

    retval = get_ic10_if(rig, infobuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* trim extra spaces */
    iflen = ic10_cmd_trim(infobuf, priv->if_len);

    /* IFggmmmkkkhhh snnnzrx yytdfcp */
    /* IFggmmmkkkhhhxxxxxrrrrrssxcctmfcp */

    *split = infobuf[iflen - 1] == '0' ? RIG_SPLIT_OFF : RIG_SPLIT_ON;

    return RIG_OK;
}


/*
 * ic10_get_mode
 * Assumes rig!=NULL, !vfo
 */
int ic10_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct kenwood_priv_caps *priv = (struct kenwood_priv_caps *)rig->caps->priv;
    char modebuf[50];
    unsigned char c;
    int retval, iflen;

    /* query RX VFO */
    retval = get_ic10_if(rig, modebuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* trim extra spaces */
    iflen = ic10_cmd_trim(modebuf, priv->if_len);

    /* IFggmmmkkkhhh snnnzrx yytdfcp */
    /* IFggmmmkkkhhhxxxxxrrrrrssxcctmfcp */

    c = modebuf[iflen - 4];

    switch (c)
    {
    case MD_CW  :   *mode = RIG_MODE_CW; break;

    case MD_USB :   *mode = RIG_MODE_USB; break;

    case MD_LSB :   *mode = RIG_MODE_LSB; break;

    case MD_FM  :   *mode = RIG_MODE_FM; break;

    case MD_AM  :   *mode = RIG_MODE_AM; break;

    case MD_FSK :   *mode = RIG_MODE_RTTY; break;

    case MD_NONE:   *mode = RIG_MODE_NONE; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, c);
        return -RIG_EINVAL;
    }

    *width = rig_passband_normal(rig, *mode);

    return RIG_OK;
}


/*
 * ic10_set_mode
 * Assumes rig!=NULL
 */
int ic10_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char modebuf[6];
    int retval;
    char mode_letter;

    switch (mode)
    {
    case RIG_MODE_LSB  :    mode_letter = MD_LSB; break;

    case RIG_MODE_USB  :    mode_letter = MD_USB; break;

    case RIG_MODE_CW   :    mode_letter = MD_CW; break;

    case RIG_MODE_FM   :    mode_letter = MD_FM; break;

    case RIG_MODE_AM   :    mode_letter = MD_AM; break;

    case RIG_MODE_RTTY :    mode_letter = MD_FSK; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(modebuf, sizeof(modebuf), "MD%c;", mode_letter);
    retval = ic10_transaction(rig, modebuf, strlen(modebuf), NULL, 0);

    return retval;
}


/*
 * ic10_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int ic10_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char infobuf[50];
    int retval;

    if (vfo != RIG_VFO_CURR)
    {
        /* targeted freq retrieval */
        return kenwood_get_freq(rig, vfo, freq);
    }

    retval = get_ic10_if(rig, infobuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* IFggmmmkkkhhh snnnzrx yytdfcp */
    /* IFggmmmkkkhhhxxxxxrrrrrssxcctmfcp */

    infobuf[13] = '\0';
    sscanf(infobuf + 2, "%011"SCNfreq, freq);

    return RIG_OK;
}


/*
 * ic10_set_freq
 * Assumes rig!=NULL
 */
int ic10_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[64];
    int retval;
    unsigned char vfo_letter;
    vfo_t   tvfo;

    if (vfo == RIG_VFO_CURR)
    {
        tvfo = rig->state.current_vfo;
    }
    else
    {
        tvfo = vfo;
    }

    switch (tvfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN: vfo_letter = 'A'; break;

    case RIG_VFO_B:
    case RIG_VFO_SUB: vfo_letter = 'B'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    // cppcheck-suppress *
    SNPRINTF(freqbuf, sizeof(freqbuf), "F%c%011"PRIll";", vfo_letter,
             (int64_t)freq);
    retval = ic10_transaction(rig, freqbuf, strlen(freqbuf), NULL, 0);

    return retval;
}


/*
 * ic10_set_ant
 * Assumes rig!=NULL
 */
int ic10_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    char buf[6], ackbuf[64];
    int ack_len, retval;

    SNPRINTF(buf, sizeof(buf), "AN%c;", ant == RIG_ANT_1 ? '1' : '2');

    retval = ic10_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    return retval;
}


/*
 * ic10_get_ant
 * Assumes rig!=NULL, ptt!=NULL
 */
int ic10_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                 ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    char infobuf[50];
    int info_len, retval;

    info_len = 4;
    retval = ic10_transaction(rig, "AN;", 3, infobuf, &info_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (info_len < 4 || infobuf[0] != 'A' || infobuf[1] != 'N')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n",
                  __func__, info_len);
        return -RIG_ERJCTED;
    }

    *ant_curr = infobuf[2] == '1' ? RIG_ANT_1 : RIG_ANT_2;

    return RIG_OK;
}


/*
 * ic10_get_ptt
 * Assumes rig!=NULL, ptt!=NULL
 */
int ic10_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct kenwood_priv_caps *priv = (struct kenwood_priv_caps *)rig->caps->priv;
    char infobuf[50];
    int retval, iflen, offset;

    retval = get_ic10_if(rig, infobuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* trim extra spaces */
    iflen = ic10_cmd_trim(infobuf, priv->if_len);

    /* IFggmmmkkkhhh snnnzrx yytdfcp */
    /* IFggmmmkkkhhhxxxxxrrrrrssxcctmfcp */
    /* IFggmmmkkkhhhxxxxxrrrrrssxcctmfcp####  what should be if p13/p14/p15 included  */
    /* IF00014074000     +00000000003000000 ; QRP QDX bad IF command -- 36 bytes instead of 33 */
    /* QRP QDX should be 37 bytes but used only 1 byte for p14 instead of 2 bytes */
    /* 12345678901234567890123456789012345678 */
    offset = 5;
    if (iflen == 36) offset = 8; // QRP QDX gets completely bogus length
    else if (iflen == 37) offset = 9; // just incase somebody does this add p13/p14x2/p15

    *ptt = infobuf[iflen - offset] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

    return RIG_OK;
}


// Not referenced anywhere
/*
 * ic10_set_ptt
 * Assumes rig!=NULL
 */
int ic10_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char pttbuf[4];
    int retval;
    unsigned char ptt_letter;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_OFF: ptt_letter = 'R'; break;

    case RIG_PTT_ON : ptt_letter = 'T'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported PTT %d\n",
                  __func__, ptt);
        return -RIG_EINVAL;
    }

    SNPRINTF(pttbuf, sizeof(pttbuf), "%cX;", ptt_letter);
    retval = ic10_transaction(rig, pttbuf, strlen(pttbuf), NULL, 0);

    return retval;
}


/*
 * ic10_get_mem
 * Assumes rig!=NULL
 */
int ic10_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct kenwood_priv_caps *priv = (struct kenwood_priv_caps *)rig->caps->priv;
    char membuf[50];
    int retval, iflen;

    retval = get_ic10_if(rig, membuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* trim extra spaces */
    iflen = ic10_cmd_trim(membuf, priv->if_len);

    /* IFggmmmkkkhhh snnnzrx yytdfcp */
    /* IFggmmmkkkhhhxxxxxrrrrrssxcctmfcp */
    membuf[iflen - 5] = '\0';
    *ch = atoi(membuf + priv->if_len - 7);

    return RIG_OK;
}


/*
 * ic10_set_mem
 * Assumes rig!=NULL
 */
int ic10_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char membuf[8], ackbuf[64];
    int ack_len, retval;

    SNPRINTF(membuf, sizeof(membuf), "MC%02d;", ch);

    retval = ic10_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);

    return retval;
}


int ic10_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    char membuf[16], infobuf[32];
    int retval, info_len;

    SNPRINTF(membuf, sizeof(membuf), "MR00%02d;", chan->channel_num);
    info_len = 24;
    retval = ic10_transaction(rig, membuf, strlen(membuf), infobuf, &info_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* MRs-ccfffffffffffml----; */
    /* 012345678901234567890123 */
    switch (infobuf[17])
    {
    case MD_CW  :   chan->mode = RIG_MODE_CW; break;

    case MD_USB :   chan->mode = RIG_MODE_USB; break;

    case MD_LSB :   chan->mode = RIG_MODE_LSB; break;

    case MD_FM  :   chan->mode = RIG_MODE_FM; break;

    case MD_AM  :   chan->mode = RIG_MODE_AM; break;

    case MD_FSK :   chan->mode = RIG_MODE_RTTY; break;

    case MD_NONE:   chan->mode = RIG_MODE_NONE; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, infobuf[17]);
        return -RIG_EINVAL;
    }

    chan->width = rig_passband_normal(rig, chan->mode);

    sscanf(infobuf + 6, "%011"SCNfreq, &chan->freq);
    chan->vfo = RIG_VFO_MEM;

    if (chan->channel_num >= 90)
    {
        chan->split = 1;
        /* TX VFO (Split channel only) */
        SNPRINTF(membuf, sizeof(membuf), "MR10%02d;", chan->channel_num);
        info_len = 24;
        retval = ic10_transaction(rig, membuf, strlen(membuf), infobuf, &info_len);

        if (retval == RIG_OK && info_len > 17)
        {

            /* MRn rrggmmmkkkhhhdz    ; */
            switch (infobuf[17])
            {
            case MD_CW  :   chan->tx_mode = RIG_MODE_CW; break;

            case MD_USB :   chan->tx_mode = RIG_MODE_USB; break;

            case MD_LSB :   chan->tx_mode = RIG_MODE_LSB; break;

            case MD_FM  :   chan->tx_mode = RIG_MODE_FM; break;

            case MD_AM  :   chan->tx_mode = RIG_MODE_AM; break;

            case MD_FSK :   chan->tx_mode = RIG_MODE_RTTY; break;

            case MD_NONE:   chan->tx_mode = RIG_MODE_NONE; break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                          __func__, infobuf[17]);
                return -RIG_EINVAL;
            }

            chan->tx_width = rig_passband_normal(rig, chan->tx_mode);

            sscanf(infobuf + 6, "%011"SCNfreq, &chan->tx_freq);
        }
    }

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}


int ic10_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char membuf[64];
    int retval, md;
    int64_t freq;

    freq = (int64_t) chan->freq;

    if (chan->channel_num < 90 &&  chan->tx_freq != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Transmit/split can only be on channels 90-99\n",
                  __func__);
        return -RIG_EINVAL;
    }

    switch (chan->mode)
    {
    case RIG_MODE_CW  : md = MD_CW; break;

    case RIG_MODE_USB : md = MD_USB; break;

    case RIG_MODE_LSB : md = MD_LSB; break;

    case RIG_MODE_FM  : md = MD_FM; break;

    case RIG_MODE_AM  : md = MD_AM; break;

    case RIG_MODE_RTTY: md = MD_FSK; break;

    case RIG_MODE_NONE: md = MD_NONE; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(chan->mode));
        return -RIG_EINVAL;
    }

    /* MWnxrrggmmmkkkhhhdzxxxx; */
    SNPRINTF(membuf, sizeof(membuf), "MW0 %02d%011"PRIll"%c0    ;",
             chan->channel_num,
             freq,
             md
            );
    retval = ic10_transaction(rig, membuf, strlen(membuf), NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* TX VFO (Split channel only) */
    freq = chan->tx_freq;

    switch (chan->tx_mode)
    {
    case RIG_MODE_CW:   md = MD_CW; break;

    case RIG_MODE_USB:  md = MD_USB; break;

    case RIG_MODE_LSB:  md = MD_LSB; break;

    case RIG_MODE_FM:   md = MD_FM; break;

    case RIG_MODE_AM:   md = MD_AM; break;

    case RIG_MODE_RTTY: md = MD_FSK; break;

    case RIG_MODE_NONE: md = MD_NONE; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(chan->tx_mode));
        return -RIG_EINVAL;
    }

    if (chan->channel_num >= 90)
    {
        /* MWnxrrggmmmkkkhhhdzxxxx; */
        SNPRINTF(membuf, sizeof(membuf), "MW1 %02d%011"PRIll"%c0    ;",
                 chan->channel_num,
                 freq,
                 md
                );
        retval = ic10_transaction(rig, membuf, strlen(membuf), NULL, 0);

        // I assume we need to check the retval here -- W9MDB
        // This was found from cppcheck
        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: transaction failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }
    }

    return RIG_OK;
}


/*
 * ic10_get_func
 * Assumes rig!=NULL, val!=NULL
 */
int ic10_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char cmdbuf[6], fctbuf[50];
    int fct_len, retval;

    fct_len = 4;

    switch (func)
    {
    case RIG_FUNC_LOCK: SNPRINTF(cmdbuf, sizeof(cmdbuf), "LK;"); break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_func %s",
                  __func__, rig_strfunc(func));
        return -RIG_EINVAL;
    }

    retval = ic10_transaction(rig, cmdbuf, strlen(cmdbuf), fctbuf, &fct_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (fct_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n",
                  __func__, fct_len);
        return -RIG_ERJCTED;
    }

    *status = fctbuf[2] == '0' ? 0 : 1;

    return RIG_OK;
}


/*
 * ic10_set_func
 * Assumes rig!=NULL, val!=NULL
 */
int ic10_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmdbuf[4], fctbuf[16], ackbuf[64];
    int ack_len;

    switch (func)
    {
    case RIG_FUNC_LOCK:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "LK");
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_func %s",
                  __func__, rig_strfunc(func));
        return -RIG_EINVAL;
    }

    SNPRINTF(fctbuf, sizeof(fctbuf), "%s%c;", cmdbuf, status == 0 ? '0' : '1');

    return ic10_transaction(rig, fctbuf, strlen(fctbuf), ackbuf, &ack_len);
}


/*
 * ic10_set_parm
 * Assumes rig!=NULL
 */
int ic10_set_parm(RIG *rig, setting_t parm, value_t val)
{
    char cmdbuf[50];
    int hours;
    int minutes;
    int seconds;

    switch (parm)
    {
    case RIG_PARM_TIME:
        minutes = val.i / 60;
        hours = minutes / 60;
        seconds = val.i - (minutes * 60);
        minutes = minutes % 60;
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "CK1%02d%02d%02d;", hours, minutes, seconds);
        return ic10_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported set_parm %s\n",
                  __func__, rig_strparm(parm));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * ic10_get_parm
 * Assumes rig!=NULL, val!=NULL
 */
int ic10_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    int retval, lvl_len, i;
    char lvlbuf[50];

    switch (parm)
    {
    case RIG_PARM_TIME:
        lvl_len = 10;
        retval = ic10_transaction(rig, "CK1;", 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* "CK1hhmmss;"*/
        if (lvl_len != 10)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n",
                      __func__, lvl_len);
            return -RIG_ERJCTED;
        }

        /* convert ASCII to numeric 0..9 */
        for (i = 3; i < 9; i++)
        {
            lvlbuf[i] -= '0';
        }

        val->i = ((10 * lvlbuf[3] + lvlbuf[4]) * 60 + /* hours */
                  10 * lvlbuf[5] + lvlbuf[6]) * 60 + /* minutes */
                 10 * lvlbuf[7] + lvlbuf[8]; /* seconds */
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported get_parm %s\n",
                  __func__, rig_strparm(parm));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * ic10_set_powerstat
 * Assumes rig!=NULL
 */
int ic10_set_powerstat(RIG *rig, powerstat_t status)
{
    char pwrbuf[16], ackbuf[64];
    int ack_len;

    SNPRINTF(pwrbuf, sizeof(pwrbuf), "PS%c;", status == RIG_POWER_ON ? '1' : '0');

    return ic10_transaction(rig, pwrbuf, strlen(pwrbuf), ackbuf, &ack_len);
}


/*
 * ic10_get_powerstat
 * Assumes rig!=NULL, trn!=NULL
 */
int ic10_get_powerstat(RIG *rig, powerstat_t *status)
{
    char pwrbuf[50];
    int pwr_len, retval;

    pwr_len = 4;
    retval = ic10_transaction(rig, "PS;", 3, pwrbuf, &pwr_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (pwr_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n",
                  __func__, pwr_len);
        return -RIG_ERJCTED;
    }

    *status = pwrbuf[2] == '0' ? RIG_POWER_OFF : RIG_POWER_ON;

    return RIG_OK;
}


/*
 * ic10_set_trn
 * Assumes rig!=NULL
 */
int ic10_set_trn(RIG *rig, int trn)
{
    char trnbuf[16], ackbuf[64];
    int ack_len;

    SNPRINTF(trnbuf, sizeof(trnbuf), "AI%c;", trn == RIG_TRN_RIG ? '1' : '0');

    return ic10_transaction(rig, trnbuf, strlen(trnbuf), ackbuf, &ack_len);
}


/*
 * ic10_get_trn
 * Assumes rig!=NULL, trn!=NULL
 */
int ic10_get_trn(RIG *rig, int *trn)
{
    char trnbuf[50];
    int trn_len, retval;

    trn_len = 38;
    retval = ic10_transaction(rig, "AI;", 3, trnbuf, &trn_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (trn_len != 38)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n",
                  __func__, trn_len);
        return -RIG_ERJCTED;
    }

    *trn = trnbuf[2] != '0' ? RIG_TRN_RIG : RIG_TRN_OFF;

    return RIG_OK;
}


/*
 * ic10_vfo_op
 * Assumes rig!=NULL
 */
int ic10_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    char *cmd, ackbuf[64];
    int ack_len;

    switch (op)
    {
    case RIG_OP_UP    : cmd = "UP;"; break;

    case RIG_OP_DOWN  : cmd = "DN;"; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported op %#x\n",
                  __func__, op);
        return -RIG_EINVAL;
    }

    return ic10_transaction(rig, cmd, 3, ackbuf, &ack_len);
}


/*
 * ic10_scan
 * Assumes rig!=NULL, val!=NULL
 */
int ic10_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    char ackbuf[64];
    int ack_len;

    return ic10_transaction(rig, scan == RIG_SCAN_STOP ? "SC0;" : "SC1;", 4,
                            ackbuf, &ack_len);
}


/*
 * ic10_get_info
 * Assumes rig!=NULL
 */
const char *ic10_get_info(RIG *rig)
{
    char firmbuf[50];
    int firm_len, retval;

    firm_len = 6;
    retval = ic10_transaction(rig, "ID;", 3, firmbuf, &firm_len);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    if (firm_len != 6)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n",
                  __func__, firm_len);
        return NULL;
    }

    switch (firmbuf[4])
    {
    case '4': return "ID: TS-440S";

    case '5': return "ID: R-5000";

    default: return "ID: unknown";
    }
}


/*
 * ic10_decode_event is called by sa_sigio, when some asynchronous
 * data has been received from the rig.
 */
int ic10_decode_event(RIG *rig)
{
    struct kenwood_priv_caps *priv = (struct kenwood_priv_caps *)rig->caps->priv;
    char asyncbuf[128], c;
    int retval, async_len = 128, iflen;
    vfo_t vfo;
    freq_t freq;
    rmode_t mode;
    ptt_t ptt;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = ic10_transaction(rig, NULL, 0, asyncbuf, &async_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Decoding message\n", __func__);



    /* --------------------------------------------------------------------- */
    if (async_len < priv->if_len || asyncbuf[0] != 'I' || asyncbuf[1] != 'F')
    {

        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported transceive cmd '%s'\n",
                  __func__, asyncbuf);
        return -RIG_ENIMPL;
    }

    /* trim extra spaces */
    iflen = ic10_cmd_trim(asyncbuf, priv->if_len);

    /* IFggmmmkkkhhh snnnzrx yytdfcp */
    /* IFggmmmkkkhhhxxxxxrrrrrssxcctmfcp */

    c = asyncbuf[iflen - 3];

    switch (c)
    {
    case '0': vfo = RIG_VFO_A; break;

    case '1': vfo = RIG_VFO_B; break;

    case '2': vfo = RIG_VFO_MEM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %c\n",
                  __func__, c);
        return -RIG_EPROTO;
    }

    c = asyncbuf[iflen - 4];

    switch (c)
    {
    case MD_CW  :   mode = RIG_MODE_CW; break;

    case MD_USB :   mode = RIG_MODE_USB; break;

    case MD_LSB :   mode = RIG_MODE_LSB; break;

    case MD_FM  :   mode = RIG_MODE_FM; break;

    case MD_AM  :   mode = RIG_MODE_AM; break;

    case MD_FSK :   mode = RIG_MODE_RTTY; break;

    case MD_NONE:   mode = RIG_MODE_NONE; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, c);
        return -RIG_EINVAL;
    }

    ptt = asyncbuf[iflen - 5] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

    asyncbuf[13] = '\0';
    sscanf(asyncbuf + 2, "%011"SCNfreq, &freq);

    /* Callback execution */
    if (rig->callbacks.vfo_event)
    {
        rig->callbacks.vfo_event(rig, vfo, rig->callbacks.vfo_arg);
    }

    if (rig->callbacks.freq_event)
    {
        rig->callbacks.freq_event(rig, vfo, freq, rig->callbacks.freq_arg);
    }

    if (rig->callbacks.mode_event)
    {
        rig->callbacks.mode_event(rig, vfo, mode, RIG_PASSBAND_NORMAL,
                                  rig->callbacks.mode_arg);
    }

    if (rig->callbacks.ptt_event)
    {
        rig->callbacks.ptt_event(rig, vfo, ptt, rig->callbacks.ptt_arg);
    }

    return RIG_OK;
}
