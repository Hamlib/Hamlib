/*
 *  Hamlib Skanti backend - TRP8255 description
 *  Copyright (c) 2010 by Stephane Fillod
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

#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "skanti.h"
#include "iofunc.h"


#define TRP8255_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define TRP8255_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)
#define TRP8255_AM_TX_MODES RIG_MODE_AM

#define TRP8255_FUNC (RIG_FUNC_MUTE)

#define TRP8255_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AGC|\
        RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|RIG_LEVEL_SQL)

#define TRP8255_PARM_ALL (RIG_PARM_TIME|RIG_PARM_BACKLIGHT)

#define TRP8255_VFO_OPS (RIG_OP_TUNE|RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_CPY)

#define TRP8255_VFO (RIG_VFO_A)

#define TRP8255_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .tx_freq = 1,   \
    }

/*
 * Private data
 */
struct cu_priv_data
{
    split_t split;  /* current emulated split state */
    int ch;         /* current memorized memory channel */
};

static int cu_transaction(RIG *rig, const char *cmd, int cmd_len);
static int cu_open(RIG *rig);
static int cu_close(RIG *rig);
static int cu_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int cu_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int cu_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int cu_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
static int cu_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int cu_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int cu_set_parm(RIG *rig, setting_t parm, value_t val);
static int cu_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
static int cu_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int cu_set_mem(RIG *rig, vfo_t vfo, int ch);
static int cu_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

/*
 * TRP 8255 rig capabilities.
 * The protocol is different than TRP8000,
 * because the TRP8255 has the "CU" (Control Unit).
 *
 */
const struct rig_caps trp8255_caps =
{
    RIG_MODEL(RIG_MODEL_TRP8255),
    .model_name = "TRP 8255 S R",
    .mfg_name =  "Skanti",
    .version =  "20200323.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  2400,
    .serial_data_bits =  7,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_ODD,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  2000,
    .retry =  3,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  TRP8255_FUNC,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_SET(TRP8255_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_SET(TRP8255_PARM_ALL),
    .vfo_ops =  TRP8255_VFO_OPS,
    .preamp =   { 10, RIG_DBLST_END },  /* TBC */
    .attenuator =   { 20, RIG_DBLST_END },  /* TBC */
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   0,  76, RIG_MTYPE_MEM, TRP8255_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(500), MHz(30), TRP8255_ALL_MODES, -1, -1, TRP8255_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {MHz(2), MHz(30), TRP8255_AM_TX_MODES, W(4), W(40), TRP8255_VFO},
        {MHz(2), MHz(30), TRP8255_OTHER_TX_MODES, W(10), W(100), TRP8255_VFO},
        RIG_FRNG_END,
    },
    .tuning_steps =  {
        {TRP8255_ALL_MODES, 10},
        {TRP8255_ALL_MODES, 100},
        {TRP8255_ALL_MODES, kHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        /* rough guesses */
        {TRP8255_ALL_MODES, kHz(2.7)},  /* intermit */
        {TRP8255_ALL_MODES, kHz(2.1)},  /* narrow */
        {TRP8255_ALL_MODES, kHz(6)},    /* wide */
        {TRP8255_ALL_MODES, Hz(500)},   /* very narrow */
        RIG_FLT_END,
    },

    .rig_open =   cu_open,
    .rig_close =  cu_close,
    .set_freq =   cu_set_freq,
    .set_mode =   cu_set_mode,
    .set_split_freq =  cu_set_split_freq,
    .set_split_vfo =  cu_set_split_vfo,
    .set_ptt =  cu_set_ptt,
    .set_mem =  cu_set_mem,
    .vfo_op =   cu_vfo_op,
    .set_level =  cu_set_level,
    .set_func  =  cu_set_func,
    .set_parm  =  cu_set_parm,
    .set_ts    =  cu_set_ts,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

#define ACK 0x06
#define NACK 0x15
#define CR "\x0d"

/* TODO: retry */
static int cu_transaction(RIG *rig, const char *cmd, int cmd_len)
{
    int i;
    char retchar;

    for (i = 0; i < cmd_len; i++)
    {

        int ret = write_block(&rig->state.rigport, (unsigned char *) &cmd[i], 1);

        if (ret != RIG_OK)
        {
            return ret;
        }

        ret = read_block(&rig->state.rigport, (unsigned char *) &retchar, 1);

        switch (retchar)
        {
        case ACK: continue;

        case NACK:
            return -RIG_ERJCTED;

        default: return -RIG_EPROTO;
        }
    }

    return RIG_OK;
}

static int cu_open(RIG *rig)
{
    char cmd[] = { 0x01, 0x02 }; /* SOH, STX */
    struct cu_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    rig->state.priv = calloc(1, sizeof(struct cu_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = (struct cu_priv_data *)rig->state.priv;

    memset(priv, 0, sizeof(struct cu_priv_data));
    priv->split = RIG_SPLIT_OFF;
    priv->ch = 0;

    return cu_transaction(rig, cmd, 2);
}

static int cu_close(RIG *rig)
{
    char cmd[] = { 0x16 }; /* DLE */
    struct cu_priv_data *priv = (struct cu_priv_data *)rig->state.priv;

    free(priv);

    return cu_transaction(rig, cmd, 1);
}

int cu_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct cu_priv_data *priv = (struct cu_priv_data *)rig->state.priv;
    char cmdbuf[16];
    int ret;

    if (freq >= MHz(100))
    {
        return -RIG_EINVAL;
    }

    /* RX freq */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), ":%06u"CR, (unsigned)(freq / Hz(100)));

    ret = cu_transaction(rig, cmdbuf, strlen(cmdbuf));

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (priv->split != RIG_SPLIT_ON)
    {
        return cu_vfo_op(rig, vfo, RIG_OP_CPY);
    }

    return RIG_OK;
}

static int cu_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct cu_priv_data *priv = (struct cu_priv_data *)rig->state.priv;

    priv->split = split;

    return RIG_OK;
}

