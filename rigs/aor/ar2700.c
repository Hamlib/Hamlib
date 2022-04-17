/*
 *  Hamlib AOR backend - AR2700 description
 *  Copyright (c) 2000-2008 by Stephane Fillod
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


#define AR2700_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define AR2700_FUNC (RIG_FUNC_ABM)

#define AR2700_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR)

#define AR2700_PARM (RIG_PARM_APO)

#define AR2700_VFO_OPS (RIG_OP_MCL|RIG_OP_UP|RIG_OP_DOWN)
#define AR2700_SCAN_OPS (RIG_SCAN_MEM)

#define AR2700_VFO_ALL (RIG_VFO_A|RIG_VFO_MEM)

/* TODO: measure and report real values */
#define AR2700_STR_CAL { 2, \
    { \
        {  0x00, -60 }, \
        {  0x3f, 60 } \
    } }

#define AR2700_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .bank_num = 1,  \
    .tuning_step = 1,   \
    .flags = 1, \
    .levels = RIG_LEVEL_ATT,    \
    .funcs = RIG_FUNC_ABM,  \
}

static int format2700_mode(RIG *rig, char *buf, int buf_len, rmode_t mode,
                           pbwidth_t width);
static int parse2700_aor_mode(RIG *rig, char aormode, char aorwidth,
                              rmode_t *mode, pbwidth_t *width);


static const struct aor_priv_caps ar2700_priv_caps =
{
    .format_mode = format2700_mode,
    .parse_aor_mode = parse2700_aor_mode,
    .bank_base1 = '0',
    .bank_base2 = '0',
};


/*
 * ar2700 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/2700.htm
 * Interface unit: CU-8232 (or equivalent)
 */
const struct rig_caps ar2700_caps =
{
    RIG_MODEL(RIG_MODEL_AR2700),
    .model_name = "AR2700",
    .mfg_name =  "AOR",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_SCANNER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  2400,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_XONXOFF,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  400,
    .retry =  3,
    .has_get_func =  AR2700_FUNC,
    .has_set_func =  AR2700_FUNC,
    .has_get_level =  AR2700_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(AR2700_LEVEL),
    .has_get_parm =  AR2700_PARM,
    .has_set_parm =  AR2700_PARM,    /* FIXME: parms */
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
    .bank_qty =   10,
    .chan_desc_sz =  0,
    .vfo_ops =  AR2700_VFO_OPS,
    .scan_ops =  AR2700_SCAN_OPS,
    .str_cal = AR2700_STR_CAL,

    .chan_list =  {
        {   0,  499, RIG_MTYPE_MEM, AR2700_MEM_CAP },   /* flat space */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(500), MHz(1300), AR2700_MODES, -1, -1, AR2700_VFO_ALL},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(500), MHz(1300), AR2700_MODES, -1, -1, AR2700_VFO_ALL},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no tx range, this is a scanner! */

    .tuning_steps =  {
        {RIG_MODE_AM | RIG_MODE_FM, kHz(5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(6.25)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(9)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(10)},
        {RIG_MODE_AM | RIG_MODE_FM, 12500},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(20)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(25)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(30)},
        {AR2700_MODES, kHz(50)},
        {AR2700_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        /* mode/filter list, .remember =  order matters! */
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_WFM, kHz(230)},
        RIG_FLT_END,
    },

    .priv = (void *)& ar2700_priv_caps,

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


/*
 * modes in use by the "MD" command of AR2700
 */
#define AR2700_WFM  '0'
#define AR2700_NFM  '1'
#define AR2700_AM   '2'

int format2700_mode(RIG *rig, char *buf, int buf_len, rmode_t mode,
                    pbwidth_t width)
{
    int aormode;

    switch (mode)
    {
    case RIG_MODE_AM:       aormode = AR2700_AM; break;

    case RIG_MODE_WFM:      aormode = AR2700_WFM; break;

    case RIG_MODE_FM:       aormode = AR2700_NFM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, buf_len, "MD%c", aormode);
    return strlen(buf);
}



int parse2700_aor_mode(RIG *rig, char aormode, char aorwidth, rmode_t *mode,
                       pbwidth_t *width)
{
    switch (aormode)
    {
    case AR2700_NFM:    *mode = RIG_MODE_FM; break;

    case AR2700_WFM:    *mode = RIG_MODE_WFM; break;

    case AR2700_AM: *mode = RIG_MODE_AM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, aormode);
        return -RIG_EPROTO;
    }

    *width = rig_passband_normal(rig, *mode);

    return RIG_OK;
}

