/*
 *  Hamlib CI-V backend - main file
 *  Copyright (c) 2000-2016 by Stephane Fillod
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

// cppcheck-suppress *
#include <stdlib.h>
// cppcheck-suppress *
#include <string.h>     /* String function definitions */
// cppcheck-suppress *
#include <unistd.h>     /* UNIX standard function definitions */
// cppcheck-suppress *
#include <math.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <cal.h>
#include <token.h>
#include <register.h>

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"
#include "event.h"

// we automatically determine availability of the 1A 03 command
enum { ENUM_1A_03_UNK, ENUM_1A_03_YES, ENUM_1A_03_NO };

static int set_vfo_curr(RIG *rig, vfo_t vfo, vfo_t curr_vfo);
static int icom_set_default_vfo(RIG *rig);
static int icom_get_spectrum_vfo(RIG *rig, vfo_t vfo);
static int icom_get_spectrum_edge_frequency_range(RIG *rig, vfo_t vfo,
        int *range_id);
static void icom_set_x25x26_ability(RIG *rig, int status);
static int icom_get_vfo_number_x25x26(RIG *rig, vfo_t vfo);

const cal_table_float_t icom_default_swr_cal =
{
    5,
    {
        {0, 1.0f},
        {48, 1.5f},
        {80, 2.0f},
        {120, 3.0f},
        {240, 6.0f}
    }
};

const cal_table_float_t icom_default_alc_cal =
{
    2,
    {
        {0, 0.0f},
        {120, 1.0f}
    }
};

const cal_table_float_t icom_default_rfpower_meter_cal =
{
    13,
    {
        { 0, 0.0f },
        { 21, 5.0f },
        { 43, 10.0f },
        { 65, 15.0f },
        { 83, 20.0f },
        { 95, 25.0f },
        { 105, 30.0f },
        { 114, 35.0f },
        { 124, 40.0f },
        { 143, 50.0f },
        { 183, 75.0f },
        { 213, 100.0f },
        { 255, 120.0f }
    }
};

const cal_table_float_t icom_default_comp_meter_cal =
{
    3,
    {
        {0, 0.0f},
        {130, 15.0f},
        {241, 30.0f}
    }
};

const cal_table_float_t icom_default_vd_meter_cal =
{
    3,
    {
        {0, 0.0f},
        {13, 10.0f},
        {241, 16.0f}
    }
};

const cal_table_float_t icom_default_id_meter_cal =
{
    4,
    {
        {0, 0.0f},
        {97, 10.0f},
        {146, 15.0f},
        {241, 25.0f}
    }
};

const struct ts_sc_list r8500_ts_sc_list[] =
{
    {10, 0x00},
    {50, 0x01},
    {100, 0x02},
    {kHz(1), 0x03},
    {12500, 0x04},
    {kHz(5), 0x05},
    {kHz(9), 0x06},
    {kHz(10), 0x07},
    {12500, 0x08},
    {kHz(20), 0x09},
    {kHz(25), 0x10},
    {kHz(100), 0x11},
    {MHz(1), 0x12},
    {0, 0x13},            /* programmable tuning step not supported */
    {0, 0},
};

const struct ts_sc_list ic737_ts_sc_list[] =
{
    {10, 0x00},
    {kHz(1), 0x01},
    {kHz(2), 0x02},
    {kHz(3), 0x03},
    {kHz(4), 0x04},
    {kHz(5), 0x05},
    {kHz(6), 0x06},
    {kHz(7), 0x07},
    {kHz(8), 0x08},
    {kHz(9), 0x09},
    {kHz(10), 0x10},
    {0, 0},
};

const struct ts_sc_list r75_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(5), 0x03},
    {6250, 0x04},
    {kHz(9), 0x05},
    {kHz(10), 0x06},
    {12500, 0x07},
    {kHz(20), 0x08},
    {kHz(25), 0x09},
    {kHz(100), 0x10},
    {MHz(1), 0x11},
    {0, 0},
};

const struct ts_sc_list r7100_ts_sc_list[] =
{
    {100, 0x00},
    {kHz(1), 0x01},
    {kHz(5), 0x02},
    {kHz(10), 0x03},
    {12500, 0x04},
    {kHz(20), 0x05},
    {kHz(25), 0x06},
    {kHz(100), 0x07},
    {0, 0},
};

const struct ts_sc_list r9000_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(5), 0x03},
    {kHz(9), 0x04},
    {kHz(10), 0x05},
    {12500, 0x06},
    {kHz(20), 0x07},
    {kHz(25), 0x08},
    {kHz(100), 0x09},
    {0, 0},
};

const struct ts_sc_list r9500_ts_sc_list[] =
{
    {1, 0x00},
    {10, 0x01},
    {100, 0x02},
    {kHz(1), 0x03},
    {kHz(2.5), 0x04},
    {kHz(5), 0x05},
    {6250, 0x06},
    {kHz(9), 0x07},
    {kHz(10), 0x08},
    {12500, 0x09},
    {kHz(20), 0x10},
    {kHz(25), 0x11},
    {kHz(100), 0x12},
    {MHz(1), 0x13},
    {0, 0},
};

const struct ts_sc_list ic718_ts_sc_list[] =
{
    {10, 0x00},
    {kHz(1), 0x01},
    {kHz(5), 0x01},
    {kHz(9), 0x01},
    {kHz(10), 0x04},
    {kHz(100), 0x05},
    {0, 0},
};

const struct ts_sc_list ic756_ts_sc_list[] =
{
    {10, 0x00},
    {kHz(1), 0x01},
    {kHz(5), 0x02},
    {kHz(9), 0x03},
    {kHz(10), 0x04},
    {0, 0},
};

const struct ts_sc_list ic756pro_ts_sc_list[] =
{
    {10, 0x00},           /* 1 if step turned off */
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(5), 0x03},
    {kHz(9), 0x04},
    {kHz(10), 0x05},
    {kHz(12.5), 0x06},
    {kHz(20), 0x07},
    {kHz(25), 0x08},
    {0, 0},
};

const struct ts_sc_list ic706_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(5), 0x03},
    {kHz(9), 0x04},
    {kHz(10), 0x05},
    {12500, 0x06},
    {kHz(20), 0x07},
    {kHz(25), 0x08},
    {kHz(100), 0x09},
    {0, 0},
};

const struct ts_sc_list ic7000_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(5), 0x03},
    {kHz(9), 0x04},
    {kHz(10), 0x05},
    {12500, 0x06},
    {kHz(20), 0x07},
    {kHz(25), 0x08},
    {kHz(100), 0x09},
    {MHz(1), 0x10},
    {0, 0},
};

const struct ts_sc_list ic7100_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(5), 0x03},
    {kHz(6.25), 0x04},
    {kHz(9), 0x05},
    {kHz(10), 0x06},
    {kHz(12.5), 0x07},
    {kHz(20), 0x08},
    {kHz(25), 0x09},
    {kHz(50), 0x0A},
    {kHz(100), 0x0B},
    {MHz(1), 0x0C},
    {0, 0x00},
};

const struct ts_sc_list ic7200_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(5), 0x03},
    {kHz(9), 0x04},
    {kHz(10), 0x05},
    {0, 0},
};

const struct ts_sc_list ic7300_ts_sc_list[] =
{
    {1, 0x00},            /* Manual says "Send/read the tuning step OFF" */
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(5), 0x03},
    {kHz(9), 0x04},
    {kHz(10), 0x05},
    {kHz(12.5), 0x06},
    {kHz(20), 0x07},
    {kHz(25), 0x08},
    {0, 0},
};

const struct ts_sc_list ic910_ts_sc_list[] =
{
    {Hz(1), 0x00},
    {Hz(10), 0x01},
    {Hz(50), 0x02},
    {Hz(100), 0x03},
    {kHz(1), 0x04},
    {kHz(5), 0x05},
    {kHz(6.25), 0x06},
    {kHz(10), 0x07},
    {kHz(12.5), 0x08},
    {kHz(20), 0x09},
    {kHz(25), 0x10},
    {kHz(100), 0x11},
    {0, 0},
};

const struct ts_sc_list r8600_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {kHz(1), 0x02},
    {kHz(2.5), 0x03},
    {3125, 0x04},
    {kHz(5), 0x05},
    {6250, 0x06},
    {8330, 0x07},
    {kHz(9), 0x08},
    {kHz(10), 0x09},
    {kHz(12.5), 0x10},
    {kHz(20), 0x11},
    {kHz(25), 0x12},
    {kHz(100), 0x13},
    {0, 0x14},            /* programmable tuning step not supported */
    {0, 0},
};

const struct ts_sc_list ic705_ts_sc_list[] =
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

const struct ts_sc_list ic9700_ts_sc_list[] =
{
    {10, 0x00},
    {100, 0x01},
    {500, 0x02},
    {kHz(1), 0x03},
    {kHz(5), 0x04},
    {kHz(6.25), 0x05},
    {kHz(10), 0x06},
    {kHz(12.5), 0x07},
    {kHz(20), 0x08},
    {kHz(25), 0x09},
    {kHz(50), 0x10},
    {kHz(100), 0x11},
    {0, 0},
};

/* rtty filter list for some DSP rigs ie PRO */
#define RTTY_FIL_NB 5
const pbwidth_t rtty_fil[] =
{
    Hz(250),
    Hz(300),
    Hz(350),
    Hz(500),
    kHz(1),
    0,
};

/* AGC Time value lookups */
const agc_time_t agc_level[] = // default
{
    0, 0.1, 0.2, 0.3, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0
};
const agc_time_t agc_level2[] = // AM Mode for 7300/9700/705
{
    0, 0.3, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0
};

struct icom_addr
{
    rig_model_t model;
    unsigned char re_civ_addr;
};


#define TOK_CIVADDR TOKEN_BACKEND(1)
#define TOK_MODE731 TOKEN_BACKEND(2)
#define TOK_NOXCHG TOKEN_BACKEND(3)
#define TOK_TONE_ENABLE TOKEN_BACKEND(4)
#define TOK_FILTER_USBD TOKEN_BACKEND(5)
#define TOK_FILTER_USB TOKEN_BACKEND(6)
#define TOK_FILTER_CW TOKEN_BACKEND(7)

const struct confparams icom_cfg_params[] =
{
    {
        TOK_CIVADDR, "civaddr", "CI-V address", "Transceiver's CI-V address",
        "0", RIG_CONF_NUMERIC, {.n = {0, 0xff, 1}}
    },
    {
        TOK_MODE731, "mode731", "CI-V 731 mode", "CI-V operating frequency "
        "data length, needed for IC731 and IC735",
        "0", RIG_CONF_CHECKBUTTON
    },
    {
        TOK_NOXCHG, "no_xchg", "No VFO XCHG",
        "Don't Use VFO XCHG to set other VFO mode and Frequency",
        "0", RIG_CONF_CHECKBUTTON
    },
    {
        TOK_TONE_ENABLE, "tone_enable", "Turn tone on",
        "Overcome a bug in IC-705 to enable tone after frequency change",
        "0", RIG_CONF_CHECKBUTTON
    },
    {
        TOK_FILTER_USBD, "filter_usbd", "Filter to use USBD", "Filter to use for USBD/LSBD when setting mode",
        "0", RIG_CONF_NUMERIC, {.n = {0, 3, 1}}
    },
    {
        TOK_FILTER_USB, "filter_usb", "Filter to use USB", "Filter to use when for USB/LSB setting mode",
        "0", RIG_CONF_NUMERIC, {.n = {0, 3, 1}}
    },
    {
        TOK_FILTER_CW, "filter_cw", "Filter to use CW", "Filter to use for CW/CWR when setting mode",
        "0", RIG_CONF_NUMERIC, {.n = {0, 3, 1}}
    },
    {RIG_CONF_END, NULL,}
};

/*
 *  Lookup table for icom_get_ext_func
 */
const struct confparams icom_ext_funcs[] =
{
    { TOK_DIGI_SEL_FUNC, "digi_sel", "DIGI-SEL enable", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_IPP_FUNC, "IPP", "IP Plus", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_TX_INHIBIT_FUNC, "TX_INHIBIT", "TX Inhibit", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_DPP_FUNC, "DPP", "Digital Pre Distortion-SEL enable", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_ICPW2_FUNC, "ICPW2", "Icom PW2 enable", "", "", RIG_CONF_CHECKBUTTON, {} },
    { RIG_CONF_END, NULL, }
};

/*
 *  Lookup table for icom_get_ext_level
 */
const struct confparams icom_ext_levels[] =
{
    { TOK_DIGI_SEL_LEVEL, "digi_sel_level", "DIGI-SEL level", "", "", RIG_CONF_NUMERIC, { .n = { 0, 255, 1 } } },
    { TOK_DRIVE_GAIN, "drive_gain", "Drive gain", "", "", RIG_CONF_NUMERIC, { .n = { 0, 255, 1 } } },
    { TOK_SCOPE_MSS, "SPECTRUM_SELECT", "Spectrum Scope Main/Sub", "", "", RIG_CONF_COMBO, { .c = { .combostr = { "Main", "Sub", NULL } } } },
    { TOK_SCOPE_SDS, "SPECTRUM_DUAL", "Spectrum Scope Single/Dual", "", "", RIG_CONF_COMBO, { .c = { .combostr = { "Single", "Dual", NULL } } } },
    { TOK_SCOPE_EDG, "SPECTRUM_EDGE", "Spectrum Scope Edge", "Edge selection for fixed scope mode", "", RIG_CONF_COMBO, { .c = { .combostr = { "1", "2", "3", "4", NULL } } } },
    { TOK_SCOPE_STX, "SPECTRUM_TX", "Spectrum Scope TX operation", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_SCOPE_CFQ, "SPECTRUM_CENTER", "Spectrum Scope Center Frequency Type", "", "", RIG_CONF_COMBO, { .c = { .combostr = { "Filter center", "Carrier point center", "Carrier point center (Abs. Freq.)", NULL } } } },
    { TOK_SCOPE_VBW, "SPECTRUM_VBW", "Spectrum Scope VBW", "Video Band Width", "", RIG_CONF_COMBO, { .c = { .combostr = { "Narrow", "Wide", NULL } } } },
    { TOK_SCOPE_RBW, "SPECTRUM_RBW", "Spectrum Scope RBW", "Resolution Band Width", "", RIG_CONF_COMBO, { .c = { .combostr = { "Wide", "Mid", "Narrow", NULL } } } },
    { RIG_CONF_END, NULL, }
};

/*
 *  Lookup table for icom_get_ext_parm
 */
const struct confparams icom_ext_parms[] =
{
    { TOK_DSTAR_DSQL, "dsdsql", "D-STAR CSQL Status", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_DSTAR_CALL_SIGN, "dscals", "D-STAR Call sign", "", "", RIG_CONF_BINARY, {} },
    { TOK_DSTAR_MESSAGE, "dsrmes", "D-STAR Rx Message", "", "", RIG_CONF_STRING, {} },
    { TOK_DSTAR_STATUS, "dsstat", "D-STAR Rx Status", "", "", RIG_CONF_BINARY, {} },
    { TOK_DSTAR_GPS_DATA, "dsgpsd", "D-STAR GPS Data", "", "", RIG_CONF_BINARY, {} },
    { TOK_DSTAR_GPS_MESS, "dsgpsm", "D-STAR GPS Message", "", "", RIG_CONF_STRING, {} },
    { TOK_DSTAR_CODE, "dscode", "D-STAR CSQL Code", "", "", RIG_CONF_NUMERIC, {} },
    { TOK_DSTAR_TX_DATA, "dstdat", "D-STAR Tx Data", "", "", RIG_CONF_BINARY, {} },
    { TOK_DSTAR_MY_CS, "dsmycs", "D-STAR MY Call Sign", "", "", RIG_CONF_STRING, {} },
    { TOK_DSTAR_TX_CS, "dstxcs", "D-STAR Tx Call Sign", "", "", RIG_CONF_BINARY, {} },
    { TOK_DSTAR_TX_MESS, "dstmes", "D-STAR Tx Message", "", "", RIG_CONF_STRING, {} },
    { RIG_CONF_END, NULL, }
};

/*
 *  Lookup table for icom_get_ext_* & icom_set_ext_* functions
 */

const struct cmdparams icom_ext_cmd[] =
{
    { {.t = TOK_DSTAR_DSQL}, CMD_PARAM_TYPE_TOKEN, C_CTL_DIG, S_DIG_DSCSQL, SC_MOD_RW, 1, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_DSTAR_CALL_SIGN}, CMD_PARAM_TYPE_TOKEN, C_CTL_DIG, S_DIG_DSCALS, SC_MOD_RW12, 2, {0}, CMD_DAT_BUF, 38 },
    { {.t = TOK_DSTAR_MESSAGE}, CMD_PARAM_TYPE_TOKEN, C_CTL_DIG, S_DIG_DSMESS, SC_MOD_RW12, 2, {0}, CMD_DAT_STR, 32 },
    { {.t = TOK_DSTAR_STATUS}, CMD_PARAM_TYPE_TOKEN, C_CTL_DIG, S_DIG_DSRSTS, SC_MOD_RW12, 2, {0}, CMD_DAT_BUF, 1 },
    { {.t = TOK_DSTAR_GPS_DATA}, CMD_PARAM_TYPE_TOKEN, C_CTL_DIG, S_DIG_DSGPSD, SC_MOD_RW12, 2, {0}, CMD_DAT_BUF, 52 },
    { {.t = TOK_DSTAR_GPS_MESS}, CMD_PARAM_TYPE_TOKEN, C_CTL_DIG, S_DIG_DSGPSM, SC_MOD_RW12, 2, {0}, CMD_DAT_STR, 52 },
    { {.t = TOK_DSTAR_CODE}, CMD_PARAM_TYPE_TOKEN, C_CTL_DIG, S_DIG_DSCSQL, SC_MOD_RW12, 2, {0}, CMD_DAT_FLT, 1 },
    { {.t = TOK_DSTAR_TX_DATA}, CMD_PARAM_TYPE_TOKEN, C_CTL_DSD, S_DSD_DSTXDT, SC_MOD_RW, 1, {0}, CMD_DAT_BUF, 30 },
    { {.t = TOK_DSTAR_MY_CS}, CMD_PARAM_TYPE_TOKEN, C_CTL_DVT, S_DVT_DSMYCS, SC_MOD_RW, 1, {0}, CMD_DAT_STR, 12 },
    { {.t = TOK_DSTAR_TX_CS}, CMD_PARAM_TYPE_TOKEN, C_CTL_DVT, S_DVT_DSTXCS, SC_MOD_RW, 1, {0}, CMD_DAT_STR, 24 },
    { {.t = TOK_DSTAR_TX_MESS}, CMD_PARAM_TYPE_TOKEN, C_CTL_DVT, S_DVT_DSTXMS, SC_MOD_RW, 1, {0}, CMD_DAT_STR, 20 },
    { {.t = TOK_DRIVE_GAIN}, CMD_PARAM_TYPE_TOKEN, C_CTL_LVL, S_LVL_DRIVE, SC_MOD_RW, 0, {0}, CMD_DAT_FLT, 2 },
    { {.t = TOK_DIGI_SEL_FUNC}, CMD_PARAM_TYPE_TOKEN, C_CTL_FUNC, S_FUNC_DIGISEL, SC_MOD_RW, 0, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_DIGI_SEL_LEVEL}, CMD_PARAM_TYPE_TOKEN, C_CTL_LVL, S_LVL_DIGI, SC_MOD_RW, 0, {0}, CMD_DAT_FLT, 2 },
    { {.t = TOK_IPP_FUNC}, CMD_PARAM_TYPE_TOKEN, C_CTL_FUNC, S_FUNC_IPP, SC_MOD_RW, 0, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_TX_INHIBIT_FUNC}, CMD_PARAM_TYPE_TOKEN, C_CTL_FUNC, S_FUNC_TX_INHIBIT, SC_MOD_RW, 0, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_DPP_FUNC}, CMD_PARAM_TYPE_TOKEN, C_CTL_FUNC, S_FUNC_DPP, SC_MOD_RW, 0, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_ICPW2_FUNC}, CMD_PARAM_TYPE_TOKEN, C_CTL_MEM, 0x05, SC_MOD_RW, 2, {0x03, 0x10}, CMD_DAT_BOL, 1 },
    { {0} }
};

/*
 * Please, if the default CI-V address of your rig is listed as UNKNOWN_ADDR,
 * send the value to <fillods@users.sourceforge.net> for inclusion. Thanks --SF
 */
static const struct icom_addr icom_addr_list[] =
{
    {RIG_MODEL_IC703, 0x68},
    {RIG_MODEL_IC706, 0x48},
    {RIG_MODEL_IC706MKII, 0x4e},
    {RIG_MODEL_IC706MKIIG, 0x58},
    {RIG_MODEL_IC271, 0x20},
    {RIG_MODEL_IC275, 0x10},
    {RIG_MODEL_IC375, 0x12},
    {RIG_MODEL_IC471, 0x22},
    {RIG_MODEL_IC475, 0x14},
    {RIG_MODEL_IC575, 0x16},
    {RIG_MODEL_IC707, 0x3e},
    {RIG_MODEL_IC725, 0x28},
    {RIG_MODEL_IC726, 0x30},
    {RIG_MODEL_IC728, 0x38},
    {RIG_MODEL_IC729, 0x3a},
    {RIG_MODEL_IC731, 0x02},  /* need confirmation */
    {RIG_MODEL_IC735, 0x04},
    {RIG_MODEL_IC736, 0x40},
    {RIG_MODEL_IC7410, 0x80},
    {RIG_MODEL_IC746, 0x56},
    {RIG_MODEL_IC746PRO, 0x66},
    {RIG_MODEL_IC737, 0x3c},
    {RIG_MODEL_IC738, 0x44},
    {RIG_MODEL_IC751, 0x1c},
    {RIG_MODEL_IC751A, 0x1c},
    {RIG_MODEL_IC756, 0x50},
    {RIG_MODEL_IC756PRO, 0x5c},
    {RIG_MODEL_IC756PROII, 0x64},
    {RIG_MODEL_IC756PROIII, 0x6e},
    {RIG_MODEL_IC7600, 0x7a},
    {RIG_MODEL_IC761, 0x1e},
    {RIG_MODEL_IC765, 0x2c},
    {RIG_MODEL_IC775, 0x46},
    {RIG_MODEL_IC7800, 0x6a},
    {RIG_MODEL_IC785x, 0x8e},
    {RIG_MODEL_IC781, 0x26},
    {RIG_MODEL_IC820, 0x42},
    {RIG_MODEL_IC821H, 0x4c},
    {RIG_MODEL_IC910, 0x60},
    {RIG_MODEL_IC9100, 0x7c},
    {RIG_MODEL_IC9700, 0xa2},
    {RIG_MODEL_IC970, 0x2e},
    {RIG_MODEL_IC1271, 0x24},
    {RIG_MODEL_IC1275, 0x18},
    {RIG_MODEL_ICR10, 0x52},
    {RIG_MODEL_ICR20, 0x6c},
    {RIG_MODEL_ICR6, 0x7e},
    {RIG_MODEL_ICR71, 0x1a},
    {RIG_MODEL_ICR72, 0x32},
    {RIG_MODEL_ICR75, 0x5a},
    {RIG_MODEL_ICRX7, 0x78},
    {RIG_MODEL_IC78, 0x62},
    {RIG_MODEL_ICR7000, 0x08},
    {RIG_MODEL_ICR7100, 0x34},
    {RIG_MODEL_ICR8500, 0x4a},
    {RIG_MODEL_ICR9000, 0x2a},
    {RIG_MODEL_ICR9500, 0x72},
    {RIG_MODEL_MINISCOUT, 0x94},
    {RIG_MODEL_IC718, 0x5e},
    {RIG_MODEL_OS535, 0x80},  /* same address as IC-7410 */
    {RIG_MODEL_ICID1, 0x01},
    {RIG_MODEL_IC7000, 0x70},
    {RIG_MODEL_IC7100, 0x88},
    {RIG_MODEL_IC7200, 0x76},
    {RIG_MODEL_IC7610, 0x98},
    {RIG_MODEL_IC7700, 0x74},
    {RIG_MODEL_PERSEUS, 0xE1},
    {RIG_MODEL_X108G, 0x70},
    {RIG_MODEL_X6100, 0x70},
    {RIG_MODEL_ICR8600, 0x96},
    {RIG_MODEL_ICR30, 0x9c},
    {RIG_MODEL_NONE, 0},
};

/*
 * This is a generic icom_init function.
 * You might want to define yours, so you can customize it for your rig
 *
 * Basically, it sets up *priv
 * REM: serial port is already open (RIGPORT(rig)->fd)
 */
