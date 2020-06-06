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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// cppcheck-suppress *
#include <stdio.h>
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
#include <misc.h>
#include <cal.h>
#include <token.h>
#include <register.h>

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"

static int set_vfo_curr(RIG *rig, vfo_t vfo, vfo_t curr_vfo);

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
    3,
    {
        {0, 0.0f},
        {143, 0.5f},
        {213, 1.0f}
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

struct icom_addr
{
    rig_model_t model;
    unsigned char re_civ_addr;
};


#define TOK_CIVADDR TOKEN_BACKEND(1)
#define TOK_MODE731 TOKEN_BACKEND(2)
#define TOK_NOXCHG TOKEN_BACKEND(3)

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
    {RIG_CONF_END, NULL,}
};

/*
 *  Lookup table for icom_get_ext_func
 */
const struct confparams icom_ext_funcs[] =
{
    {}
};

/*
 *  Lookup table for icom_get_ext_level
 */
const struct confparams icom_ext_levels[] =
{
    {}
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
    { TOK_DRIVE_GAIN, "drive_gain", "Drive gain", "", "", RIG_CONF_NUMERIC, {} },
    { TOK_DIGI_SEL_FUNC, "digi_sel", "DIGI-SEL enable", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_DIGI_SEL_LEVEL, "digi_sel_level", "DIGI-SEL level", "", "", RIG_CONF_NUMERIC, {} },
    { TOK_SCOPE_DAT, "scpdat", "Scope data", "", "", RIG_CONF_BINARY, {} },
    { TOK_SCOPE_STS, "scpsts", "Scope status", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_SCOPE_DOP, "scpdop", "Scope data output", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_SCOPE_MSS, "scpmss", "Scope main/sub", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_SCOPE_MOD, "scpmod", "Scope mode", "", "", RIG_CONF_BINARY, {} },
    { TOK_SCOPE_SPN, "scpspn", "Scope span", "", "", RIG_CONF_BINARY, {} },
    { TOK_SCOPE_EDG, "scpedg", "Scope edge", "", "", RIG_CONF_BINARY, {} },
    { TOK_SCOPE_HLD, "scphld", "Scope hold", "", "", RIG_CONF_BINARY, {} },
    { TOK_SCOPE_REF, "scpref", "Scope reference", "", "", RIG_CONF_BINARY, {} },
    { TOK_SCOPE_SWP, "scpswp", "Scope sweep", "", "", RIG_CONF_BINARY, {} },
    { TOK_SCOPE_STX, "scpstx", "Scope Tx o/p", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_SCOPE_TYP, "scptyp", "Scope type", "", "", RIG_CONF_NUMERIC, {} },
    { TOK_SCOPE_VBW, "scpvbw", "Scope vbw", "", "", RIG_CONF_BINARY, {} },
    { TOK_SCOPE_FEF, "scpfef", "Scope edge", "", "", RIG_CONF_BINARY, {} },
    { RIG_CONF_END, NULL, }
};

/*
 *  Lookup table for icom_get_ext_* & icom_set_ext_* functions
 */

const struct cmdparams icom_ext_cmd[] =
{
    { {.t = TOK_DSTAR_DSQL}, C_CTL_DIG, S_DIG_DSCSQL, SC_MOD_RW, 1, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_DSTAR_CALL_SIGN}, C_CTL_DIG, S_DIG_DSCALS, SC_MOD_RW12, 2, {0}, CMD_DAT_BUF, 38 },
    { {.t = TOK_DSTAR_MESSAGE}, C_CTL_DIG, S_DIG_DSMESS, SC_MOD_RW12, 2, {0}, CMD_DAT_STR, 32 },
    { {.t = TOK_DSTAR_STATUS}, C_CTL_DIG, S_DIG_DSRSTS, SC_MOD_RW12, 2, {0}, CMD_DAT_BUF, 1 },
    { {.t = TOK_DSTAR_GPS_DATA}, C_CTL_DIG, S_DIG_DSGPSD, SC_MOD_RW12, 2, {0}, CMD_DAT_BUF, 52 },
    { {.t = TOK_DSTAR_GPS_MESS}, C_CTL_DIG, S_DIG_DSGPSM, SC_MOD_RW12, 2, {0}, CMD_DAT_STR, 52 },
    { {.t = TOK_DSTAR_CODE}, C_CTL_DIG, S_DIG_DSCSQL, SC_MOD_RW12, 2, {0}, CMD_DAT_FLT, 1 },
    { {.t = TOK_DSTAR_TX_DATA}, C_CTL_DSD, S_DSD_DSTXDT, SC_MOD_RW, 1, {0}, CMD_DAT_BUF, 30 },
    { {.t = TOK_DSTAR_MY_CS}, C_CTL_DVT, S_DVT_DSMYCS, SC_MOD_RW, 1, {0}, CMD_DAT_STR, 12 },
    { {.t = TOK_DSTAR_TX_CS}, C_CTL_DVT, S_DVT_DSTXCS, SC_MOD_RW, 1, {0}, CMD_DAT_STR, 24 },
    { {.t = TOK_DSTAR_TX_MESS}, C_CTL_DVT, S_DVT_DSTXMS, SC_MOD_RW, 1, {0}, CMD_DAT_STR, 20 },
    { {.t = TOK_DRIVE_GAIN}, C_CTL_LVL, S_LVL_DRIVE, SC_MOD_RW, 1, {0}, CMD_DAT_FLT, 2 },
    { {.t = TOK_DIGI_SEL_FUNC}, C_CTL_FUNC, S_FUNC_DIGISEL, SC_MOD_RW, 1, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_DIGI_SEL_LEVEL}, C_CTL_LVL, S_LVL_DIGI, SC_MOD_RW, 1, {0}, CMD_DAT_FLT, 2 },
    { {.t = TOK_SCOPE_DAT}, C_CTL_SCP, S_SCP_DAT, SC_MOD_RD, 0, {0}, CMD_DAT_BUF, 481 },
    { {.t = TOK_SCOPE_STS}, C_CTL_SCP, S_SCP_STS, SC_MOD_RW, 0, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_SCOPE_DOP}, C_CTL_SCP, S_SCP_DOP, SC_MOD_RW, 0, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_SCOPE_MSS}, C_CTL_SCP, S_SCP_MSS, SC_MOD_RW, 0, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_SCOPE_MOD}, C_CTL_SCP, S_SCP_MOD, SC_MOD_RW, 0, {0}, CMD_DAT_WRD, 2 },
    { {.t = TOK_SCOPE_SPN}, C_CTL_SCP, S_SCP_SPN, SC_MOD_RW, 0, {0}, CMD_DAT_BUF, 6 },
    { {.t = TOK_SCOPE_EDG}, C_CTL_SCP, S_SCP_EDG, SC_MOD_RW, 0, {0}, CMD_DAT_WRD, 2 },
    { {.t = TOK_SCOPE_HLD}, C_CTL_SCP, S_SCP_HLD, SC_MOD_RW, 0, {0}, CMD_DAT_WRD, 2 },
    { {.t = TOK_SCOPE_REF}, C_CTL_SCP, S_SCP_REF, SC_MOD_RW, 0, {0}, CMD_DAT_BUF, 4 },
    { {.t = TOK_SCOPE_SWP}, C_CTL_SCP, S_SCP_SWP, SC_MOD_RW, 0, {0}, CMD_DAT_WRD, 2 },
    { {.t = TOK_SCOPE_STX}, C_CTL_SCP, S_SCP_STX, SC_MOD_RW, 0, {0}, CMD_DAT_BOL, 1 },
    { {.t = TOK_SCOPE_TYP}, C_CTL_SCP, S_SCP_TYP, SC_MOD_RW, 0, {0}, CMD_DAT_INT, 1 },
    { {.t = TOK_SCOPE_VBW}, C_CTL_SCP, S_SCP_VBW, SC_MOD_RW, 0, {0}, CMD_DAT_WRD, 2 },
    { {.t = TOK_SCOPE_FEF}, C_CTL_SCP, S_SCP_FEF, SC_MOD_RW, 0, {0}, CMD_DAT_BUF, 12 },
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
    {RIG_MODEL_IC821, 0x4c},
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
    {RIG_MODEL_ICR8600, 0x96},
    {RIG_MODEL_ICR30, 0x9c},
    {RIG_MODEL_NONE, 0},
};

/*
 * This is a generic icom_init function.
 * You might want to define yours, so you can customize it for your rig
 *
 * Basically, it sets up *priv
 * REM: serial port is already open (rig->state.rigport.fd)
 */
int
icom_init(RIG *rig)
{
    struct icom_priv_data *priv;
    struct icom_priv_caps *priv_caps;
    struct rig_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig->caps)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (!caps->priv)
    {
        return -RIG_ECONF;
    }

    priv_caps = (struct icom_priv_caps *) caps->priv;


    rig->state.priv = (struct icom_priv_data *) calloc(1,
                      sizeof(struct icom_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    /* TODO: CI-V address should be customizable */

    /*
     * init the priv_data from static struct
     *          + override with preferences
     */

    priv->re_civ_addr = priv_caps->re_civ_addr;
    priv->civ_731_mode = priv_caps->civ_731_mode;
    priv->no_xchg = priv_caps->no_xchg;
    priv->tx_vfo = RIG_VFO_NONE;
    priv->rx_vfo = RIG_VFO_NONE;
    priv->curr_vfo = RIG_VFO_NONE;

    rig_debug(RIG_DEBUG_TRACE, "%s: done\n", __func__);

    return RIG_OK;
}

/*
 * ICOM Generic icom_cleanup routine
 * the serial port is closed by the frontend
 */
int
icom_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
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
    int retry_save = rs->rigport.retry;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // reduce the retry here so it's quicker
    rs->rigport.retry = 1;
    // Check for echo on first
    priv->serial_USB_echo_off = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: retry temp set to %d\n", __func__,
              rs->rigport.retry);

    retval = icom_transaction(rig, C_RD_FREQ, -1, NULL, 0, ackbuf, &ack_len);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ack_len=%d\n", __func__, ack_len);

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: USB echo on detected\n",
                  __func__);
        rs->rigport.retry = retry_save;
        return RIG_OK;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s %d \n", __func__, __LINE__);
        priv->serial_USB_echo_off = 1;

        retval = icom_transaction(rig, C_RD_TRXID, 0x00, NULL, 0, ackbuf, &ack_len);

        if (retval == RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: USB echo off detected\n",
                      __func__);
            rs->rigport.retry = retry_save;
            return RIG_OK;
        }
    }

    rs->rigport.retry = retry_save;
    return retval;
}


/*
 * ICOM rig open routine
 * Detect echo state of USB serial port
 */
int
icom_rig_open(RIG *rig)
{
    int retval = RIG_OK;
    int satmode = 0;
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s %d \n", __func__, __LINE__);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s v%s\n", __func__, rig->caps->model_name,
              rig->caps->version);
    retval = icom_get_usb_echo_off(rig);

    if (retval != RIG_OK)
    {

        // maybe we need power on?
        rig_debug(RIG_DEBUG_VERBOSE, "%s trying power on\n", __func__);
        retval = abs(rig_set_powerstat(rig, 1));

        // this is only a fatal error if powerstat is implemented
        // if not iplemented than we're at an error here
        if (retval != RIG_OK && retval != RIG_ENIMPL && retval != RIG_ENAVAIL)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: unexpected retval here: %s\n",
                      __func__, rigerror(retval));

            rig_debug(RIG_DEBUG_WARN, "%s: rig_set_powerstat failed: =%s\n", __func__,
                      rigerror(retval));
            return retval;
        }

        // Now that we're powered up let's try again
        retval = icom_get_usb_echo_off(rig);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unable to determine USB echo status\n", __func__);
            return retval;
        }
    }

    retval = rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
    rig->state.cache.satmode = satmode;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: satmode=%d\n", __func__, satmode);

    // RIG_OK return means this rig has satmode capabiltiy and Main/Sub VFOs
    // Should we also set/force VFOA for Main&Sub here?
    if (retval == RIG_OK && satmode)
    {
        priv->rx_vfo = RIG_VFO_MAIN;
        priv->tx_vfo = RIG_VFO_SUB;
    }
    else if (retval == RIG_OK && !satmode)
    {
        priv->rx_vfo = RIG_VFO_MAIN;
        priv->tx_vfo = RIG_VFO_MAIN;
    }

#if 0 // do not do this here -- needs to be done when ranges are requested instead as this is very slow
    icom_get_freq_range(rig); // try get to get rig range capability dyamically
#endif

    return RIG_OK;
}

/*
 * ICOM rig close routine
 */
int
icom_rig_close(RIG *rig)
{
    // Nothing to do yet
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);
    return RIG_OK;
}

/*
 * Set default when vfo == RIG_VFO_NONE
 * Clients should be setting VFO as 1st things but some don't
 * So they will get defaults of Main/VFOA as the selected VFO
 * and we force that selection
 */
