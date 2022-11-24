/*
 *  Hamlib Kenwood backend - TS990s description
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (c) 2015 modified from ts2000.c by Bill Somerville G4WJS
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

#include <hamlib/rig.h>
#include "kenwood.h"
#include "ts990s.h"

#define TS990S_AM_MODES RIG_MODE_AM
#define TS990S_FM_MODES (RIG_MODE_FM|RIG_MODE_FMN)
#define TS990S_OTHER_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB)
#define TS990S_HP_MODES (TS990S_OTHER_MODES|TS990S_FM_MODES)
#define TS990S_ALL_MODES (TS990S_OTHER_MODES|TS990S_AM_MODES|TS990S_FM_MODES)

#define TS990S_VFOS (RIG_VFO_MAIN|RIG_VFO_SUB)

#define TS2000_FUNC_ALL (RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_BC|RIG_FUNC_NB|RIG_FUNC_NR|RIG_FUNC_ANF|RIG_FUNC_COMP)

#define TS2000_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_COMP|RIG_LEVEL_AGC|RIG_LEVEL_BKINDL|RIG_LEVEL_METER|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define TS990S_VFO_OP (RIG_OP_BAND_UP|RIG_OP_BAND_DOWN)
#define TS990S_SCAN_OP (RIG_SCAN_VFO)
#define TS990S_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3|RIG_ANT_4)

// Measurements from Wolfgang OE1MWW
#define TS990S_STR_CAL {7, {\
               {0x00, -54},\
               {0x04, -48},\
               {0x0B, -36},\
               {0x13, -24},\
               {0x1B, -12},\
               {0x23,  0},\
               {0x46,  60}}\
               }

/* memory capabilities */
#define TS990S_MEM_CAP {    \
    .freq = 1,      \
    .mode = 1,      \
    .tx_freq=1,     \
    .tx_mode=1,     \
    .split=1,       \
    .rptr_shift=1,      \
    .rptr_offs=1,       \
    .funcs=RIG_FUNC_REV|RIG_FUNC_TONE|RIG_FUNC_TSQL,\
    .tuning_step=1, \
    .ctcss_tone=1,      \
    .ctcss_sql=1,       \
    .scan_group=1,  \
    .flags=1,       \
    .channel_desc=1     \
}


/* prototypes */
static int ts990s_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

static rmode_t ts990s_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_NONE,
    [1] = RIG_MODE_LSB,
    [2] = RIG_MODE_USB,
    [3] = RIG_MODE_CW,
    [4] = RIG_MODE_FM,
    [5] = RIG_MODE_AM,
    [6] = RIG_MODE_RTTY,
    [7] = RIG_MODE_CWR,
    [8] = RIG_MODE_NONE,
    [9] = RIG_MODE_RTTYR,
    [10] = RIG_MODE_NONE,               /* A */
    [11] = RIG_MODE_NONE,               /* B */
    [12] = RIG_MODE_PKTLSB,         /* C */
    [13] = RIG_MODE_PKTUSB,         /* D */
    [14] = RIG_MODE_PKTFM,          /* E */
    [15] = RIG_MODE_NONE,               /* F */
    [16] = RIG_MODE_PKTLSB,         /* G */
    [17] = RIG_MODE_PKTUSB,         /* H */
    [18] = RIG_MODE_PKTFM,          /* I */
    [19] = RIG_MODE_NONE,               /* J */
    [20] = RIG_MODE_PKTLSB,         /* K */
    [21] = RIG_MODE_PKTUSB,         /* L */
    [22] = RIG_MODE_PKTFM,          /* M */
    [23] = RIG_MODE_NONE,               /* N */
};

static struct kenwood_priv_caps  ts990s_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .mode_table = ts990s_mode_table,
};


/*
 * ts990s rig capabilities.
 *
 * part of infos comes from http://www.kenwood.net/
 */
