/*
 *  Hamlib CI-V backend - description of IC-706 and variations
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

#include "hamlib/rig.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "idx_builtin.h"
#include "bandplan.h"


/*
 *  This function does the special bandwidth coding for IC-706, IC-706MKII
 *  and IC-706MKIIG
 *  (0 - wide, 1 - normal, 2 - narrow)
 */
static int ic706_r2i_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                          unsigned char *md, signed char *pd)
{
    int err;

    err = rig2icom_mode(rig, vfo, mode, width, md, pd);

    if (err != RIG_OK)
    {
        return err;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (*pd == -1)
        {
            *pd = PD_MEDIUM_2;
        }
        else
        {
            (*pd)--;
        }
    }

    return RIG_OK;
}


#define IC706_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_WFM)
#define IC706_1MHZ_TS_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)
#define IC706_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY)

/* tx doesn't have WFM.
 * 100W in all modes but AM (40W)
 */
#define IC706_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC706_AM_TX_MODES (RIG_MODE_AM)

#define IC706IIG_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN)

#define IC706IIG_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR)

#define IC706_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define IC706_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC706_SCAN_OPS (RIG_SCAN_MEM)

/*
 * IC706IIG_REAL_STR_CAL is accurate measurements
 * IC706IIG_STR_CAL is what the S-meter displays
 *
 * calibration data was obtained from W8WWV
 *  http://www.seed-solutions.com/gregordy/
 */
#define IC706IIG_REAL_STR_CAL { 16, \
    { \
        {  46, -27 }, /* S0 */ \
        {  54, -25 }, \
        {  64, -24 }, \
        {  76, -23 }, \
        {  84, -22 }, \
        {  94, -20 }, \
        { 104, -16 }, \
        { 113, -11 }, \
        { 123, -5 }, \
        { 133, 0 },  /* S9 */ \
        { 144, 5 },  /* +10 */ \
        { 156, 10 }, /* +20 */ \
        { 170, 16 }, /* +30 */ \
        { 181, 21 }, /* +40 */ \
        { 192, 26 }, /* +50 */ \
        { 204, 35 } /* +60 */ \
    } }

#define IC706IIG_STR_CAL { 17, \
    { \
        {  45, -60 }, \
        {  46, -54 }, /* S0 */ \
        {  54, -48 }, \
        {  64, -42 }, \
        {  76, -36 }, \
        {  84, -30 }, \
        {  94, -24 }, \
        { 104, -18 }, \
        { 113, -12 }, \
        { 123, -6 }, \
        { 133, 0 },  /* S9 */ \
        { 144, 10 }, /* +10 */ \
        { 156, 20 }, /* +20 */ \
        { 170, 30 }, /* +30 */ \
        { 181, 40 }, /* +40 */ \
        { 192, 50 }, /* +50 */ \
        { 204, 60 }  /* +60 */ \
    } }

/*
 * ic706 rigs capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
static const struct icom_priv_caps ic706_priv_caps =
{
    0x48,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic706_ts_sc_list,
    .serial_USB_echo_check = 1,  /* USB CI-V may not echo */
    .r2i_mode = ic706_r2i_mode
};

