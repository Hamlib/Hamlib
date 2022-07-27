/*
 *  Hamlib Drake backend - main file
 *  Copyright (c) 2001-2008 by Stephane Fillod
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
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"

#include "drake.h"


/*
 * Protocol information available at http://www.rldrake.com/swl/R8B.pdf
 */


#define BUFSZ 64

#define CR "\x0d"
#define LF "\x0a"
#define EOM CR

#define MD_USB  '1'
#define MD_LSB  '2'
#define MD_RTTY '3'
#define MD_CW   '4'
#define MD_FM   '5'
#define MD_AM   '6'


/*
 * drake_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 */
int drake_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                      int *data_len)
{
    int retval;
    struct rig_state *rs;

    rs = &rig->state;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* no data expected, TODO: flush input? */
    if (!data || !data_len)
    {
        return 0;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ,
                         LF, 1, 0, 1);

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

int drake_init(RIG *rig)
{
    struct drake_priv_data *priv;
    rig->state.priv = calloc(1, sizeof(struct drake_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->curr_ch = 0;

    return RIG_OK;
}

int drake_cleanup(RIG *rig)
{
    struct drake_priv_data *priv = rig->state.priv;

    free(priv);

    return RIG_OK;
}

/*
 * drake_set_freq
 * Assumes rig!=NULL
 */
int drake_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char freqbuf[16], ackbuf[16];
    int ack_len, retval;

    /*
     * 10Hz resolution
     * TODO: round nearest?
     */
    SNPRINTF((char *) freqbuf, sizeof(freqbuf), "F%07u" EOM,
             (unsigned int)freq / 10);
    retval = drake_transaction(rig, (char *) freqbuf, strlen((char *)freqbuf),
                               (char *) ackbuf,
                               &ack_len);

    return retval;
}


/*
 * drake_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int drake_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int freq_len, retval;
    char freqbuf[BUFSZ];
    double f;
    char fmult;

    retval = drake_transaction(rig, "RF" EOM, 3, freqbuf, &freq_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* RA command returns *fffff.ff*mHz<CR> */
    if (freq_len != 15)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_freq: wrong answer %s, "
                  "len=%d\n", freqbuf, freq_len);
        return -RIG_ERJCTED;
    }

    fmult = freqbuf[10];
    freqbuf[9] = '\0';

    /* extract freq */
    sscanf(freqbuf + 1, "%lf", &f);
    f *= 1000.0;

    if (fmult == 'M' || fmult == 'm')
    {
        f *= 1000.0;
    }

    *freq = (freq_t)f;

    return RIG_OK;
}

/*
 * drake_set_vfo
 * Assumes rig!=NULL
 */
int drake_set_vfo(RIG *rig, vfo_t vfo)
{
    unsigned char cmdbuf[16], ackbuf[16];
    int ack_len, retval;
    char vfo_function;

    switch (vfo)
    {
    case RIG_VFO_A  : vfo_function = 'A'; break;

    case RIG_VFO_B  : vfo_function = 'B'; break;

    case RIG_VFO_VFO: vfo_function = 'F'; break;

    case RIG_VFO_MEM: vfo_function = 'C'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "drake_set_vfo: unsupported VFO %s\n",
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if ((vfo_function == 'A') || (vfo_function == 'B'))
    {
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "V%c" EOM, vfo_function);
    }

    if ((vfo_function == 'F') || (vfo_function == 'C'))
    {
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "%c" EOM, vfo_function);
    }

    retval = drake_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) ackbuf,
                               &ack_len);
    return retval;
}


/*
 * drake_get_vfo
 * Assumes rig!=NULL
 */
int drake_get_vfo(RIG *rig, vfo_t *vfo)
{
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];

    retval = drake_transaction(rig, "RA" EOM, 3, mdbuf, &mdbuf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len < 35)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_vfo: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    if (mdbuf[0] == '*')
    {
        *vfo = RIG_VFO_MEM;
    }
    else
    {
        char cvfo = (mdbuf[9] & 0x38);

        switch (cvfo)
        {
        case '0' : *vfo = RIG_VFO_B; break;

        case '8' : *vfo = RIG_VFO_A; break;

        default : rig_debug(RIG_DEBUG_ERR,
                                "drake_get_vfo: unsupported vfo %c\n", cvfo);
            *vfo = RIG_VFO_VFO;
            return -RIG_EINVAL;
        }
    }

    return RIG_OK;
}

/*
 * drake_set_mode
 * Assumes rig!=NULL
 */
