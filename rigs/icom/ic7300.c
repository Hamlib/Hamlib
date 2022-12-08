/*
 *  Hamlib CI-V backend - description of IC-7300 and variations
 *  Adapted by J.Watson from IC-7000 code (c) 2004 by Stephane Fillod
 *  Adapted from IC-7200 (c) 2016 by Michael Black W9MDB
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
#include "idx_builtin.h"

#include "token.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "bandplan.h"
#include "tones.h"
#include "misc.h"
#include "ic7300.h"

static int ic7300_set_parm(RIG *rig, setting_t parm, value_t val);
static int ic7300_get_parm(RIG *rig, setting_t parm, value_t *val);
int ic7300_set_clock(RIG *rig, int year, int month, int day, int hour,
                     int min, int sec, double msec, int utc_offset);
int ic7300_get_clock(RIG *rig, int *year, int *month, int *day,
                     int *hour,
                     int *min, int *sec, double *msec, int *utc_offset);


#define IC7300_ALL_RX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM)
#define IC7300_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM)
#define IC7300_NOT_TS_MODES (IC7300_ALL_RX_MODES &~IC7300_1HZ_TS_MODES)

#define IC7300_OTHER_TX_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define IC7300_AM_TX_MODES (RIG_MODE_AM|RIG_MODE_PKTAM)

#define IC7300_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_SCOPE|RIG_FUNC_TUNER|RIG_FUNC_TRANSCEIVE|RIG_FUNC_SPECTRUM|RIG_FUNC_SPECTRUM_HOLD|RIG_FUNC_SEND_MORSE|RIG_FUNC_SEND_VOICE_MEM|RIG_FUNC_OVF_STATUS)

#define IC7300_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_SPECTRUM_MODE|RIG_LEVEL_SPECTRUM_SPAN|RIG_LEVEL_SPECTRUM_SPEED|RIG_LEVEL_SPECTRUM_REF|RIG_LEVEL_SPECTRUM_AVG|RIG_LEVEL_SPECTRUM_EDGE_LOW|RIG_LEVEL_SPECTRUM_EDGE_HIGH|RIG_LEVEL_USB_AF|RIG_LEVEL_AGC_TIME)

#define IC7300_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define IC7300_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_TIME|RIG_PARM_BEEP)

#define IC7300_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define IC7300_SCAN_OPS (RIG_SCAN_STOP|RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_SLCT)

#define IC7300_ANTS (RIG_ANT_1) /* ant-1 is Hf-6m */

/*
 * IC-7300 S-meter levels measured from live signals on multiple bands. Provides a good approximation.
 */
#define IC7300_STR_CAL { 7, \
    { \
        {   0, -54 }, \
        {  10, -48 }, \
        {  30, -36 }, \
        {  60, -24 }, \
        {  90, -12 }, \
        { 120,  0 }, \
        { 241,  64 } \
    } }

#define IC7300_SWR_CAL { 5, \
    { \
         { 0, 1.0f }, \
         { 48, 1.5f }, \
         { 80, 2.0f }, \
         { 120, 3.0f }, \
         { 240, 6.0f } \
    } }

#define IC7300_ALC_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 120, 1.0f } \
    } }

#define IC7300_RFPOWER_METER_CAL { 13, \
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

#define IC7300_COMP_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 130, 15.0f }, \
         { 241, 30.0f } \
    } }

#define IC7300_VD_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 13, 10.0f }, \
         { 241, 16.0f } \
    } }

#define IC7300_ID_METER_CAL { 4, \
    { \
         { 0, 0.0f }, \
         { 97, 10.0f }, \
         { 146, 15.0f }, \
         { 241, 25.0f } \
    } }


/*
 * IC705 items that differ from IC7300
 */
#define IC705_ALL_TX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_DSTAR)
#define IC705_ALL_RX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM|RIG_MODE_DSTAR)
#define IC705_OTHER_TX_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_DSTAR)
#define IC705_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_SPECTRUM_MODE|RIG_LEVEL_SPECTRUM_SPAN|RIG_LEVEL_SPECTRUM_SPEED|RIG_LEVEL_SPECTRUM_REF|RIG_LEVEL_SPECTRUM_AVG|RIG_LEVEL_SPECTRUM_EDGE_LOW|RIG_LEVEL_SPECTRUM_EDGE_HIGH)

/*
 * IC9700 items that differ from IC7300
 */
