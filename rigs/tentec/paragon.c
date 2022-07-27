/*
 *  Hamlib TenTenc backend - TT-585 Paragon description
 *  Copyright (c) 2003-2010 by Stephane Fillod
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "tentec.h"
#include "bandplan.h"
#include "iofunc.h"
#include "serial.h"
#include "misc.h"
#include "num_stdio.h"

struct tt585_priv_data
{
    unsigned char status_data[30];
    struct timeval status_tv;
    int channel_num;
};


/* RIG_MODE_FM is optional */
#define TT585_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define TT585_RXMODES (TT585_MODES|RIG_MODE_AM)

#define TT585_FUNCS (RIG_FUNC_NONE)

#define TT585_LEVELS (RIG_LEVEL_NONE)

#define TT585_ANTS (RIG_ANT_1)

#define TT585_PARMS (RIG_PARM_ANN|RIG_PARM_TIME)

#define TT585_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT585_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO|\
        RIG_OP_CPY|RIG_OP_MCL|RIG_OP_TOGGLE|\
        RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|\
        RIG_OP_TUNE)

#define TT585_CACHE_TIMEOUT 500 /* ms */

/*
 * Mem caps to be checked, maybe more like split..
 */
#define TT585_MEM_CAP {        \
        .freq = 1,      \
        .mode = 1,      \
        .width = 1,     \
        .channel_desc = 1,  \
}

static int tt585_init(RIG *rig);
static int tt585_cleanup(RIG *rig);
static int tt585_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tt585_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tt585_set_vfo(RIG *rig, vfo_t vfo);
static int tt585_get_vfo(RIG *rig, vfo_t *vfo);
static int tt585_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo);
static int tt585_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *txvfo);
static int tt585_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tt585_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int tt585_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
static int tt585_set_parm(RIG *rig, setting_t parm, value_t val);
static int tt585_set_mem(RIG *rig, vfo_t vfo, int ch);
static int tt585_get_mem(RIG *rig, vfo_t vfo, int *ch);

static int tt585_get_status_data(RIG *rig);

/*
 * tt585 transceiver capabilities,
 * with the optional model 258 RS232 Interface board.
 */
const struct rig_caps tt585_caps =
{
    RIG_MODEL(RIG_MODEL_TT585),
    .model_name = "TT-585 Paragon",
    .mfg_name =  "Ten-Tec",
    .version =  "20200305.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  1200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  100, /* instead of 20 ms */
    .post_write_delay =  200, /* FOR T=1 TO 200 on a 4.77 MHz PC */
    .timeout =  1000,
    .retry =  0,

    .has_get_func =  TT585_FUNCS,
    .has_set_func =  TT585_FUNCS,
    .has_get_level =  TT585_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(TT585_LEVELS),
    .has_get_parm =  TT585_PARMS,
    .has_set_parm =  TT585_PARMS,
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo = RIG_TARGETABLE_NONE,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  7,
    .vfo_ops = TT585_VFO_OPS,

    .chan_list =  {
        {   0, 61, RIG_MTYPE_MEM, TT585_MEM_CAP },
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(30) - 10, TT585_RXMODES, -1, -1, TT585_VFO, TT585_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TT585_MODES, W(5), W(100), TT585_VFO, TT585_ANTS),
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(100), MHz(30) - 10, TT585_RXMODES, -1, -1, TT585_VFO, TT585_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, TT585_MODES, W(5), W(100), TT585_VFO, TT585_ANTS),
        {MHz(5.25), MHz(5.40), TT585_MODES, W(5), W(100), TT585_VFO, TT585_ANTS},
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {TT585_RXMODES, 10},
        {TT585_RXMODES, 20},
        {TT585_RXMODES, 50},
        {TT585_RXMODES, 100},
        {TT585_RXMODES, 500},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_AM, kHz(1.8)},
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_AM, 500},
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_AM, 250},
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },
    .priv = (void *) NULL,

    .rig_init =  tt585_init,
    .rig_cleanup =  tt585_cleanup,
    .set_freq =  tt585_set_freq,
    .get_freq =  tt585_get_freq,
    .set_vfo =  tt585_set_vfo,
    .get_vfo =  tt585_get_vfo,
    .set_split_vfo =  tt585_set_split_vfo,
    .get_split_vfo =  tt585_get_split_vfo,
    .vfo_op =  tt585_vfo_op,
    .set_mode =  tt585_set_mode,
    .get_mode =  tt585_get_mode,
    .set_parm =  tt585_set_parm,
    .set_mem =   tt585_set_mem,
    .get_mem =   tt585_get_mem,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

/*
 * tt585_init:
 * Basically, it just sets up *priv
 */