int drake_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char mdbuf[16], ackbuf[16];
    unsigned char mode_sel;
    int ack_len, retval;

    switch (mode)
    {
    case RIG_MODE_CW:       mode_sel = MD_CW; break;

    case RIG_MODE_ECSSUSB:
    case RIG_MODE_USB:      mode_sel = MD_USB; break;

    case RIG_MODE_ECSSLSB:
    case RIG_MODE_LSB:      mode_sel = MD_LSB; break;

    case RIG_MODE_FM:       mode_sel = MD_FM; break;

    case RIG_MODE_AMS:
    case RIG_MODE_AM:       mode_sel = MD_AM; break;

    case RIG_MODE_RTTY:     mode_sel = MD_RTTY; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "drake_set_mode: "
                  "unsupported mode %s\n", rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF((char *) mdbuf, sizeof(mdbuf), "M%c" EOM, mode_sel);
    retval = drake_transaction(rig, (char *) mdbuf, strlen((char *)mdbuf),
                               (char *) ackbuf,
                               &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (mode != RIG_MODE_FM)
        {
            unsigned int width_sel;

            if (width == RIG_PASSBAND_NORMAL)
            {
                width = rig_passband_normal(rig, mode);
            }

            if (width <= 500)
            {
                width_sel = '0';
            }
            else if (width <= 1800)
            {
                width_sel = '1';
            }
            else if (width <= 2300)
            {
                width_sel = '2';
            }
            else if (width <= 4000)
            {
                width_sel = '4';
            }
            else
            {
                width_sel = '6';
            }

            SNPRINTF((char *) mdbuf, sizeof(mdbuf), "W%c" EOM, width_sel);
            retval = drake_transaction(rig, (char *) mdbuf, strlen((char *)mdbuf),
                                       (char *) ackbuf,
                                       &ack_len);
        }
    }

    if ((mode == RIG_MODE_AMS) || (mode == RIG_MODE_ECSSUSB)
            || (mode == RIG_MODE_ECSSLSB) ||
            (mode == RIG_MODE_AM) || (mode == RIG_MODE_USB) || (mode == RIG_MODE_LSB))
    {
        SNPRINTF((char *) mdbuf, sizeof(mdbuf), "S%c" EOM,
                 ((mode == RIG_MODE_AMS) || (mode == RIG_MODE_ECSSUSB)
                  || (mode == RIG_MODE_ECSSLSB))
                 ? 'O' : 'F');
        retval = drake_transaction(rig, (char *) mdbuf, strlen((char *)mdbuf),
                                   (char *) ackbuf,
                                   &ack_len);
    }

    return retval;
}


/*
 * drake_get_mode
 * Assumes rig!=NULL
 */
int drake_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];
    char cmode;
    char cwidth;
    char csynch;

    retval = drake_transaction(rig, "RM" EOM, 3, mdbuf, &mdbuf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != 8)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_mode: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    cmode = mdbuf[3];
    cwidth = mdbuf[4];
    csynch = mdbuf[5];

    switch (cwidth & 0x37)
    {
    case '0': *width = s_Hz(500); break;

    case '1': *width = s_Hz(1800); break;

    case '2': *width = s_Hz(2300); break;

    case '3': *width = s_Hz(4000); break;

    case '4': *width = s_Hz(6000); break;

    default :
        rig_debug(RIG_DEBUG_ERR,
                  "drake_get_mode: unsupported width %c\n",
                  cwidth);
        *width = RIG_PASSBAND_NORMAL;
        return -RIG_EINVAL;
    }

    if ((cwidth >= '0') && (cwidth <= '4'))
    {
        switch (cmode & 0x33)
        {
        case '0': *mode = RIG_MODE_LSB; break;

        case '1': *mode = RIG_MODE_RTTY; break;

        case '2': *mode = RIG_MODE_FM; *width = s_Hz(12000); break;

        default :
            rig_debug(RIG_DEBUG_ERR,
                      "drake_get_mode: unsupported mode %c\n",
                      cmode);
            *mode = RIG_MODE_NONE;
            return -RIG_EINVAL;
        }
    }
    else
    {
        switch (cmode & 0x33)
        {
        case '0': *mode = RIG_MODE_USB; break;

        case '1': *mode = RIG_MODE_CW; break;

        case '2': *mode = RIG_MODE_AM; break;

        default :
            rig_debug(RIG_DEBUG_ERR,
                      "drake_get_mode: unsupported mode %c\n",
                      cmode);
            *mode = RIG_MODE_NONE;
            return -RIG_EINVAL;
        }
    }

    if ((csynch & 0x34) == '4')
    {
        if (*mode == RIG_MODE_AM)
        {
            *mode = RIG_MODE_AMS;
        }
        else if (*mode == RIG_MODE_USB)
        {
            *mode = RIG_MODE_ECSSUSB;
        }
        else if (*mode == RIG_MODE_LSB)
        {
            *mode = RIG_MODE_ECSSLSB;
        }
    }

    return RIG_OK;
}