#define IC9700_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MEM|RIG_VFO_MAIN_A|RIG_VFO_MAIN_B|RIG_VFO_SUB_A|RIG_VFO_SUB_B)
#define IC9700_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_TIME|RIG_PARM_BEEP|RIG_PARM_SCREENSAVER)
#define IC9700_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_SCOPE|RIG_FUNC_SATMODE|RIG_FUNC_DUAL_WATCH|RIG_FUNC_AFC|RIG_FUNC_TRANSCEIVE|RIG_FUNC_SPECTRUM|RIG_FUNC_SPECTRUM_HOLD|RIG_FUNC_SEND_MORSE|RIG_FUNC_SEND_VOICE_MEM|RIG_FUNC_OVF_STATUS)
#define IC9700_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|RIG_LEVEL_COMP_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_SPECTRUM_MODE|RIG_LEVEL_SPECTRUM_SPAN|RIG_LEVEL_SPECTRUM_SPEED|RIG_LEVEL_SPECTRUM_REF|RIG_LEVEL_SPECTRUM_AVG|RIG_LEVEL_SPECTRUM_EDGE_LOW|RIG_LEVEL_SPECTRUM_EDGE_HIGH|RIG_LEVEL_USB_AF)
#define IC9700_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC9700_SCAN_OPS (RIG_SCAN_STOP|RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_SLCT)
#define IC9700_ALL_TX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_DSTAR|RIG_MODE_DD)
#define IC9700_ALL_RX_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_DSTAR|RIG_MODE_DD)

#define IC9700_STR_CAL { 7, \
    { \
        {   0, -54 }, \
        {  10, -48 }, \
        {  30, -36 }, \
        {  60, -24 }, \
        {  90, -12 }, \
        { 120,  0 }, \
        { 241,  64 } \
    } }

#define IC9700_SWR_CAL { 5, \
    { \
         { 0, 1.0f }, \
         { 48, 1.5f }, \
         { 80, 2.0f }, \
         { 120, 3.0f }, \
         { 240, 6.0f } \
    } }

#define IC9700_ALC_CAL { 2, \
    { \
         { 0, 0.0f }, \
         { 120, 1.0f } \
    } }

#define IC9700_RFPOWER_METER_CAL { 13, \
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

#define IC9700_COMP_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 130, 15.0f }, \
         { 210, 25.5f } \
    } }

#define IC9700_VD_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 13, 10.0f }, \
         { 241, 16.0f } \
    } }

#define IC9700_ID_METER_CAL { 3, \
    { \
         { 0, 0.0f }, \
         { 121, 10.0f }, \
         { 241, 20.0f } \
    } }

struct cmdparams ic7300_extcmds[] =
{
    { {.s = RIG_PARM_BEEP}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x23}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x81}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_TIME}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x95}, CMD_DAT_TIM, 2 },
    { {.s = RIG_PARM_AFIF}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x59}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_VOXDELAY}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x59}, CMD_DAT_INT, 1 },
    { {.s = RIG_FUNC_TRANSCEIVE}, CMD_PARAM_TYPE_FUNC, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x71}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_SPECTRUM_AVG}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x02}, CMD_DAT_INT, 1 },
    { {.s = RIG_LEVEL_USB_AF}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x60}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_NONE} }
};

struct cmdparams ic9700_extcmds[] =
{
    { {.s = RIG_PARM_BEEP}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x29}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x52}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_SCREENSAVER}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x67}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_TIME}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x80}, CMD_DAT_TIM, 2 },
    { {.s = RIG_PARM_AFIF}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x00}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_VOXDELAY}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x03, 0x30}, CMD_DAT_INT, 1 },
    { {.s = RIG_FUNC_TRANSCEIVE}, CMD_PARAM_TYPE_FUNC, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x27}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_SPECTRUM_AVG}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x92}, CMD_DAT_INT, 1 },
    { {.s = RIG_LEVEL_USB_AF}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x06}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_NONE} }
};

struct cmdparams ic705_extcmds[] =
{
    { {.s = RIG_PARM_BEEP}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x00, 0x31}, CMD_DAT_BOL, 1 },
    { {.s = RIG_PARM_BACKLIGHT}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x36}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_SCREENSAVER}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x38}, CMD_DAT_INT, 1 },
    { {.s = RIG_PARM_TIME}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x66}, CMD_DAT_TIM, 2 },
    { {.s = RIG_PARM_AFIF}, CMD_PARAM_TYPE_PARM, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x09}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_VOXDELAY}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x03, 0x59}, CMD_DAT_INT, 1 },
    { {.s = RIG_FUNC_TRANSCEIVE}, CMD_PARAM_TYPE_FUNC, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x31}, CMD_DAT_BOL, 1 },
    { {.s = RIG_LEVEL_SPECTRUM_AVG}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x78}, CMD_DAT_INT, 1 },
    { {.s = RIG_LEVEL_USB_AF}, CMD_PARAM_TYPE_LEVEL, C_CTL_MEM, S_MEM_PARM, SC_MOD_RW, 2, {0x01, 0x13}, CMD_DAT_LVL, 2 },
    { {.s = RIG_PARM_NONE} }
};

int ic7300_ext_tokens[] =
{
    TOK_SCOPE_STX, TOK_SCOPE_CFQ, TOK_SCOPE_EDG, TOK_SCOPE_VBW, TOK_BACKEND_NONE,
};

