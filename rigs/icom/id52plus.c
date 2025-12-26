/*
 *  Hamlib CI-V backend - description of ID-52A/E PLUS and variations
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
#include "token.h"
#include "icom.h"
#include "idx_builtin.h"
#include "icom_defs.h"
#include "tones.h"

/*
 * Specs and protocol details from ID-52A_E_PLUS_ENG_CI-V_0.pdf
 *
 * The ID-52A/E PLUS connects via USB Type-C which provides a serial port.
 * CI-V address is 0xB4 (from PDF data format section).
 *
 * Operating modes: FM (05 01), FM-N (05 02), DV (17 01), AM (02 01), AM-N (02 02)
 * Power levels: S-Low, Low1, Low2, Mid, High (5 levels)
 * Attenuator: OFF, 10dB (375-479 MHz), 30dB (108-374.995 MHz)
 */

#define ID52PLUS_MODES (RIG_MODE_FM|RIG_MODE_DSTAR)
#define ID52PLUS_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_AMN|ID52PLUS_MODES)

#define ID52PLUS_VFO_ALL (RIG_VFO_MAIN|RIG_VFO_SUB)

#define ID52PLUS_SCAN_OPS RIG_SCAN_NONE

#define ID52PLUS_VFO_OPS  RIG_OP_NONE

#define ID52PLUS_FUNC_ALL ( \
                            RIG_FUNC_TONE| \
                            RIG_FUNC_TSQL| \
                            RIG_FUNC_CSQL| \
                            RIG_FUNC_DSQL| \
                            RIG_FUNC_VOX)

#define ID52PLUS_LEVEL_ALL    (RIG_LEVEL_AF| \
                            RIG_LEVEL_SQL| \
                            RIG_LEVEL_RAWSTR| \
                            RIG_LEVEL_RFPOWER| \
                            RIG_LEVEL_MICGAIN| \
                            RIG_LEVEL_VOXGAIN| \
                            RIG_LEVEL_ATT)

#define ID52PLUS_PARM_ALL RIG_PARM_NONE

int id52plus_tokens[] = { TOK_DSTAR_DSQL, TOK_DSTAR_CALL_SIGN, TOK_DSTAR_MESSAGE, TOK_DSTAR_STATUS,
                      TOK_DSTAR_GPS_DATA, TOK_DSTAR_GPS_MESS, TOK_DSTAR_CODE, TOK_DSTAR_TX_DATA,
                      TOK_DSTAR_MY_CS, TOK_DSTAR_TX_CS, TOK_DSTAR_TX_MESS,
                      TOK_BACKEND_NONE
                    };

/*
 * S-meter calibration from CI-V command 15 02:
 * Response range: 00 00 to 02 55 (S0 to S9)
 */
#define ID52PLUS_STR_CAL  UNKNOWN_IC_STR_CAL



/*
 * ID-52A/E PLUS private capabilities
 * CI-V Address: 0xB4 (from PDF: "FE FE B4 E0...")
 */
static struct icom_priv_caps id52plus_priv_caps =
{
    0xB4,   /* default address */
    0,      /* 731 mode */
    1,      /* no XCHG */
};

struct rig_caps id52plus_caps =
{
    RIG_MODEL(RIG_MODEL_ID52PLUS),
    .model_name = "ID-52A/E PLUS",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_HANDHELD,
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
    .has_get_func =  ID52PLUS_FUNC_ALL,
    .has_set_func =  ID52PLUS_FUNC_ALL,
    .has_get_level =  ID52PLUS_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ID52PLUS_LEVEL_ALL),
    .has_get_parm =  ID52PLUS_PARM_ALL,
    .has_set_parm =  ID52PLUS_PARM_ALL,
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_AF] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f/40.0f } },
        [LVL_SQL] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f/11.0f } },
        [LVL_RFPOWER] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f/5.0f } },
        [LVL_MICGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f/4.0f } },
        [LVL_VOXGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f/11.0f } },
    },
    .ext_tokens = id52plus_tokens,
    .extparms = icom_ext_parms,
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  full_dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 10, 30, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ID52PLUS_VFO_OPS,
    .scan_ops =  ID52PLUS_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        // There's no memory support through CI-V,
        // but there is a clone mode apart.
        RIG_CHAN_END,
    },

    /* Europe version - rx_range_list1, tx_range_list1 */
    .rx_range_list1 =   {
        {MHz(108), MHz(174), ID52PLUS_ALL_RX_MODES, -1, -1, ID52PLUS_VFO_ALL},
        {MHz(375), MHz(550), ID52PLUS_ALL_RX_MODES, -1, -1, ID52PLUS_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {MHz(144), MHz(146), ID52PLUS_MODES, W(0.5), W(5), ID52PLUS_VFO_ALL},
        {MHz(430), MHz(440), ID52PLUS_MODES, W(0.5), W(5), ID52PLUS_VFO_ALL},
        RIG_FRNG_END,
    },

    /* USA version - rx_range_list2, tx_range_list2 */
    .rx_range_list2 =   {
        {MHz(108), MHz(174), ID52PLUS_ALL_RX_MODES, -1, -1, ID52PLUS_VFO_ALL},
        {MHz(375), MHz(550), ID52PLUS_ALL_RX_MODES, -1, -1, ID52PLUS_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {MHz(144), MHz(148), ID52PLUS_MODES, W(0.5), W(5), ID52PLUS_VFO_ALL},
        {MHz(430), MHz(450), ID52PLUS_MODES, W(0.5), W(5), ID52PLUS_VFO_ALL},
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
        {RIG_MODE_AMN, kHz(6)},
        {RIG_MODE_DSTAR, kHz(6)},
        RIG_FLT_END,
    },
    .str_cal = ID52PLUS_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& id52plus_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,
    .set_powerstat =  icom_set_powerstat,
//    .get_powerstat =  icom_get_powerstat, // not capable

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,

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