/*
 * drake_set_ant
 * Assumes rig!=NULL
 */
int drake_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    unsigned char buf[16], ackbuf[16];
    int ack_len, retval;

    SNPRINTF((char *) buf, sizeof(buf), "A%c" EOM,
             ant == RIG_ANT_1 ? '1' : (ant == RIG_ANT_2 ? '2' : 'C'));

    retval = drake_transaction(rig, (char *) buf, strlen((char *)buf),
                               (char *) ackbuf, &ack_len);

    return retval;
}

/*
 * drake_get_ant
 * Assumes rig!=NULL
 */
int drake_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                  ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];
    char cant;

    retval = drake_transaction(rig, "RM" EOM, 3, mdbuf, &mdbuf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != 8)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_ant: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    cant = mdbuf[3];

    switch (cant & 0x3c)
    {
    case '0': *ant_curr = RIG_ANT_1; break;

    case '4': *ant_curr = RIG_ANT_3; break;

    case '8': *ant_curr = RIG_ANT_2; break;

    default :
        rig_debug(RIG_DEBUG_ERR,
                  "drake_get_ant: unsupported antenna %c\n",
                  cant);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * drake_set_mem
 * Assumes rig!=NULL
 */
int drake_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int ack_len, retval;
    char buf[16], ackbuf[16];
    struct drake_priv_data *priv = rig->state.priv;

    priv->curr_ch = ch;

    SNPRINTF(buf, sizeof(buf), "C%03d" EOM, ch);

    ack_len = 0; // fix compile-time warning "possibly uninitialized"
    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    if (ack_len != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_set_mem: could not set channel %03d.\n", ch);
        retval = -RIG_ERJCTED;
    }

    return retval;
}

/*
 * drake_get_mem
 * Assumes rig!=NULL
 */
int drake_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct drake_priv_data *priv = rig->state.priv;
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];
    int chan;

    retval = drake_transaction(rig, "RC" EOM, 3, mdbuf, &mdbuf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != 6)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_mem: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    mdbuf[4] = '\0';

    /* extract channel no */
    sscanf(mdbuf + 1, "%03d", &chan);
    *ch = chan;

    priv->curr_ch = chan;

    return RIG_OK;
}

/*
 * drake_set_chan
 * Assumes rig!=NULL
 */
int drake_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct drake_priv_data *priv = rig->state.priv;
    vfo_t   old_vfo;
    int     old_chan;
    char    mdbuf[16], ackbuf[16];
    int     ack_len, retval;
    value_t dummy;

    dummy.i = 0;

    drake_get_vfo(rig, &old_vfo);
    old_chan = 0;

    /* set to vfo if needed */
    if (old_vfo == RIG_VFO_MEM)
    {
        old_chan = priv->curr_ch;
        retval = drake_set_vfo(rig, RIG_VFO_VFO);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    /* set all memory features */
    drake_set_ant(rig, RIG_VFO_CURR, chan->ant, dummy);
    drake_set_freq(rig, RIG_VFO_CURR, chan->freq);
    drake_set_mode(rig, RIG_VFO_CURR, chan->mode, chan->width);
    drake_set_func(rig, RIG_VFO_CURR, RIG_FUNC_NB,
                   (chan->funcs & RIG_FUNC_NB) == RIG_FUNC_NB);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_AGC,
                    chan->levels[rig_setting2idx(RIG_LEVEL_AGC)]);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_PREAMP,
                    chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)]);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_ATT,
                    chan->levels[rig_setting2idx(RIG_LEVEL_ATT)]);
    drake_set_func(rig, RIG_VFO_CURR, RIG_FUNC_MN,
                   (chan->funcs & RIG_FUNC_MN) == RIG_FUNC_MN);

    SNPRINTF(mdbuf, sizeof(mdbuf), "PR" EOM "%03d" EOM, chan->channel_num);
    retval = drake_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

    if (old_vfo == RIG_VFO_MEM)
    {
        drake_set_mem(rig, RIG_VFO_CURR, old_chan);
    }

    return retval;
}