int icom_set_default_vfo(RIG *rig)
{
    int retval;
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called, curr_vfo=%s\n", __func__,
              rig_strvfo(priv->curr_vfo));

    if (VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: setting default as MAIN/VFOA\n",
                  __func__);
        retval = rig_set_vfo(rig, RIG_VFO_MAIN);  // we'll default to Main in this case

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = rig_set_vfo(rig, RIG_VFO_A);  // we'll default to Main in this case

        if (retval != RIG_OK)
        {
            return retval;
        }

        priv->curr_vfo = RIG_VFO_MAIN;
        return RIG_OK;
    }

    if (VFO_HAS_MAIN_SUB_ONLY)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: setting default as MAIN\n",
                  __func__);
        retval = rig_set_vfo(rig, RIG_VFO_MAIN);  // we'll default to Main in this case
        priv->curr_vfo = RIG_VFO_MAIN;
    }
    else if (VFO_HAS_A_B)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: setting default as VFOA\n",
                  __func__);
        retval = rig_set_vfo(rig,
                             RIG_VFO_A);     // we'll default to VFOA for all others
        priv->curr_vfo = RIG_VFO_A;
    }
    else
    {
        // we don't have any VFO selection
        rig_debug(RIG_DEBUG_TRACE, "%s: setting default as VFO_CURR\n",
                  __func__);

        priv->curr_vfo = RIG_VFO_A;
        retval = RIG_OK;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

// return true if band is changing from last set_freq
// Assumes rig is currently on the VFO being changed
// This handles the case case Main/Sub cannot be on the same band
int icom_band_changing(RIG *rig, freq_t test_freq)
{
    freq_t curr_freq, freq1, freq2;
    int retval;

    // We should be sitting on the VFO we want to change so just get it's frequency
    retval = icom_get_freq(rig, RIG_VFO_CURR, &curr_freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: icom_get_freq failed??\n", __func__);
        return 0; // I guess we need to say no change in this case
    }

    // Make our HF=0, 2M = 1, 70cm = 4, and 23cm=12
    freq1 = floor(curr_freq / 1e8);
    freq2 = floor(test_freq / 1e8);

    rig_debug(RIG_DEBUG_TRACE, "%s: lastfreq=%.0f, thisfreq=%.0f\n", __func__,
              freq1, freq2);

    if (freq1 != freq2)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Band change detected\n", __func__);
        return 1;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Band change not detected\n", __func__);
    return 0;
}

/*
 * icom_set_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char freqbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int freq_len, ack_len = sizeof(ackbuf), retval;
    int cmd, subcmd;
    freq_t curr_freq;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s=%" PRIfreq "\n", __func__,
              rig_strvfo(vfo), freq);
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    if (priv->curr_vfo == RIG_VFO_NONE && vfo == RIG_VFO_CURR)
    {
        icom_set_default_vfo(rig);
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->curr_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: currVFO asked for so vfo set to %s\n", __func__,
                  rig_strvfo(vfo));
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set_vfo_curr=%s\n", __func__,
              rig_strvfo(priv->curr_vfo));
    retval = set_vfo_curr(rig, vfo, priv->curr_vfo);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = icom_get_freq(rig, RIG_VFO_CURR, &curr_freq);

    if (retval != RIG_OK)
    {
        return retval;
    }

    freq_len = priv->civ_731_mode ? 4 : 5;
    /*
     * to_bcd requires nibble len
     */
    to_bcd(freqbuf, freq, freq_len * 2);

    cmd = C_SET_FREQ;
    subcmd = -1;
    retval = icom_transaction(rig, cmd, subcmd, freqbuf, freq_len, ackbuf,
                              &ack_len);

    if (retval != RIG_OK)
    {
        // We might have a failed command if we're changing bands
        // For example, IC9700 setting Sub=VHF when Main=VHF will fail
        // So we'll try a VFO swap and see if that helps things
        rig_debug(RIG_DEBUG_VERBOSE, "%s: special check for vfo swap\n", __func__);

        if (icom_band_changing(rig, freq))
        {
            if (rig_has_vfo_op(rig, RIG_OP_XCHG))
            {
                if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: vfo_op XCHG failed: %s\n", __func__,
                              rigerror(retval));
                    return retval;
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

                    if (RIG_OK != (retval2 = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
                    {
                        rig_debug(RIG_DEBUG_ERR, "%s: 2nd vfo_op XCHG failed: %s\n", __func__,
                                  rigerror(retval));
                        return retval2;
                    }

                    return retval;
                }
            }
        }

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set freq failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    if (vfo == RIG_VFO_MAIN)
    {
        priv->main_freq = freq;
    }
    else if (vfo == RIG_VFO_SUB)
    {
        priv->sub_freq = freq;
    }

    priv->curr_freq = freq;

    switch (vfo)
    {
    case RIG_VFO_A: priv->vfoa_freq = freq; break;

    case RIG_VFO_B: priv->vfob_freq = freq; break;

    case RIG_VFO_MAIN: priv->sub_freq = freq; break;

    case RIG_VFO_SUB: priv->main_freq = freq; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown VFO?  VFO=%s\n", __func__,
                  rig_strvfo(vfo));
    }

    return RIG_OK;
}

/*
 * icom_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL, Main=VFOA, Sub=VFOB
 * Note: old rig may return less than 4/5 bytes for get_freq
 */
int icom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char freqbuf[MAXFRAMELEN];
    unsigned char ackbuf[MAXFRAMELEN];
    int freq_len, retval;
    int cmd, subcmd;
    int ack_len = sizeof(ackbuf);
    int civ_731_mode = 0; // even these rigs have 5-byte channels

    rig_debug(RIG_DEBUG_VERBOSE, "%s called for %s\n", __func__,
              rig_strvfo(vfo));
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    if (rig->caps->rig_model == RIG_MODEL_IC910 || rig->caps->rig_model == RIG_MODEL_FTDX101D)
    {
        ptt_t ptt;
        retval = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (ptt)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: split is on so returning last known freq\n",
                      __func__);
            *freq = priv->vfoa_freq;
            return RIG_OK;
        }
    }

#if 0 // disabled to test if IC9700 satmode/gpredict still works OK

    if (priv->curr_vfo == RIG_VFO_NONE)
    {
        icom_set_default_vfo(rig);
    }

#endif

    cmd = C_RD_FREQ;
    subcmd = -1;

    if (vfo == RIG_VFO_MEM && priv->civ_731_mode)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: VFO=MEM so turning off civ_731\n", __func__);
        civ_731_mode = 1;
        priv->civ_731_mode = 0;

    }

    // Pick the appropriate VFO when VFO_TX is requested
    if (vfo == RIG_VFO_TX)
    {
        if (priv->x1cx03cmdfails == 0) // we can try this command to avoid vfo swapping
        {
            cmd = 0x1c;
            subcmd = 0x03;
            retval = icom_transaction(rig, cmd, subcmd, NULL, 0, ackbuf,
                                      &ack_len);

            if (retval == RIG_OK) // then we're done!!
            {
                *freq = from_bcd(&ackbuf[2], (priv->civ_731_mode ? 4 : 5) * 2);
                return retval;
            }

            priv->x1cx03cmdfails = 1;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: VFO_TX requested, vfo=%s\n", __func__,
                  rig_strvfo(vfo));

        if (priv->split_on)
        {
            vfo = (rig->state.vfo_list & RIG_VFO_B) ? RIG_VFO_B : RIG_VFO_SUB;
        }
        else
        {
            vfo = (rig->state.vfo_list & RIG_VFO_B) ? RIG_VFO_A : RIG_VFO_MAIN;
        }
    }

#if 0 // does not work with rigs without VFO_A
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->curr_vfo;
        if (vfo == RIG_VFO_NONE) vfo = RIG_VFO_A;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: CurrVFO changed to %s\n", __func__,
                  rig_strvfo(vfo));
    }
#endif


    retval = set_vfo_curr(rig, vfo, priv->curr_vfo);

    if (retval != RIG_OK)
    {
        if (vfo == RIG_VFO_MEM && civ_731_mode) { priv->civ_731_mode = 1; }

        return retval;
    }

    // Pick the appropriate VFO when VFO_RX is requested
    if (vfo == RIG_VFO_RX)
    {
        vfo = (rig->state.vfo_list & RIG_VFO_B) ? RIG_VFO_A : RIG_VFO_MAIN;
        rig_debug(RIG_DEBUG_TRACE, "%s: VFO_RX requested, new vfo=%s\n", __func__,
                  rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_TX)
    {
        if (rig->state.vfo_list == VFO_HAS_MAIN_SUB_A_B_ONLY)
        {
            vfo = RIG_VFO_A;

            if (priv->split_on) { vfo = RIG_VFO_B; }
            else if (rig->state.cache.satmode) { vfo = RIG_VFO_SUB; }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: VFO_TX requested, new vfo=%s\n", __func__,
                  rig_strvfo(vfo));
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: using vfo=%s\n", __func__,
              rig_strvfo(vfo));

    if (priv->curr_vfo == RIG_VFO_NONE)
    {
        retval = rig_set_vfo(rig, vfo);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_vfo failed? retval=%s\n", __func__,
                      rigerror(retval));
        }
    }

    retval = icom_transaction(rig, cmd, subcmd, NULL, 0, freqbuf, &freq_len);

    if (retval != RIG_OK)
    {
        if (vfo == RIG_VFO_MEM && civ_731_mode) { priv->civ_731_mode = 1; }

        return retval;
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

        if (vfo == RIG_VFO_MEM && civ_731_mode) { priv->civ_731_mode = 1; }

        return RIG_OK;
    }

    if (freq_len != 4 && freq_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, freq_len);

        if (vfo == RIG_VFO_MEM && civ_731_mode) { priv->civ_731_mode = 1; }

        return -RIG_ERJCTED;
    }

    if (freq_len != (priv->civ_731_mode ? 4 : 5))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: freq len (%d) differs from expected\n",
                  __func__, freq_len);
    }

    /*
     * from_bcd requires nibble len
     */
    *freq = from_bcd(freqbuf + 1, freq_len * 2);

    if (vfo == RIG_VFO_MEM && civ_731_mode) { priv->civ_731_mode = 1; }

    switch (vfo)
    {
    case RIG_VFO_A: priv->vfoa_freq = *freq; break;

    case RIG_VFO_B: priv->vfob_freq = *freq; break;

    case RIG_VFO_MAIN: priv->main_freq = *freq; break;

    case RIG_VFO_SUB: priv->sub_freq = *freq; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown VFO?  VFO=%s\n", __func__,
                  rig_strvfo(vfo));
    }

    return RIG_OK;
}

int icom_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    unsigned char freqbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int freq_len, ack_len = sizeof(ackbuf), retval;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    freq_len = 2;
    /*
     * to_bcd requires nibble len
     */
    to_bcd(freqbuf, rit, freq_len * 2);

    retval = icom_transaction(rig, C_SET_OFFS, -1, freqbuf, freq_len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

int icom_get_rit_new(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    unsigned char tsbuf[MAXFRAMELEN];
    int ts_len, retval;

    retval =
        icom_transaction(rig, C_CTL_RIT, S_RIT_FREQ, NULL, 0, tsbuf, &ts_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * tsbuf nibbles should contain 10,1,1000,100 hz digits and 00=+, 01=- bit
     */
    rig_debug(RIG_DEBUG_VERBOSE, "%s: ts_len=%d\n", __func__, ts_len);

    if (ts_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__, ts_len);
        return -RIG_ERJCTED;
    }

    *ts = (shortfreq_t) from_bcd(tsbuf + 2, 4);

    if (tsbuf[4] != 0)
    {
        *ts *= -1;
    }

    return RIG_OK;
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
        return retval;
    }

    if (ts == 0)          // Turn off both RIT/XIT
    {
        if (rig->caps->has_get_func & RIG_FUNC_XIT)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: turning of XIT too\n", __func__);
            retval = icom_set_func(rig, vfo, RIG_FUNC_XIT, 0);

            if (retval != RIG_OK)
            {
                return retval;
            }
        }
        else          // some rigs don't have XIT like the 9700
        {
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: rig does not have xit command enabled\n", __func__);
        }

        retval = icom_set_func(rig, vfo, RIG_FUNC_RIT, 0);
    }
    else
    {
        retval =
            icom_set_func(rig, vfo, set_xit ? RIG_FUNC_XIT : RIG_FUNC_RIT, 1);
    }

    return retval;
}

int icom_set_rit_new(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    return icom_set_it_new(rig, vfo, ts, 0);
}

int icom_set_xit_new(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    return icom_set_it_new(rig, vfo, ts, 1);
}

/* icom_get_dsp_flt
    returns the dsp filter width in hz or 0 if the command is not implemented or error.
    This allows the default parameters to be assigned from the get_mode routine if the command is not implemented.
    Assumes rig != null and the current mode is in mode.

    Has been tested for IC-746pro,  Should work on the all dsp rigs ie pro models.
    The 746 documentation says it has the get_if_filter, but doesn't give any decoding information ? Please test.
*/

pbwidth_t icom_get_dsp_flt(RIG *rig, rmode_t mode)
{

    int retval, res_len, rfstatus;
    unsigned char resbuf[MAXFRAMELEN];
    value_t rfwidth;
    unsigned char fw_sub_cmd = RIG_MODEL_IC7200 == rig->caps->rig_model ? 0x02 :
                               S_MEM_FILT_WDTH;
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (rig_has_get_func(rig, RIG_FUNC_RF)
            && (mode & (RIG_MODE_RTTY | RIG_MODE_RTTYR)))
    {
        if (!rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_RF, &rfstatus)
                && (rfstatus))
        {
            retval = rig_get_ext_parm(rig, TOK_RTTY_FLTR, &rfwidth);

            if (retval != RIG_OK || rfwidth.i >= RTTY_FIL_NB)
            {
                return 0;     /* use default */
            }
            else
            {
                return rtty_fil[rfwidth.i];
            }
        }
    }

    if (RIG_MODEL_X108G == rig->caps->rig_model)
    {
        priv->no_1a_03_cmd = 1;
    }

    if (priv->no_1a_03_cmd)
    {
        return 0;
    }

    retval = icom_transaction(rig, C_CTL_MEM, fw_sub_cmd, 0, 0,
                              resbuf, &res_len);

    if (-RIG_ERJCTED == retval)
    {
        priv->no_1a_03_cmd = -1;  /* do not keep asking */
        return 0;
    }

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), "
                  "len=%d\n", __func__, resbuf[0], res_len);
        return 0;         /* use default */
    }

    if (res_len == 3 && resbuf[0] == C_CTL_MEM)
    {
        int i;
        i = (int) from_bcd(resbuf + 2, 2);

        if (mode & RIG_MODE_AM)
        {
            return (i + 1) * 200; /* Ic_7800 */
        }
        else if (mode &
                 (RIG_MODE_CW | RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_RTTY |
                  RIG_MODE_RTTYR))
        {
            return i < 10 ? (i + 1) * 50 : (i - 4) * 100;
        }
    }

    return 0;
}

