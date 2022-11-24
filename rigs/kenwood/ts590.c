/*
 *  Hamlib Kenwood backend - TS590 description
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

#include <stdio.h>

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "kenwood.h"
#include "cal.h"


/* Function declarations  */
const char *ts590_get_info(RIG *rig);
int ts590_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

#define TS590_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTFM|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB)
#define TS590_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS590_AM_TX_MODES RIG_MODE_AM
#define TS590_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define TS590_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|\
        RIG_LEVEL_CWPITCH|RIG_LEVEL_METER|RIG_LEVEL_SWR|RIG_LEVEL_ALC|\
        RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|\
        RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_PREAMP|RIG_LEVEL_ATT)
#define TS590_FUNC_ALL (RIG_FUNC_LOCK|RIG_FUNC_AIP|RIG_FUNC_TONE|\
        RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC)

#define TS590_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)
#define TS590_SCAN_OPS (RIG_SCAN_VFO)

#define TS590_ANTS (RIG_ANT_1|RIG_ANT_2)

#define TS590_CHANNEL_CAPS { \
        .freq=1,\
        .mode=1,\
        .tx_freq=1,\
        .tx_mode=1,\
        .split=1,\
        .funcs=RIG_FUNC_TONE, \
        .flags=RIG_CHFLAG_SKIP \
        }

#define TS590_STR_CAL {9, {\
                       { 0, -60},\
                       { 3, -48},\
                       { 6, -36},\
                       { 9, -24},\
                       {12, -12},\
                       {15,   0},\
                       {20,  20},\
                       {25,  40},\
                       {30,  60}}\
                       }


static struct kenwood_priv_caps ts590_priv_caps =
{
    .cmdtrm = EOM_KEN,
};


