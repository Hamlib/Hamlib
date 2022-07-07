/*
 *  Hamlib AOR backend - AR8200 description
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

#include <hamlib/rig.h>
#include "aor.h"


#define AR8200_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_WFM)

#define AR8200_FUNC (RIG_FUNC_TSQL|RIG_FUNC_ABM|RIG_FUNC_AFC)

#define AR8200_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR)

#define AR8200_PARM (RIG_PARM_APO|RIG_PARM_BACKLIGHT|RIG_PARM_BEEP)

#define AR8200_VFO_OPS (RIG_OP_MCL|RIG_OP_UP|RIG_OP_DOWN|RIG_OP_LEFT|RIG_OP_RIGHT)
#define AR8200_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_PROG|RIG_SCAN_SLCT)

#define AR8200_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

/* TODO: measure and report real values */
/* series-2 (black cabinet), from AR8200 Bulletin page */
#define AR8200_STR_CAL { 12, \
    { \
        {    0,-54 }, \
        {   27,-32 }, \
        {   42,-27 }, \
        {   55,-22 }, \
        {   68,-17 }, \
        {   86, -7 }, \
        {   97,  3 }, \
        {  103, 13 }, \
        {  106, 23 }, \
        {  109, 33 }, \
        {  112, 43 }, \
        {  139, 53 }, \
    } }

#define AR8200_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .bank_num = 1,  \
    .tuning_step = 1,   \
    .channel_desc = 1,  \
    .flags = 1, \
    .levels = RIG_LEVEL_ATT,    \
    .funcs = RIG_FUNC_ABM,  \
}

static const struct aor_priv_caps ar8k_priv_caps =
{
    .format_mode = format8k_mode,
    .parse_aor_mode = parse8k_aor_mode,
    .bank_base1 = 'A',
    .bank_base2 = 'a',
};


/*
 * ar8200 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/8200.htm
 */
const struct rig_caps ar8200_caps =
{
    RIG_MODEL(RIG_MODEL_AR8200),
    .model_name = "AR8200",
    .mfg_name =  "AOR",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_SCANNER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_XONXOFF,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  AR8200_FUNC,
    .has_get_level =  AR8200_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(AR8200_LEVEL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,                /* FIXME: CTCSS list */
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, }, /* TBC */
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   20,   /* A through J, and a through j */
    .chan_desc_sz =  12,
    .vfo_ops =  AR8200_VFO_OPS,
    .scan_ops =  AR8200_SCAN_OPS,
    .str_cal = AR8200_STR_CAL,

    .chan_list =  {
        {   0,  999, RIG_MTYPE_MEM, AR8200_MEM_CAP },   /* flat space */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(2040), AR8200_MODES, -1, -1, AR8200_VFO_ALL},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(2040), AR8200_MODES, -1, -1, AR8200_VFO_ALL},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no tx range, this is a scanner! */

    .tuning_steps =  {
        {AR8200_MODES, 50},
        {AR8200_MODES, 100},
        {AR8200_MODES, kHz(1)},
        {AR8200_MODES, kHz(5)},
        {AR8200_MODES, kHz(9)},
        {AR8200_MODES, kHz(10)},
        {AR8200_MODES, 12500},
        {AR8200_MODES, kHz(20)},
        {AR8200_MODES, kHz(25)},
        {AR8200_MODES, kHz(100)},
        {AR8200_MODES, MHz(1)},
#if 0
        {AR8200_MODES, 0}, /* any tuning step */
#endif
        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        /* mode/filter list, .remember =  order matters! */
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(3)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_AM, kHz(3)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(12)},
        {RIG_MODE_FM, kHz(9)},
        {RIG_MODE_WFM, kHz(230)}, /* 50kHz at -3dB, 380kHz at -20dB */
        RIG_FLT_END,
    },

    .priv = (void *)& ar8k_priv_caps,

    .rig_init =  NULL,
    .rig_cleanup =  NULL,
    .rig_open =  NULL,
    .rig_close =  aor_close,

    .set_freq =  aor_set_freq,
    .get_freq =  aor_get_freq,
    .set_vfo =  aor_set_vfo,
    .get_vfo =  aor_get_vfo,
    .set_mode =  aor_set_mode,
    .get_mode =  aor_get_mode,

    .set_level = aor_set_level,
    .get_level = aor_get_level,
    .get_dcd = aor_get_dcd,

    .set_ts =  aor_set_ts,
    .set_powerstat =  aor_set_powerstat,
    .vfo_op =  aor_vfo_op,
    .scan =  aor_scan,
    .get_info =  aor_get_info,

    .set_mem = aor_set_mem,
    .get_mem = aor_get_mem,
    .set_bank = aor_set_bank,

    .set_channel = aor_set_channel,
    .get_channel = aor_get_channel,

    .get_chan_all_cb = aor_get_chan_all_cb,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */


