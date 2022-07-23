/*
 *  Hamlib CI-V backend - description of IC-R30
 *  Copyright (c) 2018 Malcolm Herring
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


#include "hamlib/rig.h"
#include "token.h"
#include "icom.h"
#include "idx_builtin.h"
#include "icom_defs.h"
#include "frame.h"

#define ICR30_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_AM|RIG_MODE_AMN|\
    RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_WFM|\
    RIG_MODE_RTTYR|RIG_MODE_SAM|RIG_MODE_SAL|RIG_MODE_SAH|RIG_MODE_P25|\
    RIG_MODE_DSTAR|RIG_MODE_DPMR|RIG_MODE_NXDNVN|RIG_MODE_NXDN_N|RIG_MODE_DCR)

#define ICR30_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_TSQL|RIG_FUNC_AFC|RIG_FUNC_VSC|\
    RIG_FUNC_CSQL|RIG_FUNC_DSQL|RIG_FUNC_ANL|RIG_FUNC_CSQL|RIG_FUNC_SCEN)

#define ICR30_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AF|RIG_LEVEL_RF|\
    RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define ICR30_VFO_ALL (RIG_VFO_MAIN|RIG_VFO_SUB)

#define ICR30_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define ICR30_SCAN_OPS (RIG_SCAN_NONE)

#define TOK_ANL TOKEN_BACKEND(001)
#define TOK_EAR TOKEN_BACKEND(002)
#define TOK_REC TOKEN_BACKEND(003)

int icr30_tokens[] = { TOK_ANL, TOK_EAR, TOK_REC,
                       TOK_DSTAR_DSQL, TOK_DSTAR_CALL_SIGN, TOK_DSTAR_MESSAGE, TOK_DSTAR_STATUS,
                       TOK_DSTAR_GPS_DATA, TOK_DSTAR_GPS_MESS, TOK_DSTAR_CODE, TOK_DSTAR_TX_DATA,
                       TOK_BACKEND_NONE
                     };

struct confparams icr30_ext[] =
{
    { TOK_ANL, "anl", "Auto noise limiter", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_EAR, "ear", "Earphone mode", "", "", RIG_CONF_CHECKBUTTON, {} },
    { TOK_REC, "record", "Recorder on/off", "", "", RIG_CONF_CHECKBUTTON, {} },
    { 0 }
};

struct cmdparams icr30_extcmds[] =
{
    { {.t = TOK_ANL}, CMD_PARAM_TYPE_TOKEN, C_CTL_MEM, S_MEM_ANL, SC_MOD_RW, 0, {}, CMD_DAT_BOL, 1 },
    { {.t = TOK_EAR}, CMD_PARAM_TYPE_TOKEN, C_CTL_MEM, S_MEM_EAR, SC_MOD_RW, 0, {}, CMD_DAT_BOL, 1 },
    { {.t = TOK_REC}, CMD_PARAM_TYPE_TOKEN, C_CTL_MEM, S_MEM_REC, SC_MOD_WR, 0, {}, CMD_DAT_BOL, 1 },
    { {0} }
};

#define ICR30_STR_CAL { 2, \
    { \
        {  0, -60 }, /* S0 */ \
        { 255, 60 } /* +60 */ \
    } }

/*
 * This function does the special bandwidth coding for IC-R30
 * (1 - normal, 2 - narrow)
 */

static int icr30_r2i_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                          unsigned char *md, signed char *pd)
{
    int err;

    err = rig2icom_mode(rig, vfo, mode, width, md, pd);

    if (*pd == PD_NARROW_3)
    {
        *pd = PD_NARROW_2;
    }

    return err;
}

/*
 * This function handles the -N modes for IC-R30
 */

int icr30_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    if (mode & (RIG_MODE_AMN | RIG_MODE_FMN))
    {
        return icom_set_mode(rig, vfo, mode, (pbwidth_t)1);
    }
    else
    {
        return icom_set_mode(rig, vfo, mode, width);
    }
}

static struct icom_priv_caps icr30_priv_caps =
{
    0x9c, /* default address */
    0,        /* 731 mode */
    0,    /* no XCHG */
    r8500_ts_sc_list, /* wrong, but don't have set_ts anyway */
    .antack_len = 2,
    .ant_count = 2,
    .r2i_mode = icr30_r2i_mode,
    .offs_len = 4,
    .extcmds = icr30_extcmds      /* Custom ext_parm parameters */
};