int tt585_init(RIG *rig)
{
    struct tt585_priv_data *priv;

    rig->state.priv = (struct tt585_priv_data *) calloc(1, sizeof(
                          struct tt585_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct tt585_priv_data));

    return RIG_OK;
}

int tt585_cleanup(RIG *rig)
{
    if (rig->state.priv)
    {
        free(rig->state.priv);
        rig->state.priv = NULL;
    }

    return RIG_OK;
}

int tt585_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *) rig->state.priv;
    int ret;

    ret = tt585_get_status_data(rig);

    if (ret < 0)
    {
        return ret;
    }

    *vfo = (priv->status_data[9] & 0x08) ? RIG_VFO_A : RIG_VFO_B;

    return RIG_OK;
}

/*
 * tt585_set_vfo
 * Assumes rig!=NULL
 */
int tt585_set_vfo(RIG *rig, vfo_t vfo)
{
    vfo_t curr_vfo;
    int ret;

    ret = tt585_get_vfo(rig, &curr_vfo);

    if (ret < 0)
    {
        return ret;
    }

    if (vfo == curr_vfo || vfo == RIG_VFO_CURR || vfo == RIG_VFO_VFO)
    {
        return RIG_OK;
    }

    /* toggle VFOs */
    return write_block(&rig->state.rigport, (unsigned char *) "F", 1);
}

int tt585_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    split_t curr_split;
    vfo_t curr_txvfo;
    int ret;

    ret = tt585_get_split_vfo(rig, vfo, &curr_split, &curr_txvfo);

    if (ret < 0)
    {
        return ret;
    }

    if (split == curr_split)
    {
        return RIG_OK;
    }

    /* toggle split mode */
    return write_block(&rig->state.rigport, (unsigned char *) "J", 1);
}

int tt585_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *txvfo)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;
    int ret;

    ret = tt585_get_status_data(rig);

    if (ret < 0)
    {
        return ret;
    }

    *split = (priv->status_data[9] & 0x02) ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
    *txvfo = RIG_VFO_B;

    return RIG_OK;
}


/*
 * tt585_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tt585_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;
    int ret;
    unsigned char *p;

    ret = tt585_get_status_data(rig);

    if (ret < 0)
    {
        return ret;
    }

    p = priv->status_data;

    *freq = ((((((p[0] * 10 +
                  p[1]) * 10 +
                 p[2]) * 10 +
                p[3]) * 10 +
               p[4]) * 10 +
              p[5]) * 10 +
             p[6]) * 10;

    return RIG_OK;
}

/*
 * tt585_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */

int tt585_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;
#define FREQBUFSZ 16
    char buf[FREQBUFSZ], *p;

    num_snprintf(buf, FREQBUFSZ - 1, "%.5f@", (double)freq / MHz(1));
    buf[FREQBUFSZ - 1] = '\0';

    /* replace decimal point with W */
    p = strchr(buf, '.');
    *p = 'W';

    rig_force_cache_timeout(&priv->status_tv);

    return write_block(&rig->state.rigport, (unsigned char *) buf, strlen(buf));
}

/*
 * tt585_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tt585_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;
    int ret;

    ret = tt585_get_status_data(rig);

    if (ret < 0)
    {
        return ret;
    }

    if (priv->status_data[7] & 0x02)
    {
        *mode = RIG_MODE_CW;
    }
    else if (priv->status_data[7] & 0x04)
    {
        *mode = RIG_MODE_USB;
    }
    else if (priv->status_data[7] & 0x08)
    {
        *mode = RIG_MODE_LSB;
    }
    else if (priv->status_data[7] & 0x10)
    {
        *mode = RIG_MODE_AM;
    }
    else if (priv->status_data[7] & 0x20)
    {
        *mode = RIG_MODE_FM;
    }
    else if (priv->status_data[7] & 0x40)
    {
        *mode = RIG_MODE_RTTY;
    }
    else
    {
        *mode = RIG_MODE_NONE;
    }

    if (priv->status_data[8] & 0x08)
    {
        *width = 250;
    }
    else if (priv->status_data[8] & 0x10)
    {
        *width = 500;
    }
    else if (priv->status_data[8] & 0x20)
    {
        *width = 1800;
    }
    else if (priv->status_data[8] & 0x40)
    {
        *width = 2400;
    }
    else if (priv->status_data[8] & 0x80)
    {
        *width = 6000;
    }
    else
    {
        *width = 0;
    }

    return RIG_OK;
}

/*
 * tt585_set_mode
 * Assumes rig!=NULL
 */
