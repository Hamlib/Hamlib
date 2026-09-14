/*
 *  Hamlib CI-V backend - description of IC-705
 *  Adapted by J.Watson from IC-7000 code (c) 2004 by Stephane Fillod
 *  Adapted from IC-7200 (c) 2016 by Michael Black W9MDB
 *  Split into its own file, and network model added,
 *  (c) 2026 by Mikael Nousiainen OH3BHX
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

#include <stdlib.h>

#include "hamlib/rig.h"
#include "idx_builtin.h"

#include "token.h"
#include "frame.h"
#include "icom.h"
#include "icom_defs.h"
#include "misc.h"
#include "cache.h"
#include "bandplan.h"
#include "tones.h"

/* Values below are duplicated from the IC-7300/IC-705 definitions this model
 * was originally derived from. They are kept per-model so each radio's caps
 * can diverge without disturbing the others. */
#define IC705_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_SCOPE|RIG_FUNC_TUNER|RIG_FUNC_TRANSCEIVE|RIG_FUNC_SPECTRUM|RIG_FUNC_SPECTRUM_HOLD|RIG_FUNC_SEND_MORSE|RIG_FUNC_SEND_VOICE_MEM|RIG_FUNC_OVF_STATUS)
#define IC705_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define IC705_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define IC705_SCAN_OPS (RIG_SCAN_STOP|RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_SLCT|RIG_SCAN_VFO)

#define IC705_STR_CAL { 7, \
    { \
        {   0, -54 }, \
        {  10, -48 }, \
        {  30, -36 }, \
        {  60, -24 }, \
        {  90, -12 }, \
        { 120,  0 }, \
        { 241,  64 } \
    } }

#define IC705_SWR_CAL { 5, \
    { \
         { 0, 1.0f }, \
         { 48, 1.5f }, \
         { 80, 2.0f }, \
         { 120, 3.0f }, \
         { 240, 6.0f } \
    } }

#define IC705_ALC_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 120, 1.0f } \
    } }

/*
 * IC705 items that differ from IC7300
 */
#define IC705_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_SCREENSAVER|RIG_PARM_TIME|RIG_PARM_BEEP|RIG_PARM_KEYERTYPE|RIG_PARM_AFIF|RIG_PARM_AFIF_WLAN)
#define IC705_ALL_TX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM|RIG_MODE_DSTAR)
#define IC705_ALL_RX_MODES (IC705_ALL_TX_MODES|RIG_MODE_WFM)
#define IC705_OTHER_TX_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_DSTAR)
#define IC705_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_SPECTRUM_MODE|RIG_LEVEL_SPECTRUM_SPAN|RIG_LEVEL_SPECTRUM_SPEED|RIG_LEVEL_SPECTRUM_REF|RIG_LEVEL_SPECTRUM_AVG|RIG_LEVEL_SPECTRUM_EDGE_LOW|RIG_LEVEL_SPECTRUM_EDGE_HIGH)

#define IC705_RFPOWER_METER_CAL { 13, \
    { \
         { 0, 0.0f }, \
         { 21, 0.50f }, \
         { 43, 1.00f }, \
         { 65, 1.50f }, \
         { 83, 2.00f }, \
         { 95, 2.50f }, \
         { 105, 3.00f }, \
         { 114, 3.50f }, \
         { 124, 4.00f }, \
         { 143, 5.00f }, \
         { 183, 7.50f }, \
         { 213, 10.0f }, \
         { 255, 12.0f } \
    } }

// flrig-2.0.05.93     
#define IC705_COMP_METER_CAL { 12, \
    { \
         { 0, 0.0f }, \
         { 11, 0.0f }, \
         { 34, 3.0f }, \
         { 58, 6.0f }, \
         { 81, 9.0f }, \
         { 104, 12.0f }, \
         { 128, 15.0f }, \
         { 151, 18.0f }, \
         { 174, 21.0f }, \
         { 197, 24.0f }, \
         { 221, 27.0f }, \
         { 244, 30.0f } \
    } }

// flrig-2.0.05.93     
#define IC705_VD_METER_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 241, 16.0f } \
    } }


// flrig-2.0.05.93     
#define IC705_ID_METER_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 241, 4.0f } \
    } }