int ic9700_ext_tokens[] =
{
    TOK_SCOPE_MSS, TOK_SCOPE_SDS, TOK_SCOPE_STX, TOK_SCOPE_CFQ, TOK_SCOPE_EDG, TOK_SCOPE_VBW, TOK_SCOPE_MKP, TOK_BACKEND_NONE,
};

int ic705_ext_tokens[] =
{
    TOK_SCOPE_STX, TOK_SCOPE_CFQ, TOK_SCOPE_EDG, TOK_SCOPE_VBW, TOK_BACKEND_NONE,
};

/*
 * IC-7300 rig capabilities.
 */
static const struct icom_priv_caps IC7300_priv_caps =
{
    0x94,   /* default address */
    0,      /* 731 mode */
    1,      /* no XCHG to avoid display flickering */
    ic7300_ts_sc_list,
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_OFF, .icom_level = 0 }, // note this is handled by AGC time constant instead
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
        .signal_strength_min = -80,
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
            .range_id = 0,
            .low_freq = 0,
            .high_freq = 0,
        },
    },
    .extcmds = ic7300_extcmds,   /* Custom op parameters */
};

static const struct icom_priv_caps IC9700_priv_caps =
{
    0xA2,   /* default address */
    0,      /* 731 mode */
    1,      /* no XCHG to avoid display flickering */
    ic9700_ts_sc_list,
    .serial_USB_echo_check = 1,  /* USB CI-V may not echo */
    .agc_levels_present = 1,
    .agc_levels = {
        { .level = RIG_AGC_OFF, .icom_level = 0 }, // note this is handled by AGC time constant instead
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
            .low_freq = 144000000,
            .high_freq = 148000000,
        },
        {
            .range_id = 2,
            .low_freq = 430000000,
            .high_freq = 450000000,
        },
        {
            .range_id = 3,
            .low_freq = 1240000000,
            .high_freq = 1300000000,
        },
        {
            .range_id = 0,
            .low_freq = 0,
            .high_freq = 0,
        },
    },
    .extcmds = ic9700_extcmds,   /* Custom op parameters */
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
};