/*
 * drake_get_chan
 * Assumes rig!=NULL
 */
int drake_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    struct drake_priv_data *priv = rig->state.priv;
    vfo_t   old_vfo;
    int     old_chan;
    char    mdbuf[BUFSZ], freqstr[BUFSZ];
    int     mdbuf_len, retval;

    chan->vfo = RIG_VFO_MEM;
    chan->ant = RIG_ANT_NONE;
    chan->freq = 0;
    chan->mode = RIG_MODE_NONE;
    chan->width = RIG_PASSBAND_NORMAL;
    chan->tx_freq = 0;
    chan->tx_mode = RIG_MODE_NONE;
    chan->tx_width = RIG_PASSBAND_NORMAL;
    chan->split = RIG_SPLIT_OFF;
    chan->tx_vfo = RIG_VFO_NONE;
    chan->rptr_shift = RIG_RPT_SHIFT_NONE;
    chan->rptr_offs = 0;
    chan->tuning_step = 0;
    chan->rit = 0;
    chan->xit = 0;
    chan->funcs = 0;
    chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_OFF;
    chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 0;
    chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = 0;
    chan->ctcss_tone = 0;
    chan->ctcss_sql = 0;
    chan->dcs_code = 0;
    chan->dcs_sql = 0;
    chan->scan_group = 0;
    chan->flags = RIG_CHFLAG_SKIP;
    strcpy(chan->channel_desc, "       ");

    drake_get_vfo(rig, &old_vfo);
    old_chan = 0;

    if (old_vfo == RIG_VFO_MEM)
    {
        old_chan = priv->curr_ch;
    }

    //go to new channel
    retval = drake_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

    if (retval != RIG_OK)
    {
        return RIG_OK;
    }

    //now decipher it
    retval = drake_transaction(rig, "RA" EOM, 3, mdbuf, &mdbuf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len < 35)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_channel: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    if ((mdbuf[5] >= '4') && (mdbuf[5] <= '?'))
    {
        chan->funcs |= RIG_FUNC_NB;
    }

    switch (mdbuf[5] & 0x33)
    {
    case '0': chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_OFF; break;

    case '2': chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_FAST; break;

    case '3': chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_SLOW; break;

    default : chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_FAST;
    }

    if ((mdbuf[6] & 0x3c) == '8')
    {
        chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = 10;
    }

    if ((mdbuf[6] & 0x3c) == '4')
    {
        chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 10;
    }

    if ((mdbuf[6] & 0x32) == '2')
    {
        chan->funcs |= RIG_FUNC_MN;
    }

    switch (mdbuf[7] & 0x3c)
    {
    case '0': chan->ant = RIG_ANT_1; break;

    case '4': chan->ant = RIG_ANT_3; break;

    case '8': chan->ant = RIG_ANT_2; break;

    default : chan->ant = RIG_ANT_NONE;
    }

    switch (mdbuf[8] & 0x37)
    {
    case '0': chan->width = s_Hz(500); break;

    case '1': chan->width = s_Hz(1800); break;

    case '2': chan->width = s_Hz(2300); break;

    case '3': chan->width = s_Hz(4000); break;

    case '4': chan->width = s_Hz(6000); break;

    default : chan->width = RIG_PASSBAND_NORMAL;
    }

    if ((mdbuf[8] >= '0') && (mdbuf[8] <= '4'))
    {
        switch (mdbuf[7] & 0x33)
        {
        case '0': chan->mode = RIG_MODE_LSB; break;

        case '1': chan->mode = RIG_MODE_RTTY; break;

        case '2': chan->mode = RIG_MODE_FM;
            chan->width = s_Hz(12000); break;

        default : chan->mode = RIG_MODE_NONE;
        }
    }
    else
    {
        switch (mdbuf[7] & 0x33)
        {
        case '0': chan->mode = RIG_MODE_USB; break;

        case '1': chan->mode = RIG_MODE_CW; break;

        case '2': chan->mode = RIG_MODE_AM; break;

        default : chan->mode = RIG_MODE_NONE;
        }
    }

    if ((mdbuf[9] & 0x34) == '4')
    {
        if (chan->mode == RIG_MODE_AM)
        {
            chan->mode = RIG_MODE_AMS;
        }
        else if (chan->mode == RIG_MODE_USB)
        {
            chan->mode = RIG_MODE_ECSSUSB;
        }
        else if (chan->mode == RIG_MODE_LSB)
        {
            chan->mode = RIG_MODE_ECSSLSB;
        }
    }

    strncpy(freqstr, mdbuf + 11, 9);
    freqstr[9] = 0x00;

    if ((mdbuf[21] == 'k') || (mdbuf[21] == 'K'))
    {
        chan->freq = strtod(freqstr, NULL) * 1000.0;
    }

    if ((mdbuf[21] == 'm') || (mdbuf[21] == 'M'))
    {
        chan->freq = strtod(freqstr, NULL) * 1000000.0;
    }


    strncpy(chan->channel_desc, mdbuf + 25, 7);
    chan->channel_desc[7] = '\0'; // in case strncpy did not terminate the string

    //now put the radio back the way it was
    //we apparently can't do a read-only channel read
    if (old_vfo != RIG_VFO_MEM)
    {
        retval = drake_set_vfo(rig, RIG_VFO_VFO);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }
    else
    {
        retval = drake_set_mem(rig, RIG_VFO_CURR, old_chan);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return RIG_OK;
}