const struct rig_caps ic706_caps =
{
    RIG_MODEL(RIG_MODEL_IC706),
    .model_name = "IC-706",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_MOBILE,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC706_VFO_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { RIG_CHAN_END, },    /* FIXME: memory channel list */

    .rx_range_list1 =   { RIG_FRNG_END, },  /* FIXME: enter region 1 setting */
    .tx_range_list1 =   { RIG_FRNG_END, },
    .rx_range_list2 =   { {kHz(30), 199999999, IC706_ALL_RX_MODES, -1, -1, IC706_VFO_ALL}, RIG_FRNG_END, }, /* rx range */
    .tx_range_list2 =   { {kHz(1800), 1999999, IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL}, /* 100W class */
        {kHz(1800), 1999999, IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL}, /* 40W class */
        {kHz(3500), 3999999, IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(3500), 3999999, IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(7), kHz(7300), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(7), kHz(7300), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(10100), kHz(10150), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(10100), kHz(10150), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(14), kHz(14350), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(14), kHz(14350), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(18068), kHz(18168), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(18068), kHz(18168), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(21), kHz(21450), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(21), kHz(21450), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(24890), kHz(24990), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(24890), kHz(24990), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(28), kHz(29700), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(28), kHz(29700), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(50), MHz(54), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(50), MHz(54), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(144), MHz(148), IC706_OTHER_TX_MODES, 5000, 20000, IC706_VFO_ALL}, /* not sure.. */
        {MHz(144), MHz(148), IC706_AM_TX_MODES, 2000, 8000, IC706_VFO_ALL}, /* anyone? */
        RIG_FRNG_END,
    },

    .tuning_steps =     {{IC706_1HZ_TS_MODES, 1},
        {IC706_ALL_RX_MODES, 10},
        {IC706_ALL_RX_MODES, 100},
        {IC706_ALL_RX_MODES, kHz(1)},
        {IC706_ALL_RX_MODES, kHz(5)},
        {IC706_ALL_RX_MODES, kHz(9)},
        {IC706_ALL_RX_MODES, kHz(10)},
        {IC706_ALL_RX_MODES, 12500},
        {IC706_ALL_RX_MODES, kHz(20)},
        {IC706_ALL_RX_MODES, kHz(25)},
        {IC706_ALL_RX_MODES, kHz(100)},
        {IC706_1MHZ_TS_MODES, MHz(1)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)}, /* builtin FL-272 */
        {RIG_MODE_AM, kHz(8)},      /* mid w/ builtin FL-94 */
        {RIG_MODE_AM, kHz(2.4)},    /* narrow w/ builtin FL-272 */
        {RIG_MODE_FM, kHz(15)},     /* ?? TBC, mid w/ builtin FL-23+SFPC455E */
        {RIG_MODE_FM, kHz(8)},      /* narrow w/ builtin FL-94 */
        {RIG_MODE_WFM, kHz(230)},   /* WideFM, filter FL?? */
        RIG_FLT_END,
    },
    .str_cal = IC706IIG_STR_CAL,

    .async_data_supported = 1,
    .read_frame_direct = icom_read_frame_direct,
    .is_async_frame = icom_is_async_frame,
    .process_async_frame = icom_process_async_frame,

    .priv = (void *)& ic706_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .get_vfo =  icom_get_vfo,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ts =  icom_set_ts,

    .set_rptr_shift =  icom_set_rptr_shift,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


static const struct icom_priv_caps ic706mkii_priv_caps =
{
    0x4e,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic706_ts_sc_list,
    .serial_USB_echo_check = 1,  /* USB CI-V may not echo */
    .r2i_mode = ic706_r2i_mode
};