int icom_init(RIG *rig)
{
    struct icom_priv_data *priv;
    struct icom_priv_caps *priv_caps;
    struct rig_caps *caps;
    int i;

    ENTERFUNC;

    if (!rig->caps)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (!caps->priv)
    {
        RETURNFUNC(-RIG_ECONF);
    }

    priv_caps = (struct icom_priv_caps *) caps->priv;


    rig->state.priv = (struct icom_priv_data *) calloc(1,
                      sizeof(struct icom_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = rig->state.priv;

    priv->spectrum_scope_count = 0;

    for (i = 0; caps->spectrum_scopes[i].name != NULL; i++)
    {
        priv->spectrum_scope_cache[i].spectrum_data = NULL;

        if (priv_caps->spectrum_scope_caps.spectrum_line_length < 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: no spectrum scope line length defined\n",
                      __func__);
            RETURNFUNC(-RIG_ECONF);
        }

        priv->spectrum_scope_cache[i].spectrum_data = calloc(1,
                priv_caps->spectrum_scope_caps.spectrum_line_length);

        if (!priv->spectrum_scope_cache[i].spectrum_data)
        {
            RETURNFUNC(-RIG_ENOMEM);
        }

        priv->spectrum_scope_count++;
    }

    /* TODO: CI-V address should be customizable */

    /*
     * init the priv_data from static struct
     *          + override with preferences
     */

    priv->re_civ_addr = priv_caps->re_civ_addr;
    priv->civ_731_mode = priv_caps->civ_731_mode;
    priv->no_xchg = priv_caps->no_xchg;
    priv->serial_USB_echo_off = -1; // unknown at this point
    priv->x25cmdfails = 1;
    priv->x26cmdfails = 1;
    priv->x1cx03cmdfails = 0;

    // Reset 0x25/0x26 command detection for the rigs that may support it
    icom_set_x25x26_ability(rig, -1);

    rig_debug(RIG_DEBUG_TRACE, "%s: done\n", __func__);

    RETURNFUNC(RIG_OK);
}

/*
 * ICOM Generic icom_cleanup routine
 * the serial port is closed by the frontend
 */
int icom_cleanup(RIG *rig)
{
    struct icom_priv_data *priv;
    int i;

    ENTERFUNC;

    priv = rig->state.priv;

    for (i = 0; rig->caps->spectrum_scopes[i].name != NULL; i++)
    {
        if (priv->spectrum_scope_cache[i].spectrum_data)
        {
            free(priv->spectrum_scope_cache[i].spectrum_data);
            priv->spectrum_scope_cache[i].spectrum_data = NULL;
        }
    }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    RETURNFUNC(RIG_OK);
}

/**
 * Returns 1 when USB ECHO is off
 * Returns 0 when USB ECHO is on
 * \return Returns < 0 when error occurs (e.g. timeout, nimple, navail)
 */
int icom_get_usb_echo_off(RIG *rig)
{
    int retval;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    ENTERFUNC;

    // Check for echo on first by assuming echo is off and checking the answer
    priv->serial_USB_echo_off = 1;

    retval = icom_transaction(rig, C_RD_FREQ, -1, NULL, 0, ackbuf, &ack_len);

    // if rig is not powered on we get no data and TIMEOUT
    if (ack_len == 0 && retval == -RIG_ETIMEOUT) { RETURNFUNC(retval); }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ack_len=%d\n", __func__, ack_len);

    if (ack_len == 1) // then we got an echo of the cmd
    {
        unsigned char buf[16];
        priv->serial_USB_echo_off = 0;
        // we should have a freq response so we'll read it and don't really care
        // flushing doesn't always work as it depends on timing
        retval = read_icom_frame(RIGPORT(rig), buf, sizeof(buf));
        rig_debug(RIG_DEBUG_VERBOSE, "%s: USB echo on detected, get freq retval=%d\n",
                  __func__, retval);

        if (retval <= 0) { RETURNFUNC(-RIG_ETIMEOUT); }
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: USB echo off detected\n", __func__);
    }

    RETURNFUNC(priv->serial_USB_echo_off);
}

static int icom_check_ack(int ack_len, unsigned char *ackbuf)
{
    if ((ack_len >= 1 && ackbuf[0] != ACK) && (ack_len >= 2 && ackbuf[1] != NAK))
    {
        // if we don't get ACK/NAK some serial corruption occurred
        // so we'll call it a timeout for retry purposes
        return -RIG_ETIMEOUT;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: command not acknowledged (%#.2x), len=%d\n",
                  __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

#if 0 // this causes the rig to go into VFO mode when it is in memory mode
// we have to give up on determining active VFO for Icom rigs
// figure out what VFO is current for rigs with 0x25 command
static int icom_current_vfo_x25(RIG *rig, vfo_t *vfo)
{
    int freq_offset = 0;
    freq_t freq_current, freq_other, freq_current_2;
    vfo_t vfo_current = RIG_VFO_NONE;
    vfo_t vfo_check = RIG_VFO_A;
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;

    if (priv->x25cmdfails > 0 && !priv_caps->x25x26_always)
    {
        return -RIG_ENAVAIL;
    }

    rig_get_freq(rig, RIG_VFO_CURR, &freq_current);
    rig_get_freq(rig, RIG_VFO_OTHER, &freq_other);

    if (freq_current == freq_other)
    {
        if (priv->vfo_flag != 0)
        {
            // we can't change freqs unless rig is idle and we don't know that
            // so we only check vfo once when freqs are equal
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: VFO already determined, returning current_vfo %s\n",
                      __func__, rig_strvfo(rig->state.current_vfo));
            *vfo = rig->state.current_vfo;
            return RIG_OK;
        }

        priv->vfo_flag = 1;

        freq_offset = 100;
        rig_set_freq(rig, RIG_VFO_CURR, freq_current + freq_offset);
    }

    if (rig->state.current_vfo == RIG_VFO_B) { vfo_check = RIG_VFO_B; }

    rig_set_vfo(rig, vfo_check);
    rig_get_freq(rig, RIG_VFO_CURR, &freq_current_2);

    if (freq_current_2 == freq_current +
            freq_offset) // then we are on the vfo_check
    {
        vfo_current = vfo_check;
    }
    else // the other VFO is the current one
    {
        rig_set_vfo(rig, vfo_check == RIG_VFO_A ? RIG_VFO_B : RIG_VFO_A);
        vfo_current = vfo_check == RIG_VFO_A ? RIG_VFO_B : RIG_VFO_A;
    }

    if (freq_offset) // then we need to change freq_current back to original freq
    {
        rig_set_freq(rig, RIG_VFO_CURR, freq_current);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo_current=%s\n", __func__,
              rig_strvfo(vfo_current));

    *vfo = vfo_current;
    return RIG_OK;
}
#endif

static int icom_current_vfo_to_vfo_with_band(RIG *rig, vfo_t *vfo_current)
{
    int retval;

    vfo_t vfo_band;
    retval = icom_get_vfo(rig, &vfo_band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (*vfo_current == RIG_VFO_B)
    {
        if (vfo_band == RIG_VFO_SUB)
        {
            *vfo_current = RIG_VFO_SUB_B;
        }
        else
        {
            *vfo_current = RIG_VFO_MAIN_B;
        }
    }
    else
    {
        if (vfo_band == RIG_VFO_SUB)
        {
            *vfo_current = RIG_VFO_SUB_A;
        }
        else
        {
            *vfo_current = RIG_VFO_MAIN_A;
        }
    }

    return retval;
}

// figure out what VFO is current
static vfo_t icom_current_vfo(RIG *rig)
{
    int retval;
    int freq_offset = 0;
    freq_t freq_current, freq_other, freq_current_2;
    vfo_t vfo_current = RIG_VFO_NONE;
    vfo_t vfo_check = RIG_VFO_A;
    struct rig_state *rs = STATE(rig);
    struct rig_cache *cachep = CACHE(rig);
    struct icom_priv_data *priv = rs->priv;

#if 0
    // Icom rigs with both Main/Sub receivers and A/B VFOs cannot use the 0x25 command to read Sub receiver frequency
    if (rs->targetable_vfo & RIG_TARGETABLE_FREQ && !VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        // these newer rigs get special treatment
        retval = icom_current_vfo_x25(rig, &vfo_current);

        if (VFO_HAS_MAIN_SUB_ONLY || (VFO_HAS_MAIN_SUB_A_B_ONLY && cachep->satmode))
        {
            vfo_current = (vfo_current == RIG_VFO_A) ? RIG_VFO_MAIN : RIG_VFO_SUB;
        }

        if (retval == RIG_OK)
        {
            return vfo_current;
        }
    }
#endif

    if (cachep->ptt)
    {
        // don't do this if transmitting -- XCHG would mess it up
        return rs->current_vfo;
    }
    else if (!rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        // for now we will just set vfoa and be done with it
        // will take more logic for rigs without XCHG
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: defaulting to VFOA as no XCHG or x25 available\n",
                  __func__);
        rig_set_vfo(rig, RIG_VFO_A);
        return RIG_VFO_A;
    }

    rig_get_freq(rig, RIG_VFO_CURR, &freq_current);

    if (rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Using XCHG to swap\n", __func__);

        retval = icom_vfo_op(rig, vfo_current, RIG_OP_XCHG);

        if (retval != RIG_OK)
        {
            RETURNFUNC(rs->current_vfo);
        }
    }

    rig_get_freq(rig, RIG_VFO_CURR, &freq_other);

    if (rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Using XCHG to swap back\n", __func__);

        retval = icom_vfo_op(rig, vfo_current, RIG_OP_XCHG);

        if (retval != RIG_OK)
        {
            RETURNFUNC(rs->current_vfo);
        }
    }

    if (freq_current == freq_other)
    {
        if (priv->vfo_flag != 0)
        {
            // we can't change freqs unless rig is idle and we don't know that
            // so we only check vfo once when freqs are equal
            rig_debug(RIG_DEBUG_TRACE, "%s: vfo already determined...returning current_vfo",
                      __func__);
            return rs->current_vfo;
        }

        priv->vfo_flag = 1;

        freq_offset = 100;
        rig_set_freq(rig, RIG_VFO_CURR, freq_current + freq_offset);
    }

    if (rs->current_vfo == RIG_VFO_B)
    {
        vfo_check = RIG_VFO_B;
    }

    rig_set_vfo(rig, vfo_check);
    rig_get_freq(rig, RIG_VFO_CURR, &freq_current_2);

    if (freq_current_2 == freq_current + freq_offset)
    {
        vfo_current = vfo_check;
    }
    else
    {
        rig_set_vfo(rig, vfo_check == RIG_VFO_A ? RIG_VFO_B : RIG_VFO_A);
        vfo_current = vfo_check == RIG_VFO_A ? RIG_VFO_B : RIG_VFO_A;
    }

    if (freq_offset) // then we need to change freq_current back to original freq
    {
        rig_set_freq(rig, RIG_VFO_CURR, freq_current);
    }

    if (VFO_HAS_MAIN_SUB_ONLY || (VFO_HAS_MAIN_SUB_A_B_ONLY && cachep->satmode))
    {
        vfo_current = (vfo_current == RIG_VFO_A) ? RIG_VFO_MAIN : RIG_VFO_SUB;
    }
    else if (VFO_HAS_MAIN_SUB_A_B_ONLY && RIG_IS_IC9700)
    {
        // TODO: VFO op XCHG only exchanges Main/Sub, so Sub receiver VFO A/B selection cannot be determined this way
        retval = icom_current_vfo_to_vfo_with_band(rig, &vfo_current);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo_current=%s\n", __func__,
              rig_strvfo(vfo_current));

    if (vfo_current == RIG_VFO_NONE && rs->current_vfo != RIG_VFO_NONE)
    {
        vfo_current = rs->current_vfo;
    }

    return vfo_current;
}

// Some rigs, such as IC-9700, cannot execute 0x25/0x26 commands in satmode
static void icom_satmode_fix(RIG *rig, int satmode)
{
    if (RIG_IS_IC9700)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: toggling IC-9700 targetable for satmode=%d\n",
                  __func__, satmode);

        // Modify the copy of targetable_vfo in rig_state only!
        if (satmode)
        {
            rig->state.targetable_vfo = 0;
        }
        else
        {
            rig->state.targetable_vfo = rig->caps->targetable_vfo;
        }
    }
}

/*
 * ICOM rig open routine
 * Detect echo state of USB serial port
 */
int icom_rig_open(RIG *rig)
{
    int retval, retval_echo, value;
    int satmode = 0;
    struct rig_state *rs = STATE(rig);
    hamlib_port_t *rp = RIGPORT(rig);
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    int retry_flag = 1;
    short retry_save = rp->retry;

    ENTERFUNC;

    rp->retry = 0;

    priv->no_1a_03_cmd = ENUM_1A_03_UNK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s v%s\n", __func__, rig->caps->model_name,
              rig->caps->version);

    if (rs->auto_power_on && priv->poweron == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s asking for power on *****************************************\n", __func__);
        rig_set_powerstat(rig, 1);
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s asking for power on #2 =======================================\n",
                  __func__);
        priv->poweron = 1;
    }

retry_open:
    retval_echo = icom_get_usb_echo_off(rig);

    rig_debug(RIG_DEBUG_TRACE, "%s: echo status result=%d\n",  __func__,
              retval_echo);

    if (retval_echo == 0 || retval_echo == 1)
    {
        retval = RIG_OK;
    }
    else
    {
        retval = retval_echo;
    }

    if (retval == RIG_OK) // then we know our echo status
    {
        // we need to know about dual watch for later use
        rs->dual_watch = 0;
        retval = rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, &value);

        if (retval == RIG_OK)
        {
            rs->dual_watch = value;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: dual_watch=%d\n", __func__, rs->dual_watch);
        rig_debug(RIG_DEBUG_TRACE, "%s: echo status known, getting frequency\n",
                  __func__);
        rp->retry = 0;
        rig->state.current_vfo = icom_current_vfo(rig);
        // some rigs like the IC7100 still echo when in standby
        // so asking for freq now should timeout if such a rig
        freq_t tfreq;
        retval = rig_get_freq(rig, RIG_VFO_CURR, &tfreq);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig error getting frequency retry=%d, err=%s\n",
                      __func__, retry_flag, rigerror(retval));
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: echo status unknown\n", __func__);
    }

    if (retval != RIG_OK && priv->poweron == 0 && rs->auto_power_on)
    {
        // maybe we need power on?
        rig_debug(RIG_DEBUG_VERBOSE, "%s trying power on\n", __func__);
        retval = abs(rig_set_powerstat(rig, 1));

        // this is only a fatal error if powerstat is implemented
        // if not implemented than we're at an error here
        if (retval != RIG_OK)
        {
            rp->retry = retry_save;

            rig_debug(RIG_DEBUG_ERR, "%s: rig_set_powerstat failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == RIG_ENIMPL || retval == RIG_ENAVAIL)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: rig_set_powerstat not implemented for rig\n",
                          __func__);
                RETURNFUNC(-RIG_ECONF);
            }

            RETURNFUNC(retval);
        }

        // Now that we're powered up let's try again
        retval_echo = icom_get_usb_echo_off(rig);

        if (retval_echo != 0 && retval_echo != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unable to determine USB echo status\n", __func__);
            rp->retry = retry_save;
            RETURNFUNC(retval_echo);
        }
    }
    else if (retval != RIG_OK)
    {
        // didn't ask for power on so let's retry one more time

        if (retry_flag)
        {
            retry_flag = 0;
            hl_usleep(500 * 1000); // 500ms pause
            goto retry_open;
        }

        rp->retry = retry_save;
    }

    priv->poweron = (retval == RIG_OK) ? 1 : 0;

    if (priv->poweron)
    {
        if (rig->caps->has_get_func & RIG_FUNC_SATMODE)
        {
            // Getting satmode state updates RX/TX VFOs internally
            rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
        }

        rs->current_vfo = icom_current_vfo(rig);
    }

#if 0 // do not do this here -- needs to be done when ranges are requested instead as this is very slow
    icom_get_freq_range(rig); // try get to get rig range capability dyamically
#endif

    rp->retry = retry_save;

    RETURNFUNC(RIG_OK);
}

/*
 * ICOM rig close routine
 */
int icom_rig_close(RIG *rig)
{
    // Nothing to do yet
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    ENTERFUNC;

    if (priv->poweron == 0) { RETURNFUNC(RIG_OK); } // nothing to do

    if (priv->poweron == 1 && rs->auto_power_off)
    {
        // maybe we need power off?
        rig_debug(RIG_DEBUG_VERBOSE, "%s trying power off\n", __func__);
        int retval = abs(rig_set_powerstat(rig, 0));

        // this is only a fatal error if powerstat is implemented
        // if not iplemented than we're at an error here
        if (retval != RIG_OK && retval != RIG_ENIMPL && retval != RIG_ENAVAIL)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: unexpected retval here: %s\n",
                      __func__, rigerror(retval));

            rig_debug(RIG_DEBUG_WARN, "%s: rig_set_powerstat failed: =%s\n", __func__,
                      rigerror(retval));
            RETURNFUNC(retval);
        }

    }

    RETURNFUNC(RIG_OK);
}

/*
 * Set default when vfo == RIG_VFO_NONE
 * Clients should be setting VFO as 1st things but some don't
 * So they will get defaults of Main/VFOA as the selected VFO
 * and we force that selection
 */
static int icom_set_default_vfo(RIG *rig)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called, curr_vfo=%s\n", __func__,
              rig_strvfo(rig->state.current_vfo));

    // we need to know if dual watch is on
    if (VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: setting default as MAIN/VFOA\n",
                  __func__);
        HAMLIB_TRACE;
        retval = rig_set_vfo(rig, RIG_VFO_MAIN);  // we'll default to Main in this case

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        HAMLIB_TRACE;
        retval = rig_set_vfo(rig, RIG_VFO_A);  // we'll default to Main in this case

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        rig->state.current_vfo = RIG_VFO_MAIN;
        RETURNFUNC2(RIG_OK);
    }

    if (VFO_HAS_MAIN_SUB_ONLY)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: setting default as MAIN\n",
                  __func__);
        HAMLIB_TRACE;
        retval = rig_set_vfo(rig, RIG_VFO_MAIN);  // we'll default to Main in this case
        rig->state.current_vfo = RIG_VFO_MAIN;
    }
    else if (VFO_HAS_A_B)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: setting default as VFOA\n",
                  __func__);
        HAMLIB_TRACE;
        retval = RIG_OK;

        if (rig->state.current_vfo != RIG_VFO_A)
        {
            retval = rig_set_vfo(rig,
                                 RIG_VFO_A);     // we'll default to VFOA for all others
            rig->state.current_vfo = RIG_VFO_A;
        }
    }
    else
    {
        // we don't have any VFO selection
        rig_debug(RIG_DEBUG_TRACE, "%s: Unknown VFO setup so setting default as VFOA\n",
                  __func__);

        rig->state.current_vfo = RIG_VFO_A;
        retval = RIG_OK;
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: curr_vfo now %s\n", __func__,
              rig_strvfo(rig->state.current_vfo));

    RETURNFUNC2(RIG_OK);
}

// return true if band is changing from last set_freq
// Assumes rig is currently on the VFO being changed
// This handles the case case Main/Sub cannot be on the same band
int icom_band_changing(RIG *rig, freq_t test_freq)
{
    freq_t curr_freq, freq1, freq2;
    int retval;

    ENTERFUNC2;

    // We should be sitting on the VFO we want to change so just get it's frequency
    retval = rig_get_freq(rig, RIG_VFO_CURR, &curr_freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freq failed??\n", __func__);
        RETURNFUNC2(0); // I guess we need to say no change in this case
    }

    // Make our HF=0, 2M = 1, 70cm = 4, and 23cm=12
    freq1 = floor(curr_freq / 1e8);
    freq2 = floor(test_freq / 1e8);

    rig_debug(RIG_DEBUG_TRACE, "%s: lastfreq=%.0f, thisfreq=%.0f\n", __func__,
              freq1, freq2);

    if (freq1 != freq2)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Band change detected\n", __func__);
        RETURNFUNC2(1);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Band change not detected\n", __func__);
    RETURNFUNC2(0);
}

static int icom_set_freq_x25(RIG *rig, vfo_t vfo, freq_t freq, int freq_len,
                             unsigned char *freqbuf)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    int retval;

    if (!(rs->targetable_vfo & RIG_TARGETABLE_FREQ) || (priv->x25cmdfails > 0
            && !priv_caps->x25x26_always))
    {
        return -RIG_ENAVAIL;
    }

    int vfo_number = icom_get_vfo_number_x25x26(rig, vfo);
    retval = icom_transaction(rig, C_SEND_SEL_FREQ, vfo_number, freqbuf, freq_len,
                              ackbuf, &ack_len);

    if (priv->x25cmdfails < 0 || priv_caps->x25x26_always)
    {
        priv->x25cmdfails = (retval == RIG_OK) ? 0 : 1;
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        return retval;
    }

    return retval;
}

static int icom_get_freq_x25(RIG *rig, vfo_t vfo, int *ack_len,
                             unsigned char *ackbuf, int *freqbuf_offset)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    int retval;

    // Rigs like IC-7600, IC-7700, IC-7800 and IC-7100 have new firmware that implements commands 0x25 and 0x26
    // So if this succeeds we'll assume all such rigs are targetable freq & mode
    if (!(rs->targetable_vfo & RIG_TARGETABLE_FREQ) || (priv->x25cmdfails > 0
            && !priv_caps->x25x26_always))
    {
        return -RIG_ENAVAIL;
    }

    // Attempt to use the 0x25 command to get selected/unselected or Main/Sub VFO frequency directly
    // For the rigs that indicate selected/unselected VFO frequency, assume current_vfo is accurate to determine what "selected" means
    int vfo_number = icom_get_vfo_number_x25x26(rig, vfo);

    retval = icom_transaction(rig, C_SEND_SEL_FREQ, vfo_number, NULL, 0, ackbuf,
                              ack_len);

    if (retval == RIG_OK)
    {
        *freqbuf_offset = 2;
    }

    if (priv->x25cmdfails < 0 || priv_caps->x25x26_always)
    {
        priv->x25cmdfails = (retval == RIG_OK) ? 0 : 1;
    }

    return retval;
}

static int icom_get_tx_freq(RIG *rig, int *ack_len, unsigned char *ackbuf,
                            int *freqbuf_offset)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    int retval;

    if (priv->x1cx03cmdfails > 0 && !priv_caps->x1cx03_always)
    {
        return -RIG_ENAVAIL;
    }

    retval = icom_transaction(rig, C_CTL_PTT, S_RD_TX_FREQ, NULL, 0, ackbuf,
                              ack_len);

    if (retval == RIG_OK)
    {
        *freqbuf_offset = 2;
    }

    if (priv->x1cx03cmdfails < 0 || priv_caps->x1cx03_always)
    {
        priv->x1cx03cmdfails = (retval == RIG_OK) ? 0 : 1;
    }

    return retval;
}

/*
 * icom_set_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    unsigned char freqbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int freq_len, ack_len = sizeof(ackbuf), retval;
    int check_ack = 0;
    int cmd, subcmd = -1;
    int force_vfo_swap = 0;
    vfo_t vfo_save = rs->current_vfo;
    freq_t curr_freq;

    ENTERFUNC2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s=%" PRIfreq "\n", __func__,
              rig_strvfo(vfo), freq);

    // Icom 0x25 command can only manipulate VFO A/B *or* VFO Main/Sub frequencies.
    // With (usually satellite-capable) rigs that have Main/Sub + A/B for each,
    // Sub receiver frequencies must be manipulated using non-targetable commands.
    if (VFO_HAS_MAIN_SUB_A_B_ONLY && (vfo == RIG_VFO_SUB || vfo == RIG_VFO_SUB_A
                                      || vfo == RIG_VFO_SUB_B))
    {
        force_vfo_swap = 1;
    }

    if (!(rs->targetable_vfo & RIG_TARGETABLE_FREQ) || force_vfo_swap)
    {
        // Switch to the desired VFO (if needed) if frequency is not targetable
        HAMLIB_TRACE;
        rig_debug(RIG_DEBUG_TRACE, "%s: set_vfo_curr=%s\n", __func__,
                  rig_strvfo(rig->state.current_vfo));
        retval = set_vfo_curr(rig, vfo, rig->state.current_vfo);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    retval = rig_get_freq(rig, vfo, &curr_freq);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    freq_len = priv->civ_731_mode ? 4 : 5;

    if (RIG_IS_IC905)
    {
        //  > 5.85GHz is 6 bytes
        if (freq > 5.85e9) { freq_len = 6; }
    }

    to_bcd(freqbuf, freq, freq_len * 2);

    if ((rs->targetable_vfo & RIG_TARGETABLE_FREQ) && !force_vfo_swap)
    {
        cmd = C_SEND_SEL_FREQ;
        subcmd = icom_get_vfo_number_x25x26(rig, vfo);
        retval = icom_set_freq_x25(rig, vfo, freq, freq_len, freqbuf);
    }

    if (!(rs->targetable_vfo & RIG_TARGETABLE_FREQ) || retval == -RIG_ENAVAIL
            || force_vfo_swap)
    {
        cmd = C_SET_FREQ;
        subcmd = -1;

#if 0

        if (CACHE(rig)->ptt && (ICOM_IS_ID5100 || ICOM_IS_ID4100 || ICOM_IS_ID31
                                     || ICOM_IS_ID51))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s(%d): ID55100 0x00\n", __func__, __LINE__);
            // for these rigs 0x00 is setting the freq and 0x03 is just for reading
            cmd = 0x00;
            // temporary fix for ID5100 not giving ACK/NAK on 0x00 freq on E8 firmware
            retval = icom_transaction(rig, cmd, subcmd, freqbuf, freq_len, NULL,
                                      NULL);

            if (retval != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: set_freq failed: %s\n", __func__,
                          rigerror(retval));
                return retval;
            }

            return RIG_OK;
        }
        else
#endif
        {
            retval = icom_transaction(rig, cmd, subcmd, freqbuf, freq_len, ackbuf,
                                      &ack_len);
        }

        int retval2 = set_vfo_curr(rig, vfo_save, rig->state.current_vfo);

        if (retval == RIG_OK)
        {
            retval = retval2;
        }

        check_ack = 1;
    }

    if (retval != RIG_OK)
    {
        // We might have a failed command if we're changing bands
        // For example, IC-9700 setting Sub=VHF when Main=VHF will fail
        // So we'll try a VFO swap and see if that helps things
        rig_debug(RIG_DEBUG_VERBOSE, "%s: special check for vfo swap\n", __func__);

        if (icom_band_changing(rig, freq))
        {
            if (rig_has_vfo_op(rig, RIG_OP_XCHG))
            {
                retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG);

                if (retval != RIG_OK)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: vfo_op XCHG failed: %s\n", __func__,
                              rigerror(retval));
                    RETURNFUNC2(retval);
                }

                // Try the command again
                retval = icom_transaction(rig, cmd, subcmd, freqbuf, freq_len, ackbuf,
                                          &ack_len);

                // Swap back if we got an error otherwise we fall through for more processing
                if (retval != RIG_OK)
                {
                    int retval2;
                    rig_debug(RIG_DEBUG_ERR, "%s: 2nd set freq failed: %s\n", __func__,
                              rigerror(retval));

                    retval2 = icom_vfo_op(rig, vfo, RIG_OP_XCHG);

                    if (retval2 != RIG_OK)
                    {
                        rig_debug(RIG_DEBUG_ERR, "%s: 2nd vfo_op XCHG failed: %s\n", __func__,
                                  rigerror(retval2));
                        RETURNFUNC2(retval2);
                    }

                    RETURNFUNC2(retval);
                }

                check_ack = 1;
            }
        }

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set freq failed: %s\n", __func__,
                      rigerror(retval));
            RETURNFUNC2(retval);
        }
    }

    if (check_ack && (retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    if (priv->tone_enable)
    {
        // IC-705 as of 2024-02-01 turn off TONE on freq change so we turn it back on if enabled
        rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_TONE, 1);
    }

    RETURNFUNC2(RIG_OK);
}

/*
 * icom_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL, Main=VFOA, Sub=VFOB
 * Note: old rig may return less than 4/5 bytes for get_freq
 */
int icom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    unsigned char freqbuf[MAXFRAMELEN];
    int freq_len = sizeof(freqbuf);
    int freqbuf_offset = 1;
    int retval = RIG_OK;
    int civ_731_mode_save = 0;
    int force_vfo_swap = 0;
    vfo_t vfo_save = rig->state.current_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called for %s, curr_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));

    if (priv->serial_USB_echo_off == -1)
    {
        icom_get_usb_echo_off(rig);
    }

    if (vfo == RIG_VFO_MEM && (priv->civ_731_mode || RIG_IS_IC706))
    {
        // Memory channels have always 5-byte frequqncy
        rig_debug(RIG_DEBUG_TRACE, "%s: VFO=MEM so turning off civ_731\n", __func__);
        civ_731_mode_save = 1;
        priv->civ_731_mode = 0;
    }

    // Pick the appropriate VFO when VFO_TX is requested
    if (vfo == RIG_VFO_TX)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: VFO_TX requested, vfo=%s\n", __func__,
                  rig_strvfo(vfo));

        // Attempt to read the transmit frequency directly to avoid VFO swapping
        retval = icom_get_tx_freq(rig, &freq_len, freqbuf, &freqbuf_offset);

        if (retval == RIG_OK)
        {
            *freq = from_bcd(&freqbuf[freqbuf_offset], (priv->civ_731_mode ? 4 : 5) * 2);

            if (vfo == RIG_VFO_MEM && civ_731_mode_save)
            {
                priv->civ_731_mode = 1;
            }

            RETURNFUNC(retval);
        }

        // Fix VFO if the TX freq command is not available
        if (CACHE(rig)->split != RIG_SPLIT_OFF)
        {
            vfo = rs->tx_vfo;
        }
        else
        {
            vfo = rs->current_vfo;
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: using vfo=%s\n", __func__,
              rig_strvfo(vfo));

    // Icom 0x25 command can only manipulate VFO A/B *or* VFO Main/Sub frequencies.
    // With (usually satellite-capable) rigs that have Main/Sub + A/B for each,
    // Sub receiver frequencies must be manipulated using non-targetable commands.
    if (VFO_HAS_MAIN_SUB_A_B_ONLY && (vfo == RIG_VFO_SUB || vfo == RIG_VFO_SUB_A
                                      || vfo == RIG_VFO_SUB_B))
    {
        force_vfo_swap = 1;
    }

    if ((rs->targetable_vfo & RIG_TARGETABLE_FREQ) && !force_vfo_swap)
    {
        retval = icom_get_freq_x25(rig, vfo, &freq_len, freqbuf, &freqbuf_offset);

        if (freq_len == 3 && freqbuf[2] == 0xff)
        { // then we are in VFO mode
            *freq = 0;
            return RIG_OK;
        }
        if (retval == RIG_OK)
        {
            // 0x25 cmd is 1 byte longer than 0x03 cmd
            freq_len--;
        }
    }

    // If the command 0x25 is not supported, swap VFO (if required) and read the frequency
    if (!(rs->targetable_vfo & RIG_TARGETABLE_FREQ) || retval == -RIG_ENAVAIL
            || force_vfo_swap)
    {
        freqbuf_offset = 1;
        HAMLIB_TRACE;
        retval = set_vfo_curr(rig, vfo, rig->state.current_vfo);

        if (retval != RIG_OK)
        {
            if (vfo == RIG_VFO_MEM && civ_731_mode_save)
            {
                priv->civ_731_mode = 1;
            }

            RETURNFUNC(retval);
        }

        retval = icom_transaction(rig, C_RD_FREQ, -1, NULL, 0, freqbuf, &freq_len);
        HAMLIB_TRACE;

        int retval2 = set_vfo_curr(rig, vfo_save, rig->state.current_vfo);

        if (retval == RIG_OK)
        {
            retval = retval2;
        }
    }

    if (retval != RIG_OK)
    {
        if (vfo == RIG_VFO_MEM && civ_731_mode_save)
        {
            priv->civ_731_mode = 1;
        }

        RETURNFUNC(retval);
    }

    /*
     * freqbuf should contain Cn,Data area
     */
    freq_len--;

    /*
     * is it a blank mem channel ?
     */
    if (freq_len == 1 && freqbuf[1] == 0xff)
    {
        *freq = RIG_FREQ_NONE;

        if (vfo == RIG_VFO_MEM && civ_731_mode_save) { priv->civ_731_mode = 1; }

        RETURNFUNC(RIG_OK);
    }

    if (freq_len == 3 && (ICOM_IS_ID5100 || ICOM_IS_ID4100 || ICOM_IS_ID31
                          || ICOM_IS_ID51))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: 3-byte ID5100/4100 length - turn off XONXOFF flow control\n", __func__);
    }
    else if (freq_len != 4 && freq_len != 5 && freq_len != 6)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, freq_len);

        if (vfo == RIG_VFO_MEM && civ_731_mode_save) { priv->civ_731_mode = 1; }

        if (freq_len == 1 && vfo == RIG_VFO_MEM)
        {
            *freq = 0;
            rig_debug(RIG_DEBUG_ERR, "%s: Rig is in MEM mode and MEM channel is empty\n",
                      __func__);
            return -RIG_ETRUNC;
        }

        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (freq_len != 3 && freq_len != 6 && freq_len != (priv->civ_731_mode ? 4 : 5))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: freq len (%d) differs from expected\n",
                  __func__, freq_len);
    }

    *freq = from_bcd(freqbuf + freqbuf_offset, freq_len * 2);

    // 3-byte freq for ID5100 is in 10000Hz units so convert to Hz
    if (freq_len == 3) { *freq *= 10000; }

    if (vfo == RIG_VFO_MEM && civ_731_mode_save) { priv->civ_731_mode = 1; }

    rig_debug(RIG_DEBUG_VERBOSE, "%s exit vfo=%s, curr_vfo=%s, freq=%g\n", __func__,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo), *freq);

    RETURNFUNC2(RIG_OK);
}

int icom_get_rit_new(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    unsigned char tsbuf[MAXFRAMELEN];
    int ts_len, retval;

    retval =
        icom_transaction(rig, C_CTL_RIT, S_RIT_FREQ, NULL, 0, tsbuf, &ts_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    /*
     * tsbuf nibbles should contain 10,1,1000,100 hz digits and 00=+, 01=- bit
     */
    rig_debug(RIG_DEBUG_VERBOSE, "%s: ts_len=%d\n", __func__, ts_len);

    if (ts_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__, ts_len);
        RETURNFUNC2(-RIG_ERJCTED);
    }

    *ts = (shortfreq_t) from_bcd(tsbuf + 2, 4);

    if (tsbuf[4] != 0)
    {
        *ts *= -1;
    }

    RETURNFUNC2(RIG_OK);
}