/*
 * ts590 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps ts590_caps =
{
    RIG_MODEL(RIG_MODEL_TS590S),
    .model_name = "TS-590S",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".2",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_HARDWARE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 10,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,
    .agc_level_count = 4,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_FAST, RIG_AGC_ON },


    .chan_list =  { /* TBC */
        {  0, 89, RIG_MTYPE_MEM,  TS590_CHANNEL_CAPS },
        { 90, 99, RIG_MTYPE_EDGE, TS590_CHANNEL_CAPS },
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        {kHz(30),   Hz(59999999), TS590_ALL_MODES, -1, -1, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(3500),  kHz(3800),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7200),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7200),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(30),   Hz(59999999), TS590_ALL_MODES, -1, -1, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(3500),  MHz(4) - 1, TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(5250),  kHz(5450),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(5250),  kHz(5450),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7300),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7300),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS590_ALL_MODES, kHz(1)},
        {TS590_ALL_MODES, Hz(2500)},
        {TS590_ALL_MODES, kHz(5)},
        {TS590_ALL_MODES, Hz(6250)},
        {TS590_ALL_MODES, kHz(10)},
        {TS590_ALL_MODES, Hz(12500)},
        {TS590_ALL_MODES, kHz(15)},
        {TS590_ALL_MODES, kHz(20)},
        {TS590_ALL_MODES, kHz(25)},
        {TS590_ALL_MODES, kHz(30)},
        {TS590_ALL_MODES, kHz(100)},
        {TS590_ALL_MODES, kHz(500)},
        {TS590_ALL_MODES, MHz(1)},
        {TS590_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .str_cal = TS590_STR_CAL,
    .priv = (void *)& ts590_priv_caps,
    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit,   /*  FIXME should this switch to rit mode or just set the frequency? */
    .get_rit = kenwood_get_rit,
    .set_xit = kenwood_set_xit,   /* FIXME should this switch to xit mode or just set the frequency?  */
    .get_xit = kenwood_get_xit,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .get_info = ts590_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan_ops =  TS590_SCAN_OPS,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = RIG_LEVEL_SET(TS590_LEVEL_ALL),
    .has_get_level = TS590_LEVEL_ALL,
    .set_level = kenwood_set_level,
    .get_level = ts590_get_level,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .has_get_func = TS590_FUNC_ALL,
    .has_set_func = TS590_FUNC_ALL,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .ctcss_list =  kenwood38_ctcss_list,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .send_morse =  kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .set_channel =  kenwood_set_channel,
    .get_channel =  kenwood_get_channel,
    .vfo_ops = TS590_VFO_OPS,
    .vfo_op =  kenwood_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps ts590sg_caps =
{
    RIG_MODEL(RIG_MODEL_TS590SG),
    .model_name = "TS-590SG",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".1",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_HARDWARE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 10,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,


    .chan_list =  { /* TBC */
        {  0, 89, RIG_MTYPE_MEM,  TS590_CHANNEL_CAPS },
        { 90, 99, RIG_MTYPE_EDGE, TS590_CHANNEL_CAPS },
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        {kHz(30),   Hz(59999999), TS590_ALL_MODES, -1, -1, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(3500),  kHz(3800),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7200),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7200),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(30),   Hz(59999999), TS590_ALL_MODES, -1, -1, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(3500),  MHz(4) - 1, TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(5250),  kHz(5450),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(5250),  kHz(5450),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7300),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7300),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS590_ALL_MODES, kHz(1)},
        {TS590_ALL_MODES, Hz(2500)},
        {TS590_ALL_MODES, kHz(5)},
        {TS590_ALL_MODES, Hz(6250)},
        {TS590_ALL_MODES, kHz(10)},
        {TS590_ALL_MODES, Hz(12500)},
        {TS590_ALL_MODES, kHz(15)},
        {TS590_ALL_MODES, kHz(20)},
        {TS590_ALL_MODES, kHz(25)},
        {TS590_ALL_MODES, kHz(30)},
        {TS590_ALL_MODES, kHz(100)},
        {TS590_ALL_MODES, kHz(500)},
        {TS590_ALL_MODES, MHz(1)},
        {TS590_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .str_cal = TS590_STR_CAL,
    .priv = (void *)& ts590_priv_caps,
    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit,   /*  FIXME should this switch to rit mode or just set the frequency? */
    .get_rit = kenwood_get_rit,
    .set_xit = kenwood_set_xit,   /* FIXME should this switch to xit mode or just set the frequency?  */
    .get_xit = kenwood_get_xit,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .get_info = kenwood_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan_ops =  TS590_SCAN_OPS,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = RIG_LEVEL_SET(TS590_LEVEL_ALL),
    .has_get_level = TS590_LEVEL_ALL,
    .set_level = kenwood_set_level,
    .get_level = kenwood_get_level,
    .has_get_func = TS590_FUNC_ALL,
    .has_set_func = TS590_FUNC_ALL,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .ctcss_list =  kenwood38_ctcss_list,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .send_morse =  kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .set_channel =  kenwood_set_channel,
    .get_channel =  kenwood_get_channel,
    .vfo_ops = TS590_VFO_OPS,
    .vfo_op =  kenwood_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * Function definitions below
 */

/*
 * ts590_get_info
 * This is not documented in the manual as of 3/11/15 but confirmed from Kenwood
 * "TY" produces "TYK 00" for example
 */
const char *ts590_get_info(RIG *rig)
{
    char firmbuf[10];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = kenwood_safe_transaction(rig, "TY", firmbuf, 10, 6);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    switch (firmbuf[2])
    {
    case 'K': return "Firmware: USA version";

    case 'E': return "Firmware: European version";

    default: return "Firmware: unknown";
    }
}

/*
 * ts590_get_level
 * only difference from standard Kenwood is AF level which has an argument
 */
int ts590_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int lvl_len;
    int retval;
    char lvlbuf[50];


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_KEYSPD:
    case RIG_LEVEL_AGC:
    case RIG_LEVEL_SQL:
    case RIG_LEVEL_CWPITCH:
    case RIG_LEVEL_RFPOWER:
    case RIG_LEVEL_RF:
    case RIG_LEVEL_AF:
    case RIG_LEVEL_MICGAIN:
        return kenwood_get_level(rig, vfo, level, val);


    case RIG_LEVEL_METER:
        retval = kenwood_transaction(rig, "RM0", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 7)
        {
            rig_debug(RIG_DEBUG_ERR, "ts590_get_level: "
                      "unexpected answer len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        // returns the raw value in dots
        sscanf(lvlbuf + 3, "%d", &val->i);
        return retval;

    case RIG_LEVEL_SWR:
        retval = kenwood_transaction(rig, "RM1", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 7)
        {
            rig_debug(RIG_DEBUG_ERR, "ts590_get_level: "
                      "unexpected answer len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        // returns the raw value in dots
        sscanf(lvlbuf + 3, "%d", &val->i);
        return retval;

    case RIG_LEVEL_COMP:
        retval = kenwood_transaction(rig, "RM2", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 7)
        {
            rig_debug(RIG_DEBUG_ERR, "ts590_get_level: "
                      "unexpected answer len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        // returns the raw value in dots
        sscanf(lvlbuf + 3, "%d", &val->i);
        return retval;

    case RIG_LEVEL_ALC:
        retval = kenwood_transaction(rig, "RM3", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 7)
        {
            rig_debug(RIG_DEBUG_ERR, "ts590_get_level: "
                      "unexpected answer len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        // returns the raw value in dots
        sscanf(lvlbuf + 3, "%d", &val->i);
        return retval;

    case RIG_LEVEL_PREAMP:
        retval = kenwood_transaction(rig, "PA", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if (lvlbuf[2] == '0')
        {
            val->i = 0;
        }
        else if (lvlbuf[2] == '1')
        {
            val->i = rig->state.preamp[0];
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: "
                      "unexpected preamp char '%c'\n",
                      __func__, lvlbuf[2]);
            RETURNFUNC(-RIG_EPROTO);
        }

        return retval;

    case RIG_LEVEL_ATT:
        retval = kenwood_transaction(rig, "RA", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if (lvlbuf[3] == '0')
        {
            val->i = 0;
        }
        else if (lvlbuf[3] == '1')
        {
            val->i = rig->state.attenuator[0];
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: "
                      "unexpected att char '%c'\n",
                      __func__, lvlbuf[2]);
            RETURNFUNC(-RIG_EPROTO);
        }

        return retval;

    case RIG_LEVEL_RAWSTR:
    case RIG_LEVEL_STRENGTH:
        retval = kenwood_transaction(rig, "SM0", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (((lvl_len != 7)) || lvlbuf[1] != 'M')
        {
            /* TS-590 returns 8 bytes for S meter level */
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n", __func__, (int)lvl_len);
            return -RIG_ERJCTED;
        }

        /* Frontend expects:  -54 = S0, 0 = S9  */
        sscanf(lvlbuf + 3, "%d", &val->i);

        /* TS-590 main receiver returns values from 0 - 30 */
        /* Indicates # of dots on meter */
        /* so first 15 are S0-S9 and last 15 are 20/40/60 */
        if (level == RIG_LEVEL_STRENGTH)
        {
            cal_table_t str_cal = TS590_SM_CAL;
            val->i = (int) rig_raw2val(val->i, &str_cal);
        }

        return retval;

    default: return -RIG_EINVAL;
    }
}