const struct rig_caps ic706mkii_caps =
{
    RIG_MODEL(RIG_MODEL_IC706MKII),
    .model_name = "IC-706MkII",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_MOBILE,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC706_VFO_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { RIG_CHAN_END, },    /* FIXME: memory channel list */

    .rx_range_list1 =   { RIG_FRNG_END, },  /* FIXME: enter region 1 setting */
    .tx_range_list1 =   { RIG_FRNG_END, },
    .rx_range_list2 =   { {kHz(30), 199999999, IC706_ALL_RX_MODES, -1, -1, IC706_VFO_ALL},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =   { {kHz(1800), 1999999, IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL}, /* 100W class */
        {kHz(1800), 1999999, IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL}, /* 40W class */
        {kHz(3500), 3999999, IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(3500), 3999999, IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(7), kHz(7300), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(7), kHz(7300), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(10100), kHz(10150), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(10100), kHz(10150), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(14), kHz(14350), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(14), kHz(14350), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(18068), kHz(18168), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(18068), kHz(18168), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(21), kHz(21450), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(21), kHz(21450), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(24890), kHz(24990), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(24890), kHz(24990), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(28), kHz(29700), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(28), kHz(29700), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(50), MHz(54), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(50), MHz(54), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(144), MHz(148), IC706_OTHER_TX_MODES, 5000, 20000, IC706_VFO_ALL}, /* not sure.. */
        {MHz(144), MHz(148), IC706_AM_TX_MODES, 2000, 8000, IC706_VFO_ALL}, /* anyone? */
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC706_1HZ_TS_MODES, 1},
        {IC706_ALL_RX_MODES, 10},
        {IC706_ALL_RX_MODES, 100},
        {IC706_ALL_RX_MODES, kHz(1)},
        {IC706_ALL_RX_MODES, kHz(5)},
        {IC706_ALL_RX_MODES, kHz(9)},
        {IC706_ALL_RX_MODES, kHz(10)},
        {IC706_ALL_RX_MODES, 12500},
        {IC706_ALL_RX_MODES, kHz(20)},
        {IC706_ALL_RX_MODES, kHz(25)},
        {IC706_ALL_RX_MODES, kHz(100)},
        {IC706_1MHZ_TS_MODES, MHz(1)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)}, /* builtin FL-272 */
        {RIG_MODE_AM, kHz(8)},      /* mid w/ builtin FL-94 */
        {RIG_MODE_AM, kHz(2.4)},    /* narrow w/ builtin FL-272 */
        {RIG_MODE_FM, kHz(15)},     /* ?? TBC, mid w/ builtin FL-23+SFPC455E */
        {RIG_MODE_FM, kHz(8)},      /* narrow w/ builtin FL-94 */
        {RIG_MODE_WFM, kHz(230)},   /* WideFM, filter FL?? */
        RIG_FLT_END,
    },
    .str_cal = IC706IIG_STR_CAL,

    .async_data_supported = 1,
    .read_frame_direct = icom_read_frame_direct,
    .is_async_frame = icom_is_async_frame,
    .process_async_frame = icom_process_async_frame,

    .priv = (void *)& ic706mkii_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .get_vfo =  icom_get_vfo,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ts =  icom_set_ts,

    .set_rptr_shift =  icom_set_rptr_shift,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * IC706MkIIG channel caps.
 */
#define IC706MKIIG_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .split = 1, \
    .tx_freq = 1,   \
    .tx_mode = 1,   \
    .tx_width = 1,  \
    .rptr_offs = 1, \
    .rptr_shift = 1, /* only set */ \
    .funcs = IC706IIG_FUNC_ALL, \
    .levels = RIG_LEVEL_SET(IC706IIG_LEVEL_ALL), \
}

/*
 * Basically, the IC706MKIIG is an IC706MKII plus UHF, a DSP
 * and 50W VHF
 */
static const struct icom_priv_caps ic706mkiig_priv_caps =
{
    0x58,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic706_ts_sc_list,
    .serial_USB_echo_check = 1,  /* USB CI-V may not echo */
    .r2i_mode = ic706_r2i_mode
};