const struct rig_caps ts990s_caps =
{
    RIG_MODEL(RIG_MODEL_TS990S),
    .model_name = "TS-990S",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".3",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG_MICDATA,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  0, /* ms */
    .timeout =  500,
    .retry =  10,

    .has_get_func =  TS2000_FUNC_ALL,
    .has_set_func =  TS2000_FUNC_ALL,
    .has_get_level =  TS2000_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS2000_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .vfo_ops =  TS990S_VFO_OP,
    .scan_ops =  TS990S_SCAN_OP,
    .ctcss_list = kenwood42_ctcss_list,
    .preamp =   { 20, RIG_DBLST_END, },
    .attenuator =   { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =  Hz(9990),
    .max_xit =  Hz(9990),
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =  RIG_TRN_RIG,
    .agc_level_count = 5,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST, RIG_AGC_ON },
    .bank_qty =   0,
    .chan_desc_sz =  7,

    .chan_list =  {
        { 0, 299, RIG_MTYPE_MEM, TS990S_MEM_CAP  },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(300), MHz(60), TS990S_ALL_MODES, -1, -1, TS990S_VFOS, TS990S_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {kHz(1830), kHz(1850), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {kHz(1830), kHz(1850), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {kHz(3500), kHz(3800), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {kHz(3500), kHz(3800), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(7), kHz(7100), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(7), kHz(7100), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(10.1), MHz(10.15), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(10.1), MHz(10.15), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(14), kHz(14350), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(14), kHz(14350), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {kHz(18068), kHz(18168), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {kHz(18068), kHz(18168), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(21), kHz(21450), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(21), kHz(21450), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {kHz(24890), kHz(24990), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {kHz(24890), kHz(24990), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(28), kHz(29700), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(28), kHz(29700), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(50), MHz(50.2), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(50), MHz(50.2), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(300), MHz(60), TS990S_ALL_MODES, -1, -1, TS990S_VFOS, TS990S_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {kHz(1800), MHz(2), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {kHz(3500), MHz(4), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {kHz(3500), MHz(4), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(7), kHz(7300), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(7), kHz(7300), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(10.1), MHz(10.15), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(10.1), MHz(10.15), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(14), kHz(14350), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(14), kHz(14350), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {kHz(18068), kHz(18168), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {kHz(18068), kHz(18168), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(21), kHz(21450), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(21), kHz(21450), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {kHz(24890), kHz(24990), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {kHz(24890), kHz(24990), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(28), kHz(29700), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(28), kHz(29700), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        {MHz(50), MHz(54), TS990S_HP_MODES, W(5), W(200), TS990S_VFOS, TS990S_ANTS},
        {MHz(50), MHz(54), TS990S_AM_MODES, W(5), W(50), TS990S_VFOS, TS990S_ANTS},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {TS990S_OTHER_MODES, 1},
        {TS990S_OTHER_MODES, kHz(0.5)},
        {TS990S_OTHER_MODES, kHz(1)},
        {TS990S_OTHER_MODES, kHz(2.5)},
        {TS990S_OTHER_MODES, kHz(5)},
        {TS990S_OTHER_MODES, kHz(10)},
        {(TS990S_AM_MODES | TS990S_FM_MODES), kHz(5)},
        {(TS990S_AM_MODES | TS990S_FM_MODES), kHz(6.25)},
        {(TS990S_AM_MODES | TS990S_FM_MODES), kHz(10)},
        {(TS990S_AM_MODES | TS990S_FM_MODES), kHz(12.5)},
        {(TS990S_AM_MODES | TS990S_FM_MODES), kHz(15)},
        {(TS990S_AM_MODES | TS990S_FM_MODES), kHz(20)},
        {TS990S_ALL_MODES, MHz(1)},
        {TS990S_ALL_MODES, 0}, /* any tuning step */
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(2.6)}, /* default normal */
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(2.0)}, /* default narrow -
                                                                                                 arbitrary choice */
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(3.2)}, /* default wide -
                                                                                                 arbitrary choice */
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(4.8)},
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(3.8)},
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(2.8)},
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(2.4)},
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(1.8)},
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(1.4)},
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(1.2)},
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(1.0)},
        {RIG_MODE_SSB | TS990S_FM_MODES, kHz(0.8)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)}, /* default normal */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(250)}, /* default narrow -
                                                                                         arbitrary choice */
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1.0)}, /* default wide - arbitrary choice */
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(2.5)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(2.0)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1.5)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(600)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(400)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(200)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(150)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(100)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(80)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(50)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)}, /* default normal */
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(250)}, /* default narrow -
                                                                                                arbitrary choice */
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(1500)}, /* default wide -
                                                                                                arbitrary choice */
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(1000)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(400)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(300)},
        {RIG_MODE_AM, kHz(5)},          /* default normal */
        {RIG_MODE_AM, kHz(2.5)},        /* default narrow - arbitrary choice */
        {RIG_MODE_AM, kHz(4)},
        {RIG_MODE_AM, kHz(3)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.6)}, /* default normal */
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(500)}, /* default narrow -
                                                                                                        arbitrary choice */
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(13)}, /* default wide -
                                                                                                        arbitrary choice */
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.8)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.4)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.2)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.0)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(1.5)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(1.0)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(600)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(400)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(300)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(200)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(150)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(100)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(80)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(50)},
        RIG_FLT_END,
    },

    .str_cal = TS990S_STR_CAL,

    .priv = (void *)& ts990s_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  kenwood_set_xit,
    .get_xit =  kenwood_get_xit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode,
    .set_vfo =  kenwood_set_vfo_main_sub,
    .get_vfo =  kenwood_get_vfo_main_sub,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .set_ctcss_tone =  kenwood_set_ctcss_tone_tn,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .set_ctcss_sql =  kenwood_set_ctcss_sql,
    .get_ctcss_sql =  kenwood_get_ctcss_sql,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  kenwood_set_func,
    .get_func =  kenwood_get_func,
    .set_level =  kenwood_set_level,
    .get_level =  ts990s_get_level,
    .set_ant =  kenwood_set_ant,
    .get_ant =  kenwood_get_ant,
    .send_morse =  kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .vfo_op =  kenwood_vfo_op,
    .scan =  kenwood_scan,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .set_powerstat =  kenwood_set_powerstat,
    .get_powerstat =  kenwood_get_powerstat,
    .reset =  kenwood_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

