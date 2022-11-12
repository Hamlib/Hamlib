/*
 *  Hamlib CI-V backend - description of IC-756 and variations
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

#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"
#include "bandplan.h"

#define IC756_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756_1HZ_TS_MODES IC756_ALL_RX_MODES

/*
 * 100W in all modes but AM (40W)
 */
#define IC756_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756_AM_TX_MODES (RIG_MODE_AM)

#define IC756PRO_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_RF|RIG_FUNC_ANF)
#define IC756PRO_FUNC_SET (IC756PRO_FUNC_ALL|RIG_FUNC_DUAL_WATCH)

#define IC756PRO_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_BALANCE|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_COMP)

/* Note that RIG_LEVEL_VOXGAIN and RIG_LEVEL_ANTIVOX are incorrectly handled in icom.c for
 * this model.
 */

#define IC756_VFO_ALL (RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MEM)

#define IC756_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC756_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_PROG|RIG_SCAN_DELTA|RIG_SCAN_PRIO)

#define IC756_ANTS (RIG_ANT_1|RIG_ANT_2)

struct cmdparams ic756pro_cmdparms[] =
{
    { {.s = RIG_PARM_BEEP}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x20}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x09}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_TIME}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x16}, CMD_DAT_TIM, 2 },
    { {.s = RIG_LEVEL_VOXDELAY}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x60}, CMD_DAT_INT, 1 },
    { {0} }
};

#define IC756PRO_STR_CAL { 16, \
    { \
         { 0,   -60 }, \
         { 14,  -52 }, \
         { 29,  -44 }, \
         { 44,  -36 }, \
         { 60,  -28 }, \
         { 75,  -20 }, \
         { 91,  -12 }, \
         { 107, -4 }, \
         { 124,  4 }, \
         { 141, 12 }, \
         { 158, 20 }, \
         { 175, 28 }, \
         { 192, 36 }, \
         { 210, 44 }, \
         { 228, 52 }, \
         { 247 ,60 } \
    } }

/*
 *  This function deals with the older type radios with only 2 filter widths
 *  (0 - normal, 1 - narrow)
 */
static int r2i_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                    unsigned char *md, signed char *pd)
{
    int err;

    err = rig2icom_mode(rig, vfo, mode, width, md, pd);

    if (err != RIG_OK)
    {
        return err;
    }

    if (PD_WIDE_3 == *pd)
    {
        *pd = PD_MEDIUM_2;
    }
    else if (*pd > PD_WIDE_3)
    {
        (*pd)--;
    }

    return RIG_OK;
}

/*
 * IC-756 rig capabilities.
 */
static const struct icom_priv_caps ic756_priv_caps =
{
    0x50,    /* default address */
    0,       /* 731 mode */
    0,       /* no XCHG */
    ic756_ts_sc_list,
    .antack_len = 3,
    .ant_count = 2,
    .r2i_mode = r2i_mode,
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = RIG_AGC_LAST, .icom_level = -1 },
    },
};