// The Icom rigs have only one register for both RIT and Delta TX
// you can turn one or both on -- but both end up just being in sync.
static int icom_set_it_new(RIG *rig, vfo_t vfo, shortfreq_t ts, int set_xit)
{
    unsigned char tsbuf[8];
    unsigned char ackbuf[16];
    int ack_len;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ts=%d\n", __func__, (int) ts);

    to_bcd(tsbuf, abs((int) ts), 4);
    // set sign bit
    tsbuf[2] = (ts < 0) ? 1 : 0;

    retval = icom_transaction(rig, C_CTL_RIT, S_RIT_FREQ, tsbuf, 3, ackbuf,
                              &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC2(retval);
}

int icom_set_rit_new(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    RETURNFUNC2(icom_set_it_new(rig, vfo, ts, 0));
}

int icom_set_xit_new(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    RETURNFUNC2(icom_set_it_new(rig, vfo, ts, 1));
}

/* icom_get_dsp_flt
    returns the dsp filter width in hz or 0 if the command is not implemented or error.
    This allows the default parameters to be assigned from the get_mode routine if the command is not implemented.
    Assumes rig != null and the current mode is in mode.

    Has been tested for IC-746pro,  Should work on the all dsp rigs ie pro models.
    The 746 documentation says it has the get_if_filter, but doesn't give any decoding information ? Please test.

   DSP filter setting ($1A$03), but not supported by every rig,
   and some models like IC910/Omni VI Plus have a different meaning for
   this subcommand
*/

int filtericom[] = { 50, 100, 150, 200, 250, 300, 350, 400, 450, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600 };

pbwidth_t icom_get_dsp_flt(RIG *rig, rmode_t mode)
{

    int retval, res_len = 0, rfstatus;
    unsigned char resbuf[MAXFRAMELEN];
    value_t rfwidth;
    unsigned char fw_sub_cmd = RIG_IS_IC7200 ? 0x02 : S_MEM_FILT_WDTH;
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, mode=%s\n", __func__,
              rig_strrmode(mode));

    memset(resbuf, 0, sizeof(resbuf));

    if (rig_has_get_func(rig, RIG_FUNC_RF)
            && (mode & (RIG_MODE_RTTY | RIG_MODE_RTTYR)))
    {
        if (!rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_RF, &rfstatus)
                && (rfstatus))
        {
            retval = rig_get_ext_parm(rig, TOK_RTTY_FLTR, &rfwidth);

            if (retval != RIG_OK || rfwidth.i >= RTTY_FIL_NB)
            {
                return (0);    /* use default */
            }
            else
            {
                return (rtty_fil[rfwidth.i]);
            }
        }
    }

    // TODO: Skip for Xiegu G90 too????
    if (mode == RIG_MODE_FM || mode == RIG_MODE_FMN
            || RIG_MODEL_X108G == rig->caps->rig_model
            || RIG_MODEL_X5105 == rig->caps->rig_model)
    {
        priv->no_1a_03_cmd = ENUM_1A_03_NO;
    }

    if (priv->no_1a_03_cmd == ENUM_1A_03_NO)
    {
        return (0);
    }

    retval = icom_transaction(rig, C_CTL_MEM, fw_sub_cmd, 0, 0,
                              resbuf, &res_len);

    if (-RIG_ERJCTED == retval)
    {
        if (priv->no_1a_03_cmd == ENUM_1A_03_UNK)
        {
            priv->no_1a_03_cmd = ENUM_1A_03_NO;  /* do not keep asking */
            return (RIG_OK);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: 1a 03 cmd failed\n", __func__);
            return (retval);
        }
    }

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), "
                  "len=%d\n", __func__, resbuf[0], res_len);
        return (RIG_OK);        /* use default */
    }

    if (res_len == 3 && resbuf[0] == C_CTL_MEM)
    {
        int i;
        i = (int) from_bcd(resbuf + 2, 2);
        rig_debug(RIG_DEBUG_TRACE, "%s: i=%d, [0]=%02x, [1]=%02x, [2]=%02x, [3]=%02x\n",
                  __func__, i, resbuf[0], resbuf[1], resbuf[2], resbuf[3]);

        if (mode & RIG_MODE_AM)
        {
            if (i > 49)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Expected max 49, got %d for filter\n", __func__,
                          i);
                RETURNFUNC2(-RIG_EPROTO);
            }

            return ((i + 1) * 200); /* All Icoms that we know of */
        }
        else if (mode &
                 (RIG_MODE_CW | RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_RTTY |
                  RIG_MODE_RTTYR | RIG_MODE_PKTUSB | RIG_MODE_PKTLSB))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: using filtericom width=%d\n", __func__, i);
            RETURNFUNC2(filtericom[i]);
        }
    }

    RETURNFUNC2(RIG_OK);
}

int icom_set_dsp_flt(RIG *rig, rmode_t mode, pbwidth_t width)
{
    int retval, rfstatus;
    unsigned char ackbuf[MAXFRAMELEN];
    unsigned char flt_ext;
    value_t rfwidth;
    int ack_len = sizeof(ackbuf), flt_idx;
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;
    unsigned char fw_sub_cmd = RIG_IS_IC7200 ? 0x02 : S_MEM_FILT_WDTH;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: mode=%s, width=%d\n", __func__,
              rig_strrmode(mode), (int)width);


    if (RIG_PASSBAND_NOCHANGE == width)
    {
        RETURNFUNC(RIG_OK);
    }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    // TODO: Rename RIG_FUNC_RF to RIG_FUNC_RTTYFLT ? AMBIGUOUS!
    if (rig_has_get_func(rig, RIG_FUNC_RF)
            && (mode & (RIG_MODE_RTTY | RIG_MODE_RTTYR)))
    {
        if (!rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_RF, &rfstatus)
                && (rfstatus))
        {
            int i;

            for (i = 0; i < RTTY_FIL_NB; i++)
            {
                if (rtty_fil[i] == width)
                {
                    rfwidth.i = i;
                    RETURNFUNC(rig_set_ext_parm(rig, TOK_RTTY_FLTR, rfwidth));
                }
            }

            /* not found */
            RETURNFUNC(-RIG_EINVAL);
        }
    }

    if (priv->no_1a_03_cmd == ENUM_1A_03_NO) { RETURNFUNC(RIG_OK); } // don't bother to try since it doesn't work

    if (mode & RIG_MODE_AM)
    {
        flt_idx = (width / 200) - 1;  /* TBC: IC_7800? */
    }
    else if (mode & (RIG_MODE_CW | RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_RTTY |
                     RIG_MODE_RTTYR | RIG_MODE_PKTUSB | RIG_MODE_PKTLSB))
    {
        if (width == 0)
        {
            width = 1;
        }

        flt_idx =
            width <= 500 ? ((width + 49) / 50) - 1 : ((width + 99) / 100) + 4;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: unknown mode=%s\n", __func__,
                  rig_strrmode(mode));
        RETURNFUNC(RIG_OK);
    }

    to_bcd(&flt_ext, flt_idx, 2);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: flt_ext=%d, flt_idx=%d\n", __func__, flt_ext,
              flt_idx);

    retval = icom_transaction(rig, C_CTL_MEM, fw_sub_cmd, &flt_ext, 1,
                              ackbuf, &ack_len);

    if (-RIG_ERJCTED == retval)
    {
        if (priv->no_1a_03_cmd == ENUM_1A_03_UNK)
        {
            priv->no_1a_03_cmd = ENUM_1A_03_NO;  /* do not keep asking */
            return (RIG_OK);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: 1A 03 %02x failed\n", __func__, flt_ext);
            return (retval);
        }
    }

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), "
                  "len=%d\n", __func__, ackbuf[0], ack_len);
        RETURNFUNC(retval);
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: command not supported ? (%#.2x), "
                  "len=%d\n", __func__, ackbuf[0], ack_len);
        RETURNFUNC(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_mode_without_data
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int icom_set_mode_without_data(RIG *rig, vfo_t vfo, rmode_t mode,
                                      pbwidth_t width)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv =
        (struct icom_priv_data *) rs->priv;
    const struct icom_priv_caps *priv_caps =
        (const struct icom_priv_caps *) rig->caps->priv;
    const struct icom_priv_data *priv_data =
        (const struct icom_priv_data *) rig->state.priv;
    unsigned char ackbuf[MAXFRAMELEN];
    unsigned char icmode;
    signed char icmode_ext;
    int ack_len = sizeof(ackbuf);
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s called vfo=%s, mode=%s, width=%d, current_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int) width,
              rig_strvfo(rs->current_vfo));

    // We can assume here that mode is not targetable and that VFO swapping has been performed

    if (priv_caps->r2i_mode != NULL)
    {
        retval = priv_caps->r2i_mode(rig, vfo, mode, width, &icmode, &icmode_ext);
    }
    else
    {
        retval = rig2icom_mode(rig, vfo, mode, width, &icmode, &icmode_ext);
    }

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error on rig2icom_mode, result=%d\n", __func__,
                  retval);
        RETURNFUNC2(retval);
    }

    if (width == RIG_PASSBAND_NOCHANGE)
    {
        icmode_ext = priv_data->filter;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: icmode=%d, icmode_ext=%d\n", __func__, icmode,
              icmode_ext);

    /* IC-375, IC-731, IC-726,  IC-735, IC-910, IC-7000 don't support passband data */
    /* IC-726 & IC-475A/E also limited support - only on CW */
    /* TODO: G4WJS CW wide/narrow are possible with above two radios */
    if (priv->civ_731_mode || RIG_IS_OS456
            || RIG_IS_IC375
            || RIG_IS_IC726
            || RIG_IS_IC475
            || RIG_IS_IC910
            || RIG_IS_IC7000)
    {
        icmode_ext = -1;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: #2 icmode=%d, icmode_ext=%d\n", __func__,
              icmode, icmode_ext);

    retval = icom_transaction(rig, C_SET_MODE, icmode,
                              (unsigned char *) &icmode_ext,
                              (icmode_ext == -1 ? 0 : 1), ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    icom_set_dsp_flt(rig, mode, width);

    RETURNFUNC2(RIG_OK);
}

static int icom_get_mode_x26(RIG *rig, vfo_t vfo, int *mode_len,
                             unsigned char *modebuf)
{
    struct icom_priv_data *priv = rig->state.priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    int retval;

    if (priv->x26cmdfails > 0 && !priv_caps->x25x26_always)
    {
        return -RIG_ENAVAIL;
    }

    int vfo_number = icom_get_vfo_number_x25x26(rig, vfo);

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s, vfo_number=%d\n", __func__,
              rig_strvfo(vfo), vfo_number);

    retval = icom_transaction(rig, C_SEND_SEL_MODE, vfo_number, NULL, 0, modebuf,
                              mode_len);

    if (priv->x26cmdfails < 0 || priv_caps->x25x26_always)
    {
        priv->x26cmdfails = (retval == RIG_OK) ? 0 : 1;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "%s: mode_len=%d, modebuf=%02x %02x %02x %02x %02x\n", __func__, *mode_len,
              modebuf[0], modebuf[1], modebuf[2], modebuf[3], modebuf[4]);

    priv->filter = modebuf[4];
    return RIG_OK;
}

static int icom_set_mode_x26(RIG *rig, vfo_t vfo, rmode_t mode,
                             rmode_t icom_mode, int datamode,
                             int filter)
{
    struct icom_priv_data *priv = rig->state.priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    int retval;
    unsigned char buf[3];
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    int mode_len;
    unsigned char mode_buf[4];

    if (priv->x26cmdfails > 0 && !priv_caps->x25x26_always)
    {
        return -RIG_ENAVAIL;
    }

    icom_get_mode_x26(rig, vfo, &mode_len, mode_buf);
    buf[0] = icom_mode;
    buf[1] = datamode;
    // Skip filter selection, because at least IC-7300 has a bug defaulting to filter 2 when changing mode
    // Tested on IC-7300 and IC-9700
    buf[2] = priv->filter;
    //rig_debug(RIG_DEBUG_TRACE, "%s: mode=%ld, filters usbd=%d, usb=%d, cw=%d\n",
    //          __func__, mode, priv->filter_usbd, priv->filter_usb, priv->filter_cw);

    if (priv->filter_usbd > 0 && (mode == RIG_MODE_PKTUSB
                                  || mode == RIG_MODE_PKTLSB))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: filter usbd=%d\n", __func__, priv->filter_usbd);
        buf[2] = priv->filter_usbd;
    }

    if (priv->filter_usb > 0 && (mode == RIG_MODE_USB || mode == RIG_MODE_LSB))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: filter usb=%d\n", __func__, priv->filter_usb);
        buf[2] = priv->filter_usb;
    }

    if (priv->filter_cw > 0 && (mode == RIG_MODE_CW || mode == RIG_MODE_CWR))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: filter cw=%d\n", __func__, priv->filter_cw);
        buf[2] = priv->filter_cw;
    }

    int vfo_number = icom_get_vfo_number_x25x26(rig, vfo);

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s, vfo_number=%d\n", __func__,
              rig_strvfo(vfo), vfo_number);

    retval = icom_transaction(rig, C_SEND_SEL_MODE, vfo_number, buf, 3, ackbuf,
                              &ack_len);

    if (priv->x26cmdfails < 0 || priv_caps->x25x26_always)
    {
        priv->x26cmdfails = (retval == RIG_OK) ? 0 : 1;
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * icom_set_mode
 */
int icom_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct rig_state *rs = &rig->state;
    const struct icom_priv_data *priv = rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    int retval;
    rmode_t base_mode;
    rmode_t current_mode;
    pbwidth_t current_width;
    int is_data_mode = 0;
    unsigned char dm_sub_cmd =
        RIG_IS_IC7200  ? 0x04 : S_MEM_DATA_MODE;
    int force_vfo_swap = 0;
    vfo_t vfo_save = rs->current_vfo;

    ENTERFUNC;

    // Icom 0x26 command can only manipulate VFO A/B *or* VFO Main/Sub modes.
    // With (usually satellite-capable) rigs that have Main/Sub + A/B for each,
    // Sub receiver modes must be manipulated using non-targetable commands.
    if (VFO_HAS_MAIN_SUB_A_B_ONLY && (vfo == RIG_VFO_SUB || vfo == RIG_VFO_SUB_A
                                      || vfo == RIG_VFO_SUB_B))
    {
        force_vfo_swap = 1;
    }

    if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE) || force_vfo_swap)
    {
        retval = set_vfo_curr(rig, vfo, rig->state.current_vfo);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    if (!priv_caps->data_mode_supported)
    {
        // Use legacy command to set mode if data mode is not supported
        retval = icom_set_mode_without_data(rig, vfo, mode, width);

        if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE) || force_vfo_swap)
        {
            int retval2 = set_vfo_curr(rig, vfo_save, rig->state.current_vfo);

            if (retval == RIG_OK)
            {
                retval = retval2;
            }
        }

        RETURNFUNC(retval);
    }

    // Do nothing if current mode and width is not changing
    // Reading mode also sets priv->filter to current filter choice
    retval = rig_get_mode(rig, vfo, &current_mode, &current_width);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: get_mode failed: %s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    switch (mode)
    {
    case RIG_MODE_PKTUSB:
        base_mode = RIG_MODE_USB;
        is_data_mode = 1;
        break;

    case RIG_MODE_PKTLSB:
        base_mode = RIG_MODE_LSB;
        is_data_mode = 1;
        break;

    case RIG_MODE_PKTFM:
        base_mode = RIG_MODE_FM;
        is_data_mode = 1;
        break;

    case RIG_MODE_PKTAM:
        base_mode = RIG_MODE_AM;
        is_data_mode = 1;
        break;

    default:
        base_mode = mode;
        break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s mode=%d, width=%d, current_vfo=%s\n", __func__,
              (int) base_mode, (int) width, rig_strvfo(rs->current_vfo));

    // It is only necessary to change base mode if command 0x26 is not supported
    if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE) || force_vfo_swap)
    {
        retval = icom_set_mode_without_data(rig, vfo, base_mode, width);
    }
    else
    {
        retval = RIG_OK;
    }

    if (retval == RIG_OK && mode != current_mode)
    {
        unsigned char datamode[2];
        unsigned char mode_icom; // Not used, we only need the width
        signed char width_icom;

        HAMLIB_TRACE;

        datamode[0] = is_data_mode ? 0x01 : 0x00;
        // Do not change the current filter
        datamode[1] = priv->filter;

        if (priv_caps->r2i_mode != NULL)
        {
            retval = priv_caps->r2i_mode(rig, vfo, mode, width, &mode_icom, &width_icom);
        }
        else
        {
            retval = rig2icom_mode(rig, vfo, mode, width, &mode_icom, &width_icom);
        }

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error on rig2icom_mode, result=%d\n", __func__,
                      retval);
            RETURNFUNC(retval);
        }

        // Check if the filter width byte is needed
        if (priv_caps->mode_with_filter)
        {
            HAMLIB_TRACE;

            if (datamode[0] == 0) { datamode[1] = 0; } // the only good combo possible according to manual

            rig_debug(RIG_DEBUG_TRACE, "%s(%d) mode_icom=%d, datamode=%d, filter=%d\n",
                      __func__, __LINE__, mode_icom, datamode[0], datamode[1]);

            if (force_vfo_swap)
            {
                retval = -RIG_ENAVAIL;
            }
            else
            {
                retval = icom_set_mode_x26(rig, vfo, mode, mode_icom, datamode[0], datamode[1]);
            }

            if (retval != RIG_OK)
            {
                HAMLIB_TRACE;
                retval =
                    icom_transaction(rig, C_CTL_MEM, dm_sub_cmd, datamode, 2, ackbuf, &ack_len);
            }
        }
        else
        {
            HAMLIB_TRACE;
            retval =
                icom_transaction(rig, C_CTL_MEM, dm_sub_cmd, datamode, 1, ackbuf, &ack_len);

            if (retval != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), len=%d\n",
                          __func__, ackbuf[0], ack_len);
            }
            else
            {
                if (ack_len != 1 || ackbuf[0] != ACK)
                {
                    rig_debug(RIG_DEBUG_ERR,
                              "%s: command not supported ? (%#.2x), len=%d\n",
                              __func__, ackbuf[0], ack_len);
                }
            }
        }
    }

    if (((width != RIG_PASSBAND_NOCHANGE) && (width != current_width))
            || (priv->filter_usbd > 0 || priv->filter_usb > 0 || priv->filter_cw > 0))
    {
        icom_set_dsp_flt(rig, mode, width);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: width not changing, keeping filter selection\n",
                  __func__);
    }

    int retval2 = set_vfo_curr(rig, vfo_save, rig->state.current_vfo);

    if (retval == RIG_OK)
    {
        retval = retval2;
    }

    RETURNFUNC(retval);
}

/*
 * icom_get_mode_without_data
 * Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL, width!=NULL
 *
 * TODO: IC-781 doesn't send filter width in wide filter mode, making the frame 1 byte short.
 */
static int icom_get_mode_without_data(RIG *rig, vfo_t vfo, rmode_t *mode,
                                      pbwidth_t *width, int force_vfo_swap)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv_data = rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    unsigned char modebuf[MAXFRAMELEN];
    int mode_len;
    int retval;

    ENTERFUNC2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    *width = 0;

    HAMLIB_TRACE;

    // Use command 0x26 to get selected/unselected or Main/Sub VFO mode, data mode and filter width
    // IC-7800 can set, but not read with 0x26 (although manual states otherwise?)
    if ((rs->targetable_vfo & RIG_TARGETABLE_MODE) && !RIG_IS_IC7800
            && !force_vfo_swap)
    {
        retval = icom_get_mode_x26(rig, vfo, &mode_len, modebuf);

        // mode_len=5, modebuf=26 01 01 01 01
        // last 3 bytes are mode, datamode, filter (1-3)
        priv_data->datamode = modebuf[3];
        priv_data->filter = modebuf[4];
        modebuf[1] = modebuf[2]; // copy mode to 2-byte format
        modebuf[2] = modebuf[4]; // copy filter to 2-byte format
        mode_len = 2;
    }
    else
    {
        retval = icom_transaction(rig, C_RD_MODE, -1, NULL, 0, modebuf, &mode_len);
    }

    if (--mode_len == 3)
    {
        // cppcheck-suppress redundantAssignment
        priv_data->filter = modebuf[2];
        rig_debug(RIG_DEBUG_TRACE,
                  "%s(%d): modebuf[0]=0x%02x, modebuf[1]=0x%02x, modebuf[2]=0x%02x, mode_len=%d, filter=%d\n",
                  __func__, __LINE__, modebuf[0],
                  modebuf[1], modebuf[2], mode_len, priv_data->filter);
    }
    else
    {
        priv_data->filter = 0;

        if (mode_len == 2) { priv_data->filter = modebuf[2]; }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s(%d): modebuf[0]=0x%02x, modebuf[1]=0x%02x, mode_len=%d\n", __func__,
                  __LINE__, modebuf[0],
                  modebuf[1], mode_len);
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    /*
     * modebuf should contain Cn,Data area
     */

    if (mode_len != 2 && mode_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, mode_len);
        RETURNFUNC2(-RIG_ERJCTED);
    }

    if (priv_caps->i2r_mode != NULL)
    {
        priv_caps->i2r_mode(rig, modebuf[1],
                            mode_len == 2 ? modebuf[2] : -1, mode, width);
    }
    else
    {
        icom2rig_mode(rig, modebuf[1],
                      mode_len == 2 ? modebuf[2] : -1, mode, width);
    }

    // The following rigs do not support querying filter width
    if ((RIG_IS_IC910) ||
            (RIG_IS_OMNIVIP) ||
            (RIG_IS_IC706) ||
            (RIG_IS_IC706MKII) ||
            (RIG_IS_IC706MKIIG) ||
            (RIG_IS_IC756) ||
            (RIG_IS_ICR30))
    {
        RETURNFUNC2(RIG_OK);
    }

    /**
     * Most rigs return 1-wide, 2-normal, 3-narrow
     *
     * For DSP rigs these are presets that can be programmed for 30 - 41 bandwidths, depending on mode.
     *
     * The DSP filter width can be read only for the selected VFO, so use cached width for other VFOs.
     */

    pbwidth_t filter_width;

    if (vfo == rig->state.current_vfo)
    {
        filter_width = icom_get_dsp_flt(rig, *mode);
    }
    else
    {
        freq_t freq_cached;
        rmode_t mode_cached;
        pbwidth_t width_cached = 0;
        int cache_ms_freq, cache_ms_mode, cache_ms_width;
        rig_get_cache(rig, vfo, &freq_cached, &cache_ms_freq,
                      &mode_cached, &cache_ms_mode, &width_cached, &cache_ms_width);
        filter_width = width_cached;
    }

    *width = filter_width;

    if (*mode == RIG_MODE_FM) { *width = 12000; }

    RETURNFUNC2(RIG_OK);
}

/*
 * icom_get_mode
 */
int icom_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    unsigned char databuf[MAXFRAMELEN];
    int data_len, retval;
    unsigned char dm_sub_cmd = RIG_IS_IC7200 ? 0x04 : S_MEM_DATA_MODE;
    int force_vfo_swap = 0;
    vfo_t vfo_save = rs->current_vfo;

    ENTERFUNC2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    // Icom 0x26 command can only manipulate VFO A/B *or* VFO Main/Sub modes.
    // With (usually satellite-capable) rigs that have Main/Sub + A/B for each,
    // Sub receiver modes must be manipulated using non-targetable commands.
    if (VFO_HAS_MAIN_SUB_A_B_ONLY && (vfo == RIG_VFO_SUB || vfo == RIG_VFO_SUB_A
                                      || vfo == RIG_VFO_SUB_B))
    {
        force_vfo_swap = 1;
    }

    if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE) || force_vfo_swap)
    {
        retval = set_vfo_curr(rig, vfo, rig->state.current_vfo);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    retval = icom_get_mode_without_data(rig, vfo, mode, width, force_vfo_swap);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s mode=%d\n", __func__, (int) *mode);

    // Do not query data mode state for rigs that do not support them
    if (!priv_caps->data_mode_supported)
    {
        if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE) || force_vfo_swap)
        {
            int retval2 = set_vfo_curr(rig, vfo_save, rig->state.current_vfo);

            if (retval == RIG_OK)
            {
                retval = retval2;
            }
        }

        RETURNFUNC2(retval);
    }

    switch (*mode)
    {
    case RIG_MODE_USB:
    case RIG_MODE_LSB:
    case RIG_MODE_AM:
    case RIG_MODE_FM:

        // Check data mode state for the modes above
        if ((rs->targetable_vfo & RIG_TARGETABLE_MODE) && !force_vfo_swap)
        {
            // The data mode state is already read using command 0x26 for rigs with targetable mode
            // Fake the response in databuf
            databuf[2] = priv->datamode;
            data_len = 3;
        }
        else
        {
            retval =
                icom_transaction(rig, C_CTL_MEM, dm_sub_cmd, 0, 0, databuf,
                                 &data_len);

            if (retval != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), len=%d\n",
                          __func__, databuf[0], data_len);
                RETURNFUNC2(-RIG_ERJCTED);
            }
        }

        /*
         * databuf should contain Cn,Sc,D0[,D1]
         */
        data_len -= 2;

        if (data_len < 1 || data_len > 2)
        {
            /* manual says 1 byte answer
               but at least IC756 ProIII
               sends 2 - second byte
               appears to be same as
               second byte from 04 command
               which is filter preset
               number, whatever it is we
               ignore it */
            rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__,
                      data_len);
            RETURNFUNC2(-RIG_ERJCTED);
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s databuf[2]=%d, mode=%d\n", __func__,
                  (int)databuf[2], (int)*mode);

        // 0x01/0x02/0x03 -> data mode, 0x00 -> not data mode
        if (databuf[2])
        {
            switch (*mode)
            {
            case RIG_MODE_USB:
                *mode = RIG_MODE_PKTUSB;
                break;

            case RIG_MODE_LSB:
                *mode = RIG_MODE_PKTLSB;
                break;

            case RIG_MODE_AM:
                *mode = RIG_MODE_PKTAM;
                break;

            case RIG_MODE_FM:
                *mode = RIG_MODE_PKTFM;
                break;

            default:
                break;
            }
        }

    default:
        break;
    }

    if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE) || force_vfo_swap)
    {
        int retval2 = set_vfo_curr(rig, vfo_save, rig->state.current_vfo);

        if (retval == RIG_OK)
        {
            retval = retval2;
        }
    }

    RETURNFUNC2(retval);
}

/*
 * icom_get_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Only some recent Icom rigs support reading the selected band (between Main/Sub).
 * Even then, they cannot distinguish between VFO A/B.
 *
 * Supported rigs so far: IC-7600 (firmware version 2.0+), IC-7610, IC-7800 (firmware version 3.1+), IC-785x
 * While IC-9700 supports the command too, it provides Main A/B and Sub A/B VFOs too, where A/B selection
 * cannot be detected.
 */