const struct rig_caps ic7300_caps =
{
    RIG_MODEL(RIG_MODEL_IC7300),
    .model_name = "IC-7300",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".10",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  IC7300_FUNCS,
    .has_set_func =  IC7300_FUNCS,
    .has_get_level =  IC7300_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(IC7300_LEVELS),
    .has_get_parm =  IC7300_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC7300_PARMS),
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = {.min = {.i = 0}, .max = {.i = 255}},
        [LVL_VOXDELAY] = {.min = {.i = 0}, .max = {.i = 20}, .step = {.i = 1}},
        [LVL_KEYSPD] = {.min = {.i = 6}, .max = {.i = 48}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 300}, .max = {.i = 900}, .step = {.i = 1}},
        [LVL_SPECTRUM_SPEED] = {.min = {.i = 0}, .max = {.i = 2}, .step = {.i = 1}},
        [LVL_SPECTRUM_REF] = {.min = {.f = -20.0f}, .max = {.f = 20.0f}, .step = {.f = 0.5f}},
        [LVL_SPECTRUM_AVG] = {.min = {.i = 0}, .max = {.i = 3}, .step = {.i = 1}},
        [LVL_USB_AF] = {.min = {.f = 0.0f}, .max = {.f = 1.0f}, .step = {.f = 1.0f / 255.0f }},
        [LVL_AGC_TIME] = {.min = {.f = 0.0f}, .max = {.f = 8.0f}, .step = {.f = 0.1f }},
    },
    .parm_gran =  {},
    .ext_tokens = ic7300_ext_tokens,
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
    .vfo_ops =  IC7300_VFO_OPS,
    .scan_ops =  IC7300_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   1,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(30), MHz(74.8), IC7300_ALL_RX_MODES, -1, -1, IC7300_VFOS}, RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_60m(1, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_6m(1, IC7300_OTHER_TX_MODES, W(2), W(100), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_4m(1, IC7300_OTHER_TX_MODES, W(2), W(50), IC7300_VFOS, RIG_ANT_1),
        FRQ_RNG_HF(1, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_60m(1, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_6m(1, IC7300_AM_TX_MODES, W(1), W(25), IC7300_VFOS, RIG_ANT_1), /* AM class */
        FRQ_RNG_4m(1, IC7300_AM_TX_MODES, W(1), W(12.5), IC7300_VFOS, RIG_ANT_1), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {IC7300_ALL_RX_MODES, Hz(1)},
        {IC7300_ALL_RX_MODES, kHz(1)},
        {IC7300_ALL_RX_MODES, kHz(5)},
        {IC7300_ALL_RX_MODES, kHz(9)},
        {IC7300_ALL_RX_MODES, kHz(10)},
        {IC7300_ALL_RX_MODES, kHz(12.5)},
        {IC7300_ALL_RX_MODES, kHz(20)},
        {IC7300_ALL_RX_MODES, kHz(25)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! But duplication may speed up search.  Put the most commonly used modes first!  Remember these are defaults, with dsp rigs you can change them to anything you want except FM and WFM which are fixed */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(1.8)},
        {RIG_MODE_SSB | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(3)},
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
        RIG_FLT_END,
    },

    .str_cal = IC7300_STR_CAL,
    .swr_cal = IC7300_SWR_CAL,
    .alc_cal = IC7300_ALC_CAL,
    .rfpower_meter_cal = IC7300_RFPOWER_METER_CAL,
    .comp_meter_cal = IC7300_COMP_METER_CAL,
    .vd_meter_cal = IC7300_VD_METER_CAL,
    .id_meter_cal = IC7300_ID_METER_CAL,

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

    .priv = (void *)& IC7300_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
    .get_vfo =  icom_get_vfo,
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
    .set_parm =  ic7300_set_parm,
    .get_parm =  ic7300_get_parm,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_ptt =  icom_get_ptt,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .set_rptr_shift =  NULL,
    .get_rptr_shift =  NULL,
    .set_rptr_offs =  NULL,
    .get_rptr_offs =  NULL,
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
    .set_clock = ic7300_set_clock,
    .get_clock = ic7300_get_clock,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

struct rig_caps ic9700_caps =
{
    RIG_MODEL(RIG_MODEL_IC9700),
    .model_name = "IC-9700",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".11",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  38400,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  IC9700_FUNCS,
    .has_set_func =  IC9700_FUNCS,
    .has_get_level =  IC9700_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(IC9700_LEVELS),
    .has_get_parm =  IC9700_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC9700_PARMS),
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = {.min = {.i = 6}, .max = {.i = 48}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 300}, .max = {.i = 900}, .step = {.i = 1}},
        [LVL_SPECTRUM_SPEED] = {.min = {.i = 0}, .max = {.i = 2}, .step = {.i = 1}},
        [LVL_SPECTRUM_REF] = {.min = {.f = -20.0f}, .max = {.f = 20.0f}, .step = {.f = 0.5f}},
        [LVL_SPECTRUM_AVG] = {.min = {.i = 0}, .max = {.i = 3}, .step = {.i = 1}},
        [LVL_USB_AF] = {.min = {.f = 0.0f}, .max = {.f = 1.0f}, .step = {.f = 1.0f / 255.0f }},
        [LVL_AGC_TIME] = {.min = {.f = 0.0f}, .max = {.f = 8.0f}, .step = {.f = 0.1f }},
    },
    .parm_gran =  {},
    .ext_tokens = ic9700_ext_tokens,
    .extlevels = icom_ext_levels,
    .ctcss_list =  full_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { 1, 2, RIG_DBLST_END, },
    .attenuator =   { 10, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(9999),
    .max_ifshift =  Hz(0),
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW },
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE | RIG_TARGETABLE_SPECTRUM,
    .vfo_ops =  IC9700_VFO_OPS,
    .scan_ops =  IC9700_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   3,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        RIG_CHAN_END,
    },

    // Hopefully any future changes in bandplans can be fixed with firmware updates
    // So we use the global REGION2 macros in here
    .rx_range_list1 =   { // USA Version -- United States
        {MHz(144), MHz(148), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(430), MHz(450), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(1240), MHz(1300), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "USA"},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {MHz(144), MHz(148), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(430), MHz(450), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(1240), MHz(1300), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(144), MHz(148), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(430), MHz(450), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        {MHz(1240), MHz(1300), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "USA"},
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { // EUR Version -- Europe
        {MHz(144), MHz(146), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(430), MHz(440), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(1240), MHz(1300), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   {
        {MHz(144), MHz(146), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(430), MHz(440), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(1240), MHz(1300), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(144), MHz(146), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(430), MHz(440), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        {MHz(1240), MHz(1300), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "EUR"},
        RIG_FRNG_END,
    },

    .rx_range_list3 =   { // ITR Version -- Italy
        {MHz(144), MHz(146), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(430), MHz(434), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(435), MHz(438), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1240), MHz(1245), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1270), MHz(1298), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        RIG_FRNG_END,
    },
    .tx_range_list3 =  {
        {MHz(144), MHz(146), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(430), MHz(434), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(435), MHz(438), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1240), MHz(1245), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1270), MHz(1298), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(144), MHz(146), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(430), MHz(434), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(435), MHz(438), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1240), MHz(1245), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        {MHz(1270), MHz(1298), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "ITR"},
        RIG_FRNG_END,
    },

    .rx_range_list4 =   { // TPE Version -- Taiwan ??
        {MHz(144), MHz(146), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(430), MHz(432), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(1260), MHz(1265), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        RIG_FRNG_END,
    },
    .tx_range_list4 =  {
        {MHz(144), MHz(146), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(430), MHz(432), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(1260), MHz(1265), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(144), MHz(146), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(430), MHz(432), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        {MHz(1260), MHz(1265), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "TPE"},
        RIG_FRNG_END,
    },

    .rx_range_list5 =   { // KOR Version -- Republic of Korea
        {MHz(144), MHz(146), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(430), MHz(440), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(1260), MHz(1300), IC9700_ALL_RX_MODES, -1, -1, IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        RIG_FRNG_END,
    },
    .tx_range_list5 =  {
        {MHz(144), MHz(146), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(100), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(430), MHz(440), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.5), W(75), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(1260), MHz(1300), IC9700_ALL_TX_MODES ^ RIG_MODE_AM, W(0.1), W(10), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(144), MHz(146), RIG_MODE_AM, W(0.125), W(25), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(430), MHz(440), RIG_MODE_AM, W(0.125), W(18.75), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        {MHz(1260), MHz(1300), RIG_MODE_AM, W(0.025), W(2.5), IC9700_VFOS, RIG_ANT_CURR, "KOR"},
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {IC9700_ALL_RX_MODES, Hz(1)},
        {IC9700_ALL_RX_MODES, Hz(10)},
        {IC9700_ALL_RX_MODES, Hz(100)},
        {IC9700_ALL_RX_MODES, Hz(500)},
        {IC9700_ALL_RX_MODES, kHz(1)},
        {IC9700_ALL_RX_MODES, kHz(5)},
        {IC9700_ALL_RX_MODES, kHz(6.25)},
        {IC9700_ALL_RX_MODES, kHz(10)},
        {IC9700_ALL_RX_MODES, kHz(12.5)},
        {IC9700_ALL_RX_MODES, kHz(20)},
        {IC9700_ALL_RX_MODES, kHz(25)},
        {IC9700_ALL_RX_MODES, kHz(50)},
        {IC9700_ALL_RX_MODES, kHz(100)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! But duplication may speed up search.  Put the most commonly used modes first!  Remember these are defaults, with dsp rigs you can change them to anything you want except FM and WFM which are fixed */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(3)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(250)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1.2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(3)},
        {RIG_MODE_AM | RIG_MODE_PKTAM, kHz(9)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(15)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(7)},
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(10)},
        RIG_FLT_END,
    },

    .str_cal = IC9700_STR_CAL,
    .swr_cal = IC9700_SWR_CAL,
    .alc_cal = IC9700_ALC_CAL,
    .rfpower_meter_cal = IC9700_RFPOWER_METER_CAL,
    .comp_meter_cal = IC9700_COMP_METER_CAL,
    .vd_meter_cal = IC9700_VD_METER_CAL,
    .id_meter_cal = IC9700_ID_METER_CAL,

    .spectrum_scopes = {
        {
            .id = 0,
            .name = "Main",
        },
        {
            .id = 1,
            .name = "Sub",
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

    .priv = (void *)& IC9700_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
    .get_vfo =  icom_get_vfo,
    .set_vfo =  icom_set_vfo,
    .set_ant =  NULL,
    .get_ant =  NULL,

    .set_rit =  icom_set_rit_new,
    .get_rit =  icom_get_rit_new,

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
    .power2mW = icom_power2mW,
    .mW2power = icom_mW2power,
    .send_morse = icom_send_morse,
    .stop_morse = icom_stop_morse,
    .wait_morse = rig_wait_morse,
    .send_voice_mem = icom_send_voice_mem,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps ic705_caps =
{
    RIG_MODEL(RIG_MODEL_IC705),
    .model_name = "IC-705",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".8",
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
    .has_get_func =  IC7300_FUNCS,
    .has_set_func =  IC7300_FUNCS,
    .has_get_level =  IC705_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(IC705_LEVELS),
    .has_get_parm =  IC7300_PARMS,
    .has_set_parm =  RIG_PARM_SET(IC7300_PARMS),
    .level_gran = {
        [LVL_RAWSTR] = {.min = {.i = 0}, .max = {.i = 255}},
        [LVL_VOXDELAY] = {.min = {.i = 0}, .max = {.i = 20}, .step = {.i = 1}},
        [LVL_KEYSPD] = {.min = {.i = 6}, .max = {.i = 48}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 300}, .max = {.i = 900}, .step = {.i = 1}},
        [LVL_SPECTRUM_SPEED] = {.min = {.i = 0}, .max = {.i = 2}, .step = {.i = 1}},
        [LVL_SPECTRUM_REF] = {.min = {.f = -20.0f}, .max = {.f = 20.0f}, .step = {.f = 0.5f}},
        [LVL_SPECTRUM_AVG] = {.min = {.i = 0}, .max = {.i = 3}, .step = {.i = 1}},
        [LVL_USB_AF] = {.min = {.f = 0.0f}, .max = {.f = 1.0f}, .step = {.f = 1.0f / 255.0f }},
        [LVL_AGC_TIME] = {.min = {.f = 0.0f}, .max = {.f = 8.0f}, .step = {.f = 0.1f }},
    },
    .parm_gran =  {},
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
    .vfo_ops =  IC7300_VFO_OPS,
    .scan_ops =  IC7300_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   5,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM  },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(30), MHz(199.999999), IC705_ALL_RX_MODES, -1, -1, IC7300_VFOS, RIG_ANT_1, "USA"},
        {MHz(400), MHz(470), IC705_ALL_RX_MODES, -1, -1, IC7300_VFOS, RIG_ANT_1, "USA"},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        { kHz(1800), MHz(1.999999), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(3.5), MHz(3.999999), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(5.255), MHz(5.405), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(7.0), MHz(7.3), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(10.1), MHz(10.15), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(14.0), MHz(14.35), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(18.068), MHz(18.168), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(21.00), MHz(21.45), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(24.89), MHz(24.99), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(28.00), MHz(29.70), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(50.00), MHz(54.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(144.00), MHz(148.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        { MHz(430.00), MHz(450.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "USA" },
        RIG_FRNG_END,
    },

    .rx_range_list2 =   {
        {kHz(30), MHz(199.999999), IC705_ALL_RX_MODES, -1, -1, IC7300_VFOS, RIG_ANT_1, "EUR"},
        {MHz(400), MHz(470), IC705_ALL_RX_MODES, -1, -1, IC7300_VFOS, RIG_ANT_1, "EUR"},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   {
        { kHz(1810), MHz(1.999999), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(3.5), MHz(3.8), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(7.0), MHz(7.2), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(10.1), MHz(10.15), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(14.0), MHz(14.35), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(18.068), MHz(18.168), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(21.00), MHz(21.45), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(24.89), MHz(24.99), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(28.00), MHz(29.70), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(50.00), MHz(52.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(144.00), MHz(146.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        { MHz(430.00), MHz(440.00), IC705_ALL_TX_MODES, W(0.1), W(10), IC7300_VFOS, RIG_ANT_1, "EUR" },
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {IC7300_ALL_RX_MODES, Hz(100)},
        {IC7300_ALL_RX_MODES, kHz(.5)},
        {IC7300_ALL_RX_MODES, kHz(1)},
        {IC7300_ALL_RX_MODES, kHz(5)},
        {IC7300_ALL_RX_MODES, kHz(6.25)},
        {IC7300_ALL_RX_MODES, kHz(8.33)},
        {IC7300_ALL_RX_MODES, kHz(9)},
        {IC7300_ALL_RX_MODES, kHz(10)},
        {IC7300_ALL_RX_MODES, kHz(12.5)},
        {IC7300_ALL_RX_MODES, kHz(20)},
        {IC7300_ALL_RX_MODES, kHz(25)},
        {IC7300_ALL_RX_MODES, kHz(50)},
        {IC7300_ALL_RX_MODES, kHz(100)},
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
        RIG_FLT_END,
    },

    .str_cal = IC7300_STR_CAL,
    .swr_cal = IC7300_SWR_CAL,
    .alc_cal = IC7300_ALC_CAL,
    .rfpower_meter_cal = IC7300_RFPOWER_METER_CAL,
    .comp_meter_cal = IC7300_COMP_METER_CAL,
    .vd_meter_cal = IC7300_VD_METER_CAL,
    .id_meter_cal = IC7300_ID_METER_CAL,

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
    .set_mode =  icom_set_mode_with_data,
    .get_mode =  icom_get_mode_with_data,
    .get_vfo =  icom_get_vfo,
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
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

int ic7300_set_parm(RIG *rig, setting_t parm, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (parm)
    {
        unsigned char prmbuf[MAXFRAMELEN];

    case RIG_PARM_ANN:
    {
        int ann_mode = -1;
        int ann_lang = -1;

        switch (val.i)
        {
        case RIG_ANN_OFF:
            ann_mode = S_ANN_ALL;
            break;

        case RIG_ANN_FREQ:
            ann_mode = S_ANN_FREQ;
            break;

        case RIG_ANN_RXMODE:
            ann_mode = S_ANN_MODE;
            break;

        case RIG_ANN_ENG:
        case RIG_ANN_JAP:
            ann_lang = (val.i == RIG_ANN_ENG) ? 0 : 1;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "Unsupported RIG_PARM_ANN %d\n", val.i);
            return -RIG_EINVAL;
        }

        if (ann_mode >= 0)
        {
            return icom_set_raw(rig, C_CTL_ANN, ann_mode, 0, NULL, 0, 0);
        }
        else if (ann_lang >= 0)
        {
            prmbuf[0] = 0x1a;
            prmbuf[1] = 0x05;

            switch (rig->caps->rig_model)
            {
            case RIG_MODEL_IC7300:
                prmbuf[2] = 0x00;
                prmbuf[3] = 0x39;
                break;

            case RIG_MODEL_IC9700:
                prmbuf[2] = 0x01;
                prmbuf[3] = 0x77;
                break;

            case RIG_MODEL_IC705:
                prmbuf[2] = 0x00;
                prmbuf[3] = 0x53;
                break;

            default:
                return -RIG_ENIMPL;
            }

            prmbuf[4] = ann_lang;
            return icom_set_raw(rig, C_CTL_MEM, S_MEM_MODE_SLCT, 5, prmbuf, 0, 0);
        }

        rig_debug(RIG_DEBUG_ERR, "Unsupported RIG_PARM_ANN %d\n", val.i);
        return -RIG_EINVAL;
    }

    default:
        return icom_set_parm(rig, parm, val);
    }
}

int ic7300_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    unsigned char prmbuf[MAXFRAMELEN], resbuf[MAXFRAMELEN];
    int prm_len, res_len;
    int prm_cn, prm_sc;
    int icom_val = 0;
    int cmdhead;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (parm)
    {
    case RIG_PARM_ANN:
        return -RIG_ENIMPL; // How can we implement this?

    default:
        rig_debug(RIG_DEBUG_TRACE, "%s: using icom routine for PARM=%s\n", __func__,
                  rig_strparm(parm));
        return icom_get_parm(rig, parm, val);
    }

    retval = icom_transaction(rig, prm_cn, prm_sc, prmbuf, prm_len, resbuf,
                              &res_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    cmdhead = 3;
    res_len -= cmdhead;

    if (resbuf[0] != ACK && resbuf[0] != prm_cn)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__, resbuf[0],
                  res_len);
        return -RIG_ERJCTED;
    }

    switch (parm)
    {

    case RIG_PARM_ANN:
        rig_debug(RIG_DEBUG_WARN, "%s: not implemented\n", __func__);
        return -RIG_ENIMPL;

    default:
        return icom_get_parm(rig, parm, val);
    }


    rig_debug(RIG_DEBUG_TRACE, "%s: %d %d %d %f\n", __func__, res_len, icom_val,
              val->i, val->f);

    return RIG_OK;
}

// if hour < 0 then only date will be set
int ic7300_set_clock(RIG *rig, int year, int month, int day, int hour, int min,
                     int sec, double msec, int utc_offset)
{
    int cmd = 0x1a;
    int subcmd =  0x05;
    int retval = RIG_OK;
    unsigned char prmbuf[MAXFRAMELEN];

    if (year >= 0)
    {
        prmbuf[0] = 0x00;
        prmbuf[1] = 0x94;
        to_bcd(&prmbuf[2], year / 100, 2);
        to_bcd(&prmbuf[3], year % 100, 2);
        to_bcd(&prmbuf[4], month, 2);
        to_bcd(&prmbuf[5], day, 2);
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 6, NULL, NULL);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): %s\b", __func__, __LINE__, rigerror(retval));
        }
    }

    if (hour >= 0)
    {
        prmbuf[0] = 0x00;
        prmbuf[1] = 0x95;
        to_bcd(&prmbuf[2], hour, 2);
        to_bcd(&prmbuf[3], min, 2);
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 4, NULL, NULL);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): %s\b", __func__, __LINE__, rigerror(retval));
        }

        prmbuf[0] = 0x00;
        prmbuf[1] = 0x96;
        rig_debug(RIG_DEBUG_ERR, "%s: utc_offset=%d\n", __func__, utc_offset);
        to_bcd(&prmbuf[2], abs(utc_offset) / 100, 2);
        to_bcd(&prmbuf[3], abs(utc_offset) % 100, 2);
        to_bcd(&prmbuf[4], utc_offset >= 0 ? 0 : 1, 2);
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 5, NULL, NULL);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): %s\b", __func__, __LINE__, rigerror(retval));
        }
    }

    return retval;
}