#ifdef XXREMOVEDXX
// not referenced anywhere
int icom_set_dsp_flt(RIG *rig, rmode_t mode, pbwidth_t width)
{
    int retval, rfstatus;
    unsigned char ackbuf[MAXFRAMELEN];
    unsigned char flt_ext;
    value_t rfwidth;
    int ack_len = sizeof(ackbuf), flt_idx;
    unsigned char fw_sub_cmd = RIG_MODEL_IC7200 == rig->caps->rig_model ? 0x02 :
                               S_MEM_FILT_WDTH;

    if (RIG_PASSBAND_NOCHANGE == width)
    {
        return RIG_OK;
    }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

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
                    return rig_set_ext_parm(rig, TOK_RTTY_FLTR, rfwidth);
                }
            }

            /* not found */
            return -RIG_EINVAL;
        }
    }

    if (mode & RIG_MODE_AM)
    {
        flt_idx = (width / 200) - 1;  /* TBC: Ic_7800? */
    }
    else if (mode & (RIG_MODE_CW | RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_RTTY |
                     RIG_MODE_RTTYR))
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
        return RIG_OK;
    }

    to_bcd(&flt_ext, flt_idx, 2);

    retval = icom_transaction(rig, C_CTL_MEM, fw_sub_cmd, &flt_ext, 1,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), "
                  "len=%d\n", __func__, ackbuf[0], ack_len);
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: command not supported ? (%#.2x), "
                  "len=%d\n", __func__, ackbuf[0], ack_len);
        return retval;
    }

    return RIG_OK;
}
#endif

/*
 * icom_set_mode_with_data
 */
int icom_set_mode_with_data(RIG *rig, vfo_t vfo, rmode_t mode,
                            pbwidth_t width)
{
    int retval;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    rmode_t icom_mode;
    unsigned char dm_sub_cmd = RIG_MODEL_IC7200 == rig->caps->rig_model ? 0x04 :
                               S_MEM_DATA_MODE;
    int filter_byte = rig->caps->rig_model == RIG_MODEL_IC7300
                      || rig->caps->rig_model == RIG_MODEL_IC7610;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (mode)
    {
    case RIG_MODE_PKTUSB:
        icom_mode = RIG_MODE_USB;
        break;

    case RIG_MODE_PKTLSB:
        icom_mode = RIG_MODE_LSB;
        break;

    case RIG_MODE_PKTFM:
        icom_mode = RIG_MODE_FM;
        break;

    case RIG_MODE_PKTAM:
        icom_mode = RIG_MODE_AM;
        break;

    default:
        icom_mode = mode;
        break;
    }

    retval = icom_set_mode(rig, vfo, icom_mode, width);

    if (RIG_OK == retval)
    {
        unsigned char datamode[2];

        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_PKTFM:
        case RIG_MODE_PKTAM:
            /* some rigs (e.g. IC-7700 & IC-7800)
               have D1/2/3 but we cannot know
               which to set so just set D1 */
            datamode[0] = 0x01;
            break;

        default:
            datamode[0] = 0x00;
            break;
        }

        if (filter_byte)   // then we need the width byte too
        {
            unsigned char mode_icom; // not used as it will map to USB/LSB
            signed char width_icom;
            rig2icom_mode(rig, mode, width, &mode_icom, &width_icom);
            // since width_icom is 0-2 for rigs that need this here we have to make it 1-3
            datamode[1] = datamode[0] ? width_icom : 0;
            retval =
                icom_transaction(rig, C_CTL_MEM, dm_sub_cmd, datamode, width_icom == -1 ? 1 : 2,
                                 ackbuf,
                                 &ack_len);
        }
        else
        {
            retval =
                icom_transaction(rig, C_CTL_MEM, dm_sub_cmd, datamode, 1, ackbuf,
                                 &ack_len);
        }

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

    return retval;
}

/*
 * icom_set_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct icom_priv_data *priv;
    const struct icom_priv_caps *priv_caps;
    struct rig_state *rs;
    unsigned char ackbuf[MAXFRAMELEN];
    unsigned char icmode;
    signed char icmode_ext;
    int ack_len = sizeof(ackbuf), retval, err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, mode=%s, width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;

    if (priv_caps->r2i_mode != NULL)  /* call priv code if defined */
    {
        err = priv_caps->r2i_mode(rig, mode, width, &icmode, &icmode_ext);
    }
    else              /* else call default */
    {
        err = rig2icom_mode(rig, mode, width, &icmode, &icmode_ext);
    }

    if (err < 0)
    {
        return err;
    }

    /* IC-731 and IC-735 don't support passband data */
    /* IC-726 & IC-475A/E also limited support - only on CW */
    /* TODO: G4WJS CW wide/narrow are possible with above two radios */
    if (priv->civ_731_mode || rig->caps->rig_model == RIG_MODEL_OS456
            || rig->caps->rig_model == RIG_MODEL_IC726
            || rig->caps->rig_model == RIG_MODEL_IC475)
    {
        icmode_ext = -1;
    }

    retval = icom_transaction(rig, C_SET_MODE, icmode,
                              (unsigned char *) &icmode_ext,
                              (icmode_ext == -1 ? 0 : 1), ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

#if 0

    /* Tentative DSP filter setting ($1A$03), but not supported by every rig,
     * and some models like IC910/Omni VI Plus have a different meaning for
     * this subcommand
     */
    if ((rig->caps->rig_model != RIG_MODEL_IC910) &&
            (rig->caps->rig_model != RIG_MODEL_OMNIVIP))
    {
        icom_set_dsp_flt(rig, mode, width);
    }

#endif

    return RIG_OK;
}

/*
 * icom_get_mode_with_data
 *
 * newer Icom rigs support data mode with ACC-1 audio input and MIC muted
 */
int icom_get_mode_with_data(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width)
{
    unsigned char databuf[MAXFRAMELEN];
    int data_len, retval;
    unsigned char dm_sub_cmd = RIG_MODEL_IC7200 == rig->caps->rig_model ? 0x04 :
                               S_MEM_DATA_MODE;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    retval = icom_get_mode(rig, vfo, mode, width);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (*mode)
    {
    case RIG_MODE_USB:
    case RIG_MODE_LSB:
    case RIG_MODE_AM:
    case RIG_MODE_FM:
        /*
         * fetch data mode on/off
         */
        retval =
            icom_transaction(rig, C_CTL_MEM, dm_sub_cmd, 0, 0, databuf,
                             &data_len);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: protocol error (%#.2x), len=%d\n",
                      __func__, databuf[0], data_len);
            return -RIG_ERJCTED;
        }

        /*
         * databuf should contain Cn,Sc,D0[,D1]
         */
        data_len -= 2;

        if (1 > data_len || data_len > 2)
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
            return -RIG_ERJCTED;
        }

        if (databuf[2])       /* 0x01/0x02/0x03 -> data mode, 0x00 -> not data mode */
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

    return retval;
}

/*
 * icom_get_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL, width!=NULL
 *
 * TODO: some IC781's, when sending mode info, in wide filter mode, no
 *  width data is send along, making the frame 1 byte short.
 *  (Thank to Mel, VE2DC for this info)
 */
int icom_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    unsigned char modebuf[MAXFRAMELEN];
    const struct icom_priv_caps *priv_caps;
    int mode_len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));
    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;

    retval = icom_transaction(rig, C_RD_MODE, -1, NULL, 0, modebuf, &mode_len);

    rig_debug(RIG_DEBUG_TRACE,
              "%s: modebuf[0]=0x%02x, modebuf[1]=0x%02x, mode_len=%d\n", __func__, modebuf[0],
              modebuf[1], mode_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * modebuf should contain Cn,Data area
     */
    mode_len--;

    if (mode_len != 2 && mode_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, mode_len);
        return -RIG_ERJCTED;
    }

    if (priv_caps->i2r_mode != NULL)  /* call priv code if defined */
    {
        priv_caps->i2r_mode(rig, modebuf[1],
                            mode_len == 2 ? modebuf[2] : -1, mode, width);
    }
    else              /* else call default */
    {
        icom2rig_mode(rig, modebuf[1],
                      mode_len == 2 ? modebuf[2] : -1, mode, width);
    }

    /* IC910H has different meaning of command 1A, subcommand 03. So do
     * not ask for DSP filter settings */
    /* Likewise, don't ask if we happen to be an Omni VI Plus */
    /* Likewise, don't ask if we happen to be an IC-R30 */
    if ((rig->caps->rig_model == RIG_MODEL_IC910) ||
            (rig->caps->rig_model == RIG_MODEL_OMNIVIP) ||
            (rig->caps->rig_model == RIG_MODEL_ICR30))
    {
        return RIG_OK;
    }

    /* Most rigs return 1-wide, 2-normal,3-narrow
     * For DSP rigs these are presets, can be programmed for 30 - 41 bandwidths, depending on mode.
     * Lets check for dsp filters
     */

    if (width && (retval = icom_get_dsp_flt(rig, *mode)) != 0)
    {
        *width = retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s returning mode=%s, width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(*mode), (int)*width);
    return RIG_OK;
}