int cu_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char cmdbuf[16];

    if (freq >= MHz(100))
    {
        return -RIG_EINVAL;
    }

    /* TX freq */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), ";%06u"CR, (unsigned)(freq / Hz(100)));

    return cu_transaction(rig, cmdbuf, strlen(cmdbuf));
}

int cu_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char *cmd;
    int ret;

    switch (mode)
    {
    case RIG_MODE_USB: cmd = "X"; break;

    case RIG_MODE_LSB: cmd = "Y"; break;

    case RIG_MODE_AM:  cmd = "Z"; break;

    case RIG_MODE_RTTY: cmd = "["; break;

    /* case RIG_MODE_R3E: cmd = "\\"; break; */
    case RIG_MODE_CW:  cmd = "]"; break;

    /* case RIG_MODE_MCW: cmd = '^'; break; */

    default:
        return -RIG_EINVAL;
    }

    ret = cu_transaction(rig, cmd, 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return ret; }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    if (width < rig_passband_normal(rig, mode))
    {
        cmd = "D";
    }
    else if (width > rig_passband_normal(rig, mode))
    {
        cmd = "B";
    }
    else
    {
        cmd = "C";
    }

    return cu_transaction(rig, cmd, 1);
}

int cu_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char cmdbuf[16];

    cmdbuf[1] = 0;

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        cmdbuf[0] = val.i ? 'm' : 'n';
        break;

    case RIG_LEVEL_ATT:
        cmdbuf[0] = val.i ? 'o' : 'p';
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_AUTO: cmdbuf[0] = 'J'; break;

        case RIG_AGC_FAST: cmdbuf[0] = 'K'; break;

        case RIG_AGC_SLOW: cmdbuf[0] = 'L'; break;

        case RIG_AGC_OFF:  cmdbuf[0] = 'M'; break;

        default: return -RIG_EINVAL;
        }

        break;

    case RIG_LEVEL_SQL:
        cmdbuf[0] = val.i ? 'o' : 'p';
        break;

    case RIG_LEVEL_RFPOWER:
        if (val.f < 0.4)
        {
            cmdbuf[0] = 'S';    /* low */
        }
        else if (val.f < 0.6)
        {
            cmdbuf[0] = 'U';    /* medium */
        }
        else
        {
            cmdbuf[0] = 'W';    /* high */
        }

        break;

    case RIG_LEVEL_AF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "y%02u"CR, (unsigned)(99 - val.f * 99));
        break;

    default:
        return -RIG_EINVAL;
    }

    return cu_transaction(rig, cmdbuf, strlen(cmdbuf));
}

int cu_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmdbuf[16];
    int cmd_len;

    cmd_len = 1;

    switch (func)
    {
    case RIG_FUNC_MUTE:
        cmdbuf[0] = status ? 'l' : 'k';
        break;

    default:
        return -RIG_EINVAL;
    }

    return cu_transaction(rig, cmdbuf, cmd_len);
}

int cu_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    char cmdbuf[16];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "w%c"CR,
             ts >= s_kHz(1) ? '2' :
             ts >= s_Hz(100) ? '1' : '0');

    return cu_transaction(rig, cmdbuf, strlen(cmdbuf));
}

int cu_set_parm(RIG *rig, setting_t parm, value_t val)
{
    char cmdbuf[16];

    switch (parm)
    {
    case RIG_PARM_TIME:
        /* zap seconds */
        val.i /= 60;
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "f%02d%02d"CR,
                 val.i / 60, val.i % 60);
        break;

    case RIG_PARM_BACKLIGHT:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "z%1u"CR, (unsigned)(val.f * 5));
        break;

    default:
        return -RIG_EINVAL;
    }

    return cu_transaction(rig, cmdbuf, strlen(cmdbuf));
}


int cu_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char *cmd;

    cmd = ptt == RIG_PTT_ON ? "u" : "v";

    return cu_transaction(rig, cmd, 1);
}

static int cu_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct cu_priv_data *priv = (struct cu_priv_data *)rig->state.priv;

    /* memorize channel for RIG_OP_TO_VFO & RIG_OP_FROM_VFO */
    priv->ch = ch;

    return RIG_OK;
}


int cu_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct cu_priv_data *priv = (struct cu_priv_data *)rig->state.priv;
    char cmdbuf[16];

    switch (op)
    {
    case RIG_OP_TUNE:
        cmdbuf[0] = 'R';
        cmdbuf[1] = 0;
        break;

    case RIG_OP_CPY:
        cmdbuf[0] = ':';
        cmdbuf[1] = ';';
        cmdbuf[2] = 0x0d;
        cmdbuf[3] = 0;
        break;

    case RIG_OP_TO_VFO:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "<%02u"CR, (unsigned)priv->ch);
        break;

    case RIG_OP_FROM_VFO:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "d%02u"CR, (unsigned)priv->ch);
        break;

    default:
        return -RIG_EINVAL;
    }

    return cu_transaction(rig, cmdbuf, strlen(cmdbuf));
}