int ic7300_get_clock(RIG *rig, int *year, int *month, int *day, int *hour,
                     int *min, int *sec, double *msec, int *utc_offset)
{
    int cmd = 0x1a;
    int subcmd =  0x05;
    int retval = RIG_OK;
    int resplen;
    unsigned char prmbuf[MAXFRAMELEN];
    unsigned char respbuf[MAXFRAMELEN];

    prmbuf[0] = 0x00;
    prmbuf[1] = 0x94;
    resplen = sizeof(respbuf);
    retval = icom_transaction(rig, cmd, subcmd, prmbuf, 2, respbuf, &resplen);
    *year = from_bcd(&respbuf[4], 2) * 100 + from_bcd(&respbuf[5], 2);
    *month = from_bcd(&respbuf[6], 2);
    *day = from_bcd(&respbuf[7], 2);

    if (hour != NULL)
    {
        prmbuf[0] = 0x00;
        prmbuf[1] = 0x95;
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 2, respbuf, &resplen);
        *hour = from_bcd(&respbuf[4], 2);
        *min = from_bcd(&respbuf[5], 2);
        *sec = 0;
        *msec = 0;

        prmbuf[0] = 0x00;
        prmbuf[1] = 0x96;
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 2, respbuf, &resplen);
        *utc_offset = from_bcd(&respbuf[4], 2) * 100;
        *utc_offset += from_bcd(&respbuf[5], 2);

        if (respbuf[6] != 0x00) { *utc_offset *= -1; }

        //rig_debug(RIG_DEBUG_VERBOSE,
        //          "%s: %02d-%02d-%02dT%02d:%02d:%06.3lf%s%04d\n'",
        //          __func__, *year, *month, *day, *hour, *min, *sec + *msec / 1000,
        //          *utc_offset >= 0 ? "+" : "-", (unsigned)abs(*utc_offset));
    }

    return retval;
}