const struct rig_caps ic756_caps =
{
    RIG_MODEL(RIG_MODEL_IC756),
    .model_name = "IC-756",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".3",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
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
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END, }, /* FIXME: TBC */
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo =  0,
    .vfo_ops =  IC756_VFO_OPS,
    .scan_ops =  IC756_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(60), IC756_ALL_RX_MODES, -1, -1, IC756_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, IC756_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_6m(1, IC756_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_HF(1, IC756_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        FRQ_RNG_6m(1, IC756_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(60), IC756_ALL_RX_MODES, -1, -1, IC756_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC756_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_6m(2, IC756_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_HF(2, IC756_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        FRQ_RNG_6m(2, IC756_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC756_1HZ_TS_MODES, 1},
        {IC756_ALL_RX_MODES, kHz(1)},
        {IC756_ALL_RX_MODES, kHz(5)},
        {IC756_ALL_RX_MODES, kHz(9)},
        {IC756_ALL_RX_MODES, kHz(10)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_CW, kHz(2.4)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_CWR, kHz(2.4)},
        {RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(9)},
        RIG_FLT_END,
    },
    .str_cal = IC756PRO_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic756_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,
    .get_vfo =  icom_get_vfo,
    .set_ant =  icom_set_ant,
    .get_ant =  icom_get_ant,

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
    .get_ts =  icom_get_ts,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_freq_mode =  icom_set_split_freq_mode,
    .get_split_freq_mode =  icom_get_split_freq_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  NULL,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * IC-756pro rig capabilities.
 *
 * TODO: check every parameter, add antenna capabilities
 */
static const struct icom_priv_caps ic756pro_priv_caps =
{
    0x5c,    /* default address */
    0,        /* 731 mode */
    0,    /* no XCHG */
    ic756pro_ts_sc_list,
    .antack_len = 3,
    .ant_count = 2,
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = RIG_AGC_LAST, .icom_level = -2 },
    },
};

const struct rig_caps ic756pro_caps =
{
    RIG_MODEL(RIG_MODEL_IC756PRO),
    .model_name = "IC-756PRO",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".3",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
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
    .has_get_func =  IC756PRO_FUNC_ALL,
    .has_set_func =  IC756PRO_FUNC_SET,
    .has_get_level =  IC756PRO_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(IC756PRO_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_KEYSPD] = { .min = { .i = 6 }, .max = { .i = 48 }, .step = { .i = 1 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 1 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 10, 20, RIG_DBLST_END, }, /* FIXME: TBC */
    .attenuator =   { 6, 12, 18, 20, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo =  0,
    .vfo_ops =  IC756_VFO_OPS,
    .scan_ops =  IC756_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(60), IC756_ALL_RX_MODES, -1, -1, IC756_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, IC756_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_6m(1, IC756_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_HF(1, IC756_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        FRQ_RNG_6m(1, IC756_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(60), IC756_ALL_RX_MODES, -1, -1, IC756_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC756_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_6m(2, IC756_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_HF(2, IC756_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        FRQ_RNG_6m(2, IC756_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC756_1HZ_TS_MODES, 1},
        {IC756_ALL_RX_MODES, kHz(1)},
        {IC756_ALL_RX_MODES, kHz(5)},
        {IC756_ALL_RX_MODES, kHz(9)},
        {IC756_ALL_RX_MODES, kHz(10)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_AM, kHz(8)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(8)},
        RIG_FLT_END,
    },
    .str_cal = IC756PRO_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic756pro_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,
    .get_vfo =  icom_get_vfo,
    .set_ant =  icom_set_ant,
    .get_ant =  icom_get_ant,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .set_rptr_shift =  icom_set_rptr_shift,
    .get_rptr_shift =  icom_get_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
    .set_ctcss_tone =  icom_set_ctcss_tone,
    .get_ctcss_tone =  icom_get_ctcss_tone,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_freq_mode =  icom_set_split_freq_mode,
    .get_split_freq_mode =  icom_get_split_freq_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  NULL,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * IC-756proII rig capabilities.
 *
 * TODO: check every parameter, add antenna capabilities
 */
static const struct icom_priv_caps ic756pro2_priv_caps =
{
    0x64,    /* default address */
    0,       /* 731 mode */
    0,       /* no XCHG */
    ic756pro_ts_sc_list,
    .antack_len = 3,
    .ant_count = 2,
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = RIG_AGC_LAST, .icom_level = -1 },
    },
    .extcmds = ic756pro_cmdparms,   /* Custom op parameters */
};

/*
 * These TOKEN_BACKEND's are on a different name space than conf's
 */
#define TOK_MEMNAME TOKEN_BACKEND(1)
#define TOK_MYCALL  TOKEN_BACKEND(2)

static const struct confparams ic756pro2_ext_parms[] =
{
    {
        TOK_SSBBASS, "ssbbass", "SSB Tx Tone (Bass)", "SSB Tx Tone (Bass)",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } }
    },
    {
        TOK_MEMNAME, "showmem", "Show mem name", "Show memory name",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_SQLCTRL, "sqlctrl", "RF/Sql control", "set RF/Squelch control",
        NULL, RIG_CONF_COMBO, { .c = {{ "Auto", "Sql", "RF+Sql", NULL }} }
    },
    {
        TOK_MYCALL, "mycall", "Callsign", "My call sign",
        NULL, RIG_CONF_STRING, { }
    },
    {
        TOK_RTTY_FLTR, "rttyfltr", "RTTY Fltr Width preset", "Set/Read RTTY preset filter width",
        "350", RIG_CONF_COMBO, { .c = {{"250", "300", "350", "500", "1000", NULL }} }
    },
    { RIG_CONF_END, NULL, }
};


/*
 * NUMERIC: val.f
 * COMBO: val.i, starting from 0
 * STRING: val.cs for set, val.s for get
 * CHECKBUTTON: val.i 0/1
 */

/*IC-756Pro Rig parameters Only available in this namespace*/
#define S_MEM_SC_LEN        2   /* 756PRO S_MEM subcmd length */
#define S_MEM_SBASS     0x501   /* SSB TX tone bass level */
#define S_MEM_NAME      0x514   /* send/read memory name */
#define S_MEM_MYCALL        0x515
#define S_MEM_BEEP      0x520   /* Button confirmation */
#define S_MEM_SQL_CTL       0x522   /* RF/SQL ctl set 0=auto; 1 = sql; 2 = RF+SQL */
#define S_MEM_QSPLT     0x524   /* enable quick split mode */
#define S_MEM_TRCV      0x532   /* CI-V trancieve mode */
#define S_MEM_LANG      0x536   /* 0=English 1=Japanese for voice announcer */
#define S_MEM_SCN_SPD       0x556   /* 0 = low; 1 = high */
#define S_MEM_RTTY_FL_PB    0x561   /* 0=250 Hz, 1=300' 2 = 350, 3 = 500, 4 = 1 KHz */
#define S_MEM_RTTY_TWNPK    0x562   /* rtty twin peak filter off/on */


static int ic756pro2_set_ext_parm(RIG *rig, token_t token, value_t val);
static int ic756pro2_get_ext_parm(RIG *rig, token_t token, value_t *val);

#define IC756PROII_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756PROII_1HZ_TS_MODES IC756PROII_ALL_RX_MODES
#define IC756PROII_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756PROII_AM_TX_MODES (RIG_MODE_AM)

#define IC756PROII_LEVEL_ALL (IC756PRO_LEVEL_ALL|RIG_LEVEL_VOXDELAY)

#define IC756PROII_PARMS (RIG_PARM_ANN|RIG_PARM_BEEP|RIG_PARM_BACKLIGHT|RIG_PARM_TIME)

const struct rig_caps ic756pro2_caps =
{
    RIG_MODEL(RIG_MODEL_IC756PROII),
    .model_name = "IC-756PROII",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".3",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
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
    .has_get_func =  IC756PRO_FUNC_ALL,
    .has_set_func =  IC756PRO_FUNC_SET,
    .has_get_level =  IC756PROII_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(IC756PROII_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = { .min = { .i = 6 }, .max = { .i = 48 }, .step = { .i = 1 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 1 } },
    },
    .parm_gran =  {},
    .extparms =  ic756pro2_ext_parms,
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 10, 20, RIG_DBLST_END, }, /* FIXME: TBC */
    .attenuator =   { 6, 12, 18, 20, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0),
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo =  0,
    .vfo_ops =  IC756_VFO_OPS,
    .scan_ops =  IC756_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(60), IC756PROII_ALL_RX_MODES, -1, -1, IC756_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC756PROII_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_6m(1, IC756PROII_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_HF(1, IC756PROII_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        FRQ_RNG_6m(1, IC756PROII_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(60), IC756PROII_ALL_RX_MODES, -1, -1, IC756_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC756PROII_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_6m(2, IC756PROII_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_HF(2, IC756PROII_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        FRQ_RNG_6m(2, IC756PROII_AM_TX_MODES, W(2), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC756PROII_1HZ_TS_MODES, 1},
        {IC756PROII_ALL_RX_MODES, kHz(1)},
        {IC756PROII_ALL_RX_MODES, kHz(5)},
        {IC756PROII_ALL_RX_MODES, kHz(9)},
        {IC756PROII_ALL_RX_MODES, kHz(10)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(8)},
        RIG_FLT_END,
    },
    .str_cal = IC756PRO_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic756pro2_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
    .set_vfo =  icom_set_vfo,
    .get_vfo =  icom_get_vfo,
    .set_ant =  icom_set_ant,
    .get_ant =  icom_get_ant,

    .decode_event =  icom_decode_event,
    .set_parm =  icom_set_parm,
    .get_parm =  icom_get_parm,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .set_rptr_shift =  icom_set_rptr_shift,
    .get_rptr_shift =  icom_get_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
    .set_ctcss_tone =  icom_set_ctcss_tone,
    .get_ctcss_tone =  icom_get_ctcss_tone,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_freq_mode =  icom_set_split_freq_mode,
    .get_split_freq_mode =  icom_get_split_freq_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  NULL,

    .set_ext_parm =  ic756pro2_set_ext_parm,
    .get_ext_parm =  ic756pro2_get_ext_parm,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int ic756pro2_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    unsigned char epbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int ack_len, ep_len, val_len;
    int ep_cmd = C_CTL_MEM;
    int ep_sc;             /* Subcommand in $1A $05xx */
    int icom_val = 0;
    int retval;

    ep_len = 0; /* 0 implies BCD data */
    val_len = 1;

    switch (token)
    {
    case TOK_SSBBASS:
        ep_sc = S_MEM_SBASS ;
        icom_val = val.f;
        break;

    case TOK_MEMNAME:
        ep_sc = S_MEM_NAME;
        icom_val = val.i ? 1 : 0;
        break;

    case TOK_SQLCTRL:
        ep_sc = S_MEM_SQL_CTL;
        /* TODO: check range this actually doesn't decode the input type 'string' */
        icom_val = val.i;
        break;

    case TOK_MYCALL:    /* max 10 ASCII char */
        ep_len = strlen(val.cs);

        if (ep_len > 10)
        {
            return -RIG_EINVAL;
        }

        ep_sc = S_MEM_MYCALL;
        memcpy(epbuf, val.cs, ep_len);
        break;

    case TOK_RTTY_FLTR: /* RTTY filter mode 0 - 4 = 250, 300, 350, 500, 1000 */
        if (val.i < 0 || val.i > 4) { return -RIG_EINVAL; }

        ep_sc = S_MEM_RTTY_FL_PB;
        icom_val = val.i;
        break;

    default:
        return -RIG_EINVAL;
    }

    if (ep_len == 0)
    {
        to_bcd_be(epbuf, (long long)icom_val, val_len * 2);
        ep_len += val_len;
    }

    retval = icom_transaction(rig, ep_cmd, ep_sc, epbuf, ep_len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), "
                  "len=%d\n", __func__, ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
static int ic756pro2_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    const struct confparams *cfp;

    unsigned char resbuf[MAXFRAMELEN];
    int res_len, icom_val = 0;
    int cmdhead;
    int retval;

    int ep_cmd = C_CTL_MEM;
    int ep_sc;             /* Subcommand in $1A $05xx */

    switch (token)
    {
    case TOK_SSBBASS:
        ep_sc = S_MEM_SBASS ;
        break;

    case TOK_MEMNAME:
        ep_sc = S_MEM_NAME;
        break;

    case TOK_SQLCTRL:
        ep_sc = S_MEM_SQL_CTL;
        break;

    case TOK_MYCALL:    /* max 10 ASCII char */
        ep_sc = S_MEM_MYCALL;
        break;

    case TOK_RTTY_FLTR: /* RTTY filter mode 0 - 4 */
        ep_sc = S_MEM_RTTY_FL_PB;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_ext_parm %s", rig_strparm(token));
        return -RIG_EINVAL;
    }

    retval = icom_transaction(rig, ep_cmd, ep_sc, NULL, 0,
                              resbuf, &res_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * strbuf should contain Cn,Sc,Data area
     */
    cmdhead = (ep_sc == -1) ? 1 : S_MEM_SC_LEN + 1;
    res_len -= cmdhead;

    /* should echo cmd, subcmd and then data, if you get an ack something is wrong */
    if (resbuf[0] != ep_cmd)
    {
        if (resbuf[0] == ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), "
                      "len=%d\n", __func__, resbuf[0], res_len);
            return -RIG_EPROTO;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), "
                      "len=%d\n", __func__, resbuf[0], res_len);
            return -RIG_ERJCTED;
        }
    }

    cfp = rig_ext_lookup_tok(rig, token);

    switch (cfp->type)
    {
    case RIG_CONF_STRING:
        memcpy(val->s, resbuf, res_len);
        break;

    case RIG_CONF_CHECKBUTTON:
    case RIG_CONF_COMBO:
        val->i = from_bcd_be(resbuf + cmdhead, res_len * 2);
        break;

    case RIG_CONF_NUMERIC:
        val->f = from_bcd_be(resbuf + cmdhead, res_len * 2);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), "
                  "len=%d\n", __func__, resbuf[0], res_len);
        return -RIG_EPROTO;

    }

    rig_debug(RIG_DEBUG_TRACE, "%s: %d %d %d %f\n",
              __func__, res_len, icom_val, val->i, val->f);

    return RIG_OK;
}



/*
 * IC-756proIII rig capabilities.
 *
 * TODO: check every parameter, add antenna capabilities
 */
static const struct icom_priv_caps ic756pro3_priv_caps =
{
    0x6e,    /* default address */
    0,       /* 731 mode */
    0,       /* no XCHG */
    ic756pro_ts_sc_list,
    .antack_len = 3,
    .ant_count = 2,
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = RIG_AGC_LAST, .icom_level = -1 },
    },
    .extcmds = ic756pro_cmdparms,   /* Custom op parameters */
};


#define IC756PROIII_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756PROIII_1HZ_TS_MODES IC756PROIII_ALL_RX_MODES
#define IC756PROIII_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM)
#define IC756PROIII_AM_TX_MODES (RIG_MODE_AM)

#define IC756PROIII_LEVEL_ALL (IC756PROII_LEVEL_ALL|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|RIG_LEVEL_COMP_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_NB)

/*
 * TODO: check whether all func and levels are stored in memory
 *
 * No memory split support.
 * .rptr_offs = 1,  to be done for FM special mode
 *
 * Channel description should be possible through: 0x1a 0x00
 * c.f. http://www.plicht.de/ekki/civ/civ-p4305-756pro.html
 */
#define IC756PROIII_MEM_CAP {    \
        .ant = 1,  \
        .tuning_step = 1,  \
        .freq = 1,  \
        .mode = 1,  \
        .width = 1, \
        .ctcss_tone = 1, \
        .ctcss_sql = 1, \
        .funcs = IC756PRO_FUNC_ALL, \
        .levels = RIG_LEVEL_SET(IC756PRO_LEVEL_ALL), \
}