int icom_get_vfo(RIG *rig, vfo_t *vfo)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;

    // TODO: Detect if the command is available for IC-7600 and IC-7800
    // -> If not, return cached or -RIG_ENAVAIL?

    retval = icom_transaction(rig, C_SET_VFO, S_BAND_SEL, NULL, 0, ackbuf,
                              &ack_len);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error reading receiver/band selection: %s\n",
                  __func__, rigerror(retval));
        RETURNFUNC(retval);
    }

    if (ackbuf[2] == 0)
    {
        *vfo = RIG_VFO_MAIN;
    }
    else
    {
        *vfo = RIG_VFO_SUB;
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_vfo(RIG *rig, vfo_t vfo)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), icvfo, retval;
    struct rig_state *rs = STATE(rig);
    struct rig_cache *cachep = CACHE(rig);
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    if (vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Asking for currVFO,  currVFO=%s\n", __func__,
                  rig_strvfo(rig->state.current_vfo));
        RETURNFUNC2(RIG_OK);
    }

    if (vfo == RIG_VFO_MAIN && VFO_HAS_A_B_ONLY)
    {
        vfo = RIG_VFO_A;
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Rig does not have MAIN/SUB so Main changed to %s\n",
                  __func__, rig_strvfo(vfo));
    }
    else if ((vfo == RIG_VFO_SUB) &&
             (VFO_HAS_A_B_ONLY || (VFO_HAS_MAIN_SUB_A_B_ONLY
                                   && cachep->split == RIG_SPLIT_OFF && !cachep->satmode)))
    {
        // if rig doesn't have Main/Sub
        // or if rig has both Main/Sub and A/B -- e.g. 9700
        // and we don't have split or satmode turned on
        // then we don't use Sub -- instead we use Main/VFOB
        vfo = RIG_VFO_B;
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Rig does not have MAIN/SUB so Sub changed to %s\n",
                  __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_TX)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo line#%d vfo=%s\n", __func__, __LINE__,
                  rig_strvfo(vfo));
        vfo = RIG_VFO_A;

        if (VFO_HAS_A_B_ONLY && cachep->satmode) { vfo = RIG_VFO_B; }
        else if (VFO_HAS_MAIN_SUB_ONLY) { vfo = RIG_VFO_SUB; }
        else if (VFO_HAS_MAIN_SUB_A_B_ONLY && cachep->satmode) { vfo = RIG_VFO_SUB; }
    }

    else if ((vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN) && VFO_HAS_DUAL)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo line#%d vfo=%s, split=%d\n", __func__,
                  __LINE__, rig_strvfo(vfo), cachep->split);
        // If we're being asked for A/Main but we are a MainA/MainB rig change it
        vfo = RIG_VFO_MAIN;

        if (cachep->split == RIG_SPLIT_ON && !cachep->satmode) { vfo = RIG_VFO_A; }

        // Seems the IC821H reverses Main/Sub when in satmode
        if (RIG_IS_IC821H && cachep->satmode) { vfo = RIG_VFO_SUB; }
    }
    else if ((vfo == RIG_VFO_B || vfo == RIG_VFO_SUB) && VFO_HAS_DUAL)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo line#%d vfo=%s\n", __func__, __LINE__,
                  rig_strvfo(vfo));
        // If we're being asked for B/Sub but we are a MainA/MainB rig change it
        vfo = RIG_VFO_SUB;

        // If we're in satmode for rigs like IC9700 we want the 2nd VFO
        if (cachep->satmode)
        {
            vfo = RIG_VFO_SUB_A;
        }
        else if (cachep->split == RIG_SPLIT_ON) { vfo = RIG_VFO_B; }

        // Seems the IC821H reverses Main/Sub when in satmode
        if (RIG_IS_IC821H && cachep->satmode) { vfo = RIG_VFO_MAIN; }
    }
    else if ((vfo == RIG_VFO_A || vfo == RIG_VFO_B) && !VFO_HAS_A_B
             && VFO_HAS_MAIN_SUB)
    {
        // If we're being asked for A/B but we are a Main/Sub rig change it
        vfo_t vfo_old = vfo;
        vfo = vfo == RIG_VFO_A ? RIG_VFO_MAIN : RIG_VFO_SUB;
        rig_debug(RIG_DEBUG_ERR, "%s: Rig does not have VFO A/B?\n", __func__);
        rig_debug(RIG_DEBUG_ERR, "%s: Mapping %s=%s\n", __func__, rig_strvfo(vfo_old),
                  rig_strvfo(vfo));
    }


    if ((vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB) && !VFO_HAS_MAIN_SUB)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Rig does not have VFO Main/Sub?\n",
                  __func__);
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (vfo != rig->state.current_vfo)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: VFO changing from %s to %s\n", __func__,
                  rig_strvfo(rig->state.current_vfo), rig_strvfo(vfo));
        cachep->freqCurr = 0; // reset current frequency so set_freq works 1st time
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: line#%d\n", __func__, __LINE__);

    switch (vfo)
    {
    case RIG_VFO_A:
        icvfo = S_VFOA;
        break;

    case RIG_VFO_B:
        icvfo = S_VFOB;
        break;

    case RIG_VFO_MAIN:
        icvfo = S_MAIN;

        // If not split or satmode then we must want VFOA
        if (VFO_HAS_MAIN_SUB_A_B_ONLY && cachep->split == RIG_SPLIT_OFF
                && !cachep->satmode) { icvfo = S_VFOA; }

        rig_debug(RIG_DEBUG_TRACE, "%s: Main asked for, ended up with vfo=%s\n",
                  __func__, icvfo == S_MAIN ? "Main" : "VFOA");

        break;

    case RIG_VFO_SUB:
        icvfo = S_SUB;

        // If split is on these rigs can only split on Main/VFOB
        if (VFO_HAS_MAIN_SUB_A_B_ONLY && cachep->split != RIG_SPLIT_OFF) { icvfo = S_VFOB; }

        // If not split or satmode then we must want VFOB
        if (VFO_HAS_MAIN_SUB_A_B_ONLY && cachep->split == RIG_SPLIT_OFF
                && !cachep->satmode) { icvfo = S_VFOB; }

        rig_debug(RIG_DEBUG_TRACE, "%s: Sub asked for, ended up with vfo=%s\n",
                  __func__, icvfo == S_SUB ? "Sub" : "VFOB");

        break;

    case RIG_VFO_TX:
        icvfo = (cachep->split != RIG_SPLIT_OFF) ? S_VFOB : S_VFOA;
        vfo = (cachep->split != RIG_SPLIT_OFF) ? RIG_VFO_B : RIG_VFO_A;
        rig_debug(RIG_DEBUG_TRACE, "%s: RIG_VFO_TX changing vfo to %s\n", __func__,
                  rig_strvfo(vfo));
        break;

    case RIG_VFO_VFO:
        retval = icom_transaction(rig, C_SET_VFO, -1, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        rig->state.current_vfo = vfo;
        RETURNFUNC2(RIG_OK);

    case RIG_VFO_MEM:
        retval = icom_transaction(rig, C_SET_MEM, -1, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        rig->state.current_vfo = vfo;
        RETURNFUNC2(RIG_OK);

    case RIG_VFO_MAIN_A:    // we need to select Main before setting VFO
    case RIG_VFO_MAIN_B:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: MainA/B logic\n", __func__);
        retval = icom_transaction(rig, C_SET_VFO, S_MAIN, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        icvfo = vfo == RIG_VFO_MAIN_A ? S_VFOA : S_VFOB;

        break;

    case RIG_VFO_SUB_A: // we need to select Sub before setting VFO
    case RIG_VFO_SUB_B:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: SubA/B logic\n", __func__);
        retval = icom_transaction(rig, C_SET_VFO, S_SUB, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        // If SUB_A then we'll assume we're done and probably not in sat mode
        // If rig has SUB_B active this may be a problem
        if (vfo == RIG_VFO_SUB_A) { return RIG_OK; }

        icvfo = vfo == RIG_VFO_SUB_A ? S_VFOA : S_VFOB;

        break;

    case RIG_VFO_OTHER:
        switch (rig->state.current_vfo)
        {
        case RIG_VFO_CURR:
            break; // no change needed

        case RIG_VFO_A:
            vfo = RIG_VFO_B;
            icvfo = S_VFOB;
            break;

        case RIG_VFO_B:
            vfo = RIG_VFO_A;
            icvfo = S_VFOA;
            break;

        case RIG_VFO_MAIN:
            vfo = RIG_VFO_SUB;
            icvfo = S_SUB;
            break;

        case RIG_VFO_SUB:
            vfo = RIG_VFO_MAIN;
            icvfo = S_MAIN;
            break;

        case RIG_VFO_MAIN_A:
            vfo = RIG_VFO_MAIN_B;
            icvfo = S_MAIN;
            // TODO: Select also VFOB?
            break;

        case RIG_VFO_MAIN_B:
            vfo = RIG_VFO_MAIN_A;
            icvfo = S_MAIN;
            // TODO: Select also VFOA?
            break;

        case RIG_VFO_SUB_A:
            vfo = RIG_VFO_SUB_B;
            icvfo = S_SUB;
            // TODO: Select also VFOB?
            break;

        case RIG_VFO_SUB_B:
            vfo = RIG_VFO_SUB_A;
            icvfo = S_SUB;
            // TODO: Select also VFOA?
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unknown vfo '%s'\n", __func__,
                      rig_strvfo(rig->state.current_vfo));
        }

    default:
        if (priv->x25cmdfails == 0 || priv_caps->x25x26_always)
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                      rig_strvfo(vfo));

        RETURNFUNC2(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: line#%d\n", __func__, __LINE__);
    retval = icom_transaction(rig, C_SET_VFO, icvfo, NULL, 0,
                              ackbuf, &ack_len);
    rig_debug(RIG_DEBUG_TRACE, "%s: line#%d\n", __func__, __LINE__);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    rig->state.current_vfo = vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: line#%d curr_vfo=%s\n", __func__, __LINE__,
              rig_strvfo(rig->state.current_vfo));
    RETURNFUNC2(RIG_OK);
}

int icom_set_cmd(RIG *rig, vfo_t vfo, struct cmdparams *par, value_t val)
{
    ENTERFUNC;

    unsigned char cmdbuf[MAXFRAMELEN];
    int cmdlen = 0;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = 0;

    if (!(par->submod & SC_MOD_WR)) { RETURNFUNC(-RIG_EINVAL); }

    if ((par->submod & SC_MOD_RW12) == SC_MOD_RW12)
    {
        cmdbuf[0] = 0x01;
        cmdlen = 1;
    }
    else
    {
        cmdlen = par->sublen;
        memcpy(cmdbuf, par->subext, cmdlen);
    }

    int wrd = val.i;
    int i;

    switch (par->dattyp)
    {
    case CMD_DAT_WRD:
        for (i = 1; i <= par->datlen; i++)
        {
            cmdbuf[cmdlen + par->datlen - i] = wrd & 0xff;
            wrd >>= 8;
        }

        break;

    case CMD_DAT_BUF:
        memcpy(&cmdbuf[cmdlen], val.b.d, par->datlen);
        break;

    case CMD_DAT_INT:
    case CMD_DAT_BOL:
        to_bcd_be(&cmdbuf[cmdlen], val.i, (par->datlen * 2));
        break;

    case CMD_DAT_FLT:
        to_bcd_be(&cmdbuf[cmdlen], (int) val.f, (par->datlen * 2));
        break;

    case CMD_DAT_LVL:
        to_bcd_be(&cmdbuf[cmdlen], (int)(val.f * 255.0), (par->datlen * 2));
        break;

    case CMD_DAT_TIM: // returned as seconds since midnight
        to_bcd_be(&cmdbuf[cmdlen],
                  ((((int)val.i / 3600) * 100) + (((int)val.i / 60) % 60)), (par->datlen * 2));
        break;

    default:
        break;
    }

    cmdlen += par->datlen;
    RETURNFUNC(icom_transaction(rig, par->command, par->subcmd, cmdbuf, cmdlen,
                                ackbuf,
                                &ack_len));
}

int icom_get_cmd(RIG *rig, vfo_t vfo, struct cmdparams *par, value_t *val)
{

    ENTERFUNC;

    unsigned char ssc = 0x02;
    unsigned char resbuf[MAXFRAMELEN];
    int reslen = sizeof(resbuf);
    int retval;

    if (!(par->submod & SC_MOD_RD)) { RETURNFUNC(-RIG_EINVAL); }

    if ((par->submod & SC_MOD_RW12) == SC_MOD_RW12)
    {
        retval = icom_get_raw_buf(rig, par->command, par->subcmd, 1, &ssc, &reslen,
                                  resbuf);
    }
    else
    {
        retval = icom_get_raw_buf(rig, par->command, par->subcmd,
                                  par->sublen, (unsigned char *)par->subext, &reslen, resbuf);
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    switch (par->dattyp)
    {
    case CMD_DAT_WRD:
    {
        int wrd = 0;
        int i;

        for (i = 0; i < par->datlen; i++)
        {
            wrd = (wrd << 8) + resbuf[i];
        }

        val->i = wrd;
    }
    break;

    case CMD_DAT_STR:
        if (strlen(val->s) < reslen)
        {
            RETURNFUNC(-RIG_EINTERNAL);
        }

        memcpy(val->s, resbuf, reslen);
        val->s[reslen] = 0;
        break;

    case CMD_DAT_BUF:
        if (reslen > val->b.l)
        {
            RETURNFUNC(-RIG_EINTERNAL);
        }

        memcpy(val->b.d, resbuf, reslen);
        val->b.l = reslen;
        break;

    case CMD_DAT_INT:
        val->i = from_bcd_be(resbuf, (reslen * 2));
        break;

    case CMD_DAT_FLT:
        val->f = (float) from_bcd_be(resbuf, (reslen * 2));
        break;

    case CMD_DAT_LVL:
        val->f = (float) from_bcd_be(resbuf, (reslen * 2)) / 255.0;
        break;

    case CMD_DAT_BOL:
        val->i = (from_bcd_be(resbuf, (reslen * 2)) == 0) ? 0 : 1;
        break;

    case CMD_DAT_TIM:
        val->i = (from_bcd_be(resbuf, 2) * 3600) + (from_bcd_be(&resbuf[1], 2) * 60);
        break;

    default:
        val->i = 0;
        break;
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct rig_state *rs;
    unsigned char cmdbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int cmd_len, ack_len = sizeof(ackbuf);
    int lvl_cn, lvl_sc;       /* Command Number, Subcommand */
    int icom_val;
    int i, retval;
    const struct icom_priv_caps *priv_caps =
        (const struct icom_priv_caps *) rig->caps->priv;

    ENTERFUNC;

    const struct cmdparams *extcmds = priv_caps->extcmds;

    for (i = 0; extcmds && extcmds[i].id.s != 0; i++)
    {
        if (extcmds[i].cmdparamtype == CMD_PARAM_TYPE_LEVEL && extcmds[i].id.s == level)
        {
            RETURNFUNC(icom_set_cmd(rig, vfo, (struct cmdparams *)&extcmds[i], val));
        }
    }

    rs = &rig->state;

    /*
     * Many levels of float type are in [0.0..1.0] range
     */
    if (RIG_LEVEL_IS_FLOAT(level))
    {
        icom_val = val.f * 255;
    }
    else
    {
        icom_val = val.i;
    }

    /* convert values to 0 .. 255 range */
    if (RIG_IS_ICR75)
    {
        switch (level)
        {
        case RIG_LEVEL_NR:
            icom_val = val.f * 240;
            break;

        case RIG_LEVEL_PBT_IN:
        case RIG_LEVEL_PBT_OUT:
            icom_val = (val.f / 10.0) + 128;

            if (icom_val > 255)
            {
                icom_val = 255;
            }

            break;

        default:
            break;
        }
    }

    switch (level)
    {
    case RIG_LEVEL_KEYSPD:
        if (val.i < 6)
        {
            icom_val = 6;
        }
        else if (val.i > 48)
        {
            icom_val = 48;
        }

        icom_val = (int) lroundf(((float) icom_val - 6.0f) * (255.0f / 42.0f));
        break;

    case RIG_LEVEL_CWPITCH:
        if (val.i < 300)
        {
            icom_val = 300;
        }
        else if (val.i >= 900)
        {
            icom_val = 900;
        }

        icom_val = (int) lroundf(((float) icom_val - 300) * (255.0f / 600.0f));
        break;

    default:
        break;
    }

    /*
     * Most of the time, the data field is a 3 digit BCD,
     * but in *big endian* order: 0000..0255
     * (from_bcd is little endian)
     */
    cmd_len = 2;
    to_bcd_be(cmdbuf, (long long) icom_val, cmd_len * 2);

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        lvl_cn = C_CTL_FUNC;
        lvl_sc = S_FUNC_PAMP;
        cmd_len = 1;

        if (val.i == 0)
        {
            cmdbuf[0] = 0;    /* 0=OFF */
            break;
        }

        for (i = 0; i < HAMLIB_MAXDBLSTSIZ; i++)
        {
            if (rs->preamp[i] == val.i)
            {
                break;
            }
        }

        if (i == HAMLIB_MAXDBLSTSIZ || rs->preamp[i] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported preamp set_level %ddB\n",
                      __func__, val.i);
            RETURNFUNC(-RIG_EINVAL);
        }

        cmdbuf[0] = i + 1;    /* 1=P.AMP1, 2=P.AMP2 */
        break;

    case RIG_LEVEL_ATT:
        lvl_cn = C_CTL_ATT;
        /* attenuator level is dB, in BCD mode */
        lvl_sc = (val.i / 10) << 4 | (val.i % 10);
        cmd_len = 0;
        break;

    case RIG_LEVEL_AF:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_AF;
        break;

    case RIG_LEVEL_RF:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_RF;
        break;

    case RIG_LEVEL_SQL:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_SQL;
        break;

    case RIG_LEVEL_IF:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_IF;
        break;

    case RIG_LEVEL_APF:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_APF;
        break;

    case RIG_LEVEL_NR:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_NR;
        break;

    case RIG_LEVEL_NB:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_NB;
        break;

    case RIG_LEVEL_PBT_IN:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_PBTIN;
        break;

    case RIG_LEVEL_PBT_OUT:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_PBTOUT;
        break;

    case RIG_LEVEL_CWPITCH:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_CWPITCH;

        /* use 'set mode' call for CWPITCH on IC-R75 */
        if (RIG_IS_ICR75)
        {
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_MODE_SLCT;
            cmd_len = 3;
            cmdbuf[0] = S_PRM_CWPITCH;
            to_bcd_be(cmdbuf + 1, (long long) icom_val, 4);
        }

        break;

    case RIG_LEVEL_RFPOWER:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_RFPOWER;
        break;

    case RIG_LEVEL_MICGAIN:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_MICGAIN;
        break;

    case RIG_LEVEL_KEYSPD:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_KEYSPD;
        break;

    case RIG_LEVEL_NOTCHF_RAW:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_NOTCHF;
        break;

    case RIG_LEVEL_COMP:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_COMP;
        break;

    case RIG_LEVEL_AGC_TIME:
        lvl_cn = C_CTL_MEM;
        lvl_sc = 0x04;
        cmd_len = 1;
        {
            icom_val = 0;
            const float *agcp = agc_level;

            if (rig->state.current_mode == RIG_MODE_AM) { agcp = agc_level2; }

            rig_debug(RIG_DEBUG_ERR, "%s: val.f=%f\n", __func__, val.f);

            for (i = 0; i <= 13; ++i)
            {
                if (agcp[i] <= val.f)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: agcp=%f <= val.f=%f at %d\n", __func__, agcp[i],
                              val.f, i);
                    icom_val = i;
                }
            }

            cmdbuf[0] = icom_val;
        }
        break;

    case RIG_LEVEL_AGC:
        lvl_cn = C_CTL_FUNC;
        lvl_sc = S_FUNC_AGC;
        cmd_len = 1;

        if (priv_caps->agc_levels_present)
        {
            int found = 0;

            for (i = 0;
                    i <= HAMLIB_MAX_AGC_LEVELS
                    && priv_caps->agc_levels[i].level != RIG_AGC_LAST; i++)
            {
                if (priv_caps->agc_levels[i].level == val.i)
                {
                    cmdbuf[0] = priv_caps->agc_levels[i].icom_level;
                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                RETURNFUNC(-RIG_EINVAL);
            }
        }
        else
        {
            // Legacy mapping that does not apply to all rigs
            switch (val.i)
            {
            case RIG_AGC_SLOW:
                cmdbuf[0] = D_AGC_SLOW;
                break;

            case RIG_AGC_MEDIUM:
                cmdbuf[0] = D_AGC_MID;
                break;

            case RIG_AGC_FAST:
                cmdbuf[0] = D_AGC_FAST;
                break;

            case RIG_AGC_SUPERFAST:
                cmdbuf[0] = D_AGC_SUPERFAST;
                break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unsupported LEVEL_AGC %d\n",
                          __func__, val.i);
                RETURNFUNC(-RIG_EINVAL);
            }
        }

        break;

    case RIG_LEVEL_BKINDL:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_BKINDL;
        break;

    case RIG_LEVEL_BALANCE:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_BALANCE;
        break;

    case RIG_LEVEL_VOXGAIN:
        if (RIG_IS_IC910)
        {
            /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_VOXGAIN;
        }
        else
        {
            lvl_cn = C_CTL_LVL;
            lvl_sc = S_LVL_VOXGAIN;
        }

        break;

    case RIG_LEVEL_ANTIVOX:
        if (RIG_IS_IC910)
        {
            /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_ANTIVOX;
        }
        else
        {
            lvl_cn = C_CTL_LVL;
            lvl_sc = S_LVL_ANTIVOX;
        }

        break;

    case RIG_LEVEL_MONITOR_GAIN:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_MON;
        break;

    case RIG_LEVEL_SPECTRUM_MODE:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_MOD;
        cmd_len = 2;

        switch (val.i)
        {
        case RIG_SPECTRUM_MODE_CENTER:
            icom_val = SCOPE_MODE_CENTER;
            break;

        case RIG_SPECTRUM_MODE_FIXED:
            icom_val = SCOPE_MODE_FIXED;
            break;

        case RIG_SPECTRUM_MODE_CENTER_SCROLL:
            icom_val = SCOPE_MODE_SCROLL_C;
            break;

        case RIG_SPECTRUM_MODE_FIXED_SCROLL:
            icom_val = SCOPE_MODE_SCROLL_F;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported spectrum mode %d\n", __func__, val.i);
            RETURNFUNC(-RIG_EINVAL);
        }

        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        cmdbuf[1] = icom_val;
        break;

    case RIG_LEVEL_SPECTRUM_SPAN:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_SPN;
        cmd_len = 6;

        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        // Spectrum span is represented as a +/- value for Icom rigs
        to_bcd(cmdbuf + 1, val.i / 2, 5 * 2);
        break;

    case RIG_LEVEL_SPECTRUM_SPEED:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_SWP;
        cmd_len = 2;

        if (val.i < 0)
        {
            val.i = 0;
        }
        else if (val.i > 2)
        {
            val.i = 2;
        }

        switch (val.i)
        {
        case 0:
            icom_val = SCOPE_SPEED_SLOW;
            break;

        case 1:
            icom_val = SCOPE_SPEED_MID;
            break;

        case 2:
            icom_val = SCOPE_SPEED_FAST;
            break;
        }

        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        cmdbuf[1] = icom_val;
        break;

    case RIG_LEVEL_SPECTRUM_REF:
    {
        float icom_db = (roundf(val.f * 2.0f) / 2.0f) * 100.0f;

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_REF;
        cmd_len = 4;

        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);

        // Spectrum reference level is represented at 0.01dB accuracy, but needs to be rounded to nearest 0.5dB
        to_bcd_be(cmdbuf + 1, abs((int) icom_db), 2 * 2);

        // Sign
        cmdbuf[3] = (icom_db < 0) ? 1 : 0;
        break;
    }

    case RIG_LEVEL_SPECTRUM_EDGE_LOW:
    case RIG_LEVEL_SPECTRUM_EDGE_HIGH:
    {
        int range_id;
        value_t edge_number_value;
        value_t opposite_edge_value;
        setting_t level_opposite_edge =
            (level == RIG_LEVEL_SPECTRUM_EDGE_LOW) ?
            RIG_LEVEL_SPECTRUM_EDGE_HIGH : RIG_LEVEL_SPECTRUM_EDGE_LOW;

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_FEF;
        cmd_len = 12;

        // Modify the frequency range currently active
        retval = icom_get_spectrum_edge_frequency_range(rig, vfo, &range_id);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error getting spectrum edge frequency range\n",
                      __func__);
            RETURNFUNC(retval);
        }

        // Modify the edge number currently active
        retval = icom_get_ext_level(rig, vfo, TOK_SCOPE_EDG, &edge_number_value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        // Get the current opposite edge frequency
        retval = icom_get_level(rig, vfo, level_opposite_edge, &opposite_edge_value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        to_bcd(cmdbuf, range_id, 1 * 2);
        to_bcd(cmdbuf + 1, edge_number_value.i + 1, 1 * 2);

        if (level == RIG_LEVEL_SPECTRUM_EDGE_LOW)
        {
            to_bcd(cmdbuf + 2, val.i, 5 * 2);
            to_bcd(cmdbuf + 7, opposite_edge_value.i, 5 * 2);
        }
        else
        {
            to_bcd(cmdbuf + 2, opposite_edge_value.i, 5 * 2);
            to_bcd(cmdbuf + 7, val.i, 5 * 2);
        }

        break;
    }

    case RIG_LEVEL_SPECTRUM_ATT:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_ATT;
        cmd_len = 2;

        for (i = 0; i < HAMLIB_MAXDBLSTSIZ; i++)
        {
            if (rig->caps->spectrum_attenuator[i] == val.i)
            {
                break;
            }
        }

        if (val.i != 0 && (i == HAMLIB_MAXDBLSTSIZ
                           || rig->caps->spectrum_attenuator[i] == 0))
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported spectrum attenuator level %ddB\n",
                      __func__, val.i);
            RETURNFUNC(-RIG_EINVAL);
        }

        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        to_bcd(cmdbuf + 1, val.i, 5 * 2);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = icom_transaction(rig, lvl_cn, lvl_sc, cmdbuf, cmd_len, ackbuf,
                              &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 *
 */
int icom_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct rig_state *rs;
    unsigned char cmdbuf[MAXFRAMELEN], respbuf[MAXFRAMELEN];
    int cmd_len, resp_len;
    int lvl_cn, lvl_sc;       /* Command Number, Subcommand */
    int icom_val;
    int cmdhead;
    int retval;
    const struct icom_priv_caps *priv_caps =
        (const struct icom_priv_caps *) rig->caps->priv;

    ENTERFUNC;

    const struct cmdparams *extcmds = priv_caps->extcmds;
    int i;

    for (i = 0; extcmds && extcmds[i].id.s != 0; i++)
    {
        //rig_debug(RIG_DEBUG_TRACE, "%s: i=%d\n", __func__, i);

        if (extcmds[i].cmdparamtype == CMD_PARAM_TYPE_LEVEL && extcmds[i].id.s == level)
        {
            RETURNFUNC(icom_get_cmd(rig, vfo, (struct cmdparams *)&extcmds[i], val));
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: no extcmd found\n", __func__);

    rs = &rig->state;

    cmd_len = 0;

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
    case RIG_LEVEL_RAWSTR:
        lvl_cn = C_RD_SQSM;
        lvl_sc = S_SML;
        break;

    case RIG_LEVEL_ALC:
        lvl_cn = C_RD_SQSM;
        lvl_sc = S_ALC;
        break;

    case RIG_LEVEL_SWR:
        lvl_cn = C_RD_SQSM;
        lvl_sc = S_SWR;
        break;

    case RIG_LEVEL_RFPOWER_METER:
    case RIG_LEVEL_RFPOWER_METER_WATTS:
        lvl_cn = C_RD_SQSM;
        lvl_sc = S_RFML;
        break;

    case RIG_LEVEL_COMP_METER:
        lvl_cn = C_RD_SQSM;
        lvl_sc = S_CMP;
        break;

    case RIG_LEVEL_VD_METER:
        lvl_cn = C_RD_SQSM;
        lvl_sc = S_VD;
        break;

    case RIG_LEVEL_ID_METER:
        lvl_cn = C_RD_SQSM;
        lvl_sc = S_ID;
        break;

    case RIG_LEVEL_PREAMP:
        lvl_cn = C_CTL_FUNC;
        lvl_sc = S_FUNC_PAMP;
        break;

    case RIG_LEVEL_ATT:
        lvl_cn = C_CTL_ATT;
        lvl_sc = -1;
        break;

    case RIG_LEVEL_AF:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_AF;
        break;

    case RIG_LEVEL_RF:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_RF;
        break;

    case RIG_LEVEL_SQL:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_SQL;
        break;

    case RIG_LEVEL_IF:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_IF;
        break;

    case RIG_LEVEL_APF:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_APF;
        break;

    case RIG_LEVEL_NR:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_NR;
        break;

    case RIG_LEVEL_NB:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_NB;
        break;

    case RIG_LEVEL_PBT_IN:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_PBTIN;
        break;

    case RIG_LEVEL_PBT_OUT:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_PBTOUT;
        break;

    case RIG_LEVEL_CWPITCH:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_CWPITCH;

        /* use 'set mode' call for CWPITCH on IC-R75 */
        if (RIG_IS_ICR75)
        {
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_MODE_SLCT;
            cmd_len = 1;
            cmdbuf[0] = S_PRM_CWPITCH;
        }

        break;

    case RIG_LEVEL_RFPOWER:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_RFPOWER;
        break;

    case RIG_LEVEL_MICGAIN:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_MICGAIN;
        break;

    case RIG_LEVEL_KEYSPD:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_KEYSPD;
        break;

    case RIG_LEVEL_NOTCHF_RAW:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_NOTCHF;
        break;

    case RIG_LEVEL_COMP:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_COMP;
        break;

    case RIG_LEVEL_AGC:
        lvl_cn = C_CTL_FUNC;
        lvl_sc = S_FUNC_AGC;
        break;

    case RIG_LEVEL_BKINDL:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_BKINDL;
        break;

    case RIG_LEVEL_BALANCE:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_BALANCE;
        break;

    case RIG_LEVEL_VOXGAIN: /* IC-910H */
        if (RIG_IS_IC910)
        {
            /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_VOXGAIN;
        }
        else
        {
            lvl_cn = C_CTL_LVL;
            lvl_sc = S_LVL_VOXGAIN;
        }

        break;

    case RIG_LEVEL_ANTIVOX:
        if (RIG_IS_IC910)
        {
            /* IC-910H */
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_ANTIVOX;
        }
        else
        {
            lvl_cn = C_CTL_LVL;
            lvl_sc = S_LVL_ANTIVOX;
        }

        break;

    case RIG_LEVEL_MONITOR_GAIN:
        lvl_cn = C_CTL_LVL;
        lvl_sc = S_LVL_MON;
        break;

    case RIG_LEVEL_SPECTRUM_MODE:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_MOD;
        cmd_len = 1;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        break;

    case RIG_LEVEL_SPECTRUM_SPAN:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_SPN;

        cmd_len = 1;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        break;

    case RIG_LEVEL_SPECTRUM_SPEED:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_SWP;

        cmd_len = 1;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        break;

    case RIG_LEVEL_SPECTRUM_REF:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_REF;

        cmd_len = 1;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        break;

    case RIG_LEVEL_SPECTRUM_EDGE_LOW:
    case RIG_LEVEL_SPECTRUM_EDGE_HIGH:
    {
        int range_id;
        value_t edge_number_value;

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_FEF;
        cmd_len = 2;

        // Get the frequency range currently active
        retval = icom_get_spectrum_edge_frequency_range(rig, vfo, &range_id);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error getting spectrum edge frequency range\n",
                      __func__);
            RETURNFUNC(retval);
        }

        // Get the edge number currently active
        retval = icom_get_ext_level(rig, vfo, TOK_SCOPE_EDG, &edge_number_value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        to_bcd(cmdbuf, range_id, 1 * 2);
        to_bcd(cmdbuf + 1, edge_number_value.i + 1, 1 * 2);
        break;
    }

    case RIG_LEVEL_SPECTRUM_ATT:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_ATT;
        cmd_len = 1;

        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        break;

    case RIG_LEVEL_USB_AF:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_ATT;
        cmd_len = 1;

        break;

    case RIG_LEVEL_AGC_TIME:
        lvl_cn = C_CTL_MEM;
        lvl_sc = 0x04; // IC-9700, 7300, 705 so far
        cmd_len = 0;

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s\n", __func__,
                  rig_strlevel(level));
        RETURNFUNC(-RIG_EINVAL);
    }

    /* use cmdbuf and cmd_len for 'set mode' subcommand */
    retval = icom_transaction(rig, lvl_cn, lvl_sc, cmdbuf, cmd_len, respbuf,
                              &resp_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /*
     * strbuf should contain Cn,Sc,Data area
     */
    cmdhead = ((lvl_sc == -1) ? 1 : 2) + cmd_len;
    resp_len -= cmdhead;

    if (respbuf[0] != lvl_cn)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  respbuf[0], resp_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    /*
     * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
     * (from_bcd is little endian)
     */
    icom_val = from_bcd_be(respbuf + cmdhead, resp_len * 2);

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        val->i = round(rig_raw2val(icom_val, &rig->caps->str_cal));
        break;

    case RIG_LEVEL_RAWSTR:
        /* raw value */
        val->i = icom_val;
        break;

    case RIG_LEVEL_AGC:
        if (priv_caps->agc_levels_present)
        {
            int found = 0;

            for (i = 0;
                    i <= HAMLIB_MAX_AGC_LEVELS && priv_caps->agc_levels[i].level >= 0; i++)
            {
                if (priv_caps->agc_levels[i].icom_level == icom_val)
                {
                    val->i = priv_caps->agc_levels[i].level;
                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: unexpected AGC 0x%02x\n", __func__,
                          icom_val);
                RETURNFUNC(-RIG_EPROTO);
            }
        }
        else
        {
            switch (icom_val)
            {
            case D_AGC_SLOW:
                val->i = RIG_AGC_SLOW;
                break;

            case D_AGC_MID:
                val->i = RIG_AGC_MEDIUM;
                break;

            case D_AGC_FAST:
                val->i = RIG_AGC_FAST;
                break;

            case D_AGC_SUPERFAST:
                val->i = RIG_AGC_SUPERFAST;
                break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unexpected AGC 0x%02x\n", __func__,
                          icom_val);
                RETURNFUNC(-RIG_EPROTO);
            }
        }

        break;

    case RIG_LEVEL_ALC:
        if (rig->caps->alc_cal.size == 0)
        {
            val->f = rig_raw2val_float(icom_val, &icom_default_alc_cal);
        }
        else
        {
            val->f = rig_raw2val_float(icom_val, &rig->caps->alc_cal);
        }

        break;

    case RIG_LEVEL_SWR:
        if (rig->caps->swr_cal.size == 0)
        {
            val->f = rig_raw2val_float(icom_val, &icom_default_swr_cal);
        }
        else
        {
            val->f = rig_raw2val_float(icom_val, &rig->caps->swr_cal);
        }

        break;

    case RIG_LEVEL_RFPOWER_METER:

        // rig table in Watts needs to be divided by 100
        if (rig->caps->rfpower_meter_cal.size == 0)
        {
            val->f =
                rig_raw2val_float(icom_val, &icom_default_rfpower_meter_cal) * 0.01;
        }
        else
        {
            val->f =
                rig_raw2val_float(icom_val, &rig->caps->rfpower_meter_cal) * 0.01;
        }

        break;

    case RIG_LEVEL_RFPOWER_METER_WATTS:

        // All Icom backends should be in Watts now
        if (rig->caps->rfpower_meter_cal.size == 0)
        {
            val->f =
                rig_raw2val_float(icom_val, &icom_default_rfpower_meter_cal);
            rig_debug(RIG_DEBUG_TRACE, "%s: using rig table to convert %d to %.01f\n",
                      __func__, icom_val, val->f);
        }
        else
        {
            val->f =
                rig_raw2val_float(icom_val, &rig->caps->rfpower_meter_cal);
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: using default icom table to convert %d to %.01f\n", __func__, icom_val,
                      val->f);
        }

        break;

    case RIG_LEVEL_COMP_METER:
        if (rig->caps->comp_meter_cal.size == 0)
        {
            val->f = rig_raw2val_float(icom_val, &icom_default_comp_meter_cal);
        }
        else
        {
            val->f = rig_raw2val_float(icom_val, &rig->caps->comp_meter_cal);
        }

        break;

    case RIG_LEVEL_VD_METER:
        if (rig->caps->vd_meter_cal.size == 0)
        {
            val->f = rig_raw2val_float(icom_val, &icom_default_vd_meter_cal);
        }
        else
        {
            val->f = rig_raw2val_float(icom_val, &rig->caps->vd_meter_cal);
        }

        break;

    case RIG_LEVEL_ID_METER:
        if (rig->caps->id_meter_cal.size == 0)
        {
            val->f = rig_raw2val_float(icom_val, &icom_default_id_meter_cal);
        }
        else
        {
            val->f = rig_raw2val_float(icom_val, &rig->caps->id_meter_cal);
        }

        break;

    case RIG_LEVEL_CWPITCH:
        val->i = (int) lroundf(300.0f + ((float) icom_val * 600.0f / 255.0f));
        break;

    case RIG_LEVEL_KEYSPD:
        val->i = (int) lroundf((float) icom_val * (42.0f / 255.0f) + 6.0f);
        break;

    case RIG_LEVEL_PREAMP:
        if (icom_val == 0)
        {
            val->i = 0;
            break;
        }

        if (icom_val > HAMLIB_MAXDBLSTSIZ || rs->preamp[icom_val - 1] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported preamp get_level %ddB\n",
                      __func__, icom_val);
            RETURNFUNC(-RIG_EPROTO);
        }

        val->i = rs->preamp[icom_val - 1];
        break;

    case RIG_LEVEL_SPECTRUM_MODE:
        switch (icom_val)
        {
        case SCOPE_MODE_CENTER:
            val->i = RIG_SPECTRUM_MODE_CENTER;
            break;

        case SCOPE_MODE_FIXED:
            val->i = RIG_SPECTRUM_MODE_FIXED;
            break;

        case SCOPE_MODE_SCROLL_C:
            val->i = RIG_SPECTRUM_MODE_CENTER_SCROLL;
            break;

        case SCOPE_MODE_SCROLL_F:
            val->i = RIG_SPECTRUM_MODE_FIXED_SCROLL;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported spectrum mode %d\n", __func__,
                      icom_val);
            RETURNFUNC(-RIG_EINVAL);
        }

        break;

    case RIG_LEVEL_SPECTRUM_SPAN:
        icom_val = (int) from_bcd(respbuf + cmdhead, resp_len * 2);
        // Spectrum span is represented as a +/- value for Icom rigs
        val->i = icom_val * 2;
        break;

    case RIG_LEVEL_SPECTRUM_SPEED:
        switch (icom_val)
        {
        case SCOPE_SPEED_SLOW:
            val->i = 0;
            break;

        case SCOPE_SPEED_MID:
            val->i = 1;
            break;

        case SCOPE_SPEED_FAST:
            val->i = 2;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported spectrum speed %d\n", __func__,
                      icom_val);
            RETURNFUNC(-RIG_EINVAL);
        }

        break;

    case RIG_LEVEL_SPECTRUM_REF:
    {
        unsigned char *icom_ref = respbuf + cmdhead;

        // Spectrum reference level is represented at 0.01dB accuracy, but is rounded to nearest 0.5dB
        float db = (float) from_bcd_be(icom_ref, 2 * 2) / 100.0f;

        // Sign
        if (icom_ref[2] != 0)
        {
            db = -db;
        }

        val->f = db;
        break;
    }

    case RIG_LEVEL_SPECTRUM_EDGE_LOW:
        val->i = (int) from_bcd(respbuf + cmdhead, 5 * 2);
        break;

    case RIG_LEVEL_SPECTRUM_EDGE_HIGH:
        val->i = (int) from_bcd(respbuf + cmdhead + 5, 5 * 2);
        break;

    case RIG_LEVEL_AGC_TIME:

        // some rigs have different level interpretaions for different modes
        if (rig->state.current_mode == RIG_MODE_AM)
        {
            val->f = agc_level2[icom_val];
        }
        else
        {
            val->f = agc_level[icom_val];
        }

        break;

    /* RIG_LEVEL_ATT/RIG_LEVEL_SPECTRUM_ATT: returned value is already an integer in dB (coded in BCD) */
    default:
        if (RIG_LEVEL_IS_FLOAT(level))
        {
            val->f = (float) icom_val / 255;
        }
        else
        {
            val->i = icom_val;
        }
    }

    /* convert values from 0 .. 255 range */
    if (RIG_IS_ICR75)
    {
        switch (level)
        {
        case RIG_LEVEL_NR:
            val->f = (float) icom_val / 240;
            break;

        case RIG_LEVEL_PBT_IN:
        case RIG_LEVEL_PBT_OUT:
            if (icom_val == 255)
            {
                val->f = 1280.0;
            }
            else
            {
                val->f = (float)(icom_val - 128) * 10.0;
            }

            break;

        default:
            break;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: %d %d %d %f\n", __func__, resp_len,
              icom_val, val->i, val->f);

    RETURNFUNC(RIG_OK);
}