const struct rig_caps ic706mkiig_caps =
{
    RIG_MODEL(RIG_MODEL_IC706MKIIG),
    .model_name = "IC-706MkIIG",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".2",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_MOBILE,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  IC706IIG_FUNC_ALL,
    .has_set_func =  IC706IIG_FUNC_ALL,
    .has_get_level =  IC706IIG_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(IC706IIG_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC706_VFO_OPS,
    .scan_ops =  IC706_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM, IC706MKIIG_MEM_CAP },
        { 100, 105, RIG_MTYPE_EDGE, IC706MKIIG_MEM_CAP },    /* two by two */
        { 106, 107, RIG_MTYPE_CALL, IC706MKIIG_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(200) - 1, IC706_ALL_RX_MODES, -1, -1, IC706_VFO_ALL}, /* this trx also has UHF */
        {MHz(400), MHz(470), IC706_ALL_RX_MODES, -1, -1, IC706_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC706_OTHER_TX_MODES, W(5), W(100), IC706_VFO_ALL, RIG_ANT_1),
        FRQ_RNG_HF(1, IC706_AM_TX_MODES,    W(2), W(40), IC706_VFO_ALL, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(1, IC706_OTHER_TX_MODES, W(5), W(100), IC706_VFO_ALL, RIG_ANT_1),
        FRQ_RNG_6m(1, IC706_AM_TX_MODES,    W(2), W(40), IC706_VFO_ALL, RIG_ANT_1), /* AM class */
        FRQ_RNG_2m(1, IC706_OTHER_TX_MODES, W(5), W(50), IC706_VFO_ALL, RIG_ANT_1),
        FRQ_RNG_2m(1, IC706_AM_TX_MODES,    W(2), W(20), IC706_VFO_ALL, RIG_ANT_1), /* AM class */
        FRQ_RNG_70cm(1, IC706_OTHER_TX_MODES, W(5), W(20), IC706_VFO_ALL, RIG_ANT_1),
        FRQ_RNG_70cm(1, IC706_AM_TX_MODES,    W(2), W(8), IC706_VFO_ALL, RIG_ANT_1), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(200) - 1, IC706_ALL_RX_MODES, -1, -1, IC706_VFO_ALL}, /* this trx also has UHF */
        {MHz(400), MHz(470), IC706_ALL_RX_MODES, -1, -1, IC706_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { {kHz(1800), MHz(2) - 1, IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL}, /* 100W class */
        {kHz(1800), MHz(2) - 1, IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL}, /* 40W class */
        {kHz(3500), MHz(4) - 1, IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(3500), MHz(4) - 1, IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(7), kHz(7300), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(7), kHz(7300), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(10100), kHz(10150), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(10100), kHz(10150), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(14), kHz(14350), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(14), kHz(14350), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(18068), kHz(18168), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(18068), kHz(18168), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(21), kHz(21450), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(21), kHz(21450), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {kHz(24890), kHz(24990), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {kHz(24890), kHz(24990), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(28), kHz(29700), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(28), kHz(29700), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(50), MHz(54), IC706_OTHER_TX_MODES, 5000, 100000, IC706_VFO_ALL},
        {MHz(50), MHz(54), IC706_AM_TX_MODES, 2000, 40000, IC706_VFO_ALL},
        {MHz(144), MHz(148), IC706_OTHER_TX_MODES, 5000, 50000, IC706_VFO_ALL}, /* 50W */
        {MHz(144), MHz(148), IC706_AM_TX_MODES, 2000, 20000, IC706_VFO_ALL}, /* AM VHF is 20W */
        {MHz(430), MHz(450), IC706_OTHER_TX_MODES, 5000, 20000, IC706_VFO_ALL},
        {MHz(430), MHz(450), IC706_AM_TX_MODES, 2000, 8000, IC706_VFO_ALL},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC706_1HZ_TS_MODES, 1},
        {IC706_ALL_RX_MODES, 10},
        {IC706_ALL_RX_MODES, 100},
        {IC706_ALL_RX_MODES, kHz(1)},
        {IC706_ALL_RX_MODES, kHz(5)},
        {IC706_ALL_RX_MODES, kHz(9)},
        {IC706_ALL_RX_MODES, kHz(10)},
        {IC706_ALL_RX_MODES, 12500},
        {IC706_ALL_RX_MODES, kHz(20)},
        {IC706_ALL_RX_MODES, kHz(25)},
        {IC706_ALL_RX_MODES, kHz(100)},
        {IC706_1MHZ_TS_MODES, MHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)}, /* builtin FL-272 */
        {RIG_MODE_AM, kHz(8)},      /* mid w/ builtin FL-94 */
        {RIG_MODE_AM, kHz(2.4)},    /* narrow w/ builtin FL-272 */
        {RIG_MODE_FM, kHz(15)},     /* ?? TBC, mid w/ builtin FL-23+SFPC455E */
        {RIG_MODE_FM, kHz(8)},      /* narrow w/ builtin FL-94 */
        {RIG_MODE_WFM, kHz(230)},   /* WideFM, filter FL?? */
        RIG_FLT_END,
    },
    .str_cal = IC706IIG_STR_CAL,

    .async_data_supported = 1,
    .read_frame_direct = icom_read_frame_direct,
    .is_async_frame = icom_is_async_frame,
    .process_async_frame = icom_process_async_frame,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic706mkiig_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .get_vfo =  icom_get_vfo,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .set_rptr_shift =  icom_set_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  icom_mem_get_split_vfo,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