const struct rig_caps icr30_caps =
{
    RIG_MODEL(RIG_MODEL_ICR30),
    .model_name = "IC-R30",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_RECEIVER | RIG_FLAG_HANDHELD,
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
    .has_get_func =  ICR30_FUNC_ALL,
    .has_set_func =  ICR30_FUNC_ALL,
    .has_get_level =  ICR30_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ICR30_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ext_tokens = icr30_tokens,
    .extfuncs = icr30_ext,
    .extparms = icom_ext_parms,
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator = { 15, 30, 35, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICR30_VFO_OPS,
    .scan_ops =  ICR30_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {    1,  999, RIG_MTYPE_MEM  }, /* TBC */
        { 1000, 1199, RIG_MTYPE_MEM },  /* auto-write */
        { 1200, 1299, RIG_MTYPE_EDGE }, /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {   /* Other countries but France */
        {kHz(100), GHz(3.3049999), ICR30_MODES, -1, -1, ICR30_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   { RIG_FRNG_END, },

    .rx_range_list2 =   {   /* USA */
        {kHz(100), MHz(821.995), ICR30_MODES, -1, -1, ICR30_VFO_ALL},
        {MHz(851), MHz(866.995), ICR30_MODES, -1, -1, ICR30_VFO_ALL},
        {MHz(896), GHz(3.3049999), ICR30_MODES, -1, -1, ICR30_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   { RIG_FRNG_END, },

    .tuning_steps =     {
        {ICR30_MODES, Hz(10)},
        {ICR30_MODES, Hz(100)},
        {ICR30_MODES, Hz(1000)},
        {ICR30_MODES, Hz(3125)},
        {ICR30_MODES, Hz(5000)},
        {ICR30_MODES, Hz(6250)},
        {ICR30_MODES, Hz(8330)},
        {ICR30_MODES, Hz(9000)},
        {ICR30_MODES, Hz(10000)},
        {ICR30_MODES, Hz(12500)},
        {ICR30_MODES, kHz(15)},
        {ICR30_MODES, kHz(20)},
        {ICR30_MODES, kHz(25)},
        {ICR30_MODES, kHz(30)},
        {ICR30_MODES, kHz(50)},
        {ICR30_MODES, kHz(100)},
        {ICR30_MODES, kHz(125)},
        {ICR30_MODES, kHz(200)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_USB | RIG_MODE_LSB, kHz(1.8)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_AMN | RIG_MODE_FMN, kHz(12)},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_AMN | RIG_MODE_FMN, kHz(6)},
        {RIG_MODE_WFM, kHz(150)},
        RIG_FLT_END,
    },
    .str_cal = ICR30_STR_CAL,

    .cfgparams = icom_cfg_params,
    .set_conf = icom_set_conf,
    .get_conf = icom_get_conf,
    .set_powerstat = icom_set_powerstat,
    .get_powerstat = icom_get_powerstat,

    .priv = (void *)& icr30_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icr30_set_mode,
    .get_mode =  icom_get_mode,
    .vfo_op =  icom_vfo_op,
    .set_vfo =  icom_set_vfo,
    .set_rptr_offs = icom_set_rptr_offs,
    .get_rptr_offs = icom_get_rptr_offs,
    .set_rptr_shift = icom_set_rptr_shift,
    .get_rptr_shift = icom_get_rptr_shift,
    .set_ts = icom_set_ts,
    .get_ts = icom_get_ts,
    .set_ant = icom_set_ant,
    .get_ant = icom_get_ant,
    .set_bank = icom_set_bank,
    .set_mem = icom_set_mem,
    .decode_event =  icom_decode_event,
    .set_level = icom_set_level,
    .get_level = icom_get_level,
    .set_func = icom_set_func,
    .get_func = icom_get_func,
    .set_parm = icom_set_parm,
    .get_parm = icom_get_parm,
    .set_ext_parm = icom_set_ext_parm,
    .get_ext_parm = icom_get_ext_parm,
    .set_ext_level = icom_set_ext_level,
    .get_ext_level = icom_get_ext_level,
    .set_ext_func = icom_set_ext_func,
    .get_ext_func = icom_get_ext_func,
    .get_dcd = icom_get_dcd,
    .set_ctcss_sql = icom_set_ctcss_sql,
    .get_ctcss_sql = icom_get_ctcss_sql,
    .set_dcs_sql = icom_set_dcs_sql,
    .get_dcs_sql = icom_get_dcs_sql,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