int icom_set_ext_level(RIG *rig, vfo_t vfo, hamlib_token_t token, value_t val)
{
    const struct confparams *cfp = rig->caps->extlevels;
    unsigned char cmdbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int cmd_len, ack_len = sizeof(ackbuf);
    int lvl_cn, lvl_sc;       /* Command Number, Subcommand */
    int i, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: token=%ld int=%d float=%f\n", __func__,
              token, val.i, val.f);

    switch (token)
    {
    case TOK_SCOPE_MSS:
        if (val.i < 0 || val.i > 1)
        {
            RETURNFUNC2(-RIG_EINVAL);
        }

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_MSS;
        cmd_len = 1;
        cmdbuf[0] = val.i;
        break;

    case TOK_SCOPE_SDS:
        if (val.i < 0 || val.i > 1)
        {
            RETURNFUNC2(-RIG_EINVAL);
        }

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_SDS;
        cmd_len = 1;
        cmdbuf[0] = val.i;
        break;

    case TOK_SCOPE_STX:

        // TODO: Should be a func?
        if (val.i < 0 || val.i > 1)
        {
            RETURNFUNC2(-RIG_EINVAL);
        }

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_STX;
        cmd_len = 1;
        cmdbuf[0] = val.i;
        break;

    case TOK_SCOPE_CFQ:
        if (val.i < 0 || val.i > 2)
        {
            RETURNFUNC2(-RIG_EINVAL);
        }

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_CFQ;
        cmd_len = 1;
        cmdbuf[0] = val.i;
        break;

    case TOK_SCOPE_EDG:
        if (val.i < 0 || val.i > 3)
        {
            RETURNFUNC2(-RIG_EINVAL);
        }

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_EDG;
        cmd_len = 2;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        cmdbuf[1] = val.i + 1;
        break;

    case TOK_SCOPE_VBW:
        if (val.i < 0 || val.i > 1)
        {
            RETURNFUNC2(-RIG_EINVAL);
        }

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_VBW;
        cmd_len = 2;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        cmdbuf[1] = val.i;
        break;

    case TOK_SCOPE_RBW:
        if (val.i < 0 || val.i > 2)
        {
            RETURNFUNC2(-RIG_EINVAL);
        }

        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_RBW;
        cmd_len = 2;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        cmdbuf[1] = val.i;
        break;

    default:
        cfp = (cfp == NULL) ? icom_ext_levels : cfp;

        for (i = 0; (cfp[i].token != RIG_CONF_END) || (cfp != icom_ext_levels);)
        {
            if (cfp[i].token == RIG_CONF_END)
            {
                cfp = icom_ext_levels;
                i = 0;
            }
            else if (cfp[i].token == token)
            {
                RETURNFUNC2(icom_set_ext_cmd(rig, vfo, token, val));
            }
            else { i++; }
        }

        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_ext_level token: %ld\n", __func__,
                  token);
        RETURNFUNC2(-RIG_EINVAL);
    }

    retval = icom_transaction(rig, lvl_cn, lvl_sc, cmdbuf, cmd_len, ackbuf,
                              &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC2(RIG_OK);
}

int icom_get_ext_level(RIG *rig, vfo_t vfo, hamlib_token_t token, value_t *val)
{
    const struct confparams *cfp = rig->caps->extlevels;
    unsigned char cmdbuf[MAXFRAMELEN], respbuf[MAXFRAMELEN];
    int cmd_len, resp_len;
    int lvl_cn, lvl_sc;       /* Command Number, Subcommand */
    int icom_val;
    int cmdhead;
    int retval;
    int i;

    ENTERFUNC;

    cmd_len = 0;
    lvl_sc = -1;

    switch (token)
    {
    case TOK_SCOPE_MSS:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_MSS;
        break;

    case TOK_SCOPE_SDS:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_SDS;
        break;

    case TOK_SCOPE_STX:
        // TODO: Should be a func?
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_STX;
        break;

    case TOK_SCOPE_CFQ:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_CFQ;
        break;

    case TOK_SCOPE_EDG:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_EDG;
        cmd_len = 1;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        break;

    case TOK_SCOPE_VBW:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_VBW;
        cmd_len = 1;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        break;

    case TOK_SCOPE_RBW:
        lvl_cn = C_CTL_SCP;
        lvl_sc = S_SCP_RBW;
        cmd_len = 1;
        cmdbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        break;

    default:
        cfp = (cfp == NULL) ? icom_ext_levels : cfp;

        for (i = 0; (cfp[i].token != RIG_CONF_END) || (cfp != icom_ext_levels);)
        {
            if (cfp[i].token == RIG_CONF_END)
            {
                cfp = icom_ext_levels;
                i = 0;
            }
            else if (cfp[i].token == token)
            {
                RETURNFUNC(icom_get_ext_cmd(rig, vfo, token, val));
            }
            else { i++; }
        }

        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_ext_level token: %ld\n", __func__,
                  token);
        RETURNFUNC(-RIG_EINVAL);
    }

    /* use cmdbuf and cmd_len for 'set mode' subcommand */
    retval = icom_transaction(rig, lvl_cn, lvl_sc, cmdbuf, cmd_len, respbuf,
                              &resp_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    cmdhead = ((lvl_sc == -1) ? 1 : 2) + cmd_len;
    resp_len -= cmdhead;

    if (respbuf[0] != lvl_cn)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  respbuf[0], resp_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    icom_val = from_bcd_be(respbuf + cmdhead, resp_len * 2);

    switch (token)
    {
    case TOK_SCOPE_EDG:
        val->i = icom_val - 1;
        break;

    default:
        val->i = icom_val;
        break;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: %d %d %d %f\n", __func__, resp_len,
              icom_val, val->i, val->f);

    RETURNFUNC(RIG_OK);
}

int icom_set_ext_func(RIG *rig, vfo_t vfo, hamlib_token_t token, int status)
{
    ENTERFUNC;

    const struct confparams *cfp = rig->caps->extfuncs;
    cfp = (cfp == NULL) ? icom_ext_funcs : cfp;
    int i;

    for (i = 0; (cfp[i].token != RIG_CONF_END) || (cfp != icom_ext_funcs);)
    {
        if (cfp[i].token == RIG_CONF_END)
        {
            cfp = icom_ext_funcs;
            i = 0;
        }
        else if (cfp[i].token == token)
        {
            value_t value = { .i = status };
            RETURNFUNC(icom_set_ext_cmd(rig, vfo, token, value));
        }
        else { i++; }
    }

    RETURNFUNC(-RIG_EINVAL);
}

int icom_get_ext_func(RIG *rig, vfo_t vfo, hamlib_token_t token, int *status)
{
    ENTERFUNC;

    const struct confparams *cfp = rig->caps->extfuncs;
    cfp = (cfp == NULL) ? icom_ext_funcs : cfp;
    int i;

    for (i = 0; (cfp[i].token != RIG_CONF_END) || (cfp != icom_ext_funcs);)
    {
        if (cfp[i].token == RIG_CONF_END)
        {
            cfp = icom_ext_funcs;
            i = 0;
        }
        else if (cfp[i].token == token)
        {
            value_t value;
            int result = icom_get_ext_cmd(rig, vfo, token, &value);

            if (result == RIG_OK)
            {
                *status = value.i;
            }

            RETURNFUNC(result);
        }
        else { i++; }
    }

    RETURNFUNC(-RIG_EINVAL);
}

int icom_set_ext_parm(RIG *rig, hamlib_token_t token, value_t val)
{
    ENTERFUNC;

    const struct confparams *cfp = rig->caps->extparms;
    cfp = (cfp == NULL) ? icom_ext_parms : cfp;
    int i;

    for (i = 0; (cfp[i].token != RIG_CONF_END) || (cfp != icom_ext_parms);)
    {
        if (cfp[i].token == RIG_CONF_END)
        {
            cfp = icom_ext_parms;
            i = 0;
        }
        else if (cfp[i].token == token)
        {
            RETURNFUNC(icom_set_ext_cmd(rig, RIG_VFO_NONE, token, val));
        }
        else { i++; }
    }

    RETURNFUNC(-RIG_EINVAL);
}

int icom_get_ext_parm(RIG *rig, hamlib_token_t token, value_t *val)
{
    ENTERFUNC;

    const struct confparams *cfp = rig->caps->extparms;
    cfp = (cfp == NULL) ? icom_ext_parms : cfp;
    int i;

    for (i = 0; (cfp[i].token != RIG_CONF_END) || (cfp != icom_ext_parms);)
    {
        if (cfp[i].token == RIG_CONF_END)
        {
            cfp = icom_ext_parms;
            i = 0;
        }
        else if (cfp[i].token == token)
        {
            RETURNFUNC(icom_get_ext_cmd(rig, RIG_VFO_NONE, token, val));
        }
        else { i++; }
    }

    RETURNFUNC(-RIG_EINVAL);
}

int icom_get_ext_cmd(RIG *rig, vfo_t vfo, hamlib_token_t token, value_t *val)
{
    int i;

    ENTERFUNC;

    for (i = 0; rig->caps->ext_tokens
            && rig->caps->ext_tokens[i] != TOK_BACKEND_NONE; i++)
    {
        if (rig->caps->ext_tokens[i] == token)
        {
            const struct icom_priv_caps *priv = rig->caps->priv;
            const struct cmdparams *cmd = priv->extcmds ? priv->extcmds : icom_ext_cmd;

            for (i = 0; (cmd[i].id.t != 0) || (cmd != icom_ext_cmd);)
            {
                if (cmd[i].id.t == 0)
                {
                    cmd = icom_ext_cmd;
                    i = 0;
                }
                else if (cmd[i].cmdparamtype == CMD_PARAM_TYPE_TOKEN && cmd[i].id.t == token)
                {
                    RETURNFUNC(icom_get_cmd(rig, vfo, (struct cmdparams *)&cmd[i], val));
                }
                else { i++; }
            }

            RETURNFUNC(-RIG_EINVAL);
        }
    }

    RETURNFUNC(-RIG_EINVAL);
}

int icom_set_ext_cmd(RIG *rig, vfo_t vfo, hamlib_token_t token, value_t val)
{
    int i;

    ENTERFUNC;

    for (i = 0; rig->caps->ext_tokens
            && rig->caps->ext_tokens[i] != TOK_BACKEND_NONE; i++)
    {
        if (rig->caps->ext_tokens[i] == token)
        {
            const struct icom_priv_caps *priv = rig->caps->priv;
            const struct cmdparams *cmd = priv->extcmds ? priv->extcmds : icom_ext_cmd;

            for (i = 0; (cmd[i].id.t != 0) || (cmd != icom_ext_cmd);)
            {
                if (cmd[i].id.t == 0)
                {
                    cmd = icom_ext_cmd;
                    i = 0;
                }
                else if (cmd->cmdparamtype == CMD_PARAM_TYPE_TOKEN && cmd[i].id.t == token)
                {
                    RETURNFUNC(icom_set_cmd(rig, vfo, (struct cmdparams *)&cmd[i], val));
                }
                else { i++; }
            }

            RETURNFUNC(-RIG_EINVAL);
        }
    }

    RETURNFUNC(-RIG_EINVAL);
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_conf(RIG *rig, hamlib_token_t token, const char *val)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;

    ENTERFUNC;
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    switch (token)
    {
    case TOK_CIVADDR:
        if (val[0] == '0' && val[1] == 'x')
        {
            priv->re_civ_addr = strtol(val, (char **) NULL, 16);
        }
        else
        {
            priv->re_civ_addr = atoi(val);
        }

        break;

    case TOK_MODE731:
        priv->civ_731_mode = atoi(val) ? 1 : 0;
        break;

    case TOK_NOXCHG:
        priv->no_xchg = atoi(val) ? 1 : 0;
        break;

    case TOK_TONE_ENABLE:
        priv->tone_enable = atoi(val) ? 1 : 0;
        break;

    case TOK_FILTER_USBD:
        priv->filter_usbd = atoi(val);

        if (priv->filter_usbd > 3) { priv->filter_usbd = 3; }

        if (priv->filter_usbd < 1) { priv->filter_usbd = 1; }

        break;

    case TOK_FILTER_USB:
        priv->filter_usb = atoi(val);

        if (priv->filter_usb > 3) { priv->filter_usb = 3; }

        if (priv->filter_usb < 1) { priv->filter_usb = 1; }

        break;

    case TOK_FILTER_CW:
        priv->filter_cw = atoi(val);

        if (priv->filter_cw > 3) { priv->filter_cw = 3; }

        if (priv->filter_cw < 1) { priv->filter_cw = 1; }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int icom_get_conf2(RIG *rig, hamlib_token_t token, char *val, int val_len)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;

    ENTERFUNC;
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    switch (token)
    {
    case TOK_CIVADDR:
        SNPRINTF(val, val_len, "%d", priv->re_civ_addr);
        break;

    case TOK_MODE731: SNPRINTF(val, val_len, "%d", priv->civ_731_mode);
        break;

    case TOK_NOXCHG: SNPRINTF(val, val_len, "%d", priv->no_xchg);
        break;

    default: RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

int icom_get_conf(RIG *rig, hamlib_token_t token, char *val)
{
    return icom_get_conf2(rig, token, val, 128);
}



/*
 * icom_set_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char ackbuf[MAXFRAMELEN], pttbuf[1];
    int ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;
    pttbuf[0] = ptt == RIG_PTT_ON ? 1 : 0;

    retval = icom_transaction(rig, C_CTL_PTT, S_PTT, pttbuf, 1,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icom_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    unsigned char pttbuf[MAXFRAMELEN];
    int ptt_len, retval;
    int retry = 5;

    ENTERFUNC;

    do
    {
        retval = icom_transaction(rig, C_CTL_PTT, S_PTT, NULL, 0,
                                  pttbuf, &ptt_len);
    }
    while (--retry > 0 && retval != RIG_OK);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /*
     * pttbuf should contain Cn,Sc,Data area
     */
    ptt_len -= 2;

    if (ptt_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, ptt_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    *ptt = pttbuf[2] == 1 ? RIG_PTT_ON : RIG_PTT_OFF;

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_dcd
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icom_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    unsigned char dcdbuf[MAXFRAMELEN];
    int dcd_len, retval;

    ENTERFUNC;
    retval = icom_transaction(rig, C_RD_SQSM, S_SQL, NULL, 0,
                              dcdbuf, &dcd_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /*
     * dcdbuf should contain Cn,Data area
     */
    dcd_len -= 2;

    if (dcd_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, dcd_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    /*
     * 0x00=sql closed, 0x01=sql open
     */

    *dcd = dcdbuf[2] == 1 ? RIG_DCD_ON : RIG_DCD_OFF;

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_rptr_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    int rptr_sc;

    ENTERFUNC;

    switch (rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE:
        rptr_sc = S_DUP_OFF;  /* Simplex mode */
        break;

    case RIG_RPT_SHIFT_MINUS:
        rptr_sc = S_DUP_M;    /* Duplex - mode */
        break;

    case RIG_RPT_SHIFT_PLUS:
        rptr_sc = S_DUP_P;    /* Duplex + mode */
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported shift %d\n", __func__,
                  rptr_shift);
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = icom_transaction(rig, C_CTL_SPLT, rptr_sc, NULL, 0,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}


/*
 * icom_get_rptr_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL, rptr_shift!=NULL
 * will not work for IC-746 Pro
 * NOTE: seems not to work (tested on IC-706MkIIG), please report --SF
 */
int icom_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    unsigned char rptrbuf[MAXFRAMELEN];
    int rptr_len, retval;

    ENTERFUNC;
    retval = icom_transaction(rig, C_CTL_SPLT, -1, NULL, 0,
                              rptrbuf, &rptr_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /*
     * rptrbuf should contain Cn,Sc
     */
    rptr_len--;

    if (rptr_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, rptr_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    switch (rptrbuf[1])
    {
    case S_DUP_OFF:
    case S_DUP_DD_RPS:
        *rptr_shift = RIG_RPT_SHIFT_NONE; /* Simplex mode */
        break;

    case S_DUP_M:
        *rptr_shift = RIG_RPT_SHIFT_MINUS;    /* Duplex - mode */
        break;

    case S_DUP_P:
        *rptr_shift = RIG_RPT_SHIFT_PLUS; /* Duplex + mode */
        break;

    // The same command indicates split state, which means simplex mode
    case S_SPLT_OFF:
    case S_SPLT_ON:
        *rptr_shift = RIG_RPT_SHIFT_NONE; /* Simplex mode */
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported shift %d\n", __func__,
                  rptrbuf[1]);
        RETURNFUNC(-RIG_EPROTO);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_rptr_offs
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
    int offs_len;
    unsigned char offsbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    const struct icom_priv_caps *priv_caps;

    ENTERFUNC;
    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;
    offs_len = (priv_caps->offs_len) ? priv_caps->offs_len : OFFS_LEN;

    /*
     * Icoms are using a 100Hz unit (at least on 706MKIIg) -- SF
     */
    to_bcd(offsbuf, rptr_offs / 100, offs_len * 2);

    retval = icom_transaction(rig, C_SET_OFFS, -1, offsbuf, offs_len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}


/*
 * icom_get_rptr_offs
 * Assumes rig!=NULL, rig->state.priv!=NULL, rptr_offs!=NULL
 */
int icom_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
    int offs_len;
    unsigned char offsbuf[MAXFRAMELEN];
    int buf_len, retval;
    const struct icom_priv_caps *priv_caps;

    ENTERFUNC;
    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;
    offs_len = (priv_caps->offs_len) ? priv_caps->offs_len : OFFS_LEN;

    retval = icom_transaction(rig, C_RD_OFFS, -1, NULL, 0, offsbuf, &buf_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /*
     * offsbuf should contain Cn
     */
    buf_len--;

    if (buf_len != offs_len)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__,
                  buf_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    /*
     * Icoms are using a 100Hz unit (at least on 706MKIIg) -- SF
     */
    *rptr_offs = from_bcd(offsbuf + 1, buf_len * 2) * 100;

    RETURNFUNC(RIG_OK);
}

/*
 * Helper function to go back and forth split VFO
 */
int icom_get_split_vfos(RIG *rig, vfo_t *rx_vfo, vfo_t *tx_vfo)
{
    struct rig_state *rs = STATE(rig);
    struct rig_cache *cachep = CACHE(rig);

    ENTERFUNC;

    // Initialize TX VFO if not done yet
    if (rs->tx_vfo == RIG_VFO_NONE || rs->tx_vfo == RIG_VFO_CURR
            || rs->tx_vfo == RIG_VFO_TX)
    {
        if (cachep->split == RIG_SPLIT_OFF)
        {
            rs->tx_vfo = rs->current_vfo;
        }
        else
        {
            rs->tx_vfo = vfo_fixup(rig, RIG_VFO_OTHER, cachep->split);
        }
    }

    if (VFO_HAS_A_B_ONLY)
    {
        if (cachep->split == RIG_SPLIT_OFF)
        {
            *rx_vfo = *tx_vfo = rs->current_vfo;
        }
        else
        {
            *rx_vfo = rs->current_vfo;
            *tx_vfo = rs->tx_vfo;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: VFO_HAS_A_B_ONLY, split=%d, rx=%s, tx=%s\n",
                  __func__, cachep->split, rig_strvfo(*rx_vfo), rig_strvfo(*tx_vfo));
    }
    else if (VFO_HAS_MAIN_SUB_ONLY)
    {
        if (cachep->split == RIG_SPLIT_OFF)
        {
            *rx_vfo = *tx_vfo = rs->current_vfo;
        }
        else
        {
            *rx_vfo = rs->current_vfo;
            *tx_vfo = rs->tx_vfo;
        }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: VFO_HAS_MAIN_SUB_ONLY, split=%d, rx=%s, tx=%s\n",
                  __func__, cachep->split, rig_strvfo(*rx_vfo), rig_strvfo(*tx_vfo));
    }
    else if (VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        int satmode = 0;

        // e.g. IC-9700 split on Main/Sub does not work
        // only Main VFOA/B and SubRx/MainTx split works
        if (rig->caps->has_get_func & RIG_FUNC_SATMODE)
        {
            rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
        }

        // don't care about retval here, only care about satmode=1
        if (satmode)
        {
            *rx_vfo = RIG_VFO_MAIN;
            *tx_vfo = RIG_VFO_SUB;
        }
        else if (cachep->split == RIG_SPLIT_OFF)
        {
            *rx_vfo = *tx_vfo = rs->current_vfo;
        }
        else
        {
            *rx_vfo = rs->current_vfo;
            *tx_vfo = rs->tx_vfo;
        }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: VFO_HAS_MAIN_SUB_A_B_ONLY, split=%d, rx=%s, tx=%s\n",
                  __func__, cachep->split, rig_strvfo(*rx_vfo), rig_strvfo(*tx_vfo));
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown VFO setup\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_split_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *  icom_set_vfo,icom_set_freq works for this rig
 *
 * Assumes also that the current VFO is the rx VFO.
 */
int icom_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct rig_state *rs = STATE(rig);
    struct rig_cache *cachep = CACHE(rig);
    hamlib_port_t *rp = RIGPORT(rig);
    struct icom_priv_data *priv = rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t rx_vfo, tx_vfo;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called for %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: curr_vfo=%s\n", __func__,
              rig_strvfo(rs->current_vfo));

    rig_debug(RIG_DEBUG_VERBOSE, "%s: satmode=%d, tx_vfo=%s\n", __func__,
              cachep->satmode, rig_strvfo(rs->tx_vfo));

    if (vfo == RIG_VFO_TX)
    {
        if (cachep->satmode) { vfo = RIG_VFO_SUB; }
        else { vfo = rs->tx_vfo; }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo is now %s\n", __func__, rig_strvfo(vfo));

    if (rs->current_vfo == RIG_VFO_NONE || rs->current_vfo == RIG_VFO_CURR)
    {
        HAMLIB_TRACE;
        retval = icom_set_default_vfo(rig);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_default_vfo failed: %s\n", __func__,
                      rigerror(retval));
            RETURNFUNC2(retval);
        }
    }

    // Use the command 0x25 if the rig supports it
    // This eliminates VFO swapping and improves split operations
    if ((rs->targetable_vfo & RIG_TARGETABLE_FREQ) && (priv->x25cmdfails <= 0
            || priv_caps->x25x26_always))
    {
        int satmode = 0;

        // Return value is not important, as only satmode=1 means anything
        if (rig->caps->has_get_func & RIG_FUNC_SATMODE)
        {
            rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
        }

        // only worth trying if not in satmode
        if (satmode == 0)
        {
            int freq_len;
            short retry_save;
            unsigned char freqbuf[32];

            freq_len = priv->civ_731_mode ? 4 : 5;
            to_bcd(freqbuf, tx_freq, freq_len * 2);

            retry_save = rp->retry;
            rp->retry = 1;
            retval = icom_set_freq_x25(rig, vfo, tx_freq, freq_len, freqbuf);
            rp->retry = retry_save;

            if (retval == RIG_OK)
            {
                if (priv->tone_enable)
                {
                    rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_TONE, 1);
                }

                RETURNFUNC2(retval);
            }
        }
    }

    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Using XCHG to swap/set/swap\n", __func__);

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC2(retval);
        }

        if (RIG_OK != (retval = icom_set_freq(rig, RIG_VFO_CURR, tx_freq)))
        {
            RETURNFUNC2(retval);
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC2(retval);
        }

        RETURNFUNC2(retval);
    }

    /* broken if user changes split on rig :( */
    if (VFO_HAS_A_B_ONLY && cachep->split != RIG_SPLIT_OFF)
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: rx_vfo=%s, tx_vfo=%s\n", __func__,
              rig_strvfo(rx_vfo), rig_strvfo(tx_vfo));

    HAMLIB_TRACE;

    if (!(rs->targetable_vfo & RIG_TARGETABLE_FREQ))
    {
        retval = rig_set_vfo(rig, tx_vfo);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    retval = rig_set_freq(rig, tx_vfo, tx_freq);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    HAMLIB_TRACE;

    if (VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        // Then we return the VFO to the rx_vfo
        rig_debug(RIG_DEBUG_TRACE, "%s: SATMODE split_on=%d rig so setting vfo to %s\n",
                  __func__, cachep->split, rig_strvfo(rx_vfo));

        HAMLIB_TRACE;

        if (!(rs->targetable_vfo & RIG_TARGETABLE_FREQ))
        {
            retval = rig_set_vfo(rig, rx_vfo);

            if (retval != RIG_OK)
            {
                RETURNFUNC2(retval);
            }
        }
    }
    else if (RIG_OK != (retval = rig_set_vfo(rig, rx_vfo)))
    {
        HAMLIB_TRACE;
        RETURNFUNC2(retval);
    }

    if (VFO_HAS_A_B_ONLY && cachep->split != RIG_SPLIT_OFF)
    {
        /* Re-enable split */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    if (priv->tone_enable)
    {
        rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_TONE, 1);
    }

    RETURNFUNC2(retval);
}

/*
 * icom_get_split_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, rx_freq!=NULL, tx_freq!=NULL
 *  icom_set_vfo,icom_get_freq works for this rig
 */
int icom_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct rig_state *rs = STATE(rig);
    struct rig_cache *cachep = CACHE(rig);
    hamlib_port_t *rp = RIGPORT(rig);
    struct icom_priv_data *priv = rs->priv;
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t rx_vfo, tx_vfo;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: curr_vfo=%s\n", __func__,
              rig_strvfo(rig->state.current_vfo));

    if (RIG_IS_IC910)
    {
        ptt_t ptt;
        retval = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if (ptt)
        {
            int cache_ms_freq;
            rig_get_cache_freq(rig, vfo, tx_freq, &cache_ms_freq);
            rig_debug(RIG_DEBUG_TRACE, "%s: ptt is on so returning last known freq\n",
                      __func__);
            RETURNFUNC2(RIG_OK);
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s curr_vfo=%s\n", __func__,
              rig_strvfo(rig->state.current_vfo));

    if (rig->state.current_vfo == RIG_VFO_NONE)
    {
        HAMLIB_TRACE;
        icom_set_default_vfo(rig);
    }

    // Use the command 0x25 if the rig supports it
    // This eliminates VFO swapping and improves split operations
    // This does not work in satellite mode for the IC-9700
    if ((rs->targetable_vfo & RIG_TARGETABLE_FREQ) && (priv->x25cmdfails <= 0
            || priv_caps->x25x26_always))
    {
        int satmode = 0;

        // Return value is not important, as only satmode=1 means anything
        if (rig->caps->has_get_func & RIG_FUNC_SATMODE)
        {
            rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
        }

        if (satmode == 0)
        {
            // Only worth trying if the rig is not in satmode
            short retry_save = rp->retry;
            int freqbuf_offset;

            rp->retry = 0;
            retval = icom_get_freq_x25(rig, vfo, &ack_len, ackbuf, &freqbuf_offset);
            rp->retry = retry_save;

            if (retval == RIG_OK)
            {
                *tx_freq = from_bcd(&ackbuf[freqbuf_offset], (priv->civ_731_mode ? 4 : 5) * 2);
                RETURNFUNC2(retval);
            }
        }
        else
        {
            // The rig is in satmode so attempt to get the TX frequency using another command
            int freqbuf_offset;
            retval = icom_get_tx_freq(rig, &ack_len, ackbuf, &freqbuf_offset);

            if (retval == RIG_OK)
            {
                *tx_freq = from_bcd(&ackbuf[freqbuf_offset], (priv->civ_731_mode ? 4 : 5) * 2);
                RETURNFUNC2(retval);
            }
        }
    }

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC2(retval);
        }

        if (RIG_OK != (retval = rig_get_freq(rig, RIG_VFO_CURR, tx_freq)))
        {
            RETURNFUNC2(retval);
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC2(retval);
        }

        RETURNFUNC2(retval);
    }

    /* broken if user changes split on rig :( */
    if (VFO_HAS_A_B_ONLY && cachep->split != RIG_SPLIT_OFF)
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        RETURNFUNC2(retval);
    }

    HAMLIB_TRACE;

    if (RIG_OK != (retval = rig_set_vfo(rig, tx_vfo)))
    {
        RETURNFUNC2(retval);
    }

    if (RIG_OK != (retval = rig_get_freq(rig, tx_vfo, tx_freq)))
    {
        RETURNFUNC2(retval);
    }

    HAMLIB_TRACE;

    if (RIG_OK != (retval = rig_set_vfo(rig, rx_vfo)))
    {
        HAMLIB_TRACE;
        RETURNFUNC2(retval);
    }

    if (VFO_HAS_A_B_ONLY && cachep->split != RIG_SPLIT_OFF)
    {
        /* Re-enable split */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    RETURNFUNC2(retval);
}