/*
 * ts2000_get_level
 * Assumes rig!=NULL, val!=NULL
 */

int ts990s_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[50];
    int lvl, retval = RIG_OK;

    if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
    {
        if (RIG_OK != (retval = kenwood_get_vfo_main_sub(rig, &vfo)))
        {
            return retval;
        }
    }

    switch (level)
    {

    case RIG_LEVEL_PREAMP:
        retval = kenwood_safe_transaction(rig, "PA", lvlbuf, sizeof(lvlbuf), 4);

        if (retval != RIG_OK)
        {
            return retval;
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN:
            val->i = '1' == lvlbuf[2] ? rig->state.preamp[0] : 0;
            break;

        case RIG_VFO_SUB:
            val->i = '1' == lvlbuf[3] ? rig->state.preamp[0] : 0;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        break;

    case RIG_LEVEL_ATT:
    {
        char v;
        char cmd[4];

        switch (vfo)
        {
        case RIG_VFO_MAIN: v = '0'; break;

        case RIG_VFO_SUB: v = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "RA%c", v);
        retval = kenwood_safe_transaction(rig, cmd, lvlbuf, sizeof(lvlbuf), 4);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if ('0' == lvlbuf[3])
        {
            val->i = 0;
        }
        else
        {
            val->i = rig->state.attenuator[lvlbuf[3] - '1'];
        }
    }
    break;

    case RIG_LEVEL_VOXDELAY:
        retval = kenwood_safe_transaction(rig, "VD0", lvlbuf, sizeof(lvlbuf), 6);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 3, "%d", &lvl);
        val->i = lvl * 3 / 2;               /* 150ms units converted to 100ms units */
        break;

    case RIG_LEVEL_AF:
    {
        char v;
        char cmd[4];

        switch (vfo)
        {
        case RIG_VFO_MAIN: v = '0'; break;

        case RIG_VFO_SUB: v = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "AG%c", v);
        retval = kenwood_safe_transaction(rig, cmd, lvlbuf, sizeof(lvlbuf), 6);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 3, "%d", &lvl);
        val->f = lvl / 255.0;
    }
    break;

    case RIG_LEVEL_RF:
    {
        char v;
        char cmd[4];

        switch (vfo)
        {
        case RIG_VFO_MAIN: v = '0'; break;

        case RIG_VFO_SUB: v = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "RG%c", v);
        retval = kenwood_safe_transaction(rig, cmd, lvlbuf, sizeof(lvlbuf), 6);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 3, "%d", &lvl);
        val->f = lvl / 255.0;
    }
    break;

    case RIG_LEVEL_SQL:
    {
        char v;
        char cmd[4];

        switch (vfo)
        {
        case RIG_VFO_MAIN: v = '0'; break;

        case RIG_VFO_SUB: v = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "SQ%c", v);
        retval = kenwood_safe_transaction(rig, cmd, lvlbuf, sizeof(lvlbuf), 6);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 3, "%d", &lvl);
        val->f = lvl / 255.0;
    }
    break;

    case RIG_LEVEL_CWPITCH:
        retval = kenwood_safe_transaction(rig, "PT", lvlbuf, sizeof(lvlbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->i = 300 + lvl * 10;
        break;

    case RIG_LEVEL_RFPOWER:
        retval = kenwood_safe_transaction(rig, "PC", lvlbuf, sizeof(lvlbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = lvl / 200.0;               /* TODO: should we detect AM and scale? */
        break;

    case RIG_LEVEL_MICGAIN:

        return kenwood_get_level(rig, vfo, level, val);

    case RIG_LEVEL_KEYSPD:
        retval = kenwood_safe_transaction(rig, "KS", lvlbuf, sizeof(lvlbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->i = lvl;
        break;

    case RIG_LEVEL_COMP:
        retval = kenwood_safe_transaction(rig, "PL", lvlbuf, sizeof(lvlbuf), 8);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        lvl = lvl / 1000;                       /* report input level */
        val->f = lvl / 255.0;
        break;

    case RIG_LEVEL_AGC:
    {
        char v;
        char cmd[4];

        switch (vfo)
        {
        case RIG_VFO_MAIN: v = '0'; break;

        case RIG_VFO_SUB: v = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "GC%c", v);

        if (RIG_OK != (retval = kenwood_safe_transaction(rig, cmd, lvlbuf,
                                sizeof(lvlbuf), 4)))
        {
            return retval;
        }

        switch (lvlbuf[3])
        {
        case '0': val->i = RIG_AGC_OFF; break;

        case '1': val->i = RIG_AGC_SLOW; break;

        case '2': val->i = RIG_AGC_MEDIUM; break;

        case '3': val->i = RIG_AGC_FAST; break;
        }
    }
    break;

    case RIG_LEVEL_BKINDL:
        retval = kenwood_safe_transaction(rig, "SD", lvlbuf, sizeof(lvlbuf), 6);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->i = lvl / 100;
        break;

    case RIG_LEVEL_METER:
        retval = kenwood_safe_transaction(rig, "RM", lvlbuf, sizeof(lvlbuf), 7);

        if (retval != RIG_OK)
        {
            return retval;
        }

        switch (lvlbuf[2])
        {
        case '1': val->i = RIG_METER_ALC; break;

        case '2': val->i = RIG_METER_SWR; break;

        case '3': val->i = RIG_METER_COMP; break;

        case '4': val->i = RIG_METER_IC; break;

        case '5': val->i = RIG_METER_VDD; break;

        default: val->i = RIG_METER_NONE; break;
        }

        break;

    case RIG_LEVEL_VOXGAIN:
        retval = get_kenwood_level(rig, "VG00", &val->f, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        break;

    case RIG_LEVEL_ANTIVOX:
        retval = get_kenwood_level(rig, "VG00", &val->f, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = val->f * 255. / 20.;
        break;

    case RIG_LEVEL_RAWSTR:
    case RIG_LEVEL_STRENGTH:
    {
        char v;
        char cmd[4];

        switch (vfo)
        {
        case RIG_VFO_MAIN: v = '0'; break;

        case RIG_VFO_SUB: v = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "SM%c", v);
        retval = kenwood_safe_transaction(rig, cmd, lvlbuf, sizeof(lvlbuf), 7);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* Frontend expects:  -54 = S0, 0 = S9  */
        sscanf(lvlbuf + 3, "%d", &val->i);

        /* TS-990s returns values from 0 - 70 */
        /* so scale the value */
        if (level == RIG_LEVEL_STRENGTH)
        {
            val->i = (val->i * 54. / 70.) - 54;
        }
    }
    break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return retval;
}