#ifdef XXREMOVEDXX
// not implemented yet
/*
 * icom_get_vfo
 * The IC-9700 has introduced the ability to see MAIN/SUB selection
 * Maybe we'll see this in future ICOMs or firmware upgrades
 * Command 0x07 0XD2 -- but as of version 1.05 it doesn't work
 * We will, by default, force Main=VFOA and Sub=VFOB, and may want
 * an option to not force that behavior
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_vfo(RIG *rig, vfo_t *vfo)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = icom_transaction(rig, C_SET_VFO, S_SUB_SEL, NULL, 0,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s wrong frame len=%d\n", __func__, ack_len);
        return -RIG_ERJCTED;
    }

    *vfo = ackbuf[2] == 0 ? RIG_VFO_A : RIG_VFO_B;
    return RIG_OK;
}
#endif

/*
 * icom_get_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_vfo(RIG *rig, vfo_t vfo)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), icvfo, retval;
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    if (vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Asking for currVFO,  currVFO=%s\n", __func__,
                  rig_strvfo(priv->curr_vfo));
        return RIG_OK;
    }

    if (vfo == RIG_VFO_MAIN && VFO_HAS_A_B_ONLY)
    {
        vfo = RIG_VFO_A;
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Rig does not have MAIN/SUB so Main changed to %s\n",
                  __func__, rig_strvfo(vfo));
    }
    else if ((vfo == RIG_VFO_SUB) && (VFO_HAS_A_B_ONLY
                                      || (VFO_HAS_MAIN_SUB_A_B_ONLY && !priv->split_on && !rig->state.cache.satmode)))
    {
        // if rig doesn't have Main/Sub
        // or if rig has both Main/Sub and A/B -- e.g. 9700
        // and we dont' have split or satmode turned on
        // then we dont' use Sub -- instead we use Main/VFOB
        vfo = RIG_VFO_B;
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Rig does not have MAIN/SUB so Sub changed to %s\n",
                  __func__, rig_strvfo(vfo));
    }
    else if (vfo == RIG_VFO_TX)
    {
        vfo = RIG_VFO_A;

        if (VFO_HAS_A_B_ONLY && rig->state.cache.satmode) { vfo = RIG_VFO_B; }
        else if (VFO_HAS_MAIN_SUB_ONLY) { vfo = RIG_VFO_SUB; }
        else if (VFO_HAS_MAIN_SUB_A_B_ONLY && rig->state.cache.satmode) { vfo = RIG_VFO_SUB; }
    }

    if ((vfo == RIG_VFO_A || vfo == RIG_VFO_B) && !VFO_HAS_A_B && VFO_HAS_MAIN_SUB)
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
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: debug#1\n", __func__);

    if (vfo != priv->curr_vfo)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: VFO changing from %s to %s\n", __func__,
                  rig_strvfo(priv->curr_vfo), rig_strvfo(vfo));
        priv->curr_freq = 0; // reset curr_freq so set_freq works 1st time
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: debug#2\n", __func__);

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
        break;

    case RIG_VFO_SUB:
        icvfo = S_SUB;

        // If split is on these rigs can only split on Main/VFOB
        if (VFO_HAS_MAIN_SUB_A_B_ONLY && priv->split_on) { icvfo = S_VFOB; }

        // If not split or satmode then we must want VFOB
        if (VFO_HAS_MAIN_SUB_A_B_ONLY && !priv->split_on && !rig->state.cache.satmode) { icvfo = S_VFOB; }

        rig_debug(RIG_DEBUG_TRACE, "%s: Sub asked for, ended up with vfo=%s\n",
                  __func__, icvfo == S_SUB ? "Sub" : "VFOB");

        break;

    case RIG_VFO_TX:
        icvfo = priv->split_on ? S_VFOB : S_VFOA;
        vfo = priv->split_on ? RIG_VFO_B : RIG_VFO_A;
        rig_debug(RIG_DEBUG_TRACE, "%s: RIG_VFO_TX changing vfo to %s\n", __func__,
                  rig_strvfo(vfo));
        break;

    case RIG_VFO_VFO:
        retval = icom_transaction(rig, C_SET_VFO, -1, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }

        priv->curr_vfo = vfo;
        return RIG_OK;

    case RIG_VFO_MEM:
        retval = icom_transaction(rig, C_SET_MEM, -1, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }

        priv->curr_vfo = vfo;
        return RIG_OK;

    case RIG_VFO_MAIN_A:    // we need to select Main before setting VFO
    case RIG_VFO_MAIN_B:
        retval = icom_transaction(rig, C_SET_VFO, RIG_VFO_MAIN, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }

        priv->curr_vfo = vfo;
        return RIG_OK;

        break;

    case RIG_VFO_SUB_A: // we need to select Sub before setting VFO
    case RIG_VFO_SUB_B:
        retval = icom_transaction(rig, C_SET_VFO, RIG_VFO_SUB, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }

        priv->curr_vfo = vfo;
        return RIG_OK;

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: debug#3\n", __func__);
    retval = icom_transaction(rig, C_SET_VFO, icvfo, NULL, 0,
                              ackbuf, &ack_len);
    rig_debug(RIG_DEBUG_TRACE, "%s: debug#4\n", __func__);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    priv->curr_vfo = vfo;
    rig->state.current_vfo = vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: debug#5\n", __func__);
    return RIG_OK;
}

int icom_set_cmd(RIG *rig, vfo_t vfo, struct cmdparams *par, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    unsigned char cmdbuf[MAXFRAMELEN];
    int cmdlen = 0;
    unsigned char ackbuf[MAXFRAMELEN];
    int acklen = 0;

    if (!(par->submod & SC_MOD_WR)) { return -RIG_EINVAL; }

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
        to_bcd_be(&cmdbuf[cmdlen], (int) val.f, (cmdlen * 2));
        break;

    case CMD_DAT_LVL:
        to_bcd_be(&cmdbuf[cmdlen], (int)(val.f * 255.0), (cmdlen * 2));
        break;

    case CMD_DAT_TIM:
        to_bcd_be(&cmdbuf[cmdlen],
                  ((((int)val.f / 3600) * 100) + (((int)val.f / 60) % 60)), (par->datlen * 2));
        break;

    default:
        break;
    }

    cmdlen += par->datlen;
    return icom_transaction(rig, par->command, par->subcmd, cmdbuf, cmdlen, ackbuf,
                            &acklen);
}

int icom_get_cmd(RIG *rig, vfo_t vfo, struct cmdparams *par, value_t *val)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    unsigned char ssc = 0x02;
    unsigned char resbuf[MAXFRAMELEN];
    int reslen = sizeof(resbuf);
    int retval;

    if (!(par->submod & SC_MOD_RD)) { return -RIG_EINVAL; }

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
        return retval;
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
            return -RIG_EINTERNAL;
        }

        memcpy(val->s, resbuf, reslen);
        val->s[reslen] = 0;
        break;

    case CMD_DAT_BUF:
        if (reslen > val->b.l)
        {
            return -RIG_EINTERNAL;
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

    return RIG_OK;
}

/*
 * icom_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct rig_state *rs;
    unsigned char lvlbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), lvl_len;
    int lvl_cn, lvl_sc;       /* Command Number, Subcommand */
    int icom_val;
    int i, retval;
    const struct icom_priv_caps *priv_caps =
        (const struct icom_priv_caps *) rig->caps->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    const struct cmdparams *cmd = priv_caps->extcmds;

    for (i = 0; cmd && cmd[i].id.s != 0; i++)
    {
        if (cmd[i].id.s == level)
        {
            return icom_set_cmd(rig, vfo, (struct cmdparams *)&cmd[i], val);
        }
    }

    rs = &rig->state;

    /*
     * So far, levels of float type are in [0.0..1.0] range
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
    if (rig->caps->rig_model == RIG_MODEL_ICR75)
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
    lvl_len = 2;
    to_bcd_be(lvlbuf, (long long) icom_val, lvl_len * 2);

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        lvl_cn = C_CTL_FUNC;
        lvl_sc = S_FUNC_PAMP;
        lvl_len = 1;

        if (val.i == 0)
        {
            lvlbuf[0] = 0;    /* 0=OFF */
            break;
        }

        for (i = 0; i < MAXDBLSTSIZ; i++)
        {
            if (rs->preamp[i] == val.i)
            {
                break;
            }
        }

        if (i == MAXDBLSTSIZ || rs->preamp[i] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported preamp set_level %ddB",
                      __func__, val.i);
            return -RIG_EINVAL;
        }

        lvlbuf[0] = i + 1;    /* 1=P.AMP1, 2=P.AMP2 */
        break;

    case RIG_LEVEL_ATT:
        lvl_cn = C_CTL_ATT;
        /* attenuator level is dB, in BCD mode */
        lvl_sc = (val.i / 10) << 4 | (val.i % 10);
        lvl_len = 0;
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
        if (rig->caps->rig_model == RIG_MODEL_ICR75)
        {
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_MODE_SLCT;
            lvl_len = 3;
            lvlbuf[0] = S_PRM_CWPITCH;
            to_bcd_be(lvlbuf + 1, (long long) icom_val, 4);
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
        lvl_len = 1;

        if (priv_caps->agc_levels_present)
        {
            int found = 0;

            for (i = 0;
                    i <= RIG_AGC_LAST && priv_caps->agc_levels[i].level >= 0; i++)
            {
                if (priv_caps->agc_levels[i].level == val.i)
                {
                    lvlbuf[0] = priv_caps->agc_levels[i].icom_level;
                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                return -RIG_EINVAL;
            }
        }
        else
        {
            // Legacy mapping that does not apply to all rigs
            switch (val.i)
            {
            case RIG_AGC_SLOW:
                lvlbuf[0] = D_AGC_SLOW;
                break;

            case RIG_AGC_MEDIUM:
                lvlbuf[0] = D_AGC_MID;
                break;

            case RIG_AGC_FAST:
                lvlbuf[0] = D_AGC_FAST;
                break;

            case RIG_AGC_SUPERFAST:
                lvlbuf[0] = D_AGC_SUPERFAST;
                break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unsupported LEVEL_AGC %d",
                          __func__, val.i);
                return -RIG_EINVAL;
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
        if (rig->caps->rig_model == RIG_MODEL_IC910)
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
        if (rig->caps->rig_model == RIG_MODEL_IC910)
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

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    retval = icom_transaction(rig, lvl_cn, lvl_sc, lvlbuf, lvl_len, ackbuf,
                              &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * icom_get_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 *
 */
int icom_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct rig_state *rs;
    unsigned char lvlbuf[MAXFRAMELEN], lvl2buf[MAXFRAMELEN];
    int lvl_len, lvl2_len;
    int lvl_cn, lvl_sc;       /* Command Number, Subcommand */
    int icom_val;
    int cmdhead;
    int retval;
    const struct icom_priv_caps *priv_caps =
        (const struct icom_priv_caps *) rig->caps->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    const struct icom_priv_caps *priv = rig->caps->priv;
    const struct cmdparams *cmd = priv->extcmds;
    int i;

    for (i = 0; cmd && cmd[i].id.s != 0; i++)
    {
        if (cmd[i].id.s == level)
        {
            return icom_get_cmd(rig, vfo, (struct cmdparams *)&cmd[i], val);
        }
    }

    rs = &rig->state;

    lvl2_len = 0;

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
        if (rig->caps->rig_model == RIG_MODEL_ICR75)
        {
            lvl_cn = C_CTL_MEM;
            lvl_sc = S_MEM_MODE_SLCT;
            lvl2_len = 1;
            lvl2buf[0] = S_PRM_CWPITCH;
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
        if (rig->caps->rig_model == RIG_MODEL_IC910)
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
        if (rig->caps->rig_model == RIG_MODEL_IC910)
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

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    /* use lvl2buf and lvl2_len for 'set mode' subcommand */
    retval = icom_transaction(rig, lvl_cn, lvl_sc, lvl2buf, lvl2_len, lvlbuf,
                              &lvl_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * strbuf should contain Cn,Sc,Data area
     */
    cmdhead = (lvl_sc == -1) ? 1 : 2;
    lvl_len -= cmdhead;

    /* back off one char since first char in buffer is now 'set mode' subcommand */
    if ((rig->caps->rig_model == RIG_MODEL_ICR75)
            && (level == RIG_LEVEL_CWPITCH))
    {
        cmdhead = 3;
        lvl_len--;
    }

    if (lvlbuf[0] != ACK && lvlbuf[0] != lvl_cn)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  lvlbuf[0], lvl_len);
        return -RIG_ERJCTED;
    }

    /*
     * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
     * (from_bcd is little endian)
     */
    icom_val = from_bcd_be(lvlbuf + cmdhead, lvl_len * 2);

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
                    i <= RIG_AGC_LAST && priv_caps->agc_levels[i].level >= 0; i++)
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
                rig_debug(RIG_DEBUG_ERR, "%s: unexpected AGC 0x%02x", __func__,
                          icom_val);
                return -RIG_EPROTO;
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
                rig_debug(RIG_DEBUG_ERR, "%s: unexpected AGC 0x%02x", __func__,
                          icom_val);
                return -RIG_EPROTO;
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
        if (rig->caps->rfpower_meter_cal.size == 0)
        {
            val->f =
                rig_raw2val_float(icom_val, &icom_default_rfpower_meter_cal);
        }
        else
        {
            val->f =
                rig_raw2val_float(icom_val, &rig->caps->rfpower_meter_cal);
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

        if (icom_val > MAXDBLSTSIZ || rs->preamp[icom_val - 1] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported preamp get_level %ddB",
                      __func__, icom_val);
            return -RIG_EPROTO;
        }

        val->i = rs->preamp[icom_val - 1];
        break;

    /* RIG_LEVEL_ATT: returned value is already an integer in dB (coded in BCD) */
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
    if (rig->caps->rig_model == RIG_MODEL_ICR75)
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

    rig_debug(RIG_DEBUG_TRACE, "%s: %d %d %d %f\n", __func__, lvl_len,
              icom_val, val->i, val->f);

    return RIG_OK;
}

int icom_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    const struct confparams *cfp = rig->caps->extlevels;
    cfp = (cfp == NULL) ? icom_ext_levels : cfp;
    int i;

    for (i = 0; (cfp[i].token != RIG_CONF_END) || (cfp != icom_ext_levels);)
    {
        if (cfp[i].token == RIG_CONF_END)
        {
            cfp = icom_ext_levels;
            i = 0;
        }
        else if (cfp[i].token == token)
        {
            return icom_set_ext_cmd(rig, vfo, token, val);
        }
        else { i++; }
    }

    return -RIG_EINVAL;
}

int icom_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    const struct confparams *cfp = rig->caps->extlevels;
    cfp = (cfp == NULL) ? icom_ext_levels : cfp;
    int i;

    for (i = 0; (cfp[i].token != RIG_CONF_END) || (cfp != icom_ext_levels);)
    {
        if (cfp[i].token == RIG_CONF_END)
        {
            cfp = icom_ext_levels;
            i = 0;
        }
        else if (cfp[i].token == token)
        {
            return icom_get_ext_cmd(rig, vfo, token, val);
        }
        else { i++; }
    }

    return -RIG_EINVAL;
}

int icom_set_ext_func(RIG *rig, vfo_t vfo, token_t token, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
            return icom_set_ext_cmd(rig, vfo, token, (value_t)status);
        }
        else { i++; }
    }

    return -RIG_EINVAL;
}

int icom_get_ext_func(RIG *rig, vfo_t vfo, token_t token, int *status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
            return icom_get_ext_cmd(rig, vfo, token, (value_t *)status);
        }
        else { i++; }
    }

    return -RIG_EINVAL;
}

int icom_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
            return icom_set_ext_cmd(rig, RIG_VFO_NONE, token, val);
        }
        else { i++; }
    }

    return -RIG_EINVAL;
}

int icom_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
            return icom_get_ext_cmd(rig, RIG_VFO_NONE, token, val);
        }
        else { i++; }
    }

    return -RIG_EINVAL;
}

int icom_get_ext_cmd(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
                else if (cmd[i].id.t == token)
                {
                    return icom_get_cmd(rig, vfo, (struct cmdparams *)&cmd[i], val);
                }
                else { i++; }
            }

            return -RIG_EINVAL;
        }
    }

    return -RIG_EINVAL;
}