/*
 * drake_vfo_op
 * Assumes rig!=NULL
 */
int drake_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct drake_priv_data *priv = rig->state.priv;
    char buf[16], ackbuf[16];
    int len, ack_len, retval;

    switch (op)
    {
    case RIG_OP_UP:
        SNPRINTF(buf, sizeof(buf), "U");
        break;

    case RIG_OP_DOWN:
        SNPRINTF(buf, sizeof(buf), "D");
        break;

    case RIG_OP_CPY:
        SNPRINTF(buf, sizeof(buf), "A E B" EOM);
        break;

    case RIG_OP_TO_VFO:
        /* len = SNPRINTF(buf,"C%03d" EOM, priv->curr_ch); */
        SNPRINTF(buf, sizeof(buf), "F" EOM);
        break;

    case RIG_OP_MCL:
        SNPRINTF(buf, sizeof(buf), "EC%03d" EOM, priv->curr_ch);
        break;

    case RIG_OP_FROM_VFO:
        SNPRINTF(buf, sizeof(buf), "PR" EOM "%03d" EOM, priv->curr_ch);
        break;

    default:
        return -RIG_EINVAL;
    }

    len = strlen(buf);
    retval = drake_transaction(rig, buf, len, buf[len - 1] == 0x0d ? ackbuf : NULL,
                               &ack_len);

    return retval;
}

/*
 * drake_set_func
 * Assumes rig!=NULL
 */
int drake_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[16], ackbuf[16];
    int ack_len, retval;

    switch (func)
    {
    case RIG_FUNC_MN:
        SNPRINTF(buf, sizeof(buf), "N%c" EOM, status ? 'O' : 'F');
        break;

    case RIG_FUNC_LOCK:
        SNPRINTF(buf, sizeof(buf), "L%c" EOM, status ? 'O' : 'F');
        break;

    case RIG_FUNC_NB:
        /* TODO: NB narrow */
        SNPRINTF(buf, sizeof(buf), "B%c" EOM, status ? 'W' : 'F');
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    return retval;
}

/*
 * drake_get_func
 * Assumes rig!=NULL
 */
int drake_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];
    char mc;

    retval = drake_transaction(rig, "RM" EOM, 3, mdbuf, &mdbuf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != 8)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_func: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    switch (func)
    {
    case RIG_FUNC_MN:
        mc = mdbuf[2];
        *status = ((mc & 0x32) == '2');
        break;

    case RIG_FUNC_NB:
        /* TODO: NB narrow */
        mc = mdbuf[1];
        *status = ((mc >= '4') && (mc <= '?'));
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get func %s\n", rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * drake_set_level
 * Assumes rig!=NULL
 */
int drake_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[16], ackbuf[16];
    int ack_len, retval;

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        SNPRINTF(buf, sizeof(buf), "G%c" EOM, val.i ? '+' : '0');
        break;

    case RIG_LEVEL_ATT:
        SNPRINTF(buf, sizeof(buf), "G%c" EOM, val.i ? '-' : '0');
        break;

    case RIG_LEVEL_AGC:
        SNPRINTF(buf, sizeof(buf), "A%c" EOM,
                 val.i == RIG_AGC_OFF ? 'O' :
                 (val.i == RIG_AGC_FAST ? 'F' : 'S'));
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    return retval;
}

/*
 * drake_get_level
 * Assumes rig!=NULL
 */
