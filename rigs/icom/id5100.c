/*
 *  Hamlib CI-V backend - description of ID-5100 and variations
 *  Copyright (c) 2015 by Stephane Fillod
 *  Copyright (c) 2019 by Malcolm Herring
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

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"

enum
{
    MAIN_ON_LEFT,
    MAIN_ON_RIGHT
};

/*
 * Specs and protocol details comes from the chapter 13 of ID-5100_Full-Inst_Manual.pdf
 *
 * NB: while the port labeled "Data" is used for firmware upgrades,
 * you have to use the port labeled "SP2" for rig control.
 *
 * TODO:
 * - DV mode
 * - GPS support
 * - Single/dual watch (RIG_LEVEL_BALANCE)
 */

#define ID5100_MODES (RIG_MODE_AM|RIG_MODE_AMN|RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_DSTAR)
#define ID5100_ALL_RX_MODES (RIG_MODE_AM|ID5100_MODES)

#define ID5100_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MAIN|RIG_VFO_SUB)

#define ID5100_SCAN_OPS RIG_SCAN_NONE

#define ID5100_VFO_OPS  RIG_OP_NONE

#define ID5100_FUNC_ALL ( \
                            RIG_FUNC_TONE| \
                            RIG_FUNC_TSQL| \
                            RIG_FUNC_CSQL| \
                            RIG_FUNC_DSQL| \
                            RIG_FUNC_DUAL_WATCH| \
                            RIG_FUNC_VOX)

#define ID5100_LEVEL_ALL    (RIG_LEVEL_AF| \
                            RIG_LEVEL_SQL| \
                            RIG_LEVEL_RAWSTR| \
                            RIG_LEVEL_RFPOWER| \
                            RIG_LEVEL_MICGAIN| \
                            RIG_LEVEL_VOXGAIN)

#define ID5100_PARM_ALL RIG_PARM_NONE

int id5100_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);

int id5100_set_vfo(RIG *rig, vfo_t vfo)
{
    struct rig_state *rs = STATE(rig);
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;

    if (vfo == RIG_VFO_CURR) { vfo = rs->current_vfo; }

    // if user requests VFOA/B we automatically turn of dual watch mode
    // if user requests  Main/Sub we automatically turn on dual watch mode
    // hopefully this is a good idea and just prevents users/clients from having set the mode themselves
#if 0

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_B)
    {
        // and 0x25 works in this mode
        priv->x25cmdfails = 1;

        if (priv->dual_watch)
        {
            // then we need to turn off dual watch
            if (RIG_OK != (retval = icom_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH,
                                                  0)))
            {
                RETURNFUNC(retval);
            }

            priv->dual_watch = 0;
        }
    }
    else if (vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB)
    {
        // x25 does not work in DUAL_WATCH mode
        priv->x25cmdfails = 1;

        if (priv->dual_watch == 0)
        {
            if (RIG_OK != (retval = icom_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH,
                                                  1)))
            {
                RETURNFUNC(retval);
            }

            priv->dual_watch = 1;
        }
    }

#endif

    int myvfo = S_MAIN;
    priv->dual_watch_main_sub = MAIN_ON_LEFT;
    rs->current_vfo = RIG_VFO_A;

    if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
    {
        myvfo = S_SUB;
        priv->dual_watch_main_sub = MAIN_ON_RIGHT;
        rs->current_vfo = vfo;
    }

    if (RIG_OK != (retval = icom_transaction(rig, C_SET_VFO, myvfo, NULL, 0, ackbuf,
                            &ack_len)))
    {
        RETURNFUNC(retval);
    }

    RETURNFUNC(retval);
}


int id5100_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int cmd = C_SET_FREQ;
    int subcmd = -1;
    unsigned char freqbuf[MAXFRAMELEN];
    int freq_len = 5;
    int retval;
    struct rig_state *rs = STATE(rig);
    //struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    vfo_t currvfo = rs->current_vfo;

    if (vfo == RIG_VFO_CURR) { vfo = rs->current_vfo; }

    if (rs->dual_watch == 0 && (vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB)) { id5100_set_split_vfo(rig, RIG_VFO_SUB, 1, RIG_VFO_MAIN); }

    if (rs->dual_watch == 1 && (vfo == RIG_VFO_A || vfo == RIG_VFO_B)) { id5100_set_split_vfo(rig, RIG_VFO_A, 0, RIG_VFO_A); }

    if (vfo != currvfo) { id5100_set_vfo(rig, vfo); }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): vfo=%s\n", __func__, __LINE__,
              rig_strvfo(vfo));

    to_bcd(freqbuf, freq, freq_len * 2);
    retval = icom_transaction(rig, cmd, subcmd, freqbuf, freq_len, NULL,
                              NULL);

    if (vfo != currvfo) { id5100_set_vfo(rig, currvfo); }

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: set_freq failed: %s\n", __func__,
                  rigerror(retval));
        return retval;
    }

    return RIG_OK;
}