int icom_set_ext_cmd(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
                else if (cmd[i].id.t == token)
                {
                    return icom_set_cmd(rig, vfo, (struct cmdparams *)&cmd[i], val);
                }
                else { i++; }
            }

            return -RIG_EINVAL;
        }
    }

    return -RIG_EINVAL;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_conf(RIG *rig, token_t token, const char *val)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
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

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int icom_get_conf(RIG *rig, token_t token, char *val)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    switch (token)
    {
    case TOK_CIVADDR:
        sprintf(val, "%d", priv->re_civ_addr);
        break;

    case TOK_MODE731: sprintf(val, "%d", priv->civ_731_mode);
        break;

    case TOK_NOXCHG: sprintf(val, "%d", priv->no_xchg);
        break;

    default: return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * icom_set_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char ackbuf[MAXFRAMELEN], pttbuf[1];
    int ack_len = sizeof(ackbuf), retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    pttbuf[0] = ptt == RIG_PTT_ON ? 1 : 0;

    retval = icom_transaction(rig, C_CTL_PTT, S_PTT, pttbuf, 1,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * icom_get_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icom_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    unsigned char pttbuf[MAXFRAMELEN];
    int ptt_len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    retval = icom_transaction(rig, C_CTL_PTT, S_PTT, NULL, 0,
                              pttbuf, &ptt_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * pttbuf should contain Cn,Sc,Data area
     */
    ptt_len -= 2;

    if (ptt_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, ptt_len);
        return -RIG_ERJCTED;
    }

    *ptt = pttbuf[2] == 1 ? RIG_PTT_ON : RIG_PTT_OFF;

    return RIG_OK;
}

/*
 * icom_get_dcd
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icom_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    unsigned char dcdbuf[MAXFRAMELEN];
    int dcd_len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    retval = icom_transaction(rig, C_RD_SQSM, S_SQL, NULL, 0,
                              dcdbuf, &dcd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * dcdbuf should contain Cn,Data area
     */
    dcd_len -= 2;

    if (dcd_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, dcd_len);
        return -RIG_ERJCTED;
    }

    /*
     * 0x00=sql closed, 0x01=sql open
     */

    *dcd = dcdbuf[2] == 1 ? RIG_DCD_ON : RIG_DCD_OFF;

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported shift %d", __func__,
                  rptr_shift);
        return -RIG_EINVAL;
    }

    retval = icom_transaction(rig, C_CTL_SPLT, rptr_sc, NULL, 0,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    retval = icom_transaction(rig, C_CTL_SPLT, -1, NULL, 0,
                              rptrbuf, &rptr_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * rptrbuf should contain Cn,Sc
     */
    rptr_len--;

    if (rptr_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, rptr_len);
        return -RIG_ERJCTED;
    }

    switch (rptrbuf[1])
    {
    case S_DUP_OFF:
        *rptr_shift = RIG_RPT_SHIFT_NONE; /* Simplex mode */
        break;

    case S_DUP_M:
        *rptr_shift = RIG_RPT_SHIFT_MINUS;    /* Duplex - mode */
        break;

    case S_DUP_P:
        *rptr_shift = RIG_RPT_SHIFT_PLUS; /* Duplex + mode */
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported shift %d", __func__,
                  rptrbuf[1]);
        return -RIG_EPROTO;
    }

    return RIG_OK;
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

    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;
    offs_len = (priv_caps->offs_len) ? priv_caps->offs_len : OFFS_LEN;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    /*
     * Icoms are using a 100Hz unit (at least on 706MKIIg) -- SF
     */
    to_bcd(offsbuf, rptr_offs / 100, offs_len * 2);

    retval = icom_transaction(rig, C_SET_OFFS, -1, offsbuf, offs_len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;
    offs_len = (priv_caps->offs_len) ? priv_caps->offs_len : OFFS_LEN;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    retval = icom_transaction(rig, C_RD_OFFS, -1, NULL, 0, offsbuf, &buf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * offsbuf should contain Cn
     */
    buf_len--;

    if (buf_len != offs_len)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__,
                  buf_len);
        return -RIG_ERJCTED;
    }

    /*
     * Icoms are using a 100Hz unit (at least on 706MKIIg) -- SF
     */
    *rptr_offs = from_bcd(offsbuf + 1, buf_len * 2) * 100;

    return RIG_OK;
}

/*
 * Helper function to go back and forth split VFO
 */
int icom_get_split_vfos(RIG *rig, vfo_t *rx_vfo, vfo_t *tx_vfo)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rs = (struct rig_state *) &rig->state;
    priv = (struct icom_priv_data *) rs->priv;


    if (VFO_HAS_A_B_ONLY)
    {
        *rx_vfo = *tx_vfo = RIG_VFO_A;

        if (priv->split_on)
        {
            *rx_vfo = RIG_VFO_A;
            *tx_vfo = RIG_VFO_B;  /* rig doesn't enforce this but
                   convention is needed here */
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: VFO_HAS_A_B_ONLY, split=%d, rx=%s, tx=%s\n",
                  __func__,
                  priv->split_on, rig_strvfo(*rx_vfo), rig_strvfo(*tx_vfo));
    }
    else if (VFO_HAS_MAIN_SUB_ONLY)
    {
        *rx_vfo = *tx_vfo = RIG_VFO_MAIN;

        if (priv->split_on)
        {
            *rx_vfo = RIG_VFO_MAIN;
            *tx_vfo = RIG_VFO_SUB;
        }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: VFO_HAS_MAIN_SUB_ONLY, split=%d, rx=%s, tx=%s\n",
                  __func__, priv->split_on, rig_strvfo(*rx_vfo), rig_strvfo(*tx_vfo));
    }
    else if (VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        int satmode = 0;
        // e.g. IC9700 split on Main/Sub does not work
        // only Main VFOA/B and SubRx/MainTx split works
        rig_get_func((RIG *)rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
        rig->state.cache.satmode = satmode;

        // don't care about retval here...only care about satmode=1
        if (satmode)
        {
            *rx_vfo = priv->rx_vfo = RIG_VFO_MAIN;
            *tx_vfo = priv->tx_vfo = RIG_VFO_SUB;
        }
        else if (priv->split_on)
        {
            *rx_vfo = priv->rx_vfo = RIG_VFO_A;
            *tx_vfo = priv->tx_vfo = RIG_VFO_B;
        }
        else
        {
            *rx_vfo = priv->rx_vfo = RIG_VFO_A;
            *tx_vfo = priv->tx_vfo = RIG_VFO_A;
        }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: VFO_HAS_MAIN_SUB_A_B_ONLY, split=%d, rx=%s, tx=%s\n",
                  __func__, priv->split_on, rig_strvfo(*rx_vfo), rig_strvfo(*tx_vfo));
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s invalid vfo setup?\n", __func__);
        return -RIG_ENAVAIL;
    }

    return RIG_OK;
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
    int retval;
    vfo_t rx_vfo, tx_vfo;
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t save_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called for %s\n", __func__, rig_strvfo(vfo));
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: satmode=%d, subvfo=%s\n", __func__,
              rig->state.cache.satmode, rig_strvfo(priv->tx_vfo));

    if (RIG_VFO_TX)
    {
        if (rig->state.cache.satmode) { vfo = RIG_VFO_SUB; }
        else { vfo = priv->tx_vfo; }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo is now %s\n", __func__, rig_strvfo(vfo));

    if (rig->state.cache.satmode && vfo == RIG_VFO_TX) { vfo = RIG_VFO_SUB; }

    if (priv->curr_vfo == RIG_VFO_NONE)
    {
        retval = icom_set_default_vfo(rig);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_default_vfo failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }
    }

#if 0
    retval = set_vfo_curr(rig, RIG_VFO_TX, RIG_VFO_TX);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: set_default_vfo failed: %s\n", __func__,
                  rigerror(retval));
        return retval;
    }
#endif

    // If the rigs supports the 0x25 command we'll use it
    // This eliminates VFO swapping and improves split operations
    if (priv->x25cmdfails == 0)
    {
        int satmode = 0;
        // retval not important here...only satmode=1 means anything
        rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
        rig->state.cache.satmode = satmode;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: satmode=%d\n", __func__, satmode);

        if (satmode == 0) // only worth trying if not in satmode
        {
            int cmd, subcmd, freq_len;
            unsigned char freqbuf[32];
            freq_len = priv->civ_731_mode ? 4 : 5;
            /*
             * to_bcd requires nibble len
             */
            to_bcd(freqbuf, tx_freq, freq_len * 2);

            cmd = C_SEND_SEL_FREQ;
            subcmd = 0x01; // set the unselected vfo
            retval = icom_transaction(rig, cmd, subcmd, freqbuf, freq_len, ackbuf,
                                      &ack_len);

            if (retval == RIG_OK) // then we're done!!
            {
                return retval;
            }
        }

        priv->x25cmdfails = 1;
    }


    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Using XCHG to swap/set/swap\n", __func__);

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        if (RIG_OK != (retval = icom_set_freq(rig, RIG_VFO_CURR, tx_freq)))
        {
            return retval;
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        return retval;
    }

    /* In the case of rigs with an A/B VFO arrangement we assume the
       current VFO is VFO A and the split Tx VFO is always VFO B. These
       assumptions allow us to deal with the lack of VFO and split
       queries */
    if (VFO_HAS_A_B_ONLY
            && priv->split_on)   /* broken if user changes split on rig :( */
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: rx_vfo=%s, tx_vfo=%s\n", __func__,
              rig_strvfo(rx_vfo), rig_strvfo(tx_vfo));

    if (RIG_OK != (retval = icom_set_vfo(rig, tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = rig_set_freq(rig, tx_vfo, tx_freq)))
    {
        return retval;
    }

    if (VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        // Then we return the VFO to the rx_vfo
        save_vfo = rx_vfo;

        rig_debug(RIG_DEBUG_TRACE, "%s: SATMODE split_on=%d rig so setting vfo to %s\n",
                  __func__,
                  priv->split_on, rig_strvfo(save_vfo));

        if (RIG_OK != (retval = icom_set_vfo(rig, save_vfo)))
        {
            return retval;
        }
    }
    else if (RIG_OK != (retval = icom_set_vfo(rig, rx_vfo)))
    {
        return retval;
    }

    if (VFO_HAS_A_B_ONLY && priv->split_on)
    {
        /* Re-enable split */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }
    }

    // Update our internal freqs to match what we just did
    if (vfo == RIG_VFO_MAIN)
    {
        priv->main_freq = tx_freq;
    }
    else if (vfo == RIG_VFO_SUB)
    {
        priv->sub_freq = tx_freq;
    }

    return retval;
}

/*
 * icom_get_split_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, rx_freq!=NULL, tx_freq!=NULL
 *  icom_set_vfo,icom_get_freq works for this rig
 */
int icom_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    int retval;
    vfo_t rx_vfo, tx_vfo;
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t save_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s\n", __func__, rig_strvfo(vfo));

    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: ic910#1\n", __func__);

    if (rig->caps->rig_model == RIG_MODEL_IC910)
    {
        ptt_t ptt;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ic910#2\n", __func__);
        retval = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (ptt)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: ptt is on so returning last known freq\n",
                      __func__);
            *tx_freq = priv->vfob_freq;
            return RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s curr_vfo=%s\n", __func__,
              rig_strvfo(priv->curr_vfo));

    if (priv->curr_vfo == RIG_VFO_NONE)
    {
        icom_set_default_vfo(rig);
    }

    // If the rigs supports the 0x25 command we'll use it
    // This eliminates VFO swapping and improves split operations
    // This does not work in satellite mode for the 9700
    if (priv->x25cmdfails == 0)
    {
        int cmd, subcmd;
        int satmode = 0;
        // don't care about the retval here..only satmode=1 is important
        rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
        rig->state.cache.satmode = satmode;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: satmode=%d\n", __func__, satmode);

        if (satmode == 0) // only worth trying if not in satmode
        {
            if (priv->x25cmdfails == 0)
            {
                cmd = C_SEND_SEL_FREQ;
                subcmd = 0x01; // get the unselected vfo
                retval = icom_transaction(rig, cmd, subcmd, NULL, 0, ackbuf,
                                          &ack_len);

                if (retval == RIG_OK) // then we're done!!
                {
                    *tx_freq = from_bcd(ackbuf + 2, (priv->civ_731_mode ? 4 : 5) * 2);
                    return retval;
                }

                priv->x25cmdfails = 1;
            }
        }
        else   // we're in satmode so we try another command
        {
            if (priv->x1cx03cmdfails == 0)
            {
                cmd = 0x1c;
                subcmd = 0x03;
                retval = icom_transaction(rig, cmd, subcmd, NULL, 0, ackbuf,
                                          &ack_len);

                if (retval == RIG_OK) // then we're done!!
                {
                    *tx_freq = from_bcd(&ackbuf[2], (priv->civ_731_mode ? 4 : 5) * 2);
                    return retval;
                }

                priv->x1cx03cmdfails = 1;
            }
        }

    }

    save_vfo = priv->curr_vfo; // so we can restore it later

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        if (RIG_OK != (retval = icom_get_freq(rig, RIG_VFO_CURR, tx_freq)))
        {
            return retval;
        }

        priv->vfob_freq = *tx_freq;

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        return retval;
    }

    /* In the case of rigs with an A/B VFO arrangement we assume the
       current VFO is VFO A and the split Tx VFO is always VFO B. These
       assumptions allow us to deal with the lack of VFO and split
       queries */
    if (VFO_HAS_A_B_ONLY
            && priv->split_on)   /* broken if user changes split on rig :( */
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_set_vfo(rig, tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_get_freq(rig, tx_vfo, tx_freq)))
    {
        return retval;
    }

    if (VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        // Then we return the VFO to where it was
        rig_debug(RIG_DEBUG_TRACE, "%s: SATMODE rig so returning vfo to %s\n", __func__,
                  rig_strvfo(save_vfo));

        if (RIG_OK != (retval = icom_set_vfo(rig, save_vfo)))
        {
            return retval;
        }
    }
    else if (RIG_OK != (retval = icom_set_vfo(rig, rx_vfo)))
    {
        return retval;
    }

    if (VFO_HAS_A_B_ONLY && priv->split_on)
    {
        /* Re-enable split */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }
    }

    priv->vfob_freq = *tx_freq;
    return retval;
}

