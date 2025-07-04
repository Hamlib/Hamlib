/*
 *  Hamlib Drake backend - R-8 description
 *  Copyright (c) 2001-2010, 2025 by Stephane Fillod
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

//#include <stdio.h>
#include <stdlib.h>
//#include <stdbool.h>
//#include <string.h>  /* String function definitions */
//#include <unistd.h>  /* UNIX standard function definitions */

#include <hamlib/rig.h>
#include "idx_builtin.h"
//#include "serial.h"
//#include "misc.h"
//#include "cal.h"
//#include "register.h"

#include "drake.h"

int drake_r8_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan);
int drake_r8_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);

#define BUFSZ 64

#define CR "\x0d"
#define LF "\x0a"
#define EOM CR

#define R8_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM|RIG_MODE_AMS|RIG_MODE_FM)

#define R8_FUNC (RIG_FUNC_MN|RIG_FUNC_NB|RIG_FUNC_NB2)

#define R8_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define R8_PARM_ALL (RIG_PARM_TIME)

#define R8_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_VFO|RIG_VFO_MEM)

#define R8_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

#define R8_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)

#define R8_STR_CAL { 2, { \
        {   0, -60 }, \
        {   1,   0 }, \
    } }

/*
 * channel caps.
 */
#define DRAKE_MEM_CAP { \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .ant = 1,   \
    .funcs = R8_FUNC,  \
    .levels = RIG_LEVEL_AGC|RIG_LEVEL_ATT|RIG_LEVEL_PREAMP, \
}

/*
 * R-8 rig capabilities.
 *
 * specs: http://www.dxing.com/rx/r8.htm
 *
 */

struct rig_caps r8_caps =
{
    RIG_MODEL(RIG_MODEL_DKR8),
    .model_name = "R-8",
    .mfg_name =  "Drake",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_NEW,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  7,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_EVEN,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  1,
    .post_write_delay =  100, //1,
    .timeout =  250,
    .retry =  3,

    .has_get_func =  R8_FUNC,
    .has_set_func =  R8_FUNC,
    .has_get_level =  R8_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(R8_LEVEL_ALL),
    .has_get_parm =  R8_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(R8_PARM_ALL),
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_ATT] = { .min = { .i = 0 }, .max = { .i = 10 } },
        [LVL_PREAMP] = { .min = { .i = 0 }, .max = { .i = 10 } },
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END },
    .attenuator =   { 10, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .vfo_ops =  R8_VFO_OPS,
    .scan_ops = 0,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .priv =  NULL,

    .chan_list =  {
        {   0,  99, RIG_MTYPE_MEM, DRAKE_MEM_CAP },
        RIG_CHAN_END
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(30), R8_MODES, -1, -1, R8_VFO, R8_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(30), R8_MODES, -1, -1, R8_VFO, R8_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  {
        {R8_MODES, 10},
        {R8_MODES, 100},
        {R8_MODES, kHz(1)},
        {R8_MODES, kHz(10)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(4)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(2.3)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(1.8)},
        {RIG_MODE_AM | RIG_MODE_AMS, Hz(500)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(2.3)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(1.8)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(4)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(6)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_CW, kHz(1.8)},
        {RIG_MODE_CW, kHz(2.3)},
        {RIG_MODE_CW, kHz(4)},
        {RIG_MODE_CW, kHz(6)},
        RIG_FLT_END,
    },
    .str_cal = R8_STR_CAL,

    .rig_init = drake_init,
    .rig_cleanup = drake_cleanup,

    .set_freq =  drake_set_freq,
    .get_freq =  drake_get_freq,
    .set_vfo =  drake_set_vfo,
    .get_vfo =  drake_get_vfo,
    .set_mode =  drake_set_mode,
    .get_mode =  drake_get_mode,
    .set_func = drake_set_func,
    .get_func = drake_get_func,
    .set_level = drake_set_level,
    .get_level = drake_get_level,
    .set_ant =  drake_set_ant,
    .get_ant =  drake_get_ant,
    .set_mem = drake_set_mem,
    .get_mem = drake_get_mem,
    .set_channel = drake_r8_set_chan,
    .get_channel = drake_r8_get_chan,
    .vfo_op = drake_vfo_op,
    .set_powerstat = drake_set_powerstat,
    .get_powerstat = drake_get_powerstat,
    .get_info =  drake_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

/*
 * drake_set_chan
 * Assumes rig!=NULL
 */
int drake_r8_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    const struct drake_priv_data *priv = STATE(rig)->priv;
    vfo_t   old_vfo;
    int     old_chan;
    char    mdbuf[16];
    char    ackbuf[BUFSZ];
    int     ack_len;
    int     retval;
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
    drake_set_func(rig, RIG_VFO_CURR, RIG_FUNC_NB2,
                  (chan->funcs & RIG_FUNC_NB2) == RIG_FUNC_NB2);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_AGC,
                   chan->levels[rig_setting2idx(RIG_LEVEL_AGC)]);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_PREAMP,
                   chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)]);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_ATT,
                   chan->levels[rig_setting2idx(RIG_LEVEL_ATT)]);
    drake_set_func(rig, RIG_VFO_CURR, RIG_FUNC_MN,
                  (chan->funcs & RIG_FUNC_MN) == RIG_FUNC_MN);

    SNPRINTF(mdbuf, sizeof(mdbuf), "PR" EOM "%02d" EOM, chan->channel_num);
    retval = drake_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[2] = {0x0d, 0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ackbuf[1] = testbuf[1];
        ack_len = 2;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

    drake_trans_rept("drake_set_chan", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);

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
int drake_r8_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    vfo_t   old_vfo;
    int     old_chan;
    int     retval;
    const struct drake_priv_data *priv = STATE(rig)->priv;

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

    retval = drake_report_all(rig, "drake_get_chan");

    if (retval != RIG_OK)
    {
        return RIG_OK;
    }

    if (priv->curr_nb)
    {
        chan->funcs |= RIG_FUNC_NB;
    }
    if (priv->curr_nb2)
    {
        chan->funcs |= RIG_FUNC_NB2;
    }

    chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = priv->curr_agc;
    chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = (priv->curr_pre ? 10 : 0);
    chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = (priv->curr_att ? 10 : 0);

    if (priv->curr_notch)
    {
        chan->funcs |= RIG_FUNC_MN;
    }
 
    chan->ant = priv->curr_ant;
    chan->width = priv->curr_width;
    chan->mode = priv->curr_mode;
    chan->freq = priv->curr_freq;

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

