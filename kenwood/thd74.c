/*
 *  Hamlib Kenwood backend - TH-D74 description
 *                            cloned after TH-D72
 *  Copyright (c) 2018 by Sebastian Denz
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "kenwood.h"
#include "th.h"
#include "num_stdio.h"
#include "iofunc.h"
#include "serial.h"


#define THD74_MODES   (RIG_MODE_FM|RIG_MODE_AM)
#define THD74_MODES_TX (RIG_MODE_FM)

#define THD74_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_MON|    \
                       RIG_FUNC_SQL|    \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO)

#define THD74_LEVEL_ALL (RIG_LEVEL_STRENGTH| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_AF| \
                        RIG_LEVEL_RF|\
                        RIG_LEVEL_MICGAIN)

#define THD74_PARMS (RIG_PARM_BACKLIGHT)

#define THD74_VFO_OP (RIG_OP_NONE)

/*
 * TODO: Band A & B
 */
#define THD74_VFO (RIG_VFO_A|RIG_VFO_B)

static rmode_t td74_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_DR,
    [2] = RIG_MODE_AM,
    [3] = RIG_MODE_LSB,
    [4] = RIG_MODE_USB,
    [5] = RIG_MODE_CW,
    [6] = RIG_MODE_FMN,
    [7] = RIG_MODE_DR,
    [8] = RIG_MODE_WFM,
    [9] = RIG_MODE_CWR,
};

static struct kenwood_priv_caps  thd74_priv_caps  =
{
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = td74_mode_table,
};

//static int thd74_open(RIG *rig);
static int thd74_get_chan_all_cb(RIG *rig, chan_cb_t chan_cb, rig_ptr_t arg);

static int thd74_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int thd74_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int thd74_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int thd74_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static char thd74_get_vfo_letter(RIG *rig, vfo_t vfo);
static int thd74_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);

/*
 * th-d72a rig capabilities.
 */
const struct rig_caps thd74_caps =
{
    .rig_model =  RIG_MODEL_THD74,
    .model_name = "TH-D74",
    .mfg_name =  "Kenwood",
    .version =  TH_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_HANDHELD | RIG_FLAG_APRS | RIG_FLAG_TNC | RIG_FLAG_DXCLUSTER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_XONXOFF,
    .write_delay =  100,
    .post_write_delay =  100,
    .timeout =  250,
    .retry =  3,

    .has_get_func =  THD74_FUNC_ALL,
    .has_set_func =  THD74_FUNC_ALL,
    .has_get_level =  THD74_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(THD74_LEVEL_ALL),
    .has_get_parm =  THD74_PARMS,
    .has_set_parm =  THD74_PARMS,    /* FIXME: parms */
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 5 } },
        [LVL_SQL] = { .min = { .i = 0 }, .max = { .i = 5 } },
        [LVL_RFPOWER] = { .min = { .i = 3 }, .max = { .i = 0 } },
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  THD74_VFO_OP,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  6, /* TBC */

    .chan_list =  {
        {  0,  999, RIG_MTYPE_MEM, {TH_CHANNEL_CAPS}},   /* TBC MEM */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {MHz(118), MHz(174), THD74_MODES, -1, -1, THD74_VFO},
        {MHz(320), MHz(524), THD74_MODES, -1, -1, THD74_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {MHz(144), MHz(148), THD74_MODES_TX, W(0.05), W(5), THD74_VFO},
        {MHz(430), MHz(440), THD74_MODES_TX, W(0.05), W(5), THD74_VFO},
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {THD74_MODES, kHz(5)},
        {THD74_MODES, kHz(6.25)},
        /* kHz(8.33)  ?? */
        {THD74_MODES, kHz(10)},
        {THD74_MODES, kHz(12.5)},
        {THD74_MODES, kHz(15)},
        {THD74_MODES, kHz(20)},
        {THD74_MODES, kHz(25)},
        {THD74_MODES, kHz(30)},
        {THD74_MODES, kHz(50)},
        {THD74_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(14)},
        {RIG_MODE_AM, kHz(9)},
        RIG_FLT_END,
    },
    .priv = (void *)& thd74_priv_caps,

    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .set_vfo =  th_set_vfo,
    .get_vfo =  th_get_vfo,

// need to understand
//.get_chan_all_cb = thd74_get_chan_all_cb,

    .get_info =  th_get_info,

// add functions step by step...

    .set_freq = thd74_set_freq,
    .get_freq = thd74_get_freq,
    .set_trn = kenwood_set_trn,
    .get_trn = kenwood_get_trn,
//.set_rit = kenwood_set_rit,
//.get_rit = kenwood_get_rit,
//.set_xit = kenwood_set_xit,
//.get_xit = kenwood_get_xit,
    .set_mode = thd74_set_mode,
    .get_mode = thd74_get_mode,
//.set_split_vfo = kenwood_set_split_vfo,
//.get_split_vfo = kenwood_get_split_vfo_if,
//.get_ptt = kenwood_get_ptt,
    .set_ptt = thd74_set_ptt,
    .get_dcd = th_get_dcd,

};