/*
 * icom_set_split_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *  icom_set_vfo,icom_set_mode works for this rig
 */
int icom_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                        pbwidth_t tx_width)
{
    struct rig_state *rs = STATE(rig);
    struct rig_cache *cachep = CACHE(rig);
    struct icom_priv_data *priv = rs->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t rx_vfo, tx_vfo;
    int retval;

    ENTERFUNC;

    if ((rs->targetable_vfo & RIG_TARGETABLE_MODE)
            && rs->current_vfo != RIG_VFO_MEM)
    {
        RETURNFUNC(icom_set_mode(rig, vfo, tx_mode, tx_width));
    }

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC(retval);
        }

        if (RIG_OK != (retval = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                                tx_width)))
        {
            RETURNFUNC(retval);
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC(retval);
        }

        RETURNFUNC(retval);
    }

    /* broken if user changes split on rig :( */
    if (VFO_HAS_A_B_ONLY && cachep->split != RIG_SPLIT_OFF)
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        RETURNFUNC(retval);
    }

    HAMLIB_TRACE;

    if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE)
            && RIG_OK != (retval = rig_set_vfo(rig, tx_vfo)))
    {
        RETURNFUNC(retval);
    }

    if (RIG_OK != (retval = rig->caps->set_mode(rig, tx_vfo, tx_mode,
                            tx_width)))
    {
        RETURNFUNC(retval);
    }

    HAMLIB_TRACE;

    if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE)
            && RIG_OK != (retval = rig_set_vfo(rig, rx_vfo)))
    {
        RETURNFUNC(retval);
    }

    if (VFO_HAS_A_B_ONLY && cachep->split != RIG_SPLIT_OFF)
    {
        /* Re-enable split */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    RETURNFUNC(retval);
}

/*
 * icom_get_split_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *  rx_mode!=NULL, rx_width!=NULL, tx_mode!=NULL, tx_width!=NULL
 *  icom_set_vfo,icom_get_mode works for this rig
 */
int icom_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                        pbwidth_t *tx_width)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = rs->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t rx_vfo, tx_vfo;
    int retval;

    ENTERFUNC;

    if ((rs->targetable_vfo & RIG_TARGETABLE_MODE)
            && rs->current_vfo != RIG_VFO_MEM)
    {
        RETURNFUNC(icom_get_mode(rig, vfo, tx_mode, tx_width));
    }

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC(retval);
        }

        if (RIG_OK != (retval = rig->caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                                tx_width)))
        {
            RETURNFUNC(retval);
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC(retval);
        }

        RETURNFUNC(retval);
    }

    /* broken if user changes split on rig :( */
    if (VFO_HAS_A_B_ONLY && CACHE(rig)->split != RIG_SPLIT_OFF)
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        RETURNFUNC(retval);
    }

    HAMLIB_TRACE;

    if (RIG_OK != (retval = rig_set_vfo(rig, tx_vfo)))
    {
        RETURNFUNC(retval);
    }

    if (RIG_OK != (retval = rig->caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                            tx_width)))
    {
        RETURNFUNC(retval);
    }

    HAMLIB_TRACE;

    if (RIG_OK != (retval = rig_set_vfo(rig, rx_vfo)))
    {
        RETURNFUNC(retval);
    }

    if (VFO_HAS_A_B_ONLY && CACHE(rig)->split != RIG_SPLIT_OFF)
    {
        /* Re-enable split */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    RETURNFUNC(retval);
}

/*
 * icom_set_split_freq_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *  icom_set_vfo,icom_set_mode works for this rig
 */
int icom_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t tx_freq,
                             rmode_t tx_mode, pbwidth_t tx_width)
{
    struct rig_state *rs = STATE(rig);
    struct icom_priv_data *priv = rs->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t rx_vfo, tx_vfo;
    int split_assumed = 0;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, curr_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(rs->current_vfo));

    // If the user is asking to set split on VFO_CURR we'll assume split mode
    // WSJT-X calls this function before turning on split mode
    if (vfo == RIG_VFO_CURR) { split_assumed = 1; }

    if (rs->current_vfo == RIG_VFO_NONE)
    {
        HAMLIB_TRACE;
        icom_set_default_vfo(rig);
    }

    if (rs->current_vfo != RIG_VFO_MEM)
    {
        if ((rs->targetable_vfo & RIG_TARGETABLE_FREQ)
                && (rs->targetable_vfo & RIG_TARGETABLE_MODE))
        {
            retval = icom_set_freq(rig, vfo, tx_freq);

            if (retval != RIG_OK)
            {
                RETURNFUNC2(retval);
            }

            RETURNFUNC2(icom_set_mode(rig, vfo, tx_mode, tx_width));
        }
    }

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC2(retval);
        }

        if (RIG_OK != (retval = rig_set_freq(rig, RIG_VFO_CURR, tx_freq)))
        {
            RETURNFUNC2(retval);
        }

        if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE)
                && RIG_OK != (retval = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                                       tx_width)))
        {
            RETURNFUNC2(retval);
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC2(retval);
        }

        RETURNFUNC2(retval);
    }

    /* broken if user changes split on rig :( */
    if (VFO_HAS_A_B && (split_assumed || CACHE(rig)->split != RIG_SPLIT_OFF))
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: before get_split_vfos rx_vfo=%s tx_vfo=%s\n", __func__,
              rig_strvfo(rs->rx_vfo), rig_strvfo(rs->tx_vfo));

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        RETURNFUNC2(retval);
    }

    // WSJT-X calls this function before setting split
    // So in this case we have to force the tx_vfo
    if (split_assumed && vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: split_assumed so tx_vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        tx_vfo = VFO_HAS_A_B_ONLY ? RIG_VFO_B : RIG_VFO_SUB;
    }


    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: after get_split_vfos  rx_vfo=%s tx_vfo=%s\n", __func__,
              rig_strvfo(rs->rx_vfo), rig_strvfo(rs->tx_vfo));

    // if not asking for RIG_VFO_CURR we'll use the requested VFO in the function call as tx_vfo
    if (CACHE(rig)->split == RIG_SPLIT_OFF && vfo != RIG_VFO_CURR)
    {
        tx_vfo = vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: split not on so using requested vfo=%s\n",
                  __func__, rig_strvfo(tx_vfo));
    }

    HAMLIB_TRACE;

    if (!(rs->targetable_vfo & RIG_TARGETABLE_FREQ)
            && RIG_OK != (retval = rig_set_vfo(rig, tx_vfo)))
    {
        RETURNFUNC2(retval);
    }

    if (RIG_OK != (retval = rig_set_freq(rig, RIG_VFO_CURR, tx_freq)))
    {
        RETURNFUNC2(retval);
    }

    HAMLIB_TRACE;

    if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE)
            && RIG_OK != (retval = rig_set_vfo(rig, tx_vfo)))
    {
        RETURNFUNC2(retval);
    }

    if (RIG_OK != (retval = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                            tx_width)))
    {
        RETURNFUNC2(retval);
    }

    HAMLIB_TRACE;

    if (!(rs->targetable_vfo & RIG_TARGETABLE_MODE)
            && RIG_OK != (retval = rig_set_vfo(rig, rx_vfo)))
    {
        RETURNFUNC2(retval);
    }

    if (VFO_HAS_A_B && CACHE(rig)->split != RIG_SPLIT_OFF)
    {
        /* Re-enable split */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    RETURNFUNC2(retval);
}

/*
 * icom_get_split_freq_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *  rx_mode!=NULL, rx_width!=NULL, tx_mode!=NULL, tx_width!=NULL
 *  icom_set_vfo,icom_get_mode works for this rig
 */
int icom_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *tx_freq,
                             rmode_t *tx_mode, pbwidth_t *tx_width)
{
    struct rig_state *rs = STATE(rig);
    struct icom_priv_data *priv = rs->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t rx_vfo, tx_vfo;
    int retval;

    ENTERFUNC;

    if (rs->current_vfo != RIG_VFO_MEM)
    {
        if ((rs->targetable_vfo & RIG_TARGETABLE_FREQ)
                && (rs->targetable_vfo & RIG_TARGETABLE_MODE))
        {
            retval = icom_get_freq(rig, vfo, tx_freq);

            if (retval != RIG_OK)
            {
                RETURNFUNC2(retval);
            }

            RETURNFUNC2(icom_get_mode(rig, vfo, tx_mode, tx_width));
        }
    }

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC(retval);
        }

        if (RIG_OK != (retval = rig_get_freq(rig, RIG_VFO_CURR, tx_freq)))
        {
            RETURNFUNC(retval);
        }

        if (RIG_OK != (retval = rig->caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                                tx_width)))
        {
            RETURNFUNC(retval);
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            RETURNFUNC(retval);
        }

        RETURNFUNC(retval);
    }

    /* broken if user changes split on rig :( */
    if (VFO_HAS_A_B_ONLY && CACHE(rig)->split != RIG_SPLIT_OFF)
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        RETURNFUNC(retval);
    }

    HAMLIB_TRACE;

    if (RIG_OK != (retval = rig_set_vfo(rig, tx_vfo)))
    {
        RETURNFUNC(retval);
    }

    if (RIG_OK != (retval = rig_get_freq(rig, RIG_VFO_CURR, tx_freq)))
    {
        RETURNFUNC(retval);
    }

    if (RIG_OK != (retval = rig->caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                            tx_width)))
    {
        RETURNFUNC(retval);
    }

    HAMLIB_TRACE;

    if (RIG_OK != (retval = rig_set_vfo(rig, rx_vfo)))
    {
        RETURNFUNC(retval);
    }

    if (VFO_HAS_A_B_ONLY && CACHE(rig)->split != RIG_SPLIT_OFF)
    {
        /* Re-enable split */
        retval = icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    RETURNFUNC(retval);
}

/*
 * icom_set_split
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_split_vfo(RIG *rig, vfo_t rx_vfo, split_t split, vfo_t tx_vfo)
{
    struct rig_state *rs = STATE(rig);
    struct rig_cache *cachep = CACHE(rig);
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    int split_sc;

    /* For Icom which VFO is active for this call is not important
     * S VFOA 1 VFOB -- RX on VFOA, TX on VFOB
     * S VFOB 1 VFOA -- RX on VFOB, TX on VFOA
     * S Main 1 Sub -- RX on Main, TX on Sub
     * S Sub 1 Main -- RX on Sub, TX on Main
     */
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s called rx_vfo='%s', split=%d, tx_vfo=%s, curr_vfo=%s\n", __func__,
              rig_strvfo(rx_vfo), split, rig_strvfo(tx_vfo),
              rig_strvfo(rig->state.current_vfo));

    // This should automatically switch between satmode on/off based on the requested split rx_vfo
    if (rig->caps->has_get_func & RIG_FUNC_SATMODE)
    {
        int satmode = 0;
        // Check SATMODE status, because it affects commands related to split
        rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);

        if ((tx_vfo == RIG_VFO_MAIN || tx_vfo == RIG_VFO_SUB)
                && !cachep->satmode)
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: requesting split for Main/Sub VFO and satmode is OFF so turning satmode ON\n",
                      __func__);
            retval = rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, 1);
            // Split cannot be turned on in satmode, so return after enabling satmode
            RETURNFUNC2(retval);
        }
        else if ((tx_vfo == RIG_VFO_A || tx_vfo == RIG_VFO_B)
                 && cachep->satmode)
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: requesting split for VFO A/B and satmode is ON so turning satmode OFF\n",
                      __func__);
            rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, 0);
        }
        else if ((tx_vfo == RIG_VFO_MAIN || tx_vfo == RIG_VFO_SUB)
                 && cachep->satmode && split == RIG_SPLIT_ON)
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: requesting split for Main/Sub VFO and rig is already in satmode so setting split on "
                      "is redundant and will result in an error, returning OK\n",
                      __func__);
            // Return OK as satmode is a split mode and gpredict wants to see the OK response here
            RETURNFUNC2(RIG_OK);
        }
    }

    if (rs->current_vfo != rx_vfo && rx_vfo != RIG_VFO_CURR)
    {
        // Icom split command requires switching to the RX VFO first
        retval = rig_set_vfo(rig, rx_vfo);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    switch (split)
    {
    case RIG_SPLIT_OFF:
        split_sc = S_SPLT_OFF;
        break;

    case RIG_SPLIT_ON:
        split_sc = S_SPLT_ON;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported split %d", __func__, split);
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (priv_caps->dualwatch_split)
    {
        int wvfo = (tx_vfo & (RIG_VFO_A | RIG_VFO_MAIN)) ? S_SUB : S_MAIN;

        retval = icom_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, split_sc);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        retval = icom_transaction(rig, C_SET_VFO, wvfo, NULL, 0, ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }
    else
    {
        retval = icom_transaction(rig, C_CTL_SPLT, split_sc, NULL, 0, ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: curr_vfo=%s rx_vfo=%s tx_vfo=%s split=%d\n",
              __func__, rig_strvfo(rig->state.current_vfo),
              rig_strvfo(rx_vfo), rig_strvfo(tx_vfo), split);

    RETURNFUNC2(RIG_OK);
}

/*
 * icom_get_split_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL, split!=NULL
 *
 * Does not appear to be supported by any mode?
 * \sa icom_mem_get_split_vfo()
 */
int icom_get_split_vfo(RIG *rig, vfo_t rx_vfo, split_t *split, vfo_t *tx_vfo)
{
    unsigned char splitbuf[MAXFRAMELEN];
    int split_len, retval, satmode = 0;
    struct rig_state *rs = STATE(rig);

    ENTERFUNC;
    retval = icom_transaction(rig, C_CTL_SPLT, -1, NULL, 0,
                              splitbuf, &split_len);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CTL_SPLT failed?\n", __func__);
        RETURNFUNC(retval);
    }

    /*
     * splitbuf should contain Cn,Sc
     */
    split_len--;

    if (split_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, split_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    switch (splitbuf[1])
    {
    case S_SPLT_OFF:
        *split = RIG_SPLIT_OFF;
        break;

    case S_SPLT_ON:
        *split = RIG_SPLIT_ON;
        break;

    // The same command indicates repeater shift state, which means that split is off
    case S_DUP_OFF:
    case S_DUP_M:
    case S_DUP_P:
    case S_DUP_DD_RPS:
        *split = RIG_SPLIT_OFF;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported split %d", __func__,
                  splitbuf[1]);
        RETURNFUNC(-RIG_EPROTO);
    }

    if (rig->caps->has_get_func & RIG_FUNC_SATMODE)
    {
        // Check SATMODE status, because it affects commands related to split
        rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
    }

    // Update cache early for icom_get_split_vfos()
    CACHE(rig)->split = *split;

    icom_get_split_vfos(rig, &rs->rx_vfo, &rs->tx_vfo);

    *tx_vfo = rs->tx_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: rx_vfo=%s rx_vfo=%s tx_vfo=%s split=%d\n",
              __func__, rig_strvfo(rx_vfo), rig_strvfo(rs->rx_vfo),
              rig_strvfo(rs->tx_vfo), *split);

    RETURNFUNC(RIG_OK);
}


/*
 * icom_mem_get_split_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL, split!=NULL
 */
int icom_mem_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                           vfo_t *tx_vfo)
{
    int retval;

    ENTERFUNC;

    /* this hacks works only when in memory mode
     * I have no clue how to detect split in regular VFO mode
     */
    if (rig->state.current_vfo != RIG_VFO_MEM ||
            !rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        *split = CACHE(rig)->split; // we set this but still return ENAVAIL
        RETURNFUNC(-RIG_ENAVAIL);
    }

    retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG);

    if (retval == RIG_OK)
    {
        *split = RIG_SPLIT_ON;
        /* get it back to normal */
        retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG);

        if (retval != RIG_OK) { RETURNFUNC(retval); }
    }
    else if (retval == -RIG_ERJCTED)
    {
        *split = RIG_SPLIT_OFF;
    }
    else
    {
        /* this is really an error! */
        RETURNFUNC(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_ts
 * Assumes rig!=NULL, rig->caps->priv!=NULL
 */
int icom_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    const struct icom_priv_caps *priv_caps;
    unsigned char ackbuf[MAXFRAMELEN];
    int i, ack_len = sizeof(ackbuf), retval;
    int ts_sc = 0;

    ENTERFUNC;
    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;

    for (i = 0; i < HAMLIB_TSLSTSIZ; i++)
    {
        if (priv_caps->ts_sc_list[i].ts == ts)
        {
            ts_sc = priv_caps->ts_sc_list[i].sc;
            break;
        }
    }

    if (i >= HAMLIB_TSLSTSIZ)
    {
        RETURNFUNC(-RIG_EINVAL);   /* not found, unsupported */
    }

    retval = icom_transaction(rig, C_SET_TS, ts_sc, NULL, 0, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_ts
 * Assumes rig!=NULL, rig->caps->priv!=NULL, ts!=NULL
 * NOTE: seems not to work (tested on IC-706MkIIG), please report --SF  Not available on 746pro
 */
int icom_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    const struct icom_priv_caps *priv_caps;
    unsigned char tsbuf[MAXFRAMELEN];
    int ts_len, i, retval;

    ENTERFUNC;
    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;

    retval = icom_transaction(rig, C_SET_TS, -1, NULL, 0, tsbuf, &ts_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /*
     * tsbuf should contain Cn,Sc
     */
    ts_len--;

    if (ts_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__, ts_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    for (i = 0; i < HAMLIB_TSLSTSIZ; i++)
    {
        if (priv_caps->ts_sc_list[i].sc == tsbuf[1])
        {
            *ts = priv_caps->ts_sc_list[i].ts;
            break;
        }
    }

    if (i >= HAMLIB_TSLSTSIZ)
    {
        RETURNFUNC(-RIG_EPROTO);   /* not found, unsupported */
    }

    RETURNFUNC(RIG_OK);
}


/*
 * icom_set_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    unsigned char fctbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int fct_len, ack_len, retval;
    int fct_cn, fct_sc;       /* Command Number, Subcommand */
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    ENTERFUNC;

    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    const struct cmdparams *extcmds = priv_caps->extcmds;
    int i;

    value_t value = { .i = status };

    for (i = 0; extcmds && extcmds[i].id.s != 0; i++)
    {
        if (extcmds[i].cmdparamtype == CMD_PARAM_TYPE_FUNC && extcmds[i].id.s == func)
        {
            RETURNFUNC(icom_set_cmd(rig, vfo, (struct cmdparams *)&extcmds[i], value));
        }
    }

    fctbuf[0] = status ? 0x01 : 0x00;
    fct_len = 1;

    switch (func)
    {
    case RIG_FUNC_NB:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_NB;
        break;

    case RIG_FUNC_COMP: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_COMP;
        break;

    case RIG_FUNC_VOX: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_VOX;
        break;

    case RIG_FUNC_TONE:   /* repeater tone */
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_TONE;
        break;

    case RIG_FUNC_TSQL: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_TSQL;
        break;

    case RIG_FUNC_SBKIN: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_BKIN;

        if (status != 0)
        {
            fctbuf[0] = 0x01;
        }
        else
        {
            fctbuf[0] = 0x00;
        }

        break;

    case RIG_FUNC_FBKIN:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_BKIN;

        if (status != 0)
        {
            fctbuf[0] = 0x02;
        }
        else
        {
            fctbuf[0] = 0x00;
        }

        break;

    case RIG_FUNC_ANF:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_ANF;
        break;

    case RIG_FUNC_NR:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_NR;
        break;

    case RIG_FUNC_APF:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_APF;
        break;

    case RIG_FUNC_MON:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_MON;
        break;

    case RIG_FUNC_MN:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_MN;
        break;

    case RIG_FUNC_RF:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_RF;
        break;

    case RIG_FUNC_VSC:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_VSC;
        break;

    case RIG_FUNC_LOCK:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_DIAL_LK;
        break;

    case RIG_FUNC_AFC:      /* IC-910H */
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_AFC;
        break;

    case RIG_FUNC_SCOPE:
        fct_cn = C_CTL_SCP;
        fct_sc = S_SCP_STS;
        fctbuf[0] = status;
        fct_len = 1;
        break;

    case RIG_FUNC_SPECTRUM:
        fct_cn = C_CTL_SCP;
        fct_sc = S_SCP_DOP;
        fctbuf[0] = status;
        fct_len = 1;
        break;

    case RIG_FUNC_SPECTRUM_HOLD:
        fct_cn = C_CTL_SCP;
        fct_sc = S_SCP_HLD;

        fct_len = 2;
        fctbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        fctbuf[1] = status;
        break;

    case RIG_FUNC_RESUME:   /* IC-910H  & IC-746-Pro */
        fct_cn = C_CTL_SCAN;
        fct_sc = status ? S_SCAN_RSMON : S_SCAN_RSMOFF;
        fct_len = 0;
        break;

    case RIG_FUNC_CSQL:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_CSQL;
        break;

    case RIG_FUNC_DSQL:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_DSSQL;

        if (status <= 2)
        {
            fctbuf[0] = status;
        }
        else
        {
            fctbuf[0] = 0;
        }

        break;

    case RIG_FUNC_AFLT:
        fct_cn = C_CTL_MEM;
        fct_sc = S_MEM_AFLT;
        break;

    case RIG_FUNC_ANL:
        fct_cn = C_CTL_MEM;
        fct_sc = S_MEM_ANL;
        break;

    case RIG_FUNC_AIP:      /* IC-R8600 IP+ function, misusing AIP since RIG_FUNC_ word is full (32 bit) */
        fct_cn = C_CTL_MEM;
        fct_sc = S_FUNC_IPPLUS;
        break;

    case RIG_FUNC_RIT:
        fct_cn = C_CTL_RIT;
        fct_sc = S_RIT;
        break;

    case RIG_FUNC_XIT:
        fct_cn = C_CTL_RIT;
        fct_sc = S_XIT;
        break;

    case RIG_FUNC_TUNER:
        fct_cn = C_CTL_PTT;
        fct_sc = S_ANT_TUN;
        break;

    case RIG_FUNC_DUAL_WATCH:
        if ((RIG_IS_IC9100)
                || (RIG_IS_IC9700)
                || (RIG_IS_ID5100))
        {
            fct_cn = C_CTL_FUNC;
            fct_sc = S_MEM_DUALMODE;
        }
        else
        {
            fct_cn = C_SET_VFO;
            fct_sc = status ? S_DUAL_ON : S_DUAL_OFF;
            fct_len = 0;
        }

        break;

    case RIG_FUNC_SATMODE:
        if (RIG_IS_IC910)
        {
            // Is the 910 the only one that uses this command?
            fct_cn = C_CTL_MEM;
            fct_sc = S_MEM_SATMODE910;
        }
        else
        {
            fct_cn = C_CTL_FUNC;
            fct_sc = S_MEM_SATMODE;
        }

        break;


    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_func %s", __func__,
                  rig_strfunc(func));
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = icom_transaction(rig, fct_cn, fct_sc, fctbuf, fct_len, ackbuf,
                              &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (ack_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__, ack_len);
        RETURNFUNC(-RIG_EPROTO);
    }

    if (func == RIG_FUNC_SATMODE)
    {
        int satmode = status ? 1 : 0;

        if (satmode != CACHE(rig)->satmode)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): changed satmode=%d\n",
                      __func__, __LINE__, satmode);

            // Reset x25cmdfails to current status, because it fails in SATMODE
            icom_set_x25x26_ability(rig, satmode ? 1 : -1);
            // Reset x1cx03cmdfails to test it again
            priv->x1cx03cmdfails = 0;
        }
        else
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): satmode=%d\n",
                      __func__, __LINE__, satmode);
        }

        CACHE(rig)->satmode = satmode;
        icom_satmode_fix(rig, satmode);

        // Turning satmode ON/OFF can change the TX/RX VFOs
        // Split is OFF always in satmode
        if (VFO_HAS_MAIN_SUB_A_B_ONLY)
        {
            vfo_t tx_vfo;
            split_t split;

            // Update split status (updates rig state/cache internally)
            retval = icom_get_split_vfo(rig, RIG_VFO_CURR, &split, &tx_vfo);

            if (retval != RIG_OK)
            {
                RETURNFUNC(retval);
            }
        }
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 * FIXME: IC8500 and no-sc, any support?
 */
int icom_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    int fct_cn, fct_sc;       /* Command Number, Subcommand */
    unsigned char fctbuf[MAXFRAMELEN];
    int fct_len = 0;

    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    const struct cmdparams *extcmds = priv_caps->extcmds;
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    ENTERFUNC;

    value_t value;

    for (i = 0; extcmds && extcmds[i].id.s != 0; i++)
    {
        //rig_debug(RIG_DEBUG_TRACE, "%s: i=%d\n", __func__, i);

        if (extcmds[i].cmdparamtype == CMD_PARAM_TYPE_FUNC && extcmds[i].id.s == func)
        {
            int result = icom_get_cmd(rig, vfo, (struct cmdparams *)&extcmds[i], &value);

            if (result == RIG_OK)
            {
                *status = value.i;
            }

            RETURNFUNC(result);
        }
    }

    switch (func)
    {
    case RIG_FUNC_NB:
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_NB;
        break;

    case RIG_FUNC_COMP: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_COMP;
        break;

    case RIG_FUNC_VOX: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_VOX;
        break;

    case RIG_FUNC_TONE:   /* repeater tone */
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_TONE;
        break;

    case RIG_FUNC_TSQL: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_TSQL;
        break;

    case RIG_FUNC_SBKIN:  /* returns 1 for semi and 2 for full adjusted below */
    case RIG_FUNC_FBKIN: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_BKIN;
        break;

    case RIG_FUNC_ANF: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_ANF;
        break;

    case RIG_FUNC_NR: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_NR;
        break;

    case RIG_FUNC_APF: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_APF;
        break;

    case RIG_FUNC_MON: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_MON;
        break;

    case RIG_FUNC_MN: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_MN;
        break;

    case RIG_FUNC_RF: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_RF;
        break;

    case RIG_FUNC_VSC: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_VSC;
        break;

    case RIG_FUNC_LOCK: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_DIAL_LK;
        break;

    case RIG_FUNC_AFC:    /* IC-910H */
        fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_AFC;
        break;

    case RIG_FUNC_SCOPE:
        fct_cn = C_CTL_SCP;
        fct_sc = S_SCP_STS;
        break;

    case RIG_FUNC_SPECTRUM:
        fct_cn = C_CTL_SCP;
        fct_sc = S_SCP_DOP;
        break;

    case RIG_FUNC_SPECTRUM_HOLD:
        fct_cn = C_CTL_SCP;
        fct_sc = S_SCP_HLD;

        fctbuf[0] = icom_get_spectrum_vfo(rig, vfo);
        fct_len = 1;
        break;

    case RIG_FUNC_AIP:    /* IC-R8600 IP+ function, misusing AIP since RIG_FUNC_ word is full (32 bit) */
        fct_cn = C_CTL_MEM;   /* 1a */
        fct_sc = S_FUNC_IPPLUS;
        break;

    case RIG_FUNC_CSQL: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_CSQL;
        break;

    case RIG_FUNC_DSQL: fct_cn = C_CTL_FUNC;
        fct_sc = S_FUNC_DSSQL;
        break;

    case RIG_FUNC_AFLT: fct_cn = C_CTL_MEM;
        fct_sc = S_MEM_AFLT;
        break;

    case RIG_FUNC_ANL: fct_cn = C_CTL_MEM;
        fct_sc = S_MEM_ANL;
        break;

    case RIG_FUNC_RIT: fct_cn = C_CTL_RIT;
        fct_sc = S_RIT;
        break;

    case RIG_FUNC_XIT: fct_cn = C_CTL_RIT;
        fct_sc = S_XIT;
        break;

    case RIG_FUNC_TUNER: fct_cn = C_CTL_PTT;
        fct_sc = S_ANT_TUN;
        break;

    case RIG_FUNC_DUAL_WATCH:
        if ((RIG_IS_IC9100) ||
                (RIG_IS_IC9700) ||
                (RIG_IS_ID5100))
        {
            fct_cn = C_CTL_FUNC;
            fct_sc = S_MEM_DUALMODE;
        }
        else
        {
            fct_cn = C_SET_VFO;
            fct_sc = S_DUAL;
        }

        break;

    case RIG_FUNC_SATMODE:
        if (RIG_IS_IC910)
        {
            // Is the 910 the only one that uses this command?
            fct_cn = C_CTL_MEM;
            fct_sc = S_MEM_SATMODE910;
        }
        else
        {
            fct_cn = C_CTL_FUNC;
            fct_sc = S_MEM_SATMODE;
        }

        break;

    case RIG_FUNC_OVF_STATUS:
    {
        fct_cn = C_RD_SQSM;
        fct_sc = S_OVF;
        break;
    }

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_func %s\n", __func__,
                  rig_strfunc(func));
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = icom_transaction(rig, fct_cn, fct_sc, fctbuf, fct_len, ackbuf,
                              &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (ack_len != (3 + fct_len))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__,
                  ack_len);
        RETURNFUNC(-RIG_EPROTO);
    }

    if (func == RIG_FUNC_FBKIN)
    {
        *status = ackbuf[2] == 2 ? 1 : 0;
    }
    else if (func == RIG_FUNC_SATMODE)
    {
        struct rig_state *rs = STATE(rig);
        struct icom_priv_data *priv = rs->priv;
        int satmode = ackbuf[2 + fct_len];

        *status = satmode;

        if (satmode != CACHE(rig)->satmode)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): changed satmode=%d\n",
                      __func__, __LINE__, satmode);

            // Reset x25cmdfails to current status, because it fails in SATMODE
            icom_set_x25x26_ability(rig, satmode ? 1 : -1);
            // Reset x1cx03cmdfails to test it again
            priv->x1cx03cmdfails = 0;
        }
        else
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): satmode=%d\n",
                      __func__, __LINE__, satmode);
        }

        CACHE(rig)->satmode = satmode;
        icom_satmode_fix(rig, satmode);
    }
    else
    {
        *status = ackbuf[2 + fct_len];
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_parm
 * Assumes rig!=NULL
 *
 * NOTE: Most of the parm commands are rig-specific.
 *
 * See the IC-7300 backend how to implement them for newer rigs that have 0x1A 0x05-based commands.
 *
 * For older rigs, see the IC-R75 backend where icom_set_raw()/icom_get_raw() are used.
 */
int icom_set_parm(RIG *rig, setting_t parm, value_t val)
{
    ENTERFUNC;

    int i;
    const struct icom_priv_caps *priv = rig->caps->priv;
    const struct cmdparams *extcmds = priv->extcmds;

    for (i = 0; extcmds && extcmds[i].id.s != 0; i++)
    {
        if (extcmds[i].cmdparamtype == CMD_PARAM_TYPE_PARM && extcmds[i].id.s == parm)
        {
            RETURNFUNC(icom_set_cmd(rig, RIG_VFO_NONE, (struct cmdparams *)&extcmds[i],
                                    val));
        }
    }

    switch (parm)
    {
    case RIG_PARM_ANN:
    {
        int ann_mode;

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

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported RIG_PARM_ANN %d\n",
                      __func__, val.i);
            RETURNFUNC(-RIG_EINVAL);
        }

        RETURNFUNC(icom_set_raw(rig, C_CTL_ANN, ann_mode, 0, NULL, 0, 0));
    }

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_parm %s\n", __func__,
                  rig_strparm(parm));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(-RIG_EINVAL);
}