/*
 * icom_set_split_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *  icom_set_vfo,icom_set_mode works for this rig
 */
int icom_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                        pbwidth_t tx_width)
{
    int retval;
    vfo_t rx_vfo, tx_vfo;
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        if (RIG_OK != (retval = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                                tx_width)))
        {
            return retval;
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        return retval;
    }

    /* In the case of rigs with an A/B VFO arrangement we assume the
       current VFO is VFO A and the split Tx VFO is always VFO B. These
       assumptions allow us to deal with the lack of VFO and split
       queries */
    if (VFO_HAS_A_B_ONLY
            && priv->split_on)   /* broken if user changes split on rig :( */
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_set_vfo(rig, tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                            tx_width)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_set_vfo(rig, rx_vfo)))
    {
        return retval;
    }

    if (VFO_HAS_A_B_ONLY && priv->split_on)
    {
        /* Re-enable split */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }
    }

    return retval;
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
    int retval;
    vfo_t rx_vfo, tx_vfo;
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        if (RIG_OK != (retval = rig->caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                                tx_width)))
        {
            return retval;
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        return retval;
    }

    /* In the case of rigs with an A/B VFO arrangement we assume the
       current VFO is VFO A and the split Tx VFO is always VFO B. These
       assumptions allow us to deal with the lack of VFO and split
       queries */
    if (VFO_HAS_A_B_ONLY
            && priv->split_on)   /* broken if user changes split on rig :( */
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_set_vfo(rig, tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = rig->caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                            tx_width)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_set_vfo(rig, rx_vfo)))
    {
        return retval;
    }

    if (VFO_HAS_A_B_ONLY && priv->split_on)
    {
        /* Re-enable split */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }
    }

    return retval;
}

/*
 * icom_set_split_freq_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL,
 *  icom_set_vfo,icom_set_mode works for this rig
 */
int icom_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t tx_freq,
                             rmode_t tx_mode, pbwidth_t tx_width)
{
    int retval;
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);
    vfo_t rx_vfo, tx_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__,
              rig_strvfo(vfo));

    if (priv->curr_vfo == RIG_VFO_NONE)
    {
        icom_set_default_vfo(rig);
    }

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        if (RIG_OK != (retval = rig_set_freq(rig, RIG_VFO_CURR, tx_freq)))
        {
            return retval;
        }

        if (RIG_OK != (retval = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                                tx_width)))
        {
            return retval;
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        return retval;
    }

    /* In the case of rigs with an A/B VFO arrangement we assume the
       current VFO is VFO A and the split Tx VFO is always VFO B. These
       assumptions allow us to deal with the lack of VFO and split
       queries */
    if (VFO_HAS_A_B
            && priv->split_on)    /* broken if user changes split on rig :( */
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: before get_split_vfos rx_vfo=%s tx_vfo=%s\n", __func__,
              rig_strvfo(priv->rx_vfo), rig_strvfo(priv->tx_vfo));

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        return retval;
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: after get_split_vfos  rx_vfo=%s tx_vfo=%s\n", __func__,
              rig_strvfo(priv->rx_vfo), rig_strvfo(priv->tx_vfo));


    if (RIG_OK != (retval = icom_set_vfo(rig, tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = rig_set_freq(rig, RIG_VFO_CURR, tx_freq)))
    {
        return retval;
    }

    if (RIG_OK != (retval = rig->caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                            tx_width)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_set_vfo(rig, rx_vfo)))
    {
        return retval;
    }

    if (VFO_HAS_A_B && priv->split_on)
    {
        /* Re-enable split */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }
    }

    return retval;
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
    int retval;
    vfo_t rx_vfo, tx_vfo;
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    /* This method works also in memory mode(RIG_VFO_MEM) */
    if (!priv->no_xchg && rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        if (RIG_OK != (retval = rig_get_freq(rig, RIG_VFO_CURR, tx_freq)))
        {
            return retval;
        }

        if (RIG_OK != (retval = rig->caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                                tx_width)))
        {
            return retval;
        }

        if (RIG_OK != (retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG)))
        {
            return retval;
        }

        return retval;
    }

    /* In the case of rigs with an A/B VFO arrangement we assume the
       current VFO is VFO A and the split Tx VFO is always VFO B. These
       assumptions allow us to deal with the lack of VFO and split
       queries */
    if (VFO_HAS_A_B_ONLY
            && priv->split_on)   /* broken if user changes split on rig :( */
    {
        /* VFO A/B style rigs swap VFO on split Tx so we need to disable
           split for certainty */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_OFF, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }
    }

    if (RIG_OK != (retval = icom_get_split_vfos(rig, &rx_vfo, &tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_set_vfo(rig, tx_vfo)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_get_freq(rig, RIG_VFO_CURR, tx_freq)))
    {
        return retval;
    }

    if (RIG_OK != (retval = rig->caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                            tx_width)))
    {
        return retval;
    }

    if (RIG_OK != (retval = icom_set_vfo(rig, rx_vfo)))
    {
        return retval;
    }

    if (VFO_HAS_A_B_ONLY && priv->split_on)
    {
        /* Re-enable split */
        if (RIG_OK !=
                (retval =
                     icom_transaction(rig, C_CTL_SPLT, S_SPLT_ON, NULL, 0, ackbuf,
                                      &ack_len)))
        {
            return retval;
        }
    }

    return retval;
}


/*
 * icom_set_split
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    int split_sc;
    vfo_t vfo_final = RIG_VFO_NONE;  // where does the VFO end up?

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s called vfo='%s', split=%d, tx_vfo=%s, curr_vfo=%s\n", __func__,
              rig_strvfo(vfo), split, rig_strvfo(tx_vfo), rig_strvfo(priv->curr_vfo));

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->curr_vfo;
    }

    // This should automaticaly switch between satmode on/off based on the requested split vfo
    if ((tx_vfo == RIG_VFO_SUB || tx_vfo == RIG_VFO_MAIN) && !rig->state.cache.satmode)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: VFO_SUB and satmode is off so turning on\n",
                  __func__);
        rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, 1);
        rig->state.cache.satmode = 1;
        priv->tx_vfo = RIG_VFO_SUB;
    }
    else if ((tx_vfo == RIG_VFO_A || tx_vfo == RIG_VFO_B) && rig->state.cache.satmode)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: VFO_B and satmode is on so turning off\n",
                  __func__);
        rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, 0);
        rig->state.cache.satmode = 0;
        priv->tx_vfo = RIG_VFO_B;
    }
    else if (tx_vfo == RIG_VFO_SUB && rig->state.cache.satmode && split == 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: rig in satmode so setting split on is redundant and will create error...returning OK\n",
                  __func__);
        // we'll return OK anyways as this is a split mode
        // and gpredict wants to see the OK response here
        return RIG_OK;  // we'll return OK anyways as this is a split mode
    }

    switch (split)
    {
    case RIG_SPLIT_OFF:

        // if either VFOA or B is the vfo we set to VFOA when split is turned off
        if (tx_vfo == RIG_VFO_A || tx_vfo == RIG_VFO_B)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo=%s\n", __func__,
                      rig_strvfo(tx_vfo));
            priv->tx_vfo = RIG_VFO_A;
            //vfo_final = RIG_VFO_A; // do we need to switch back at all?
        }
        // otherwise if Main or Sub we set Main or VFOA as the current vfo
        else if (tx_vfo == RIG_VFO_MAIN || tx_vfo == RIG_VFO_SUB)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: vfo is VFO_MAIN/SUB tx_vfo=%s\n",
                      __func__, rig_strvfo(tx_vfo));

            //rig_set_vfo(rig, RIG_VFO_MAIN);
            //vfo_final = RIG_VFO_MAIN; // do we need to switch back at all?

            if (VFO_HAS_A_B_ONLY)
            {
                priv->tx_vfo = RIG_VFO_A;
                priv->rx_vfo = RIG_VFO_A;
            }
            else
            {
                priv->tx_vfo = RIG_VFO_MAIN;
                priv->rx_vfo = RIG_VFO_MAIN;
            }
        }

        split_sc = S_SPLT_OFF;
        break;

    case RIG_SPLIT_ON:
        split_sc = S_SPLT_ON;

        /* If asking for Sub or Main on rig that doesn't have it map it */
        if (VFO_HAS_A_B_ONLY && ((tx_vfo == RIG_VFO_MAIN || tx_vfo == RIG_VFO_SUB)
                                 || vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: vfo clause 1\n", __func__);

            if (tx_vfo == RIG_VFO_MAIN) { tx_vfo = RIG_VFO_A; }
            else if (tx_vfo == RIG_VFO_SUB) { tx_vfo = RIG_VFO_B; }

            if (vfo == RIG_VFO_MAIN) { vfo = RIG_VFO_A; }
            else if (vfo == RIG_VFO_SUB) { vfo = RIG_VFO_B; }

            priv->tx_vfo = tx_vfo;
            //vfo_final = RIG_VFO_A; // do we need to switch back at all?
        }

        /* ensure VFO A is Rx and VFO B is Tx as we assume that elsewhere */
        if (VFO_HAS_A_B && (tx_vfo == RIG_VFO_A || tx_vfo == RIG_VFO_B))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: vfo clause 2\n", __func__);
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: rx_vfo to VFO_A, tx_vfo to VFO_B because tx_vfo=%s\n", __func__,
                      rig_strvfo(tx_vfo));

            priv->tx_vfo = RIG_VFO_B;
            priv->rx_vfo = RIG_VFO_A;
            vfo_final = RIG_VFO_A;
        }
        else if (VFO_HAS_MAIN_SUB_A_B_ONLY && (tx_vfo == RIG_VFO_MAIN
                                               || tx_vfo == RIG_VFO_SUB))
        {
            // do we need another case for tx_vfo = A/B ?
            rig_debug(RIG_DEBUG_TRACE, "%s: vfo clause 3\n", __func__);
            // if we're asking for split in this case we split Main on A/B
            priv->tx_vfo = RIG_VFO_SUB;
            priv->rx_vfo = RIG_VFO_MAIN;
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: tx=%s, rx=%s because tx_vfo=%s\n", __func__,
                      rig_strvfo(priv->tx_vfo), rig_strvfo(priv->rx_vfo), rig_strvfo(tx_vfo));
            tx_vfo = RIG_VFO_SUB;

#if 0 // is this needed for satmode?

            // make sure we're on Main/VFOA
            if (RIG_OK != (retval = icom_set_vfo(rig, RIG_VFO_MAIN)))
            {
                return retval;
            }

            if (RIG_OK != (retval = icom_set_vfo(rig, RIG_VFO_A)))
            {
                return retval;
            }

#endif
        }
        else if (VFO_HAS_MAIN_SUB && (tx_vfo == RIG_VFO_MAIN || tx_vfo == RIG_VFO_SUB))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: vfo clause 4\n", __func__);
            rig_debug(RIG_DEBUG_TRACE, "%s: set_vfo because tx_vfo=%s\n", __func__,
                      rig_strvfo(tx_vfo));

#if 0 // do we need this for satmode?

            if (RIG_OK != (retval = icom_set_vfo(rig, tx_vfo)))
            {
                return retval;
            }

#endif

            priv->rx_vfo = vfo;
            priv->tx_vfo = tx_vfo;
            //vfo_final = RIG_VFO_MAIN;

            split_sc = S_SPLT_ON;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: split on vfo=%s not known\n", __func__,
                      rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported split %d", __func__, split);
        return -RIG_EINVAL;
    }

    if (RIG_OK != (retval = icom_transaction(rig, C_CTL_SPLT, split_sc, NULL, 0,
                            ackbuf, &ack_len)))
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    priv->split_on = RIG_SPLIT_ON == split;

    if (vfo_final != RIG_VFO_NONE && vfo_final != priv->curr_vfo)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo_final set %s\n", __func__,
                  rig_strvfo(vfo_final));
        retval = rig_set_vfo(rig, vfo_final);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: vfo_final set failed? err=%s\n", __func__,
                      rigerror(retval));
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: vfo=%s curr_vfo=%s rx_vfo=%s tx_vfo=%s split=%d\n",
              __func__, rig_strvfo(vfo), rig_strvfo(priv->curr_vfo), rig_strvfo(priv->rx_vfo),
              rig_strvfo(priv->tx_vfo), split);
    return RIG_OK;
}

/*
 * icom_get_split_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL, split!=NULL
 *
 * Does not appear to be supported by any mode?
 * \sa icom_mem_get_split_vfo()
 */
int icom_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    unsigned char splitbuf[MAXFRAMELEN];
    int split_len, retval, satmode = 0;
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    retval = icom_transaction(rig, C_CTL_SPLT, -1, NULL, 0,
                              splitbuf, &split_len);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CTL_SPLT failed?\n", __func__);
        return retval;
    }

    /*
     * splitbuf should contain Cn,Sc
     */
    split_len--;

    if (split_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, split_len);
        return -RIG_ERJCTED;
    }

    switch (splitbuf[1])
    {
    case S_SPLT_OFF:
        *split = RIG_SPLIT_OFF;
        break;

    case S_SPLT_ON:
        *split = RIG_SPLIT_ON;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported split %d", __func__,
                  splitbuf[1]);
        return -RIG_EPROTO;
    }

    rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_SATMODE, &satmode);
    rig->state.cache.satmode = satmode;

    priv->split_on = RIG_SPLIT_ON == *split;

    icom_get_split_vfos(rig, &priv->rx_vfo, &priv->tx_vfo);

    *tx_vfo = priv->tx_vfo;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s rx_vfo=%s tx_vfo=%s split=%d\n",
              __func__, rig_strvfo(vfo), rig_strvfo(priv->rx_vfo),
              rig_strvfo(priv->tx_vfo), *split);
    return RIG_OK;
}