#define CMD_SZ 5
#define BLOCK_SZ 256
#define BLOCK_COUNT 256

static char thd74_get_vfo_letter(RIG *rig, vfo_t vfo)
{
    unsigned char vfo_letter;
    vfo_t tvfo;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    tvfo = (vfo == RIG_VFO_CURR
            || vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

    switch (tvfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
        vfo_letter = '0';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        vfo_letter = '1';
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __func__, vfo);
        return -RIG_EINVAL;
    }

    return vfo_letter;
}

static int thd74_get_block(RIG *rig, int block_num, char *block)
{
    hamlib_port_t *rp = &rig->state.rigport;
    char cmd[CMD_SZ] = "R\0\0\0\0";
    char resp[CMD_SZ];
    int ret;

    /* fetching block i */
    cmd[2] = block_num & 0xff;

    ret = write_block(rp, cmd, CMD_SZ);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* read response first */
    ret = read_block(rp, resp, CMD_SZ);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (resp[0] != 'W' || memcmp(cmd + 1, resp + 1, CMD_SZ - 1))
    {
        return -RIG_EPROTO;
    }

    /* read block */
    ret = read_block(rp, block, BLOCK_SZ);

    if (ret != BLOCK_SZ)
    {
        return ret;
    }

    ret = write_block(rp, "\006", 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_block(rp, resp, 1);

    if (ret != 1)
    {
        return ret;
    }

    if (resp[0] != 0x06)
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int thd74_get_chan_all_cb(RIG *rig, chan_cb_t chan_cb, rig_ptr_t arg)
{
    int i, j, ret;
    hamlib_port_t *rp = &rig->state.rigport;
    channel_t *chan;
    chan_t *chan_list = rig->state.chan_list;
    int chan_next = chan_list[0].start;
    char block[BLOCK_SZ];
    char resp[CMD_SZ];

    ret = kenwood_transaction(rig, "0M PROGRAM", NULL, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    rp->parm.serial.rate = 57600;

    serial_setup(rp);

    /* let the pcr settle and flush any remaining data*/
    usleep(100 * 1000);
    serial_flush(rp);

    /* setRTS or Hardware flow control? */
    ret = ser_set_rts(rp, 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /*
     * setting chan to NULL means the application
     * has to provide a struct where to store data
     * future data for channel channel_num
     */
    chan = NULL;
    ret = chan_cb(rig, &chan, chan_next, chan_list, arg);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (chan == NULL)
    {
        return -RIG_ENOMEM;
    }


    for (i = 0; i < BLOCK_COUNT; i++)
    {

        ret = thd74_get_block(rig, i, block);

        if (ret != RIG_OK)
        {
            return ret;
        }

        /*
         * Most probably, there's 64 bytes per channel (256*256 / 1000+)
         */
#define CHAN_PER_BLOCK 4

        for (j = 0; j < CHAN_PER_BLOCK; j++)
        {

            char *block_chan = block + j * (BLOCK_SZ / CHAN_PER_BLOCK);

            memset(chan, 0, sizeof(channel_t));
            chan->vfo = RIG_VFO_MEM;
            chan->channel_num = i * CHAN_PER_BLOCK + j;

            /* What are the extra 64 channels ? */
            if (chan->channel_num >= 1000)
            {
                break;
            }

            /* non-empty channel ? */
            // if (block_chan[0] != 0xff) {
            // since block_chan is *signed* char, this maps to -1
            if (block_chan[0] != -1)
            {

                memcpy(chan->channel_desc, block_chan, 8);
                /* TODO: chop off trailing chars */
                chan->channel_desc[8] = '\0';

                /* TODO: parse block and fill in chan */
            }

            /* notify the end? */
            chan_next = chan_next < chan_list[i].end ? chan_next + 1 : chan_next;

            /*
             * provide application with channel data,
             * and ask for a new channel structure
             */
            chan_cb(rig, &chan, chan_next, chan_list, arg);
        }
    }

    ret = write_block(rp, "E", 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_block(rp, resp, 1);

    if (ret != 1)
    {
        return ret;
    }

    if (resp[0] != 0x06)
    {
        return -RIG_EPROTO;
    }

    /* setRTS?? getCTS needed? */
    ret = ser_set_rts(rp, 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    return RIG_OK;
}


/*
 * thd74_get_freq
 */
int thd74_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !freq)
    {
        return -RIG_EINVAL;
    }

    char freqbuf[50];
    char cmdbuf[5];
    int retval;
    unsigned char vfo_letter;
    vfo_t tvfo;

    tvfo = (vfo == RIG_VFO_CURR
            || vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

    if (RIG_VFO_CURR == tvfo)
    {
        /* fetch from rig */
        retval = rig_get_vfo(rig, &tvfo);

        if (RIG_OK != retval) { return retval; }
    }

    /* memory frequency cannot be read with an Fx command, use IF */
    if (tvfo == RIG_VFO_MEM)
    {

        return kenwood_get_freq_if(rig, vfo, freq);
    }

    vfo_letter = thd74_get_vfo_letter(rig, vfo);

    snprintf(cmdbuf, sizeof(cmdbuf), "FQ %c", vfo_letter);
    rig_debug(RIG_DEBUG_ERR, "%s: cmd: %s\n", __func__, cmdbuf);

    retval = kenwood_safe_transaction(rig, cmdbuf, freqbuf, 50, 15);

    if (retval != RIG_OK)
    {
        return retval;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: response is %s\n", __func__, freqbuf);
    sscanf(freqbuf, "FQ %d,%10"SCNfreq, &retval, freq);

    return RIG_OK;
}

/*
 * thd74_set_freq
 */
int thd74_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    char freqbuf[16], replybuf[16];
    unsigned char vfo_letter;
    int err;
    vfo_t tvfo;

    tvfo = (vfo == RIG_VFO_CURR
            || vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

    if (RIG_VFO_CURR == tvfo)
    {
        /* fetch from rig */
        err = rig_get_vfo(rig, &tvfo);

        if (RIG_OK != err) { return err; }
    }

    vfo_letter = thd74_get_vfo_letter(rig, vfo);

    snprintf(freqbuf, sizeof(freqbuf), "FQ %c,%010ld", vfo_letter, (int64_t)freq);
    rig_debug(RIG_DEBUG_ERR, "%s: freqbuf: %s\n", __func__, freqbuf);

    err = kenwood_transaction(rig, freqbuf, replybuf, 16);
    return err;
}


/*
 * thd74_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int thd74_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[64];
    char modebuf[8];
    int retval;
    const struct kenwood_priv_caps *priv = (const struct kenwood_priv_caps *)
                                           rig->caps->priv;
    unsigned char vfo_letter;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (vfo != RIG_VFO_CURR && vfo != rig->state.current_vfo)
    {
        return kenwood_wrong_vfo(__func__, vfo);
    }

    vfo_letter = thd74_get_vfo_letter(rig, vfo);

    snprintf(modebuf, sizeof(modebuf), "MD %c", vfo_letter);
    retval = kenwood_safe_transaction(rig, modebuf, buf, sizeof(buf), 6);

    //retval = kenwood_safe_transaction(rig, modebuf, buf, sizeof(buf), 0);
    if (retval != RIG_OK)
    {
        return retval;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: buf%s\n", __func__, buf);

    if (buf[5] < '0' || buf[5] > '9')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
        return -RIG_ERJCTED;
    }

    if (priv->mode_table)
    {
        *mode = kenwood2rmode(buf[5] - '0', priv->mode_table);

        if (*mode == RIG_MODE_NONE)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Mode (table)value '%c'\n",
                      __func__, buf[5]);
            return -RIG_EINVAL;
        }
    }
    else
    {
        return -RIG_EINVAL;
    }

    if (width)
    {
        *width = RIG_PASSBAND_NORMAL;
    }

    return RIG_OK;
}
/*
 * th_set_mode
 * Assumes rig!=NULL
 */
int thd74_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char kmode, mdbuf[8], replybuf[8];
    int retval;
    const struct kenwood_priv_caps *priv = (const struct kenwood_priv_caps *)
                                           rig->caps->priv;
    unsigned char vfo_letter;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (vfo != RIG_VFO_CURR && vfo != rig->state.current_vfo)   // FIXME thinkabout
    {
        return kenwood_wrong_vfo(__func__, vfo);
    }

    vfo_letter = thd74_get_vfo_letter(rig, vfo);

    if (priv->mode_table)
    {
        kmode = rmode2kenwood(mode, priv->mode_table);

        if (kmode == -1)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: Unsupported Mode value '%s'\n",
                      __func__, rig_strrmode(mode));
            return -RIG_EINVAL;
        }

        kmode += '0';
    }
    else
    {
        switch (mode)
        {
        case RIG_MODE_FM: kmode = '0'; break;

        case RIG_MODE_AM: kmode = '1'; break;

        //  case RIG_MODE_AM: kmode = '2'; break;   // FIXME
        case RIG_MODE_LSB: kmode = '3'; break;

        case RIG_MODE_USB: kmode = '4'; break;

        case RIG_MODE_CW: kmode = '5'; break;

        case RIG_MODE_FMN: kmode = '6'; break;

        case RIG_MODE_DR: kmode = '7'; break;

        case RIG_MODE_WFM: kmode = '8'; break;

        case RIG_MODE_CWR: kmode = '9'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Mode %d\n", __func__, mode);
            return -RIG_EINVAL;
        }
    }

    sprintf(mdbuf, "MD %c,%c", vfo_letter, kmode);
    rig_debug(RIG_DEBUG_ERR, "%s: mdbuf: %s\n", __func__, mdbuf);

    retval = kenwood_transaction(rig, mdbuf, replybuf, 7);
    rig_debug(RIG_DEBUG_ERR, "%s: retval: %d\n", __func__, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

static int thd74_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char buf[6];
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);
    return kenwood_transaction(rig, (ptt == RIG_PTT_ON) ? "TX" : "RX", buf, 5);
}