static int id5100_get_freq2(RIG *rig, vfo_t vfo, freq_t *freq)
{
    unsigned char freqbuf[MAXFRAMELEN];
    int freq_len = 5;
    int retval;
    int freqbuf_offset = 1;
    int cmd = 0x03;
    int subcmd = -1;
    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): vfo=%s\n", __func__, __LINE__,
              rig_strvfo(vfo));
    retval = icom_transaction(rig, cmd, subcmd, NULL, 0, freqbuf, &freq_len);

    if (retval != RIG_OK)
    {
        return -retval;
    }

    *freq = from_bcd(freqbuf + freqbuf_offset, freq_len * 2);
    return RIG_OK;
}

int id5100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct rig_state *rs = STATE(rig);
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    int retval;

    vfo_t currvfo = rs->current_vfo;

    if (rs->dual_watch == 1 && rs->current_vfo != RIG_VFO_SUB) { id5100_set_split_vfo(rig, RIG_VFO_SUB, 0, RIG_VFO_MAIN); }

    if (rs->dual_watch) // dual watch is different
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Dual watch is on\n", __func__);

        if (priv->dual_watch_main_sub == MAIN_ON_LEFT
                || currvfo == RIG_VFO_A) // Then Main is on left
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Main on left\n", __func__, __LINE__);

            if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Method#1\n", __func__);
                id5100_set_vfo(rig, RIG_VFO_A);
                retval = id5100_get_freq2(rig, vfo, freq);
                id5100_set_vfo(rig, RIG_VFO_B);
                return retval;
            }
            else // Sub read -- don't need to do anything as it's on the left side
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Method#2\n", __func__);
                retval = id5100_get_freq2(rig, vfo, freq);
                return retval;
            }
        }

        else //
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Sub on left\n", __func__, __LINE__);

            if ((currvfo == RIG_VFO_B || currvfo == RIG_VFO_SUB) && (vfo == RIG_VFO_B
                    || vfo == RIG_VFO_SUB))
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Method#3\n", __func__);
                id5100_set_vfo(rig, RIG_VFO_MAIN);
                retval = id5100_get_freq2(rig, vfo, freq);
                id5100_set_vfo(rig, RIG_VFO_SUB);
                return retval;
            }
            else
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Method#4\n", __func__);
                retval = id5100_get_freq2(rig, vfo, freq);
                return retval;
            }
        }
    }
    else // not dual watch
    {
        if (currvfo != vfo)
        {
            id5100_set_vfo(rig, vfo);
        }

        retval = id5100_get_freq2(rig, vfo, freq);

        if (currvfo != vfo)
        {
            id5100_set_vfo(rig, currvfo);
        }

        return retval;
    }

#if 0
    else if ((vfo == RIG_VFOvfo == RIG_VFO_SUB
              && rs->dual_watch_main_sub == MAIN_ON_RIGHT)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Sub/A vfo=%s\n", __func__, __LINE__,
              rig_strvfo(vfo));
        *freq = CACHE(rig)->freqSubA;
        int cache_ms_freq, cache_ms_mode, cache_ms_width;
        pbwidth_t width;
        freq_t tfreq;
        rmode_t mode;
        retval = rig_get_cache(rig, RIG_VFO_SUB, &tfreq, &cache_ms_freq, &mode,
                               &cache_ms_mode,
    }
             else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Dual watch is off\n", __func__);
    }
    if (vfo == RIG_VFO_CURR) { vfo = rs->current_vfo; }