/*
 * icom_mem_get_split_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL, split!=NULL
 */
int icom_mem_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                           vfo_t *tx_vfo)
{
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* this hacks works only when in memory mode
     * I have no clue how to detect split in regular VFO mode
     */
    if (rig->state.current_vfo != RIG_VFO_MEM ||
            !rig_has_vfo_op(rig, RIG_OP_XCHG))
    {
        *split = rig->state.cache.split; // we set this but still return ENAVAIL
        return -RIG_ENAVAIL;
    }

    retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG);

    if (retval == RIG_OK)
    {
        *split = RIG_SPLIT_ON;
        /* get it back to normal */
        retval = icom_vfo_op(rig, vfo, RIG_OP_XCHG);

        if (retval != RIG_OK) { return retval; }
    }
    else if (retval == -RIG_ERJCTED)
    {
        *split = RIG_SPLIT_OFF;
    }
    else
    {
        /* this is really an error! */
        return retval;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;

    for (i = 0; i < TSLSTSIZ; i++)
    {
        if (priv_caps->ts_sc_list[i].ts == ts)
        {
            ts_sc = priv_caps->ts_sc_list[i].sc;
            break;
        }
    }

    if (i >= TSLSTSIZ)
    {
        return -RIG_EINVAL;   /* not found, unsupported */
    }

    retval = icom_transaction(rig, C_SET_TS, ts_sc, NULL, 0, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    priv_caps = (const struct icom_priv_caps *) rig->caps->priv;

    retval = icom_transaction(rig, C_SET_TS, -1, NULL, 0, tsbuf, &ts_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * tsbuf should contain Cn,Sc
     */
    ts_len--;

    if (ts_len != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__, ts_len);
        return -RIG_ERJCTED;
    }

    for (i = 0; i < TSLSTSIZ; i++)
    {
        if (priv_caps->ts_sc_list[i].sc == tsbuf[1])
        {
            *ts = priv_caps->ts_sc_list[i].ts;
            break;
        }
    }

    if (i >= TSLSTSIZ)
    {
        return -RIG_EPROTO;   /* not found, unsupported */
    }

    return RIG_OK;
}


/*
 * icom_set_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    unsigned char fctbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int fct_len, acklen, retval;
    int fct_cn, fct_sc;       /* Command Number, Subcommand */
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
        // The command 0x27 0x10 is supported by many newer Icom rigs
        fct_cn = 0x27;
        fct_sc = 0x10;
        fctbuf[0] = status;
        fct_len = 1;
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
        fct_cn = C_SET_VFO;
        fct_sc = S_DUAL;
        break;

    case RIG_FUNC_SATMODE:
        if (rig->caps->rig_model == RIG_MODEL_IC910)
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

        priv->x25cmdfails = 0; // we reset this to try it again
        priv->x1cx03cmdfails = 0; // we reset this to try it again
        rig->state.cache.satmode = status;

        break;


    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_func %s", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }

    retval = icom_transaction(rig, fct_cn, fct_sc, fctbuf, fct_len, ackbuf,
                              &acklen);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (acklen != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__, acklen);
        return -RIG_EPROTO;
    }

    // turning satmode on/off can change the tx/rx vfos
    // when in satmode split=off always
    if (VFO_HAS_MAIN_SUB_A_B_ONLY)
    {
        vfo_t tx_vfo;
        split_t split;

        // update split status
        retval = icom_get_split_vfo(rig, RIG_VFO_CURR, &split, &tx_vfo);

        if (retval != RIG_OK) { return retval; }

        priv->tx_vfo = RIG_VFO_A;

        if (priv->split_on)   // must have turned off satmode
        {
            priv->tx_vfo = RIG_VFO_B;
        }
        else if (status)   // turned on satmode so tx is always Sub
        {
            priv->tx_vfo = RIG_VFO_SUB;
        }
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
        // The command 0x27 0x10 is supported by many newer Icom rigs
        fct_cn = 0x27;
        fct_sc = 0x10;
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

    case RIG_FUNC_DUAL_WATCH: fct_cn = C_SET_VFO;
        fct_sc = S_DUAL;
        break;

    case RIG_FUNC_SATMODE: if (rig->caps->rig_model == RIG_MODEL_IC910)
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
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_func %s\n", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }

    retval = icom_transaction(rig, fct_cn, fct_sc, NULL, 0, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n", __func__,
                  ack_len);
        return -RIG_EPROTO;
    }

    if (func != RIG_FUNC_FBKIN)
    {
        *status = ackbuf[2];
    }
    else
    {
        *status = ackbuf[2] == 2 ? 1 : 0;
    }

    return RIG_OK;
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
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    int i;
    const struct icom_priv_caps *priv = rig->caps->priv;
    const struct cmdparams *cmd = priv->extcmds;

    for (i = 0; cmd && cmd[i].id.s != 0; i++)
    {
        if (cmd[i].id.s == parm)
        {
            return icom_set_cmd(rig, RIG_VFO_NONE, (struct cmdparams *)&cmd[i], val);
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
            return -RIG_EINVAL;
        }

        return icom_set_raw(rig, C_CTL_ANN, ann_mode, 0, NULL, 0, 0);
    }

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_parm %s\n", __func__,
                  rig_strparm(parm));
        return -RIG_EINVAL;
    }
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
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    const struct icom_priv_caps *priv = rig->caps->priv;
    const struct cmdparams *cmd = priv->extcmds;
    int i;

    for (i = 0; cmd && cmd[i].id.s != 0; i++)
    {
        if (cmd[i].id.s == parm)
        {
            return icom_get_cmd(rig, RIG_VFO_NONE, (struct cmdparams *)&cmd[i], val);
        }
    }

    switch (parm)
    {
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_parm %s", __func__,
                  rig_strparm(parm));
        return -RIG_EINVAL;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
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
            return -RIG_EINVAL;
        }
    }

    /* Sent as frequency in tenth of Hz */

    tone_len = 3;
    to_bcd_be(tonebuf, tone, tone_len * 2);

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_RPTR,
                              tonebuf, tone_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    caps = rig->caps;

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_RPTR, NULL, 0,
                              tonebuf, &tone_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* cn,sc,data*3 */
    if (tone_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  tonebuf[0], tone_len);
        return -RIG_ERJCTED;
    }

    tone_len -= 2;
    *tone = from_bcd_be(tonebuf + 2, tone_len * 2);

    if (!caps->ctcss_list)
    {
        return RIG_OK;
    }

    /* check this tone exists. That's better than nothing. */
    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (caps->ctcss_list[i] == *tone)
        {
            return RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%#.2x)\n", __func__, tonebuf[2]);
    return -RIG_EPROTO;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
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
        return -RIG_EINVAL;
    }

    /* Sent as frequency in tenth of Hz */

    tone_len = 3;
    to_bcd_be(tonebuf, tone, tone_len * 2);

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_SQL,
                              tonebuf, tone_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    caps = rig->caps;

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_SQL, NULL, 0,
                              tonebuf, &tone_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (tone_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  tonebuf[0], tone_len);
        return -RIG_ERJCTED;
    }

    tone_len -= 2;
    *tone = from_bcd_be(tonebuf + 2, tone_len * 2);

    /* check this tone exists. That's better than nothing. */
    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (caps->ctcss_list[i] == *tone)
        {
            return RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%#.2x)\n", __func__, tonebuf[2]);
    return -RIG_EPROTO;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
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
        return -RIG_EINVAL;
    }

    /* DCS Polarity ignored, by setting code_len to 3 it's foretval to 0 (= Tx:norm, Rx:norm). */
    code_len = 3;
    to_bcd_be(codebuf, code, code_len * 2);

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_DTCS,
                              codebuf, code_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    caps = rig->caps;

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_DTCS, NULL, 0,
                              codebuf, &code_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* cn,sc,data*3 */
    if (code_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  codebuf[0], code_len);
        return -RIG_ERJCTED;
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
            return RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: DTCS NG (%#.2x)\n", __func__, codebuf[2]);
    return -RIG_EPROTO;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
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
        return -RIG_EINVAL;
    }

    /* DCS Polarity ignored, by setting code_len to 3 it's forced to 0 (= Tx:norm, Rx:norm). */
    code_len = 3;
    to_bcd_be(codebuf, code, code_len * 2);

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_DTCS,
                              codebuf, code_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    caps = rig->caps;

    retval = icom_transaction(rig, C_SET_TONE, S_TONE_DTCS, NULL, 0,
                              codebuf, &code_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* cn,sc,data*3 */
    if (code_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  codebuf[0], code_len);
        return -RIG_ERJCTED;
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
            return RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: DTCS NG (%#.2x)\n", __func__, codebuf[2]);
    return -RIG_EPROTO;
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
    // so we'll do up to 175 for 115,200
    int fe_max = 175;
    unsigned char fe_buf[fe_max]; // for FE's to power up
    int i;
    int retry;
    struct rig_state *rs = &rig->state;
    struct icom_priv_data *priv = (struct icom_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called status=%d\n", __func__,
              (int) status);

    switch (status)
    {
    case RIG_POWER_ON:

        sleep(1);         // let serial bus idle for a while
        rig_debug(RIG_DEBUG_TRACE, "%s: PWR_ON failed, trying 0xfe's\n",
                  __func__);
        // ic7300 manual says ~150 for 115,200
        // we'll just send a few more to be sure for all speeds
        memset(fe_buf, 0xfe, fe_max);
        // sending more than enough 0xfe's to wake up the rs232
        write_block(&rs->rigport, (char *) fe_buf, fe_max);

        hl_usleep(100 * 1000);
        // we'll try 0x18 0x01 now -- should work on STBY rigs too
        pwr_sc = S_PWR_ON;
        fe_buf[0] = 0;
        retry = rs->rigport.retry;
        rs->rigport.retry = 0;
        priv->serial_USB_echo_off = 1;
        retval =
            icom_transaction(rig, C_SET_PWR, pwr_sc, NULL, 0, ackbuf, &ack_len);
        rs->rigport.retry = retry;

        break;

    default:
        pwr_sc = S_PWR_OFF;
        fe_buf[0] = 0;
        retry = rs->rigport.retry;
        rs->rigport.retry = 0;
        retval =
            icom_transaction(rig, C_SET_PWR, pwr_sc, NULL, 0, ackbuf, &ack_len);
        rs->rigport.retry = retry;
    }

    i = 0;
    retry = 1;

    if (status == RIG_POWER_ON)   // wait for wakeup only
    {

        for (i = 0; i < retry; ++i)   // up to 10 attempts
        {
            freq_t freq;
            sleep(1);
            // need to see if echo is on or not first
            // until such time as rig is awake we don't know
            icom_get_usb_echo_off(rig);

            // Use get_freq as all rigs should repond to this
            retval = rig_get_freq(rig, RIG_VFO_CURR, &freq);

            if (retval == RIG_OK)
            {
                return retval;
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

    if (i == retry)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Wait failed for get_powerstat\n",
                  __func__);
        retval = -RIG_ETIMEOUT;
    }

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: retval != RIG_OK, =%s\n", __func__,
                  rigerror(retval));
        return retval;
    }

    if (status == RIG_POWER_OFF && (ack_len != 1 || ackbuf[0] != ACK))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * icom_get_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_powerstat(RIG *rig, powerstat_t *status)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* r75 has no way to get power status, so fake it */
    if (rig->caps->rig_model == RIG_MODEL_ICR75)
    {
        /* getting the mode doesn't work if a memory is blank */
        /* so use one of the more innculous 'set mode' commands instead */
        int cmd_len = 1;
        unsigned char cmdbuf[MAXFRAMELEN];
        cmdbuf[0] = S_PRM_TIME;
        retval = icom_transaction(rig, C_CTL_MEM, S_MEM_MODE_SLCT,
                                  cmdbuf, cmd_len, ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        *status = ((ack_len == 6) && (ackbuf[0] == C_CTL_MEM)) ?
                  RIG_POWER_ON : RIG_POWER_OFF;
    }
    else
    {
        retval = icom_transaction(rig, C_SET_PWR, -1, NULL, 0,
                                  ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (ack_len != 1 || ackbuf[0] != ACK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
            return -RIG_ERJCTED;
        }

        *status = ackbuf[1] == S_PWR_ON ? RIG_POWER_ON : RIG_POWER_OFF;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    chan_len = ch < 100 ? 1 : 2;

    to_bcd_be(membuf, ch, chan_len * 2);
    retval = icom_transaction(rig, C_SET_MEM, -1, membuf, chan_len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    to_bcd_be(bankbuf, bank, BANK_NB_LEN * 2);
    retval = icom_transaction(rig, C_SET_MEM, S_BANK,
                              bankbuf, CHAN_NB_LEN, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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
        return -RIG_EINVAL;
    }

    if (ant > RIG_ANT_4)
    {
        return -RIG_EDOM;
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
        return -RIG_EINVAL;
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
            return retval;
        }
    }

    // Some rigs have 3-byte ant cmd so there is an option to be set too
    if (priv_caps->antack_len == 3)
    {
        if (option.i != 0 && option.i != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: option.i != 0 or 1, ==%d?\n", __func__, option.i);
            return -RIG_EINVAL;
        }

        antopt_len = 1;
        antopt[0] = option.i;
        // we have to set the rx option by itself apparently
        rig_debug(RIG_DEBUG_TRACE, "%s: setting antopt=%d\n", __func__, antopt[0]);
        retval = icom_transaction(rig, C_CTL_ANT, i_ant,
                                  antopt, antopt_len, ackbuf, &ack_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        antopt_len = 0;
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
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * icom_get_ant
 * Assumes rig!=NULL, rig->state.priv!=NULL
 * only meaningfull for HF
 */
int icom_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                 ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = sizeof(ackbuf), retval;
    struct icom_priv_caps *priv_caps = (struct icom_priv_caps *) rig->caps->priv;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called, ant=0x%02x\n", __func__, ant);

    *ant_tx = *ant_rx = RIG_ANT_UNKNOWN;

    if (ant != RIG_ANT_CURR)
    {
        ant = rig_setting2idx(ant);

        if (ant >= priv_caps->ant_count)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ant index=%u > ant_count=%d\n", __func__, ant,
                      priv_caps->ant_count);
            return -RIG_EINVAL;
        }
    }

    // Should be able to use just C_CTL_ANT for 1 or 2 antennas hopefully
    if (ant == RIG_ANT_CURR || priv_caps->ant_count <= 2)
    {
        retval = icom_transaction(rig, C_CTL_ANT, -1, NULL, 0, ackbuf, &ack_len);
    }
    else if (rig->caps->rig_model == RIG_MODEL_IC785x)
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
            return RIG_OK;
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: asking for non-current antenna and ant_count==0?\n", __func__);
        rig_debug(RIG_DEBUG_ERR, "%s: need to implement ant control for this rig?\n",
                  __func__);
        return -RIG_EINVAL;
    }

    if (retval != RIG_OK)
    {
        return retval;
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
        return -RIG_ERJCTED;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: ackbuf= 0x%02x 0x%02x 0x%02x\n", __func__,
              ackbuf[0], ackbuf[1], ackbuf[2]);

    *ant_curr = rig_idx2setting(ackbuf[1]);

    // Note: with IC756/IC-756Pro/IC-7800 and more, ackbuf[2] deals with [RX ANT]
    // Hopefully any ack_len=3 can fit in the option field
    if (ack_len == 3)
    {
        option->i = ackbuf[2];
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
            return -RIG_ENAVAIL;
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
        return -RIG_EINVAL;
    }

    retval =
        icom_transaction(rig, mv_cn, mv_sc, mvbuf, mv_len, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        if (op != RIG_OP_XCHG)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      ackbuf[0], ack_len);
        }

        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    scan_len = 0;
    scan_cn = C_CTL_SCAN;

    switch (scan)
    {
    case RIG_SCAN_STOP:
        scan_sc = S_SCAN_STOP;
        break;

    case RIG_SCAN_MEM: retval = icom_set_vfo(rig, RIG_VFO_MEM);

        if (retval != RIG_OK)
        {
            return retval;
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
        retval = icom_set_vfo(rig, RIG_VFO_MEM);

        if (retval != RIG_OK)
        {
            return retval;
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
            return retval;
        }

        retval = icom_set_vfo(rig, RIG_VFO_VFO);

        if (retval != RIG_OK)
        {
            return retval;
        }

        scan_sc = S_SCAN_START;
        break;

    case RIG_SCAN_DELTA:
        scan_sc = S_SCAN_DELTA;   /* TODO: delta-f support */
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported scan %#x", __func__, scan);
        return -RIG_EINVAL;
    }

    retval = icom_transaction(rig, scan_cn, scan_sc, scanbuf, scan_len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    len = strlen(msg);

    if (len > 30)
    {
        len = 30;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: %s\n", __func__, msg);

    retval = icom_transaction(rig, C_SND_CW, -1, (unsigned char *) msg, len,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

int icom_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq,
                  rmode_t mode)
{
    int rig_id;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_id = rig->caps->rig_model;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (rig_id)
    {
    default:
        /* Normal 100 Watts */
        *mwpower = power * 100000;
        break;
    }

    return RIG_OK;
}

int icom_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq,
                  rmode_t mode)
{
    int rig_id;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_id = rig->caps->rig_model;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed mwpower = %u\n", __func__,
              mwpower);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %" PRIfreq " Hz\n",
              __func__, freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));

    if (mwpower > 100000)
    {
        return -RIG_EINVAL;
    }

    switch (rig_id)
    {
    default:            /* Default to a 100W radio */
        *power = ((float) mwpower / 100000);
        break;
    }

    return RIG_OK;
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
    int frm_len;
    rmode_t mode;
    pbwidth_t width;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rs = &rig->state;
    priv = (struct icom_priv_data *) rs->priv;

    frm_len = read_icom_frame(&rs->rigport, buf, sizeof(buf));

    if (frm_len == -RIG_ETIMEOUT)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: got a timeout before the first character\n", __func__);
    }

    if (frm_len < 1)
    {
        return 0;
    }

    switch (buf[frm_len - 1])
    {
    case COL:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: saw a collision\n", __func__);
        /* Collision */
        return -RIG_BUSBUSY;

    case FI:
        /* Ok, normal frame */
        break;

    default:
        /* Timeout after reading at least one character */
        /* Problem on ci-v bus? */
        return -RIG_EPROTO;
    }

    if (buf[3] != BCASTID && buf[3] != priv->re_civ_addr)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: CI-V %#x called for %#x!\n", __func__,
                  priv->re_civ_addr, buf[3]);
    }

    /*
     * the first 2 bytes must be 0xfe
     * the 3rd one the emitter
     * the 4rd one 0x00 since this is transceive mode
     * then the command number
     * the rest is data
     * and don't forget one byte at the end for the EOM
     */
    switch (buf[4])
    {
    case C_SND_FREQ:

        /*
         * TODO: the freq length might be less than 4 or 5 bytes
         *          on older rigs!
         */
        if (rig->callbacks.freq_event)
        {
            freq_t freq;
            freq = from_bcd(buf + 5, (priv->civ_731_mode ? 4 : 5) * 2);
            return rig->callbacks.freq_event(rig, RIG_VFO_CURR, freq,
                                             rig->callbacks.freq_arg);
        }
        else
        {
            return -RIG_ENAVAIL;
        }

        break;

    case C_SND_MODE:
        if (rig->callbacks.mode_event)
        {
            icom2rig_mode(rig, buf[5], buf[6], &mode, &width);
            return rig->callbacks.mode_event(rig, RIG_VFO_CURR,
                                             mode, width,
                                             rig->callbacks.mode_arg);
        }
        else
        {
            return -RIG_ENAVAIL;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: transceive cmd unsupported %#2.2x\n",
                  __func__, buf[4]);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

int icom_set_raw(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                 unsigned char *subcmdbuf, int val_bytes, int val)
{
    unsigned char cmdbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int acklen = sizeof(ackbuf);
    int cmdbuflen = subcmdbuflen;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (subcmdbuflen > 0)
    {
        if (subcmdbuf == NULL)
        {
            return -RIG_EINTERNAL;
        }

        memcpy(cmdbuf, subcmdbuf, subcmdbuflen);
    }

    if (val_bytes > 0)
    {
        to_bcd_be(cmdbuf + subcmdbuflen, (long long) val, val_bytes * 2);
        cmdbuflen += val_bytes;
    }

    retval =
        icom_transaction(rig, cmd, subcmd, cmdbuf, cmdbuflen, ackbuf, &acklen);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (acklen != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], acklen);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

int icom_get_raw_buf(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                     unsigned char *subcmdbuf, int *reslen,
                     unsigned char *res)
{
    unsigned char ackbuf[MAXFRAMELEN];
    int acklen = sizeof(ackbuf);
    int cmdhead = subcmdbuflen;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval =
        icom_transaction(rig, cmd, subcmd, subcmdbuf, subcmdbuflen, ackbuf,
                         &acklen);

    if (retval != RIG_OK)
    {
        return retval;
    }

    cmdhead += (subcmd == -1) ? 1 : 2;
    acklen -= cmdhead;

    if (ackbuf[0] != ACK && ackbuf[0] != cmd)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], acklen);
        return -RIG_ERJCTED;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: %d\n", __func__, acklen);

    if (*reslen < acklen || res == NULL)
    {
        return -RIG_EINTERNAL;
    }

    memcpy(res, ackbuf + cmdhead, acklen);
    *reslen = acklen;

    return RIG_OK;
}