int tt585_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;
    const char *mcmd, *wcmd;
    int ret;

    switch (mode)
    {
    case RIG_MODE_LSB: mcmd = "N"; break;

    case RIG_MODE_USB: mcmd = "O"; break;

    case RIG_MODE_CW:  mcmd = "P"; break;

    case RIG_MODE_FM:  mcmd = "L"; break;

    case RIG_MODE_AM:  mcmd = "M"; break;

    case RIG_MODE_RTTY: mcmd = "XP"; break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong MODE */
    }

    rig_force_cache_timeout(&priv->status_tv);

    ret =  write_block(&rig->state.rigport, (unsigned char *) mcmd, strlen(mcmd));

    if (ret < 0)
    {
        return ret;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return ret; }

    if (RIG_PASSBAND_NORMAL == width)
    {
        width = rig_passband_normal(rig, mode);
    }

    if (width <= 250)
    {
        wcmd = "V";
    }
    else if (width <= 500)
    {
        wcmd = "U";
    }
    else if (width <= 1800)
    {
        wcmd = "T";
    }
    else if (width <= 2400)
    {
        wcmd = "S";
    }
    else /* 6000 (or FM?) */
    {
        wcmd = "R";
    }

    return write_block(&rig->state.rigport, (unsigned char *) wcmd, strlen(mcmd));
}

int tt585_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;
    char buf[16];

    if (ch < 0 || ch > 61)
    {
        return -RIG_EINVAL;
    }

    priv->channel_num = ch;

    /* does it work without a command after the channel number? */
    SNPRINTF(buf, sizeof(buf), ":%02d", ch);

    return write_block(&rig->state.rigport, (unsigned char *) buf, strlen(buf));
}

int tt585_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *) rig->state.priv;
    int ret;

    ret = tt585_get_status_data(rig);

    if (ret < 0)
    {
        return ret;
    }

    /* 63 means not in MEM mode, 0xfe means mem full */
    if (priv->status_data[11] > 61)
    {
        return -RIG_ERJCTED;
    }

    *ch = priv->status_data[11];

    return RIG_OK;
}


/*
 * private helper function. Retrieves status data from rig.
 * using buffer indicated in *priv struct.
 *
 * need to use this when doing tt585_get_* stuff
 */
int tt585_get_status_data(RIG *rig)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;
    hamlib_port_t *rigport;
    int ret;

    rigport = &rig->state.rigport;

    if (!rig_check_cache_timeout(&priv->status_tv, TT585_CACHE_TIMEOUT))
    {
        return RIG_OK;
    }

    rig_flush(rigport);

    /* send STATUS command to fetch data*/

    ret = write_block(rigport, (unsigned char *) "\\", 1);

    if (ret < 0)
    {
        return ret;
    }

    ret = read_block(rigport, (unsigned char *)(char *) priv->status_data,
                     sizeof(priv->status_data));

    if (ret < 0)
    {
        return ret;
    }

    /* update cache date */
    gettimeofday(&priv->status_tv, NULL);

    return RIG_OK;
}

int tt585_set_parm(RIG *rig, setting_t parm, value_t val)
{
    int ret;

    switch (parm)
    {
    case RIG_PARM_ANN:
        /* FIXME: > is a toggle command only */
        ret = write_block(&rig->state.rigport, (unsigned char *) ">", 1);

        if (ret < 0)
        {
            return ret;
        }

        /* exact additional delay TBC */
        sleep(1);
        return RIG_OK;

    /* TODO: RIG_PARM_TIME */

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported parm %s\n", __func__,
                  rig_strparm(parm));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * tt585_vfo_op
 * Assumes rig!=NULL
 */
int tt585_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;
    const char *cmd;
    char buf[16];

    switch (op)
    {
    case RIG_OP_TUNE: cmd = "Q"; break;

    case RIG_OP_MCL:
        SNPRINTF(buf, sizeof(buf), ":%02dXD", priv->channel_num);
        cmd = buf;
        break;

    case RIG_OP_TO_VFO:
        SNPRINTF(buf, sizeof(buf), ":%02d", priv->channel_num);
        cmd = buf;
        break;

    case RIG_OP_FROM_VFO:
        SNPRINTF(buf, sizeof(buf), "<%02d", priv->channel_num);
        cmd = buf;
        break;

    case RIG_OP_CPY: cmd = "E"; break;

    case RIG_OP_TOGGLE: cmd = "F"; break;

    case RIG_OP_DOWN: cmd = "]"; break;

    case RIG_OP_UP: cmd = "["; break;

    case RIG_OP_BAND_DOWN: cmd = "XY"; break;

    case RIG_OP_BAND_UP: cmd = "XZ"; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported op %#x\n", __func__, op);
        return -RIG_EINVAL;
    }

    rig_force_cache_timeout(&priv->status_tv);

    return write_block(&rig->state.rigport, (unsigned char *) cmd, strlen(cmd));
}