// if hour < 0 then only date will be set
int ic9700_set_clock(RIG *rig, int year, int month, int day, int hour, int min,
                     int sec, double msec, int utc_offset)
{
    int cmd = 0x1a;
    int subcmd =  0x05;
    int retval = RIG_OK;
    unsigned char prmbuf[MAXFRAMELEN];

    if (year >= 0)
    {
        prmbuf[0] = 0x01;
        prmbuf[1] = 0x79;
        to_bcd(&prmbuf[2], year / 100, 2);
        to_bcd(&prmbuf[3], year % 100, 2);
        to_bcd(&prmbuf[4], month, 2);
        to_bcd(&prmbuf[5], day, 2);
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 6, NULL, NULL);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): %s\b", __func__, __LINE__, rigerror(retval));
        }
    }

    if (hour >= 0)
    {
        prmbuf[0] = 0x01;
        prmbuf[1] = 0x80;
        to_bcd(&prmbuf[2], hour, 2);
        to_bcd(&prmbuf[3], min, 2);
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 4, NULL, NULL);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): %s\b", __func__, __LINE__, rigerror(retval));
        }

        prmbuf[0] = 0x01;
        prmbuf[1] = 0x84;
        rig_debug(RIG_DEBUG_ERR, "%s: utc_offset=%d\n", __func__, utc_offset);
        to_bcd(&prmbuf[2], abs(utc_offset) / 100, 2);
        to_bcd(&prmbuf[3], abs(utc_offset) % 100, 2);
        to_bcd(&prmbuf[4], utc_offset >= 0 ? 0 : 1, 2);
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 5, NULL, NULL);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): %s\b", __func__, __LINE__, rigerror(retval));
        }
    }

    return retval;
}