static const struct ts_sc_list ic705_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {500, 0x02},
    {kHz(1), 0x03},
    {kHz(5), 0x04},
    {kHz(6.25), 0x05},
    {kHz(8.33), 0x06},
    {kHz(9), 0x07},
    {kHz(10), 0x08},
    {kHz(12.5), 0x09},
    {kHz(20), 0x10},
    {kHz(25), 0x11},
    {kHz(50), 0x12},
    {kHz(100), 0x13},
    {0, 0},
};
static const struct cmdparams ic705_extcmds[] =
{
    { {.s = RIG_PARM_ANN}, CMD_PARAM_TYPE_PARM, C_CTL_ANN, 0, SC_MOD_WR, 0, {0x00}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_BEEP}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x31}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x36}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_SCREENSAVER}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x38}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_TIME}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x66}, CMD_DAT_TIM, 2 },
    { {.s = RIG_PARM_AFIF}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x09}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_AFIF_WLAN}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x14}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_VOXDELAY}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x03, 0x59}, CMD_DAT_INT, 1 },
    { {.s = RIG_FUNC_TRANSCEIVE}, CMD_PARAM_TYPE_FUNC, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x31}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_SPECTRUM_AVG}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x78}, CMD_DAT_INT, 1 },
    { {.s = RIG_LEVEL_USB_AF}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x10}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_KEYERTYPE}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x02, 0x55}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_NONE} }
};
static int ic705_ext_tokens[] =
{
    TOK_SCOPE_STX, TOK_SCOPE_CFQ, TOK_SCOPE_EDG, TOK_SCOPE_VBW, TOK_BACKEND_NONE,
};
static const struct icom_clock_cmds ic705_clock_cmds = {
  .date_cmds = { 0x01, 0x65 }, .time_cmds = { 0x01, 0x66 }, .offset_cmds = { 0x01, 0x69 }
};

static const struct icom_priv_caps IC705_priv_caps =
{
    0xA4,   /* default address */
    0,      /* 731 mode */
    1,      /* no XCHG to avoid display flickering */
    ic705_ts_sc_list,
    .serial_USB_echo_check = 1,  /* USB CI-V may not echo */
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_OFF, .icom_level = 0 },
        { .level = RIG_AGC_FAST, .icom_level = 1 },
        { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
        { .level = RIG_AGC_SLOW, .icom_level = 3 },
        { .level = RIG_AGC_LAST, .icom_level = -1 },
    },
    .spectrum_scope_caps = {
        .spectrum_line_length = 475,
        .single_frame_data_length = 50,
        .data_level_min = 0,
        .data_level_max = 160,
        .signal_strength_min = -80, // TODO: signal strength to be confirmed
        .signal_strength_max = 0,
    },
    .spectrum_edge_frequency_ranges = {
        {
            .range_id = 1,
            .low_freq = 30000,
            .high_freq = 1600000,
        },
        {
            .range_id = 2,
            .low_freq = 1600000,
            .high_freq = 2000000,
        },
        {
            .range_id = 3,
            .low_freq = 2000000,
            .high_freq = 6000000,
        },
        {
            .range_id = 4,
            .low_freq = 6000000,
            .high_freq = 8000000,
        },
        {
            .range_id = 5,
            .low_freq = 8000000,
            .high_freq = 11000000,
        },
        {
            .range_id = 6,
            .low_freq = 11000000,
            .high_freq = 15000000,
        },
        {
            .range_id = 7,
            .low_freq = 15000000,
            .high_freq = 20000000,
        },
        {
            .range_id = 8,
            .low_freq = 20000000,
            .high_freq = 22000000,
        },
        {
            .range_id = 9,
            .low_freq = 22000000,
            .high_freq = 26000000,
        },
        {
            .range_id = 10,
            .low_freq = 26000000,
            .high_freq = 30000000,
        },
        {
            .range_id = 11,
            .low_freq = 30000000,
            .high_freq = 45000000,
        },
        {
            .range_id = 12,
            .low_freq = 45000000,
            .high_freq = 60000000,
        },
        {
            .range_id = 13,
            .low_freq = 60000000,
            .high_freq = 74800000,
        },
        {
            .range_id = 13,
            .low_freq = 60000000,
            .high_freq = 74800000,
        },
        {
            .range_id = 14,
            .low_freq = 74800000,
            .high_freq = 108000000,
        },
        {
            .range_id = 15,
            .low_freq = 108000000,
            .high_freq = 137000000,
        },
        {
            .range_id = 16,
            .low_freq = 137000000,
            .high_freq = 200000000,
        },
        {
            .range_id = 17,
            .low_freq = 400000000,
            .high_freq = 470000000,
        },
        {
            .range_id = 0,
            .low_freq = 0,
            .high_freq = 0,
        },
    },
    .extcmds = ic705_extcmds,     /* Custom parameters */
    .x25x26_always = 1,
    .x25x26_possibly = 1,
    .x1cx03_always = 1,
    .x1cx03_possibly = 1,
    .x1ax03_supported = 1,
    .mode_with_filter = 1,
    .data_mode_supported = 1,
    .fm_filters = { 7000, 10000, 15000 },
    .clock_cmds = &ic705_clock_cmds,
    .power_status_read_not_supported = 1
};