const char *icom_get_band(RIG *rig, int band)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    const char *s = rig_get_band_str(rig, band, 1);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %d=%s\n", __func__, band, s);

    return s;
}

/*
 * icom_get_parm
 * Assumes rig!=NULL
 *
 * NOTE: Most of the parm commands are rig-specific.
 *
 * See the IC-7300 backend how to implement them for newer rigs that have 0x1A 0x05-based commands.
 *
 * For older rigs, see the IC-R75 backend where icom_set_raw()/icom_get_raw() are used.
 */
int icom_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    ENTERFUNC;

    const struct icom_priv_caps *priv = rig->caps->priv;
    const struct cmdparams *cmd = priv->extcmds;
    int i;

    for (i = 0; cmd && cmd[i].id.s != 0; i++)
    {
        if (cmd[i].cmdparamtype == CMD_PARAM_TYPE_PARM && cmd[i].id.s == parm)
        {
            int retval = icom_get_cmd(rig, RIG_VFO_NONE, (struct cmdparams *)&cmd[i], val);

            if (parm == RIG_PARM_BANDSELECT)
            {
                char *s = (char *)icom_get_band(rig, val->i);
                val->s = s;
            }

            RETURNFUNC(retval);
        }
    }

    switch (parm)
    {
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_parm %s", __func__,
                  rig_strparm(parm));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Works for 746 pro and should work for 756 xx and 7800
 */
int icom_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps;
    unsigned char tonebuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int tone_len, ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;
    caps = rig->caps;

    if (caps->ctcss_list)
    {
        int i;

        for (i = 0; caps->ctcss_list[i] != 0; i++)
        {
            if (caps->ctcss_list[i] == tone)
            {
                break;
            }
        }

        if (caps->ctcss_list[i] != tone)
        {
            RETURNFUNC(-RIG_EINVAL);
        }
    }

    /* Sent as frequency in tenth of Hz */

    tone_len = 3;
    to_bcd_be(tonebuf, tone, tone_len * 2);

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_RPTR,
                              tonebuf, tone_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    const struct rig_caps *caps;
    unsigned char tonebuf[MAXFRAMELEN];
    int tone_len, retval;
    int i;

    ENTERFUNC;
    caps = rig->caps;

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_RPTR, NULL, 0,
                              tonebuf, &tone_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* cn,sc,data*3 */
    if (tone_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  tonebuf[0], tone_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    tone_len -= 2;
    *tone = from_bcd_be(tonebuf + 2, tone_len * 2);

    if (!caps->ctcss_list)
    {
        RETURNFUNC(RIG_OK);
    }

    /* check this tone exists. That's better than nothing. */
    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (caps->ctcss_list[i] == *tone)
        {
            RETURNFUNC(RIG_OK);
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%#.2x)\n", __func__, tonebuf[2]);
    RETURNFUNC(-RIG_EPROTO);
}

/*
 * icom_set_ctcss_sql
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps;
    unsigned char tonebuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int tone_len, ack_len = sizeof(ackbuf), retval;
    int i;

    ENTERFUNC;
    caps = rig->caps;

    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (caps->ctcss_list[i] == tone)
        {
            break;
        }
    }

    if (caps->ctcss_list[i] != tone)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* Sent as frequency in tenth of Hz */

    tone_len = 3;
    to_bcd_be(tonebuf, tone, tone_len * 2);

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_SQL,
                              tonebuf, tone_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_ctcss_sql
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    const struct rig_caps *caps;
    unsigned char tonebuf[MAXFRAMELEN];
    int tone_len, retval;
    int i;

    ENTERFUNC;
    caps = rig->caps;

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_SQL, NULL, 0,
                              tonebuf, &tone_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (tone_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  tonebuf[0], tone_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    tone_len -= 2;
    *tone = from_bcd_be(tonebuf + 2, tone_len * 2);

    /* check this tone exists. That's better than nothing. */
    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (caps->ctcss_list[i] == *tone)
        {
            RETURNFUNC(RIG_OK);
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%#.2x)\n", __func__, tonebuf[2]);
    RETURNFUNC(-RIG_EPROTO);
}

/*
 * icom_set_dcs_code
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    const struct rig_caps *caps;
    unsigned char codebuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int code_len, ack_len = sizeof(ackbuf), retval;
    int i;

    ENTERFUNC;
    caps = rig->caps;

    for (i = 0; caps->dcs_list[i] != 0; i++)
    {
        if (caps->dcs_list[i] == code)
        {
            break;
        }
    }

    if (caps->dcs_list[i] != code)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* DCS Polarity ignored, by setting code_len to 3 it's forced to 0 (= Tx:norm, Rx:norm). */
    code_len = 3;
    to_bcd_be(codebuf, code, code_len * 2);

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_DTCS,
                              codebuf, code_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_dcs_code
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    const struct rig_caps *caps;
    unsigned char codebuf[MAXFRAMELEN];
    int code_len, retval;
    int i;

    ENTERFUNC;
    caps = rig->caps;

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_DTCS, NULL, 0,
                              codebuf, &code_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* cn,sc,data*3 */
    if (code_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  codebuf[0], code_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    /* buf is cn,sc, polarity, code_lo, code_hi, so code bytes start at 3, len is 2
       polarity is not decoded yet, hard to do without breaking ABI
     */

    code_len -= 3;
    *code = from_bcd_be(codebuf + 3, code_len * 2);

    /* check this code exists. That's better than nothing. */
    for (i = 0; caps->dcs_list[i] != 0; i++)
    {
        if (caps->dcs_list[i] == *code)
        {
            RETURNFUNC(RIG_OK);
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: DTCS NG (%#.2x)\n", __func__, codebuf[2]);
    RETURNFUNC(-RIG_EPROTO);
}

/*
 * icom_set_dcs_sql
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    const struct rig_caps *caps;
    unsigned char codebuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int code_len, ack_len = sizeof(ackbuf), retval;
    int i;

    ENTERFUNC;
    caps = rig->caps;

    for (i = 0; caps->dcs_list[i] != 0; i++)
    {
        if (caps->dcs_list[i] == code)
        {
            break;
        }
    }

    if (caps->dcs_list[i] != code)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* DCS Polarity ignored, by setting code_len to 3 it's forced to 0 (= Tx:norm, Rx:norm). */
    code_len = 3;
    to_bcd_be(codebuf, code, code_len * 2);

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_DTCS,
                              codebuf, code_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_dcs_sql
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    const struct rig_caps *caps;
    unsigned char codebuf[MAXFRAMELEN];
    int code_len, retval;
    int i;

    ENTERFUNC;
    caps = rig->caps;

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_DTCS, NULL, 0,
                              codebuf, &code_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* cn,sc,data*3 */
    if (code_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  codebuf[0], code_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    /* buf is cn,sc, polarity, code_lo, code_hi, so code bytes start at 3, len is 2
       polarity is not decoded yet, hard to do without breaking ABI
     */

    code_len -= 3;
    *code = from_bcd_be(codebuf + 3, code_len * 2);

    /* check this code exists. That's better than nothing. */
    for (i = 0; caps->dcs_list[i] != 0; i++)
    {
        if (caps->dcs_list[i] == *code)
        {
            RETURNFUNC(RIG_OK);
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: DTCS NG (%#.2x)\n", __func__, codebuf[2]);
    RETURNFUNC(-RIG_EPROTO);
}

/*
 * icom_set_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_powerstat(RIG *rig, powerstat_t status)
{
    unsigned char ackbuf[200];
    int ack_len = sizeof(ackbuf), retval = RIG_OK;
    int pwr_sc;
    // so we'll do up to 150 for 115,200
    int fe_max = 150;
    unsigned char fe_buf[fe_max]; // for FE's to power up
    int i;
    int retry;
    short retry_save, timeout_retry_save;
    struct rig_state *rs = &rig->state;
    hamlib_port_t *rp = RIGPORT(rig);
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called status=%d\n", __func__,
              (int) status);

    // elimininate retries to speed this up
    // especially important when rig is not turned on
    retry_save = rp->retry;
    timeout_retry_save = rp->timeout_retry;

    rp->retry = 0;
    rp->timeout_retry = 0;

    switch (status)
    {
    case RIG_POWER_ON:

        // ic7300 manual says ~150 for 115,200
        // we'll just send a few more to be sure for all speeds
        switch (rp->parm.serial.rate)
        {
        case 4800:
            fe_max = 7;
            break;

        case 9600:
            fe_max = 13;
            break;

        case 19200:
            fe_max = 25;
            break;

        case 38400:
            fe_max = 50;
            break;

        case 57600:
            fe_max = 75;
            break;

        case 115200:
        default:
            fe_max = 150;
        }

        memset(fe_buf, 0xfe, fe_max);
        // sending more than enough 0xfe's to wake up the rs232
        write_block(rp, fe_buf, fe_max);
        // need to wait a bit for RigPI and others to queue the echo
        hl_usleep(400 * 1000);

        // we'll try 0x18 0x01 now -- should work on STBY rigs too
        pwr_sc = S_PWR_ON;
        fe_buf[0] = 0;
        priv->serial_USB_echo_off = 1;
        retval =
            icom_transaction(rig, C_SET_PWR, pwr_sc, NULL, 0, ackbuf, &ack_len);

        float sec_wait =
            5.5; // 5.5 worked for IC-9700 -- we default to worst-case-found

        if (RIG_IS_IC7300) { sec_wait = 3.8; }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: waiting %g seconds for rig to wake up\n",
                  __func__, sec_wait);
        hl_usleep(sec_wait * 1000 *
                  1000);  // some are slow to start up -- may need to add more rigs

        // poweron == 0 means never powered -- == 2 means CAT turned off
        if (priv->poweron == 0 || priv->poweron == 2)
        {
            int echo_status = -1;

            for (i = 0; i < 10 && echo_status < 0; ++i)
            {
                echo_status = icom_get_usb_echo_off(rig);

                if (echo_status < 0)
                {
                    hl_usleep(500 * 1000);
                }
            }

            if (echo_status >= 0)
            {
                priv->poweron = 1;
            }

            rp->retry = retry_save;
            rp->timeout_retry = timeout_retry_save;
            return RIG_OK;
        }

        break;

    default:
        pwr_sc = S_PWR_OFF;
        fe_buf[0] = 0;
        retval =
            icom_transaction(rig, C_SET_PWR, pwr_sc, NULL, 0, ackbuf, &ack_len);
        priv->poweron = 2;
    }

    i = 0;
    retry = 3;

    if (status == RIG_POWER_ON)   // wait for wakeup only
    {
        for (i = 0; i < retry; ++i)   // up to 10 attempts
        {
            freq_t freq;
            // need to see if echo is on or not first
            // until such time as rig is awake we don't know
            retval = icom_get_usb_echo_off(rig);

            if (retval == -RIG_ETIMEOUT)
            {
                rig_debug(RIG_DEBUG_WARN, "%s: get_usb_echo_off timeout...try#%d\n", __func__,
                          i + 1);
                continue;
            }

            // Use get_freq as all rigs should repond to this
            retval = rig_get_freq(rig, RIG_VFO_CURR, &freq);

            if (retval == RIG_OK)
            {
                int satmode;

                if (rig->caps->has_get_func & RIG_FUNC_SATMODE)
                {
                    // Getting satmode state updates RX/TX VFOs internally
                    rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
                }

                rs->current_vfo = icom_current_vfo(rig);
                rp->retry = retry_save;
                rp->timeout_retry = timeout_retry_save;
                RETURNFUNC2(retval);
            }
            else
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: get_freq err=%s\n", __func__,
                          rigerror(retval));
            }

            rig_debug(RIG_DEBUG_TRACE, "%s: Wait %d of %d for get_powerstat\n",
                      __func__, i + 1, retry);
        }
    }

    rp->retry = retry_save;
    rp->timeout_retry = timeout_retry_save;

    if (i == retry)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Wait failed for get_powerstat\n",
                  __func__);
        // close and re-open the rig
        // on linux the USB gets reset during power on
        rig_close(rig);
        sleep(1);
        rig_open(rig);

        retval = -RIG_ETIMEOUT;
    }

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: retval != RIG_OK, =%s\n", __func__,
                  rigerror(retval));
        RETURNFUNC2(retval);
    }

    if (status == RIG_POWER_OFF && (ack_len != 1 || ackbuf[0] != ACK))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        RETURNFUNC2(-RIG_ERJCTED);
    }

    RETURNFUNC2(RIG_OK);
}

/*
 * icom_get_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_powerstat(RIG *rig, powerstat_t *status)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;

    *status = RIG_POWER_OFF; // default return until proven otherwise

    /* r75 has no way to get power status, so fake it */
    if (RIG_IS_ICR75)
    {
        /* getting the mode doesn't work if a memory is blank */
        /* so use one of the more innocuous 'set mode' commands instead */
        int cmd_len = 1;
        unsigned char cmdbuf[MAXFRAMELEN];
        cmdbuf[0] = S_PRM_TIME;
        retval = icom_transaction(rig, C_CTL_MEM, S_MEM_MODE_SLCT,
                                  cmdbuf, cmd_len, ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        *status = ((ack_len == 6) && (ackbuf[0] == C_CTL_MEM)) ?
                  RIG_POWER_ON : RIG_POWER_OFF;
    }

    HAMLIB_TRACE;

    if (RIG_IS_IC2730
            || RIG_IS_IC705
            || RIG_IS_IC7100
            || RIG_IS_IC7300
            || RIG_IS_IC7600
            || RIG_IS_IC7610
            || RIG_IS_IC7700
            || RIG_IS_IC7800
            || RIG_IS_IC785X
            || RIG_IS_IC9700
            || RIG_IS_IC905)
    {
        freq_t freq;
        hamlib_port_t *rp = RIGPORT(rig);
        short retry_save = rp->retry;
        short timeout_retry_save = rp->timeout_retry;
        HAMLIB_TRACE;

        rp->retry = 0;
        rp->timeout_retry = 0;

        retval = rig_get_freq(rig, RIG_VFO_A, &freq);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: get freq failed, assuming power is off\n",
                      __func__);
        }

        HAMLIB_TRACE;

        rp->retry = retry_save;
        rp->timeout_retry = timeout_retry_save;

        // Assume power is OFF if get_freq fails
        *status = retval == RIG_OK ? RIG_POWER_ON : RIG_POWER_OFF;

        // Modify rig_state powerstat directly to reflect power ON/OFF status, but return the result of rig_get_freq,
        // because the error could indicate other connectivity issues too
        rig->state.powerstat = *status;
        return retval;
    }
    else
    {
        retval = icom_transaction(rig, C_SET_PWR, -1, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            // Assume power is OFF if getting power status fails
            // Modify rig_state powerstat directly to reflect power ON/OFF status, but return the result of rig_get_freq,
            // because the error could indicate other connectivity issues too
            rig_debug(RIG_DEBUG_WARN, "%s: get powerstat failed, assuming power is off\n",
                      __func__);
            rig->state.powerstat = RIG_POWER_OFF;
            RETURNFUNC(retval);
        }

        *status = ackbuf[1] == S_PWR_ON ? RIG_POWER_ON : RIG_POWER_OFF;
    }

    RETURNFUNC(RIG_OK);
}


/*
 * icom_set_mem
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    unsigned char membuf[2];
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    int chan_len;

    ENTERFUNC;
    chan_len = ch < 100 ? 1 : 2;

    to_bcd_be(membuf, ch, chan_len * 2);
    retval = icom_transaction(rig, C_SET_MEM, -1, membuf, chan_len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_bank
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_bank(RIG *rig, vfo_t vfo, int bank)
{
    unsigned char bankbuf[2];
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;
    to_bcd_be(bankbuf, bank, BANK_NB_LEN * 2);
    retval = icom_transaction(rig, C_SET_MEM, S_BANK,
                              bankbuf, CHAN_NB_LEN, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_set_ant
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    unsigned char antopt[2];
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval, i_ant = 0;
    int antopt_len = 0;
    const struct icom_priv_caps *priv_caps = (const struct icom_priv_caps *)
            rig->caps->priv;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s called, ant=0x%02x, option=%d, antack_len=%d\n", __func__, ant, option.i,
              priv_caps->antack_len);

    // query the antennas once and find out how many we have
    if (ant >= rig_idx2setting(priv_caps->ant_count))
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (ant > RIG_ANT_4)
    {
        RETURNFUNC2(-RIG_EDOM);
    }

    switch (ant)
    {
    case RIG_ANT_1:
        i_ant = 0;
        break;

    case RIG_ANT_2:
        i_ant = 1;
        break;

    case RIG_ANT_3:
        i_ant = 2;
        break;

    case RIG_ANT_4:
        i_ant = 3;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported ant %#x", __func__, ant);
        RETURNFUNC2(-RIG_EINVAL);
    }


    if (priv_caps->antack_len == 0)   // we need to find out the antack_len
    {
        ant_t tmp_ant, ant_tx, ant_rx;
        int ant0 = 0;
        value_t tmp_option;
        retval = rig_get_ant(rig, vfo, ant0, &tmp_option, &tmp_ant, &ant_tx, &ant_rx);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_get_ant error: %s \n", __func__,
                      rigerror(retval));
            RETURNFUNC2(retval);
        }
    }

    // Some rigs have 3-byte ant cmd so there is an option to be set too
    if (priv_caps->antack_len == 3)
    {
        if (option.i != 0 && option.i != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: option.i != 0 or 1, ==%d?\n", __func__, option.i);
            RETURNFUNC2(-RIG_EINVAL);
        }

        antopt_len = 1;
        antopt[0] = option.i;
        // we have to set the rx option by itself apparently
        rig_debug(RIG_DEBUG_TRACE, "%s: setting antopt=%d\n", __func__, antopt[0]);
        retval = icom_transaction(rig, C_CTL_ANT, i_ant,
                                  antopt, antopt_len, ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: antack_len=%d so antopt_len=%d, antopt=0x%02x\n",
                  __func__, priv_caps->antack_len, antopt_len, antopt[0]);
    }
    else if (priv_caps->antack_len == 2)
    {
        antopt_len = 0;
        rig_debug(RIG_DEBUG_TRACE, "%s: antack_len=%d so antopt_len=%d\n", __func__,
                  priv_caps->antack_len, antopt_len);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: antack_len=%d so antopt_len=%d\n", __func__,
                  priv_caps->antack_len, antopt_len);
        antopt_len = 0;
        rig_debug(RIG_DEBUG_ERR,
                  "%s: rig does not have antenna select? antack_len=%d\n", __func__,
                  priv_caps->antack_len);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: i_ant=%d, antopt=0x%02x, antopt_len=%d\n",
              __func__, i_ant, antopt[0], antopt_len);
    retval = icom_transaction(rig, C_CTL_ANT, i_ant,
                              antopt, antopt_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC2(RIG_OK);
}

/*
 * icom_get_ant
 * Assumes rig!=NULL, rig->state.priv!=NULL
 * only meaningful for HF
 */
int icom_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                 ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval = -RIG_EINTERNAL;
    struct icom_priv_caps *priv_caps = (struct icom_priv_caps *) rig->caps->priv;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called, ant=0x%02x\n", __func__, ant);

    if (ant != RIG_ANT_CURR)
    {
        ant = rig_setting2idx(ant);

        if (ant >= priv_caps->ant_count)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ant index=%u > ant_count=%d\n", __func__, ant,
                      priv_caps->ant_count);
            RETURNFUNC2(-RIG_EINVAL);
        }
    }

    // Should be able to use just C_CTL_ANT for 1 or 2 antennas hopefully
    if (ant == RIG_ANT_CURR || priv_caps->ant_count <= 2)
    {
        retval = icom_transaction(rig, C_CTL_ANT, -1, NULL, 0, ackbuf, &ack_len);
    }
    else if (RIG_IS_IC785X)
    {
        unsigned char buf[2];
        buf[0] = 0x03;
        buf[1] = 0x05 + ant;
        *ant_curr = ant;
        retval = icom_transaction(rig, C_CTL_MEM, 0x05, buf, sizeof(buf), ackbuf,
                                  &ack_len);

        if (retval == RIG_OK)
        {
            option->i = ackbuf[4];
            RETURNFUNC2(RIG_OK);
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: asking for non-current antenna and ant_count==0?\n", __func__);
        rig_debug(RIG_DEBUG_ERR, "%s: need to implement ant control for this rig?\n",
                  __func__);
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    // ack_len should be either 2 or 3
    // ant cmd format is one of
    // 0x12 0xaa
    // 0x12 0xaa 0xrr
    // Where aa is a zero-base antenna number and rr is a binary for rx only

    if ((ack_len != 2 && ack_len != 3) || ackbuf[0] != C_CTL_ANT ||
            ackbuf[1] > 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d, ant=%d\n", __func__,
                  ackbuf[0], ack_len, ackbuf[1]);
        RETURNFUNC2(-RIG_ERJCTED);
    }

    rig_debug(RIG_DEBUG_ERR, "%s: ackbuf= 0x%02x 0x%02x 0x%02x\n", __func__,
              ackbuf[0], ackbuf[1], ackbuf[2]);

    *ant_curr = *ant_tx = *ant_rx = rig_idx2setting(ackbuf[1]);

    // Note: with IC756/IC-756Pro/IC-7800 and more, ackbuf[2] deals with [RX ANT]
    // Hopefully any ack_len=3 can fit in the option field
    if (ack_len == 3)
    {
        option->i = ackbuf[2];
        *ant_rx = rig_idx2setting(ackbuf[2]);
    }

    RETURNFUNC2(RIG_OK);
}


/*
 * icom_vfo_op, Mem/VFO operation
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    unsigned char mvbuf[MAXFRAMELEN];
    unsigned char ackbuf[MAXFRAMELEN];
    int mv_len = 0, ack_len = sizeof(ackbuf), retval;
    int mv_cn, mv_sc;
    int vfo_list;

    ENTERFUNC;

    switch (op)
    {
    case RIG_OP_CPY:
        mv_cn = C_SET_VFO;
        vfo_list = rig->state.vfo_list;

        if ((vfo_list & (RIG_VFO_A | RIG_VFO_B)) == (RIG_VFO_A | RIG_VFO_B))
        {
            mv_sc = S_BTOA;
        }
        else if ((vfo_list & (RIG_VFO_MAIN | RIG_VFO_SUB)) == (RIG_VFO_MAIN |
                 RIG_VFO_SUB))
        {
            mv_sc = S_SUBTOMAIN;
        }
        else
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        break;

    case RIG_OP_XCHG:
        mv_cn = C_SET_VFO;
        mv_sc = S_XCHNG;
        break;

    case RIG_OP_FROM_VFO:
        mv_cn = C_WR_MEM;
        mv_sc = -1;
        break;

    case RIG_OP_TO_VFO:
        mv_cn = C_MEM2VFO;
        mv_sc = -1;
        break;

    case RIG_OP_MCL:
        mv_cn = C_CLR_MEM;
        mv_sc = -1;
        break;

    case RIG_OP_TUNE:
        mv_cn = C_CTL_PTT;
        mv_sc = S_ANT_TUN;
        mvbuf[0] = 2;
        mv_len = 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mem/vfo op %#x", __func__,
                  op);
        RETURNFUNC(-RIG_EINVAL);
    }

    retval =
        icom_transaction(rig, mv_cn, mv_sc, mvbuf, mv_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    // since we're messing with VFOs our cache may be invalid
    CACHE_RESET;

    if ((ack_len >= 1 && ackbuf[0] != ACK) && (ack_len >= 2 && ackbuf[1] != NAK))
    {
        //  if we don't get ACK/NAK some serial corruption occurred
        // so we'll call it a timeout for retry purposes
        RETURNFUNC(-RIG_ETIMEOUT);
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        if (op != RIG_OP_XCHG)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
        }

        RETURNFUNC(-RIG_ERJCTED);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_scan, scan operation
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    unsigned char scanbuf[MAXFRAMELEN];
    unsigned char ackbuf[MAXFRAMELEN];
    int scan_len, ack_len = sizeof(ackbuf), retval;
    int scan_cn, scan_sc;

    ENTERFUNC;
    scan_len = 0;
    scan_cn = C_CTL_SCAN;

    switch (scan)
    {
    case RIG_SCAN_STOP:
        scan_sc = S_SCAN_STOP;
        break;

    case RIG_SCAN_MEM:
        HAMLIB_TRACE;
        retval = rig_set_vfo(rig, RIG_VFO_MEM);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        /* Looks like all the IC-R* have this command,
         * but some old models don't have it.
         * Should be put in icom_priv_caps ?
         */
        if (rig->caps->rig_type == RIG_TYPE_RECEIVER)
        {
            scan_sc = S_SCAN_MEM2;
        }
        else
        {
            scan_sc = S_SCAN_START;
        }

        break;

    case RIG_SCAN_SLCT:
        HAMLIB_TRACE;
        retval = rig_set_vfo(rig, RIG_VFO_MEM);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        scan_sc = S_SCAN_START;
        break;

    case RIG_SCAN_PRIO:
    case RIG_SCAN_PROG:
        /* TODO: for SCAN_PROG, check this is an edge chan */
        /* BTW, I'm wondering if this is possible with CI-V */
        retval = icom_set_mem(rig, RIG_VFO_CURR, ch);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        HAMLIB_TRACE;
        retval = rig_set_vfo(rig, RIG_VFO_VFO);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        scan_sc = S_SCAN_START;
        break;

    case RIG_SCAN_DELTA:
        scan_sc = S_SCAN_DELTA;   /* TODO: delta-f support */
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported scan %#x", __func__, scan);
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = icom_transaction(rig, scan_cn, scan_sc, scanbuf, scan_len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_send_morse
 * Assumes rig!=NULL, msg!=NULL
 */
int icom_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    int len;
    int retry = 20;

    ENTERFUNC;
    len = strlen(msg);

    if (len > 30)
    {
        len = 30;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: %s\n", __func__, msg);

morse_retry:
    retval = icom_transaction(rig, C_SND_CW, -1, (unsigned char *) msg, len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        if (retval == -RIG_ERJCTED)
        {
            if (len == 1 && --retry > 0)
            {
                // 50 retries should be around 200ms --plenty of time to clear out some characters
                hl_usleep(10 * 1000);
                goto morse_retry;
            }
        }

        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_stop_morse
 * Assumes rig!=NULL, msg!=NULL
 */
int icom_stop_morse(RIG *rig, vfo_t vfo)
{
    unsigned char ackbuf[MAXFRAMELEN];
    unsigned char cmd[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;

    cmd[0] = 0xff;

    retval = icom_transaction(rig, C_SND_CW, -1, (unsigned char *) cmd, 1,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

int icom_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq,
                  rmode_t mode)
{
    int rig_id;

    ENTERFUNC;
    rig_id = rig->caps->rig_model;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (rig_id)
    {
    case RIG_MODEL_IC705:
    case RIG_MODEL_IC905:
        *mwpower = power * 10000;
        break;

    default:
        /* Normal 100 Watts */
        *mwpower = power * 100000;
        break;
    }

    RETURNFUNC(RIG_OK);
}

int icom_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq,
                  rmode_t mode)
{
    int rig_id;

    ENTERFUNC;
    rig_id = rig->caps->rig_model;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed mwpower = %u\n", __func__,
              mwpower);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %" PRIfreq " Hz\n",
              __func__, freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));

    if (mwpower > 100000)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (rig_id)
    {
    default:            /* Default to a 100W radio */
        *power = ((float) mwpower / 100000);
        break;
    }

    RETURNFUNC(RIG_OK);
}


#if defined(HAVE_PTHREAD)
static int icom_parse_spectrum_frame(RIG *rig, size_t length,
                                     const unsigned char *frame_data)
{
    struct rig_caps *caps = rig->caps;
    struct icom_priv_caps *priv_caps = (struct icom_priv_caps *) caps->priv;
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;
    struct icom_spectrum_scope_cache *cache;

    int division = (int) from_bcd(frame_data + 1, 1 * 2);
    int max_division = (int) from_bcd(frame_data + 2, 1 * 2);

    size_t spectrum_data_length_in_frame;
    const unsigned char *spectrum_data_start_in_frame;

    ENTERFUNC;

    // The first byte indicates spectrum scope ID/VFO: 0 = Main, 1 = Sub
    int spectrum_id = frame_data[0];

    if (spectrum_id >= priv->spectrum_scope_count)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid spectrum scope ID from CI-V frame: %d\n",
                  __func__, spectrum_id);
        RETURNFUNC(-RIG_EPROTO);
    }

    cache = &priv->spectrum_scope_cache[spectrum_id];

    if (division == 1)
    {
        int spectrum_scope_mode = frame_data[3];
        int out_of_range = frame_data[14];

        cache->spectrum_mode = RIG_SPECTRUM_MODE_NONE;

        switch (spectrum_scope_mode)
        {
        case SCOPE_MODE_CENTER:
            cache->spectrum_mode = RIG_SPECTRUM_MODE_CENTER;
            cache->spectrum_center_freq = (freq_t) from_bcd(frame_data + 4, 5 * 2);
            cache->spectrum_span_freq = (freq_t) from_bcd(frame_data + 9, 5 * 2) * 2;
            cache->spectrum_low_edge_freq = cache->spectrum_center_freq -
                                            cache->spectrum_span_freq / 2;
            cache->spectrum_high_edge_freq = cache->spectrum_center_freq +
                                             cache->spectrum_span_freq / 2;
            break;

        case SCOPE_MODE_FIXED:
            cache->spectrum_mode = RIG_SPECTRUM_MODE_FIXED;

        case SCOPE_MODE_SCROLL_C:
            if (cache->spectrum_mode == RIG_SPECTRUM_MODE_NONE)
            {
                cache->spectrum_mode = RIG_SPECTRUM_MODE_CENTER_SCROLL;
            }

        case SCOPE_MODE_SCROLL_F:
            if (cache->spectrum_mode == RIG_SPECTRUM_MODE_NONE)
            {
                cache->spectrum_mode = RIG_SPECTRUM_MODE_FIXED_SCROLL;
            }

            cache->spectrum_low_edge_freq = (freq_t) from_bcd(frame_data + 4, 5 * 2);
            cache->spectrum_high_edge_freq = (freq_t) from_bcd(frame_data + 9, 5 * 2);
            cache->spectrum_span_freq = (cache->spectrum_high_edge_freq -
                                         cache->spectrum_low_edge_freq);
            cache->spectrum_center_freq = cache->spectrum_high_edge_freq -
                                          cache->spectrum_span_freq / 2;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unknown Icom spectrum scope mode: %d\n", __func__,
                      spectrum_scope_mode);
            RETURNFUNC(-RIG_EPROTO);
        }

        spectrum_data_length_in_frame = length - 15;
        spectrum_data_start_in_frame = frame_data + 15;

        memset(cache->spectrum_data, 0,
               priv_caps->spectrum_scope_caps.spectrum_line_length);

        cache->spectrum_data_length = 0;
        cache->spectrum_metadata_valid = 1;

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Spectrum line start: id=%d division=%d max_division=%d mode=%d center=%.0f span=%.0f low_edge=%.0f high_edge=%.0f oor=%d data_length=%d\n",
                  __func__, spectrum_id, division, max_division, spectrum_scope_mode,
                  cache->spectrum_center_freq, cache->spectrum_span_freq,
                  cache->spectrum_low_edge_freq, cache->spectrum_high_edge_freq, out_of_range,
                  (int)spectrum_data_length_in_frame);
    }
    else
    {
        spectrum_data_length_in_frame = length - 3;
        spectrum_data_start_in_frame = frame_data + 3;
    }

    if (spectrum_data_length_in_frame > 0)
    {
        int frame_length = priv_caps->spectrum_scope_caps.single_frame_data_length;
        int data_frame_index = (max_division > 1) ? (division - 2) : (division - 1);
        int offset = data_frame_index * frame_length;

        if (offset + spectrum_data_length_in_frame >
                priv_caps->spectrum_scope_caps.spectrum_line_length)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: too much spectrum scope data received: %d bytes > %d bytes expected\n",
                      __func__, (int)(offset + spectrum_data_length_in_frame),
                      priv_caps->spectrum_scope_caps.spectrum_line_length);
            RETURNFUNC(-RIG_EPROTO);
        }

        memcpy(cache->spectrum_data + offset, spectrum_data_start_in_frame,
               spectrum_data_length_in_frame);
        cache->spectrum_data_length = offset + spectrum_data_length_in_frame;
    }

    if (cache->spectrum_metadata_valid && division == max_division)
    {
        struct rig_spectrum_line spectrum_line =
        {
            .id = spectrum_id,
            .data_level_min = priv_caps->spectrum_scope_caps.data_level_min,
            .data_level_max = priv_caps->spectrum_scope_caps.data_level_max,
            .signal_strength_min = priv_caps->spectrum_scope_caps.signal_strength_min,
            .signal_strength_max = priv_caps->spectrum_scope_caps.signal_strength_max,
            .spectrum_mode = cache->spectrum_mode,
            .center_freq = cache->spectrum_center_freq,
            .span_freq = cache->spectrum_span_freq,
            .low_edge_freq = cache->spectrum_low_edge_freq,
            .high_edge_freq = cache->spectrum_high_edge_freq,
            .spectrum_data_length = cache->spectrum_data_length,
            .spectrum_data = cache->spectrum_data,
        };

#if defined(HAVE_PTHREAD)
        rig_fire_spectrum_event(rig, &spectrum_line);
#endif

        cache->spectrum_metadata_valid = 0;
    }

    RETURNFUNC(RIG_OK);
}
#endif