int ic9700_get_clock(RIG *rig, int *year, int *month, int *day, int *hour,
                     int *min, int *sec, double *msec, int *utc_offset)
{
    int cmd = 0x1a;
    int subcmd =  0x05;
    int retval = RIG_OK;
    int resplen;
    unsigned char prmbuf[MAXFRAMELEN];
    unsigned char respbuf[MAXFRAMELEN];

    prmbuf[0] = 0x01;
    prmbuf[1] = 0x79;
    resplen = sizeof(respbuf);
    retval = icom_transaction(rig, cmd, subcmd, prmbuf, 2, respbuf, &resplen);
    *year = from_bcd(&respbuf[4], 2) * 100 + from_bcd(&respbuf[5], 2);
    *month = from_bcd(&respbuf[6], 2);
    *day = from_bcd(&respbuf[7], 2);

    if (hour != NULL)
    {
        prmbuf[0] = 0x01;
        prmbuf[1] = 0x80;
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 2, respbuf, &resplen);
        *hour = from_bcd(&respbuf[4], 2);
        *min = from_bcd(&respbuf[5], 2);
        *sec = 0;
        *msec = 0;

        prmbuf[0] = 0x01;
        prmbuf[1] = 0x84;
        retval = icom_transaction(rig, cmd, subcmd, prmbuf, 2, respbuf, &resplen);
        *utc_offset = from_bcd(&respbuf[4], 2) * 100;
        *utc_offset += from_bcd(&respbuf[5], 2);

        if (respbuf[6] != 0x00) { *utc_offset *= -1; }

        //rig_debug(RIG_DEBUG_VERBOSE,
        //          "%s: %02d-%02d-%02dT%02d:%02d:%06.3lf%s%04d\n'",
        //          __func__, *year, *month, *day, *hour, *min, *sec + *msec / 1000,
        //          *utc_offset >= 0 ? "+" : "-", (unsigned)abs(*utc_offset));
    }

    return retval;
}