if (vfo == RIG_VFO_MAIN && priv->dual_watch_main_sub == MAIN_ON_LEFT)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Main/A vfo=%s\n", __func__, __LINE__,
              rig_strvfo(vfo));
    }
    if (priv->dual_watch_main_sub == MAIN_ON_LEFT || currvfo == RIG_VFO_A
            || currvfo == RIG_VFO_MAIN) // Then Main is on left
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Main on left\n", __func__, __LINE__);

        if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN)
        {
            return id5100_get_freq2(rig, vfo, freq);
        }
        else
        {
            id5100_set_vfo(rig, RIG_VFO_B);
            hl_usleep(50 * 1000);
            retval = id5100_get_freq2(rig, vfo, freq);
            id5100_set_vfo(rig, RIG_VFO_A);
            return retval;
        }
    }
    else // MAIN_ON_RIGHT
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Sub on left\n", __func__, __LINE__);

        if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
        {
            if (rs->dual_watch)
            {
                id5100_set_vfo(rig, RIG_VFO_A);
            }

            id5100_get_freq2(rig, vfo, freq);

            if (rs->dual_watch)
            {
                id5100_set_vfo(rig, RIG_VFO_B);
            }
        }
        else
        {
            retval = id5100_get_freq2(rig, vfo, freq);
            return retval;
        }
    }
#endif
    return RIG_OK;
}

/*
 * FIXME: real measurement
 */
#define ID5100_STR_CAL  UNKNOWN_IC_STR_CAL

int id5100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    unsigned char modebuf;
    unsigned char ackbuf[MAXFRAMELEN];
    int icmode = 2;
    int ack_len = sizeof(ackbuf);

    switch (mode)
    {
    case RIG_MODE_AM:  icmode = 2; modebuf = 1; break;

    case RIG_MODE_AMN: icmode = 2; modebuf = 2; break;

    case RIG_MODE_FM:  icmode = 5; modebuf = 1; break;

    case RIG_MODE_FMN: icmode = 5; modebuf = 2; break;

    case RIG_MODE_DSTAR: icmode = 0x17; modebuf = 1; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown mode=%s\n", __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d, modebuf=%c\n", __func__, icmode,
              modebuf);

    retval = icom_transaction(rig, C_SET_MODE, icmode, &modebuf, 1, ackbuf,
                              &ack_len);

    return retval;
}

int id5100_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    int mode_len;
    unsigned char modebuf[4];

    retval = icom_transaction(rig, C_RD_MODE, -1, NULL, 0, modebuf, &mode_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (modebuf[1])
    {
    case 2:
        *mode = modebuf[2] == 1 ? RIG_MODE_AM : RIG_MODE_AMN;
        *width = modebuf[2] == 1 ? 12000 : 6000; break;

    case 5:
        *mode = modebuf[2] == 1 ? RIG_MODE_FM : RIG_MODE_FMN;
        *width = modebuf[2] == 1 ? 10000 : 5000; break;

    case 0x17:
        *mode = RIG_MODE_DSTAR;
        *width = 6000; break;
    }

    return RIG_OK;
}

int id5100_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct rig_state *rs = STATE(rig);
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    if (tx_vfo != RIG_VFO_MAIN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s Split VFO must be Main\n", __func__);
        return -RIG_EINVAL;
    }

    if (rs->dual_watch == 0 || split == RIG_SPLIT_OFF)
    {
        if (RIG_OK != (retval = icom_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH,
                                              split)))
        {
            RETURNFUNC2(retval);
        }

        rs->dual_watch = split;

//        if (split == RIG_SPLIT_OFF) { rig_set_vfo(rig, RIG_VFO_A); }

        return RIG_OK;
    }

    priv->dual_watch_main_sub = MAIN_ON_LEFT;

    // ID5100 puts tx on Main and rx on Left side
    // So we put Main on right side to match gpredict positions
    // we must set RX vfo to SUB
    retval = id5100_set_vfo(rig, RIG_VFO_SUB);
    rs->current_vfo = RIG_VFO_SUB;
    priv->dual_watch_main_sub = MAIN_ON_RIGHT;

    return retval;
}


int id5100_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    int freq_len = 5;
    int retval;
    int cmd;
    int subcmd;
    unsigned char freqbuf[MAXFRAMELEN];


    ENTERFUNC;

    to_bcd(freqbuf, tx_freq, freq_len * 2);

    cmd = 0x0;
    subcmd = -1;
    // Main is always TX
    retval = icom_transaction(rig, cmd, subcmd, freqbuf, freq_len, NULL, NULL);

    RETURNFUNC(retval);
}