int icom_get_raw(RIG *rig, int cmd, int subcmd, int subcmdbuflen,
                 unsigned char *subcmdbuf, int *val)
{
    unsigned char resbuf[MAXFRAMELEN];
    int reslen = sizeof(resbuf);
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval =
        icom_get_raw_buf(rig, cmd, subcmd, subcmdbuflen, subcmdbuf, &reslen,
                         resbuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    * val = from_bcd_be(resbuf, reslen * 2);

    rig_debug(RIG_DEBUG_TRACE, "%s: %d %d\n", __func__, reslen, *val);

    return RIG_OK;
}

int icom_set_level_raw(RIG *rig, setting_t level, int cmd, int subcmd,
                       int subcmdbuflen, unsigned char *subcmdbuf,
                       int val_bytes, value_t val)
{
    int icom_val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        icom_val = (int)(val.f * 255.0f);
    }
    else
    {
        icom_val = val.i;
    }

    return icom_set_raw(rig, cmd, subcmd, subcmdbuflen, subcmdbuf, val_bytes,
                        icom_val);
}

int icom_get_level_raw(RIG *rig, setting_t level, int cmd, int subcmd,
                       int subcmdbuflen, unsigned char *subcmdbuf,
                       value_t *val)
{
    int icom_val;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval =
        icom_get_raw(rig, cmd, subcmd, subcmdbuflen, subcmdbuf, &icom_val);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        val->f = (float) icom_val / 255.0f;
    }
    else
    {
        val->i = icom_val;
    }

    return RIG_OK;
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    to_bcd_be(chbuf, ch, 2);

    retval = icom_transaction(rig, C_SND_VOICE, 0, chbuf, 1,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
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
        return RIG_OK;
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

    for (i = 0; i < FRQRANGESIZ
            && !RIG_IS_FRNG_END(rig->caps->rx_range_list1[i]); i++)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: rig chan %d, low=%.0f, high=%.0f\n", __func__,
                  i, (double)rig->caps->rx_range_list1[i].startf,
                  (double)rig->caps->rx_range_list1[i].endf);
    }

    return RIG_OK;
}

// Sets rig vfo && priv->curr_vfo to default VFOA, or current vfo, or the vfo requested
static int set_vfo_curr(RIG *rig, vfo_t vfo, vfo_t curr_vfo)
{
    int retval;
    struct icom_priv_data *priv = (struct icom_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s, curr_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(curr_vfo));

    if (vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Asking for currVFO,  currVFO=%s\n", __func__,
                  rig_strvfo(priv->curr_vfo));
        vfo = priv->curr_vfo;
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
    if (priv->curr_vfo == RIG_VFO_NONE && vfo == RIG_VFO_CURR)
    {
        icom_set_default_vfo(rig);
    }
    // asking for vfo_curr so give it to them
    else if (priv->curr_vfo != RIG_VFO_NONE && vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: using curr_vfo=%s\n", __func__,
                  rig_strvfo(priv->curr_vfo));
        vfo = priv->curr_vfo;
    }
    // only need to set vfo if it's changed
    else if (priv->curr_vfo != vfo)
    {
        if (!(VFO_HAS_MAIN_SUB_A_B_ONLY && !priv->split_on && !rig->state.cache.satmode
                && vfo == RIG_VFO_SUB && priv->curr_vfo == RIG_VFO_B))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: setting new vfo=%s\n", __func__,
                      rig_strvfo(vfo));
            retval = rig_set_vfo(rig, vfo);

            if (retval != RIG_OK)
            {
                return retval;
            }

            priv->curr_vfo = vfo;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: curr_vfo now=%s\n", __func__,
              rig_strvfo(priv->curr_vfo));

    rig->state.current_vfo = vfo;

    return RIG_OK;
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
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
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
            return RIG_MODEL_NONE;
        }

        /*
         * try all possible addresses on the CI-V bus
         * FIXME: actualy, old rigs do not support C_RD_TRXID cmd!
         *      Try to be smart, and deduce model depending
         *      on freq range, return address, and
         *      available commands.
         */
        for (civ_addr = 0x01; civ_addr <= 0x7f; civ_addr++)
        {

            frm_len = make_cmd_frame((char *) buf, civ_addr, CTRLID,
                                     C_RD_TRXID, S_RD_TRXID, NULL, 0);

            serial_flush(port);
            write_block(port, (char *) buf, frm_len);

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
                return RIG_MODEL_NONE;
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

            frm_len = make_cmd_frame((char *) buf, civ_addr, CTRLID,
                                     C_CTL_MISC, S_OPTO_RDID, NULL, 0);

            serial_flush(port);
            write_block(port, (char *) buf, frm_len);

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
            return model;
        }
    }

    return model;
}

/*
 * initrigs_icom is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(icom)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&ic703_caps);
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

    rig_register(&ic820h_caps);
    rig_register(&ic821h_caps);
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
    rig_register(&ic471_caps);
    rig_register(&ic475_caps);
    rig_register(&ic1275_caps);

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

    return RIG_OK;
}