#define IC756PROIII_SWR_CAL { 5, \
    { \
         { 0, 1.0f }, \
         { 48, 1.5f }, \
         { 80, 2.0f }, \
         { 120, 3.0f }, \
         { 240, 6.0f } \
    } }

#define IC756PROIII_ALC_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 120, 1.0f } \
    } }

#define IC756PROIII_RFPOWER_METER_CAL { 13, \
    { \
         { 0, 0.0f }, \
         { 21, 5.0f }, \
         { 43, 10.0f }, \
         { 65, 15.0f }, \
         { 83, 20.0f }, \
         { 95, 25.0f }, \
         { 105, 30.0f }, \
         { 114, 35.0f }, \
         { 124, 40.0f }, \
         { 143, 50.0f }, \
         { 183, 75.0f }, \
         { 213, 100.0f }, \
         { 255, 120.0f } \
    } }


#define IC756PROIII_COMP_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 130, 15.0f }, \
         { 241, 30.0f } \
    } }

const struct rig_caps ic756pro3_caps =
{
    RIG_MODEL(RIG_MODEL_IC756PROIII),
    .model_name = "IC-756PROIII",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".3",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
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
    .has_get_func =  IC756PRO_FUNC_ALL | RIG_FUNC_TUNER,
    .has_set_func =  IC756PRO_FUNC_SET | RIG_FUNC_TUNER,
    .has_get_level =  IC756PROIII_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(IC756PROIII_LEVEL_ALL),
    .has_get_parm =  IC756PROII_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC756PROII_PARMS),
    .level_gran =
    {
#include "level_gran_icom.h"
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = { .min = { .i = 6 }, .max = { .i = 48 }, .step = { .i = 1 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 1 } },
    },
    .parm_gran =  {},
    .extparms =  ic756pro2_ext_parms,
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 10, 20, RIG_DBLST_END, }, /* FIXME: TBC values */
    .attenuator =   { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0),
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo =  0,
    .vfo_ops =  IC756_VFO_OPS | RIG_OP_TUNE,
    .scan_ops =  IC756_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  10, /* TODO */

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM,  IC756PROIII_MEM_CAP },
        { 100, 101, RIG_MTYPE_EDGE, IC756PROIII_MEM_CAP },    /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(60), IC756PROIII_ALL_RX_MODES, -1, -1, IC756_VFO_ALL, IC756_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC756PROIII_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_6m(1, IC756PROIII_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_HF(1, IC756PROIII_AM_TX_MODES, W(5), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        FRQ_RNG_6m(1, IC756PROIII_AM_TX_MODES, W(5), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(60), IC756PROIII_ALL_RX_MODES, -1, -1, IC756_VFO_ALL, IC756_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC756PROIII_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_6m(2, IC756PROIII_OTHER_TX_MODES, W(5), W(100), IC756_VFO_ALL, IC756_ANTS),
        FRQ_RNG_HF(2, IC756PROIII_AM_TX_MODES, W(5), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        FRQ_RNG_6m(2, IC756PROIII_AM_TX_MODES, W(5), W(40), IC756_VFO_ALL, IC756_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC756PROIII_1HZ_TS_MODES, 1},
        {IC756PROIII_1HZ_TS_MODES, 100},
        {IC756PROIII_ALL_RX_MODES, kHz(1)},
        {IC756PROIII_ALL_RX_MODES, kHz(5)},
        {IC756PROIII_ALL_RX_MODES, kHz(9)},
        {IC756PROIII_ALL_RX_MODES, kHz(10)},
        {IC756PROIII_ALL_RX_MODES, kHz(12.5)},
        {IC756PROIII_ALL_RX_MODES, kHz(20)},
        {IC756PROIII_ALL_RX_MODES, kHz(25)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(3.6)}, /* wide */
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(50)}, /* narrow */
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(8)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR, RIG_FLT_ANY}, /* almost any filter */
        RIG_FLT_END,
    },
    .str_cal = IC756PRO_STR_CAL,
    .swr_cal = IC756PROIII_SWR_CAL,
    .alc_cal = IC756PROIII_ALC_CAL,
    .rfpower_meter_cal = IC756PROIII_RFPOWER_METER_CAL,
    .comp_meter_cal = IC756PROIII_COMP_METER_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic756pro3_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
    .set_vfo =  icom_set_vfo,
    .get_vfo =  icom_get_vfo,
    .set_ant =  icom_set_ant,
    .get_ant =  icom_get_ant,

    .decode_event =  icom_decode_event,
    .set_parm =  icom_set_parm,
    .get_parm =  icom_get_parm,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .set_ctcss_tone =  icom_set_ctcss_tone,
    .get_ctcss_tone =  icom_get_ctcss_tone,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_freq_mode =  icom_set_split_freq_mode,
    .get_split_freq_mode =  icom_get_split_freq_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  NULL,

    .set_ext_parm =  ic756pro2_set_ext_parm,
    .get_ext_parm =  ic756pro2_get_ext_parm,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