int id5100_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    int retval;
    //struct rig_state *rs = STATE(rig);
    //struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    //vfo_t currvfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): vfo=%s\n", __func__, __LINE__,
              rig_strvfo(vfo));
#if 0
    currvfo = rs->current_vfo;

    if (priv->dual_watch_main_sub == MAIN_ON_LEFT && (currvfo == RIG_VFO_MAIN
            || currvfo == RIG_VFO_A) && vfo == RIG_VFO_TX)
    {
        rig_set_vfo(rig, RIG_VFO_SUB);
        retval = rig_get_freq(rig, RIG_VFO_CURR, tx_freq);
        rig_set_vfo(rig, RIG_VFO_MAIN);
    }
    else
#endif
    {
        retval = rig_get_freq(rig, RIG_VFO_CURR, tx_freq);
    }

    return retval;
}



/*
 */
static struct icom_priv_caps id5100_priv_caps =
{
    0x8C,   /* default address */
    0,      /* 731 mode */
    1,      /* no XCHG */
    .dualwatch_split = 1
};

struct rig_caps id5100_caps =
{
    RIG_MODEL(RIG_MODEL_ID5100),
    .model_name = "ID-5100",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".9",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_MOBILE,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  0,
    .has_get_func =  ID5100_FUNC_ALL,
    .has_set_func =  ID5100_FUNC_ALL,
    .has_get_level =  ID5100_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ID5100_LEVEL_ALL),
    .has_get_parm =  ID5100_PARM_ALL,
    .has_set_parm =  ID5100_PARM_ALL,
    .level_gran =
    {
#include "level_gran_icom.h"
    },
    .extparms = icom_ext_parms,
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  full_dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ID5100_VFO_OPS,
    .scan_ops =  ID5100_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        // There's no memory support through CI-V,
        // but there is a clone mode apart.
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {MHz(118), MHz(174), ID5100_ALL_RX_MODES, -1, -1, ID5100_VFO_ALL},
        {MHz(375), MHz(550), ID5100_ALL_RX_MODES, -1, -1, ID5100_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {MHz(144), MHz(146), ID5100_MODES, W(5), W(25), ID5100_VFO_ALL},
        {MHz(430), MHz(440), ID5100_MODES, W(5), W(25), ID5100_VFO_ALL},
        RIG_FRNG_END,
    },

    .rx_range_list2 =   {
        {MHz(118), MHz(174), ID5100_ALL_RX_MODES, -1, -1, ID5100_VFO_ALL},
        {MHz(375), MHz(550), ID5100_ALL_RX_MODES, -1, -1, ID5100_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {MHz(144), MHz(148), ID5100_MODES, W(5), W(50), ID5100_VFO_ALL},
        {MHz(430), MHz(450), ID5100_MODES, W(5), W(50), ID5100_VFO_ALL},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        // Rem: no support for changing tuning step
        {RIG_MODE_ALL, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM | RIG_MODE_AM, kHz(12)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(6)},
        RIG_FLT_END,
    },
    .str_cal = ID5100_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& id5100_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  id5100_set_freq,
    .get_freq =  id5100_get_freq,
    .get_split_freq = id5100_get_split_freq,
    .set_split_freq = id5100_set_split_freq,
    .set_mode =  id5100_set_mode,
    .get_mode =  id5100_get_mode,
    .set_vfo =  id5100_set_vfo,
    .set_split_vfo = id5100_set_split_vfo,

    .set_powerstat = icom_set_powerstat,
    //.get_powerstat = icom_get_powerstat, // ID-5100 cannot get power status
    .decode_event =  icom_decode_event,

    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_parm =  icom_set_parm,
    .get_parm =  icom_get_parm,
    .set_ext_parm =  icom_set_ext_parm,
    .get_ext_parm =  icom_get_ext_parm,

    .set_ptt =  icom_set_ptt,
    .get_ptt =  icom_get_ptt,
    .get_dcd =  icom_get_dcd,

    .set_rptr_shift =  icom_set_rptr_shift,
    .get_rptr_shift =  icom_get_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
    .set_ctcss_tone =  icom_set_ctcss_tone,
    .get_ctcss_tone =  icom_get_ctcss_tone,
    .set_dcs_code =  icom_set_dcs_code,
    .get_dcs_code =  icom_get_dcs_code,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_dcs_sql =  icom_set_dcs_sql,
    .get_dcs_sql =  icom_get_dcs_sql,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