int icom_is_async_frame(RIG *rig, size_t frame_length,
                        const unsigned char *frame)
{
    if (frame_length < ACKFRMLEN)
    {
        return 0;
    }

    /* Spectrum scope data is not CI-V transceive data, but handled the same way as it is pushed by the rig */
    return frame[2] == BCASTID || (frame[2] == CTRLID && frame[4] == C_CTL_SCP
                                   && frame[5] == S_SCP_DAT);
}

int icom_process_async_frame(RIG *rig, size_t frame_length,
                             const unsigned char *frame)
{
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;
    rmode_t mode;
    pbwidth_t width;

    ENTERFUNC;

    /*
     * the first 2 bytes must be 0xfe
     * the 3rd one 0x00 since this is transceive mode
     * the 4th one the emitter
     * then the command number
     * the rest is data
     * and don't forget one byte at the end for the EOM
     */
    switch (frame[4])
    {
    case C_SND_FREQ:
    {
        // TODO: The freq length might be less than 4 or 5 bytes on older rigs!
        // TODO: Disable cache timeout for frequency after first transceive packet once we figure out how to get active VFO reliably with transceive updates
        // TODO: rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_FREQ, HAMLIB_CACHE_ALWAYS);
        freq_t freq = (freq_t) from_bcd(frame + 5, (priv->civ_731_mode ? 4 : 5) * 2);
#if defined(HAVE_PTHREAD)
        rig_fire_freq_event(rig, RIG_VFO_CURR, freq);
#endif

#if 0

        if (rs->use_cached_freq != 1)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): use_cached_freq turning on\n", __func__,
                      __LINE__);
            rs->use_cached_freq = 0;
        }

#endif

        break;
    }

    case C_SND_MODE:
        // TODO: Disable cache timeout for frequency after first transceive packet once we figure out how to get active VFO reliably with transceive updates
        // TODO: rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_MODE, HAMLIB_CACHE_ALWAYS);
        icom2rig_mode(rig, frame[5], frame[6], &mode, &width);
#if defined(HAVE_PTHREAD)
        rig_fire_mode_event(rig, RIG_VFO_CURR, mode, width);
#endif

        if (rs->use_cached_mode != 1)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): use_cached_mode turning on\n", __func__,
                      __LINE__);
            rs->use_cached_mode = 0;
        }

        break;

#if defined(HAVE_PTHREAD)

    case C_CTL_SCP:
        if (frame[5] == S_SCP_DAT)
        {
            icom_parse_spectrum_frame(rig, frame_length - (6 + 1), frame + 6);
        }

        break;
#endif

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: transceive cmd unsupported %#2.2x\n",
                  __func__, frame[4]);
        RETURNFUNC(-RIG_ENIMPL);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_decode is called by sa_sigio, when some asynchronous
 * data has been received from the rig
 */
int icom_decode_event(RIG *rig)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char buf[MAXFRAMELEN];
    int retval, frm_len;

    ENTERFUNC;

    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    frm_len = read_icom_frame(RIGPORT(rig), buf, sizeof(buf));

    if (frm_len == -RIG_ETIMEOUT)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: got a timeout before the first character\n", __func__);
        RETURNFUNC(-RIG_ETIMEOUT);
    }

    if (frm_len < 1)
    {
        RETURNFUNC(RIG_OK);
    }

    retval = icom_frame_fix_preamble(frm_len, buf);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    frm_len = retval;

    if (frm_len < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "Unexpected frame len=%d\n", frm_len);
        RETURNFUNC(-RIG_EPROTO);
    }


    switch (buf[frm_len - 1])
    {
    case COL:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: saw a collision\n", __func__);
        /* Collision */
        RETURNFUNC(-RIG_BUSBUSY);

    case FI:
        /* Ok, normal frame */
        break;

    default:
        /* Timeout after reading at least one character */
        /* Problem on ci-v bus? */
        RETURNFUNC(-RIG_EPROTO);
    }

    if (!icom_is_async_frame(rig, frm_len, buf))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: CI-V %#x called for %#x!\n", __func__,
                  priv->re_civ_addr, buf[2]);
    }

    RETURNFUNC(icom_process_async_frame(rig, frm_len, buf));
}

int icom_read_frame_direct(RIG *rig, size_t buffer_length,
                           const unsigned char *buffer)
{
    return read_icom_frame_direct(RIGPORT(rig), buffer, buffer_length);
}

int icom_set_raw(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                 unsigned char *subcmdbuf, int val_bytes, int val)
{
    unsigned char cmdbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    int cmdbuflen = subcmdbuflen;
    int retval;

    ENTERFUNC;

    if (subcmdbuflen > 0)
    {
        if (subcmdbuf == NULL)
        {
            RETURNFUNC(-RIG_EINTERNAL);
        }

        memcpy(cmdbuf, subcmdbuf, subcmdbuflen);
    }

    if (val_bytes > 0)
    {
        to_bcd_be(cmdbuf + subcmdbuflen, (long long) val, val_bytes * 2);
        cmdbuflen += val_bytes;
    }

    retval =
        icom_transaction(rig, cmd, subcmd, cmdbuf, cmdbuflen, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

int icom_get_raw_buf(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                     unsigned char *subcmdbuf, int *reslen,
                     unsigned char *res)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    int cmdhead = subcmdbuflen;
    int retval;

    ENTERFUNC;

    retval =
        icom_transaction(rig, cmd, subcmd, subcmdbuf, subcmdbuflen, ackbuf,
                         &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    cmdhead += (subcmd == -1) ? 1 : 2;
    ack_len -= cmdhead;

    rig_debug(RIG_DEBUG_TRACE, "%s: ack_len=%d\n", __func__, ack_len);

    if (ack_len < 0)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    if (*reslen < ack_len || res == NULL)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    memcpy(res, ackbuf + cmdhead, ack_len);
    *reslen = ack_len;

    RETURNFUNC(RIG_OK);
}

int icom_get_raw(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                 unsigned char *subcmdbuf, int *val)
{
    unsigned char resbuf[MAXFRAMELEN];
    int reslen = sizeof(resbuf);
    int retval;

    ENTERFUNC;

    retval =
        icom_get_raw_buf(rig, cmd, subcmd, subcmdbuflen, subcmdbuf, &reslen,
                         resbuf);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    * val = from_bcd_be(resbuf, reslen * 2);

    rig_debug(RIG_DEBUG_TRACE, "%s: %d %d\n", __func__, reslen, *val);

    RETURNFUNC(RIG_OK);
}

int icom_set_level_raw(RIG *rig, setting_t level, int cmd, int subcmd,
                       int subcmdbuflen, unsigned char *subcmdbuf,
                       int val_bytes, value_t val)
{
    int icom_val;

    ENTERFUNC;

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        icom_val = (int)(val.f * 255.0f);
    }
    else
    {
        icom_val = val.i;
    }

    RETURNFUNC(icom_set_raw(rig, cmd, subcmd, subcmdbuflen, subcmdbuf, val_bytes,
                            icom_val));
}

int icom_get_level_raw(RIG *rig, setting_t level, int cmd, int subcmd,
                       int subcmdbuflen, unsigned char *subcmdbuf,
                       value_t *val)
{
    int icom_val;
    int retval;

    ENTERFUNC;

    retval =
        icom_get_raw(rig, cmd, subcmd, subcmdbuflen, subcmdbuf, &icom_val);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        val->f = (float) icom_val / 255.0f;
    }
    else
    {
        val->i = icom_val;
    }

    RETURNFUNC(RIG_OK);
}

int icom_stop_voice_mem(RIG *rig, vfo_t vfo)
{
    return icom_send_voice_mem(rig, vfo, 0);
}
/*
 * icom_send_voice_mem
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_send_voice_mem(RIG *rig, vfo_t vfo, int ch)
{
    unsigned char chbuf[1];
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;

    to_bcd_be(chbuf, ch, 2);

    retval = icom_transaction(rig, C_SND_VOICE, 0, chbuf, 1,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_get_freq_range
 * Assumes rig!=NULL, rig->state.priv!=NULL
 * Always returns RIG_OK
 */
int icom_get_freq_range(RIG *rig)
{
    int nrange = 0;
    int i;
    int cmd, subcmd;
    int retval;
    unsigned char cmdbuf[MAXFRAMELEN];
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
//    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;
//    int freq_len = priv->civ_731_mode ? 4 : 5;
    int freq_len = 5;

    cmd = C_CTL_EDGE;
    subcmd = 0;
    retval = icom_transaction(rig, cmd, subcmd, NULL, 0, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: rig does not have 0x1e command so skipping this check\n", __func__);
        RETURNFUNC2(RIG_OK);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: ackbuf[0]=%02x, ackbuf[1]=%02x\n", __func__,
              ackbuf[0], ackbuf[1]);
    nrange = from_bcd(&ackbuf[2], 2);
    rig_debug(RIG_DEBUG_TRACE, "%s: nrange=%d\n", __func__, nrange);

    for (i = 1; i <= nrange; ++i)
    {
        cmd = C_CTL_EDGE;
        subcmd = 1;
        to_bcd(cmdbuf, i, 2);
        retval = icom_transaction(rig, cmd, subcmd, cmdbuf, 1, ackbuf,
                                  &ack_len);

        if (retval == RIG_OK)
        {
            freq_t freqlo, freqhi;
            rig_debug(RIG_DEBUG_TRACE, "%s: ackbuf= %02x %02x %02x %02x...\n", __func__,
                      ackbuf[0], ackbuf[1], ackbuf[2], ackbuf[3]);
            freqlo = from_bcd(&ackbuf[3], freq_len * 2);
            freqhi = from_bcd(&ackbuf[3 + freq_len + 1], freq_len * 2);
            rig_debug(RIG_DEBUG_TRACE, "%s: rig chan %d, low=%.0f, high=%.0f\n", __func__,
                      i, freqlo, freqhi);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error from C_CTL_EDGE?  err=%s\n", __func__,
                      rigerror(retval));
        }
    }

    // To be implemented
    // Automatically fill in the freq range for this rig if available
    rig_debug(RIG_DEBUG_TRACE, "%s: Hamlib ranges\n", __func__);

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(rig->caps->rx_range_list1[i]); i++)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: rig chan %d, low=%.0f, high=%.0f\n", __func__,
                  i, (double)rig->caps->rx_range_list1[i].startf,
                  (double)rig->caps->rx_range_list1[i].endf);
    }

    RETURNFUNC2(RIG_OK);
}

// Sets rig vfo && rig->state.current_vfo to default VFOA, or current vfo, or the vfo requested
static int set_vfo_curr(RIG *rig, vfo_t vfo, vfo_t curr_vfo)
{
    int retval;
    struct rig_state *rs = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s, curr_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(curr_vfo));

    if (vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Asking for currVFO,  currVFO=%s\n", __func__,
                  rig_strvfo(rs->current_vfo));
        vfo = rs->current_vfo;
        RETURNFUNC2(RIG_OK);
    }

    if (vfo == RIG_VFO_MAIN && VFO_HAS_A_B_ONLY)
    {
        vfo = RIG_VFO_A;
        rig_debug(RIG_DEBUG_TRACE, "%s: Rig does not have MAIN/SUB so Main=%s\n",
                  __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_SUB && VFO_HAS_A_B_ONLY)
    {
        vfo = RIG_VFO_B;
        rig_debug(RIG_DEBUG_TRACE, "%s: Rig does not have MAIN/SUB so Sub=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    /* This method works also in memory mode(RIG_VFO_MEM) */
    // first time we will set default to VFOA or Main as
    // So if you ask for frequency or such without setting VFO first you'll get Main/VFOA
    if (rs->current_vfo == RIG_VFO_NONE && vfo == RIG_VFO_CURR)
    {
        HAMLIB_TRACE;
        icom_set_default_vfo(rig);
    }
    // asking for vfo_curr so give it to them
    else if (rs->current_vfo != RIG_VFO_NONE && vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: using curr_vfo=%s\n", __func__,
                  rig_strvfo(rs->current_vfo));
        vfo = rs->current_vfo;
    }
    // only need to set vfo if it's changed
    else if (rs->current_vfo != vfo)
    {
        if (!(VFO_HAS_MAIN_SUB_A_B_ONLY && CACHE(rig)->split == RIG_SPLIT_OFF
	        && !CACHE(rig)->satmode
                && vfo == RIG_VFO_SUB && rs->current_vfo == RIG_VFO_B))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: setting new vfo=%s\n", __func__,
                      rig_strvfo(vfo));
            HAMLIB_TRACE;
            retval = rig_set_vfo(rig, vfo);

            if (retval != RIG_OK)
            {
                RETURNFUNC2(retval);
            }
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: curr_vfo now=%s\n", __func__,
              rig_strvfo(rs->current_vfo));

    rs->current_vfo = vfo;

    RETURNFUNC2(RIG_OK);
}

static int icom_get_spectrum_vfo(RIG *rig, vfo_t vfo)
{
    if (rig->state.targetable_vfo & RIG_TARGETABLE_SPECTRUM)
    {
        RETURNFUNC2(ICOM_GET_VFO_NUMBER(vfo));
    }

    RETURNFUNC2(0);
}

static int icom_get_spectrum_edge_frequency_range(RIG *rig, vfo_t vfo,
        int *range_id)
{
    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
    int cache_ms_freq, cache_ms_mode, cache_ms_width;
    int i, retval;
    struct icom_priv_caps *priv_caps = (struct icom_priv_caps *) rig->caps->priv;

    retval = rig_get_cache(rig, vfo, &freq, &cache_ms_freq, &mode, &cache_ms_mode,
                           &width, &cache_ms_width);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    // Get frequency if it is not cached or value is old
    if (freq == 0 || cache_ms_freq >= 1000)
    {
        retval = rig_get_freq(rig, vfo, &freq);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    for (i = 0; i < ICOM_MAX_SPECTRUM_FREQ_RANGES; i++)
    {
        int id = priv_caps->spectrum_edge_frequency_ranges[i].range_id;

        if (id < 1)
        {
            break;
        }

        if (freq >= priv_caps->spectrum_edge_frequency_ranges[i].low_freq
                && freq < priv_caps->spectrum_edge_frequency_ranges[i].high_freq)
        {
            *range_id = id;
            RETURNFUNC2(RIG_OK);
        }
    }

    RETURNFUNC2(-RIG_EINVAL);
}

/*
 * init_icom is called by rig_probe_all (register.c)
 *
 * probe_icom reports all the devices on the CI-V bus.
 *
 * rig_model_t probeallrigs_icom(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(icom)
{
    unsigned char buf[MAXFRAMELEN], civ_addr, civ_id;
    int frm_len, i;
    rig_model_t model = RIG_MODEL_NONE;
    int rates[] = { 19200, 9600, 300, 0 };
    int rates_idx;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!port)
    {
        return (RIG_MODEL_NONE);
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return (RIG_MODEL_NONE);
    }

    port->write_delay = port->post_write_delay = 0;
    port->retry = 1;

    /*
     * try for all different baud rates
     */
    for (rates_idx = 0; rates[rates_idx]; rates_idx++)
    {
        int retval;
        port->parm.serial.rate = rates[rates_idx];
        port->timeout = 2 * 1000 / rates[rates_idx] + 40;

        retval = serial_open(port);

        if (retval != RIG_OK)
        {
            return (RIG_MODEL_NONE);
        }

        /*
         * try all possible addresses on the CI-V bus
         * FIXME: actually, old rigs do not support C_RD_TRXID cmd!
         *      Try to be smart, and deduce model depending
         *      on freq range, return address, and
         *      available commands.
         */
        for (civ_addr = 0x01; civ_addr <= 0x7f; civ_addr++)
        {

            frm_len = make_cmd_frame(buf, civ_addr, CTRLID,
                                     C_RD_TRXID, S_RD_TRXID, NULL, 0);

            rig_flush(port);
            write_block(port, buf, frm_len);

            /* read out the bytes we just sent
             * TODO: check this is what we expect
             */
            read_icom_frame(port, buf, sizeof(buf));

            /* this is the reply */
            frm_len = read_icom_frame(port, buf, sizeof(buf));

            /* timeout.. nobody's there */
            if (frm_len <= 0)
            {
                continue;
            }

            if (buf[7] != FI && buf[5] != FI)
            {
                /* protocol error, unexpected reply.
                 * is this a CI-V device?
                 */
                close(port->fd);
                return (RIG_MODEL_NONE);
            }
            else if (buf[4] == NAK)
            {
                /*
                 * this is an Icom, but it does not support transceiver ID
                 * try to guess from the return address
                 */
                civ_id = buf[3];
            }
            else
            {
                civ_id = buf[6];
            }

            for (i = 0; icom_addr_list[i].model != RIG_MODEL_NONE; i++)
            {
                if (icom_addr_list[i].re_civ_addr == civ_id)
                {
                    rig_debug(RIG_DEBUG_VERBOSE, "%s: found %#x at %#x\n",
                              __func__, civ_id, buf[3]);
                    model = icom_addr_list[i].model;

                    if (cfunc)
                    {
                        (*cfunc)(port, model, data);
                    }

                    break;
                }
            }

            /*
             * not found in known table....
             * update icom_addr_list[]!
             */
            if (icom_addr_list[i].model == RIG_MODEL_NONE)
                rig_debug(RIG_DEBUG_WARN, "%s: found unknown device "
                          "with CI-V ID %#x, please report to Hamlib "
                          "developers.\n", __func__, civ_id);
        }

        /*
         * Try to identify OptoScan
         */
        for (civ_addr = 0x80; civ_addr <= 0x8f; civ_addr++)
        {

            frm_len = make_cmd_frame(buf, civ_addr, CTRLID,
                                     C_CTL_MISC, S_OPTO_RDID, NULL, 0);

            rig_flush(port);
            write_block(port, buf, frm_len);

            /* read out the bytes we just sent
             * TODO: check this is what we expect
             */
            read_icom_frame(port, buf, sizeof(buf));

            /* this is the reply */
            frm_len = read_icom_frame(port, buf, sizeof(buf));

            /* timeout.. nobody's there */
            if (frm_len <= 0)
            {
                continue;
            }

            /* wrong protocol? */
            if (frm_len != 7 || buf[4] != C_CTL_MISC || buf[5] != S_OPTO_RDID)
            {
                continue;
            }

            rig_debug(RIG_DEBUG_VERBOSE, "%s: "
                      "found OptoScan%c%c%c, software version %d.%d, "
                      "interface version %d.%d, at %#x\n",
                      __func__,
                      buf[2], buf[3], buf[4],
                      buf[5] >> 4, buf[5] & 0xf,
                      buf[6] >> 4, buf[6] & 0xf, civ_addr);

            if (buf[6] == '5' && buf[7] == '3' && buf[8] == '5')
            {
                model = RIG_MODEL_OS535;
            }
            else if (buf[6] == '4' && buf[7] == '5' && buf[8] == '6')
            {
                model = RIG_MODEL_OS456;
            }
            else
            {
                continue;
            }

            if (cfunc)
            {
                (*cfunc)(port, model, data);
            }

            break;
        }

        close(port->fd);

        /*
         * Assumes all the rigs on the bus are running at same speed.
         * So if one at least has been found, none will be at lower speed.
         */
        if (model != RIG_MODEL_NONE)
        {
            return (model);
        }
    }

    return (model);
}

static int icom_is_x25x26_potentially_supported(RIG *rig)
{
    const struct icom_priv_caps *priv_caps = rig->caps->priv;

    // Handle rigs that will never support the 0x25 or 0x26 commands
    // The IC-751 doesn't even reject the command and has to time out
    if (priv_caps->x25x26_possibly || priv_caps->x25x26_always)
    {
        return 1;
    }

    return 0;
}

static void icom_set_x25x26_ability(RIG *rig, int status)
{
    struct icom_priv_data *priv = rig->state.priv;

    if (!icom_is_x25x26_potentially_supported(rig))
    {
        // No change for rigs that don't support these commands anyway
        return;
    }

    priv->x25cmdfails = status;
    priv->x26cmdfails = status;
}

/**
 * Resolves the VFO number for Icom commands 0x25 and 0x26
 */
static int icom_get_vfo_number_x25x26(RIG *rig, vfo_t vfo)
{
    int vfo_number = 0x00;
    struct rig_cache *cachep = CACHE(rig);

    // Rigs with *only* Main/Sub VFOs can directly address VFOs: 0 = Main, 1 = Sub
    if (RIG_IS_IC7600 || RIG_IS_IC7610 || RIG_IS_IC7800 || RIG_IS_IC785X)
    {
        vfo_t actual_vfo = vfo_fixup(rig, vfo, cachep->split);

        if (actual_vfo == RIG_VFO_CURR)
        {
            actual_vfo = rig->state.current_vfo;
        }

        if (actual_vfo & (RIG_VFO_B | RIG_VFO_SUB))
        {
            vfo_number = 0x01;
        }
    }
    else
    {
        // Other Icom rigs have: 0 = selected VFO, 1 = unselected VFO
        if (vfo == RIG_VFO_CURR)
        {
            vfo_number = 0x00; // selected VFO
        }
        else if (vfo == RIG_VFO_OTHER)
        {
            vfo_number = 0x01; // unselected VFO
        }
        else
        {
            vfo_t vfo_unselected =
                RIG_VFO_B | RIG_VFO_SUB | RIG_VFO_SUB_B | RIG_VFO_MAIN_B | RIG_VFO_OTHER;

            // Check if we are on the requested VFO already
            if (rig->state.current_vfo & vfo_unselected)
            {
                HAMLIB_TRACE;
                vfo_unselected =
                    RIG_VFO_A | RIG_VFO_MAIN | RIG_VFO_SUB_A | RIG_VFO_MAIN_A | RIG_VFO_OTHER;
            }

            // Check if we are not on the unselected VFO
            if ((vfo & vfo_unselected) && !(rig->state.current_vfo & vfo_unselected))
            {
                HAMLIB_TRACE;
                vfo_number = 0x01;  // unselected VFO
            }

            // The split VFO is active when transmitting in split mode
            vfo_number = (cachep->split && cachep->ptt) ? !vfo_number : vfo_number;
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s(%d): current_vfo=%s, vfo=%s -> vfo_number=%d\n", __func__, __LINE__,
              rig_strvfo(rig->state.current_vfo), rig_strvfo(vfo), vfo_number);

    return vfo_number;
}

/*
 * initrigs_icom is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(icom)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&ic703_caps);
    rig_register(&ic705_caps);
    rig_register(&ic706_caps);
    rig_register(&ic706mkii_caps);
    rig_register(&ic706mkiig_caps);
    rig_register(&ic718_caps);
    rig_register(&ic725_caps);
    rig_register(&ic726_caps);
    rig_register(&ic735_caps);
    rig_register(&ic736_caps);
    rig_register(&ic737_caps);
    rig_register(&ic738_caps);
    rig_register(&ic7410_caps);
    rig_register(&ic746_caps);
    rig_register(&ic746pro_caps);
    rig_register(&ic751_caps);
    rig_register(&ic761_caps);
    rig_register(&ic775_caps);
    rig_register(&ic756_caps);
    rig_register(&ic756pro_caps);
    rig_register(&ic756pro2_caps);
    rig_register(&ic756pro3_caps);
    rig_register(&ic7600_caps);
    rig_register(&ic765_caps);
    rig_register(&ic7700_caps);
    rig_register(&ic78_caps);
    rig_register(&ic7800_caps);
    rig_register(&ic785x_caps);
    rig_register(&ic7000_caps);
    rig_register(&ic7100_caps);
    rig_register(&ic7200_caps);
    rig_register(&ic7300_caps);
    rig_register(&ic7610_caps);
    rig_register(&ic781_caps);
    rig_register(&ic707_caps);
    rig_register(&ic728_caps);
    rig_register(&ic729_caps);

    rig_register(&ic820h_caps);
    rig_register(&ic821h_caps);
    rig_register(&ic905_caps);
    rig_register(&ic910_caps);
    rig_register(&ic9100_caps);
    rig_register(&ic970_caps);
    rig_register(&ic9700_caps);

    rig_register(&icrx7_caps);
    rig_register(&icr6_caps);
    rig_register(&icr10_caps);
    rig_register(&icr20_caps);
    rig_register(&icr30_caps);
    rig_register(&icr71_caps);
    rig_register(&icr72_caps);
    rig_register(&icr75_caps);
    rig_register(&icr7000_caps);
    rig_register(&icr7100_caps);
    rig_register(&icr8500_caps);
    rig_register(&icr8600_caps);
    rig_register(&icr9000_caps);
    rig_register(&icr9500_caps);

    rig_register(&ic271_caps);
    rig_register(&ic275_caps);
    rig_register(&ic375_caps);
    rig_register(&ic471_caps);
    rig_register(&ic475_caps);
    rig_register(&ic575_caps);
    rig_register(&ic1275_caps);
    rig_register(&icf8101_caps);

    rig_register(&os535_caps);
    rig_register(&os456_caps);

    rig_register(&omnivip_caps);
    rig_register(&delta2_caps);

    rig_register(&ic92d_caps);
    rig_register(&id1_caps);
    rig_register(&id31_caps);
    rig_register(&id51_caps);
    rig_register(&id4100_caps);
    rig_register(&id5100_caps);
    rig_register(&ic2730_caps);

    rig_register(&perseus_caps);

    rig_register(&x108g_caps);
    rig_register(&x6100_caps);
    rig_register(&g90_caps);
    rig_register(&x5105_caps);

    return (RIG_OK);
}