struct rig_caps ic705_caps =
{
    RIG_MODEL(RIG_MODEL_IC705),
    .model_name = "IC-705",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".11",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
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
    .retry =  3,
    .has_get_func =  IC705_FUNCS,
    .has_set_func =  IC705_FUNCS,
    .has_get_level =  IC705_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(IC705_LEVELS),
    .has_get_parm =  IC705_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC705_PARMS),
    .level_gran = {
#define NO_LVL_KEYSPD
#define NO_LVL_CWPITCH
#define NO_LVL_USB_AF
#include "level_gran_icom.h"
#undef NO_LVL_KEYSPD
#undef NO_LVL_CWPITCH
#undef NO_LVL_USB_AF
        [LVL_KEYSPD] = {.min = {.i = 6}, .max = {.i = 48}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 300}, .max = {.i = 900}, .step = {.i = 1}},
        [LVL_SPECTRUM_SPEED] = {.min = {.i = 0}, .max = {.i = 2}, .step = {.i = 1}},
        [LVL_SPECTRUM_REF] = {.min = {.f = -20.0f}, .max = {.f = 20.0f}, .step = {.f = 0.5f}},
        [LVL_SPECTRUM_AVG] = {.min = {.i = 0}, .max = {.i = 3}, .step = {.i = 1}},
        [LVL_USB_AF] = {.min = {.f = 0.0f}, .max = {.f = 1.0f}, .step = {.f = 1.0f / 255.0f }},
    },
    .parm_gran =  {
        [PARM_BACKLIGHT] = {.min = {.f = 0.0f}, .max = {.f = 1.0f}, .step = {.f = 1.0f / 255.0f}},
        [PARM_BANDSELECT] = {.step = {.s = "BANDUNUSED,BAND160M,BAND80M,BAND40M,BAND30M,BAND20M,BAND17M,BAND15M,BAND12M,BAND10M,BAND6M,BANDWFM,BANDAIR,BAND70CM,BAND33CM,BANDGEN"}},
        [PARM_BEEP] = {.min = {.i = 0}, .max = {.i = 1}},
        [PARM_SCREENSAVER] = {.min = {.i = 0}, .max = {.i = 3}, .step = {.i = 1}},
    },
    .ext_tokens = ic705_ext_tokens,
    .extlevels = icom_ext_levels,
    .ctcss_list =  full_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 1, 2, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0),
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .vfo_ops =  IC705_VFO_OPS,
    .scan_ops =  IC705_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   5,
    .chan_desc_sz =  0,
    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        {   1,   8, RIG_MTYPE_VOICE },
        {   1,   8, RIG_MTYPE_MORSE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(30), MHz(199.999999), IC705_ALL_RX_MODES, -1, -1, IC705_VFOS, RIG_ANT_1, "USA"},
        {MHz(400), MHz(470), IC705_ALL_RX_MODES, -1, -1, IC705_VFOS, RIG_ANT_1, "USA"},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        { kHz(1800), MHz(1.999999), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(3.5), MHz(3.999999), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(5.255), MHz(5.405), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(7.0), MHz(7.3), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(10.1), MHz(10.15), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(14.0), MHz(14.35), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(18.068), MHz(18.168), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(21.00), MHz(21.45), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(24.89), MHz(24.99), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(28.00), MHz(29.70), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(50.00), MHz(54.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(144.00), MHz(148.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        { MHz(430.00), MHz(450.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "USA" },
        RIG_FRNG_END,
    },

    .rx_range_list2 =   {
        {kHz(30), MHz(199.999999), IC705_ALL_RX_MODES, -1, -1, IC705_VFOS, RIG_ANT_1, "EUR"},
        {MHz(400), MHz(470), IC705_ALL_RX_MODES, -1, -1, IC705_VFOS, RIG_ANT_1, "EUR"},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   {
        { kHz(1810), MHz(1.999999), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(3.5), MHz(3.8), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(7.0), MHz(7.2), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(10.1), MHz(10.15), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(14.0), MHz(14.35), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(18.068), MHz(18.168), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(21.00), MHz(21.45), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(24.89), MHz(24.99), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(28.00), MHz(29.70), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(50.00), MHz(52.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(144.00), MHz(146.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        { MHz(430.00), MHz(440.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC705_VFOS, RIG_ANT_1, "EUR" },
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {IC705_ALL_RX_MODES, Hz(100)},
        {IC705_ALL_RX_MODES, kHz(.5)},
        {IC705_ALL_RX_MODES, kHz(1)},
        {IC705_ALL_RX_MODES, kHz(5)},
        {IC705_ALL_RX_MODES, kHz(6.25)},
        {IC705_ALL_RX_MODES, kHz(8.33)},
        {IC705_ALL_RX_MODES, kHz(9)},
        {IC705_ALL_RX_MODES, kHz(10)},
        {IC705_ALL_RX_MODES, kHz(12.5)},
        {IC705_ALL_RX_MODES, kHz(20)},
        {IC705_ALL_RX_MODES, kHz(25)},
        {IC705_ALL_RX_MODES, kHz(50)},
        {IC705_ALL_RX_MODES, kHz(100)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! But duplication may speed up search.  Put the most commonly used modes first!  Remember these are defaults, with dsp rigs you can change them to anything you want except FM and WFM which are fixed */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(1.8)},
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(3.0)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(250)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1.2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(3)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(9)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(10)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(7)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(15)},
        {RIG_MODE_WFM, kHz(200)},
        RIG_FLT_END,
    },

    .str_cal = IC705_STR_CAL,
    .swr_cal = IC705_SWR_CAL,
    .alc_cal = IC705_ALC_CAL,
    .rfpower_meter_cal = IC705_RFPOWER_METER_CAL,
    .comp_meter_cal = IC705_COMP_METER_CAL,
    .vd_meter_cal = IC705_VD_METER_CAL,
    .id_meter_cal = IC705_ID_METER_CAL,

    .spectrum_scopes = {
        {
            .id = 0,
            .name = "Main",
        },
        {
            .id = -1,
            .name = NULL,
        },
    },
    .spectrum_modes = {
        RIG_SPECTRUM_MODE_CENTER,
        RIG_SPECTRUM_MODE_FIXED,
        RIG_SPECTRUM_MODE_CENTER_SCROLL,
        RIG_SPECTRUM_MODE_FIXED_SCROLL,
        RIG_SPECTRUM_MODE_NONE,
    },
    .spectrum_spans = {
        5000,
        10000,
        20000,
        50000,
        100000,
        200000,
        500000,
        1000000,
        0,
    },
    .spectrum_avg_modes = {
        {
            .id = 0,
            .name = "OFF",
        },
        {
            .id = 1,
            .name = "2",
        },
        {
            .id = 2,
            .name = "3",
        },
        {
            .id = 3,
            .name = "4",
        },
    },

    .async_data_supported = 1,
    .read_frame_direct = icom_read_frame_direct,
    .is_async_frame = icom_is_async_frame,
    .process_async_frame = icom_process_async_frame,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& IC705_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
//    .get_vfo =  icom_get_vfo,
    .set_vfo =  icom_set_vfo,
    .set_ant =  NULL,
    .get_ant =  NULL,

    .set_rit =  icom_set_rit_new,
    .get_rit =  icom_get_rit_new,
    .get_xit =  icom_get_rit_new,
    .set_xit =  icom_set_xit_new,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_ext_level =  icom_set_ext_level,
    .get_ext_level =  icom_get_ext_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_parm =  icom_set_parm,
    .get_parm =  icom_get_parm,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_ptt =  icom_get_ptt,
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
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  icom_get_split_vfo,
    .set_powerstat = icom_set_powerstat,
    .get_powerstat = icom_get_powerstat,
    .power2mW = icom_power2mW,
    .mW2power = icom_mW2power,
    .send_morse = icom_send_morse,
    .stop_morse = icom_stop_morse,
    .wait_morse = rig_wait_morse,
    .send_voice_mem = icom_send_voice_mem,
    .stop_voice_mem = icom_stop_voice_mem,
    .set_clock = icom_set_clock,
    .get_clock = icom_get_clock,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