int drake_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int lvl_len, retval, ss;
    char lvlbuf[BUFSZ];
    char mc;

    if ((level != RIG_LEVEL_RAWSTR) && (level != RIG_LEVEL_STRENGTH))
    {
        retval = drake_transaction(rig, "RM" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 8)
        {
            rig_debug(RIG_DEBUG_ERR, "drake_get_level: wrong answer %s, "
                      "len=%d\n", lvlbuf, lvl_len);
            return -RIG_ERJCTED;
        }
    }

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        retval = drake_transaction(rig, "RSS" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "drake_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        lvlbuf[3] = '\0';
        val->i =  strtol(lvlbuf + 1, (char **)NULL, 16);
        break;

    case RIG_LEVEL_STRENGTH:
        retval = drake_transaction(rig, "RSS" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "drake_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        lvlbuf[3] = '\0';
        ss =  strtol(lvlbuf + 1, (char **)NULL, 16);
        val->i = (int)rig_raw2val(ss, &rig->caps->str_cal);
        break;

    case RIG_LEVEL_PREAMP:
        mc = lvlbuf[2];

        if ((mc & 0x3c) == '8')
        {
            val->i = 10;
        }
        else
        {
            val->i = 0;
        }

        break;

    case RIG_LEVEL_ATT:
        mc = lvlbuf[2];

        if ((mc & 0x3c) == '4')
        {
            val->i = 10;
        }
        else
        {
            val->i = 0;
        }

        break;

    case RIG_LEVEL_AGC:
        mc = lvlbuf[1];

        switch (mc & 0x33)
        {
        case '0': val->i = RIG_AGC_OFF; break;

        case '2': val->i = RIG_AGC_FAST; break;

        case '3': val->i = RIG_AGC_SLOW; break;

        default : val->i = RIG_AGC_FAST;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %s\n", rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int drake_set_powerstat(RIG *rig, powerstat_t status)
{
    char buf[16], ackbuf[16];
    int ack_len, retval;

    SNPRINTF(buf, sizeof(buf), "P%c" EOM, status == RIG_POWER_OFF ? 'F' : 'O');

    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    return retval;
}

int drake_get_powerstat(RIG *rig, powerstat_t *status)
{
    int mdlen, retval;
    char mdbuf[BUFSZ];

    retval = drake_transaction(rig, "RM" EOM, 3, mdbuf, &mdlen);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *status = (mdlen == 8);

    return RIG_OK;
}



/*
 * drake_set_freq
 * Assumes rig!=NULL
 */
const char *drake_get_info(RIG *rig)
{
    static char idbuf[BUFSZ];
    int retval, id_len;

    retval = drake_transaction(rig, "ID" EOM, 3, idbuf, &id_len);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    idbuf[id_len] = '\0';

    return idbuf;
}


/*
 * initrigs_drake is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(drake)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&r8a_caps);
    rig_register(&r8b_caps);

    return RIG_OK;
}

/*
 * probe_drake(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(drake)
{
    static char idbuf[BUFSZ];
    int retval, id_len;

    if (!port)
    {
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
    }

    port->parm.serial.rate = r8b_caps.serial_rate_max;
    port->write_delay = port->post_write_delay = 0;
    port->timeout = 50;
    port->retry = 1;

    retval = serial_open(port);

    if (retval != RIG_OK)
    {
        return RIG_MODEL_NONE;
    }

    retval = write_block(port, (unsigned char *) "ID" EOM, 3);
    id_len = read_string(port, (unsigned char *) idbuf, BUFSZ, LF, 1, 0, 1);

    close(port->fd);

    if (retval != RIG_OK || id_len <= 0 || id_len >= BUFSZ)
    {
        return RIG_MODEL_NONE;
    }

    idbuf[id_len] = '\0';

    if (!strcmp(idbuf, "R8B"))
    {
        if (cfunc)
        {
            (*cfunc)(port, RIG_MODEL_DKR8B, data);
        }

        return RIG_MODEL_DKR8B;
    }

    if (!strcmp(idbuf, "R8A"))      /* TBC */
    {
        if (cfunc)
        {
            (*cfunc)(port, RIG_MODEL_DKR8A, data);
        }

        return RIG_MODEL_DKR8A;
    }

    /*
     * not found...
     */
    if (memcmp(idbuf, "ID" EOM, 3)) /* catch loopback serial */
        rig_debug(RIG_DEBUG_VERBOSE, "probe_drake: found unknown device "
                  "with ID '%s', please report to Hamlib "
                  "developers.\n", idbuf);

    return RIG_MODEL_NONE;
}

