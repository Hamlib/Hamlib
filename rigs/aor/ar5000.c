/*
 *  Hamlib AOR backend - AR5000 description
 *
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

#include <stdio.h>
#include <string.h>
#include <hamlib/rig.h>
#include "aor.h"


#define AR5000_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM| \
              RIG_MODE_WFM|RIG_MODE_SAM|RIG_MODE_SAL|RIG_MODE_SAH)

#define AR5000_FUNC_ALL (RIG_FUNC_TSQL | RIG_FUNC_ABM)

#define AR5000_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AGC | RIG_LEVEL_RAWSTR)

#define AR5000_PARM (RIG_PARM_NONE)

#define AR5000_VFO_OPS (RIG_OP_MCL | RIG_OP_UP | RIG_OP_DOWN)
#define AR5000_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_PROG|RIG_SCAN_SLCT)

#define AR5000_VFO (RIG_VFO_A | RIG_VFO_B | RIG_VFO_C | RIG_VFO_N(3) | RIG_VFO_N(4))

/* As reported with rigctl 'l RAWSTR' for AR5000A S/n: 171218
   on 7040kHz / CW / 3kHz Bw.

   The data available on http://www.aoruk.com did not match very well on HF */
#define AR5000_STR_CAL { 16, { \
        {   0, -60 }, \
        {   3, -48 }, \
        {  14, -42 }, \
        {  26, -36 }, \
        {  34, -30 }, \
        {  42, -24 }, \
        {  52, -18 }, \
        {  62, -12 }, \
        {  74, -6 }, \
        {  87,  0 }, \
        { 101, 10 }, \
        { 117, 20 }, \
        { 135, 30 }, \
        { 152, 40 }, \
        { 202, 50 }, \
        { 255, 60 }, \
    } }


#define AR5000_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .bank_num = 1,  \
    .tuning_step = 1,   \
    .channel_desc = 1,  \
    .flags = 1, \
    .levels = RIG_LEVEL_ATT | RIG_LEVEL_AGC,    \
    .funcs = RIG_FUNC_ABM,    \
}


static int format5k_mode(RIG *rig, char *buf, int buf_len, rmode_t mode,
                         pbwidth_t width);
static int parse5k_aor_mode(RIG *rig, char aormode, char aorwidth,
                            rmode_t *mode, pbwidth_t *width);

static const struct aor_priv_caps ar5k_priv_caps =
{
    .format_mode = format5k_mode,
    .parse_aor_mode = parse5k_aor_mode,
    .bank_base1 = '0',
    .bank_base2 = '0',
};

/*
 * ar5000 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/5000.htm
 *
 * TODO: retrieve BW info, and rest of commands
 */
const struct rig_caps ar5000_caps =
{
    RIG_MODEL(RIG_MODEL_AR5000),
    .model_name = "AR5000",
    .mfg_name =  "AOR",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
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
    .has_set_func =  AR5000_FUNC_ALL,
    .has_get_level =  AR5000_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(AR5000_LEVEL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,                /* FIXME: CTCSS list */
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 10, 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   10,
    .chan_desc_sz =  8,
    .vfo_ops =  AR5000_VFO_OPS,
    .scan_ops =  AR5000_SCAN_OPS,
    .str_cal = AR5000_STR_CAL,

    .chan_list =  {
        {   0,  999, RIG_MTYPE_MEM, AR5000_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(10), MHz(2600), AR5000_MODES, -1, -1, AR5000_VFO},
        RIG_FRNG_END,
    },

    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {kHz(10), MHz(2600), AR5000_MODES, -1, -1, AR5000_VFO},
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list2 =  { RIG_FRNG_END, },   /* no tx range, this is a scanner! */

    .tuning_steps =  {
        {AR5000_MODES, 1},
        {AR5000_MODES, 10},
        {AR5000_MODES, 50},
        {AR5000_MODES, 100},
        {AR5000_MODES, 500},
        {AR5000_MODES, kHz(1)},
        {AR5000_MODES, kHz(5)},
        {AR5000_MODES, kHz(6.25)},
        {AR5000_MODES, kHz(9)},
        {AR5000_MODES, kHz(10)},
        {AR5000_MODES, kHz(12.5)},
        {AR5000_MODES, kHz(20)},
        {AR5000_MODES, kHz(25)},
        {AR5000_MODES, kHz(30)},
        {AR5000_MODES, kHz(50)},
        {AR5000_MODES, kHz(100)},
        {AR5000_MODES, kHz(500)},
#if 0
        {AR5000_MODES, 0},  /* any tuning step */
#endif
        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_SAL | RIG_MODE_SAH | RIG_MODE_CW, kHz(3)},
        {RIG_MODE_CW, Hz(500)},   /* narrow */
        {RIG_MODE_AM | RIG_MODE_SAM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_SAM, kHz(3)},  /* narrow */
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SAM, kHz(15)},
        {RIG_MODE_FM, kHz(6)},    /* narrow */
        {RIG_MODE_FM, kHz(30)},   /*  wide  */
        {RIG_MODE_WFM, kHz(110)},
        {RIG_MODE_WFM, kHz(30)},  /* narrow */
        {RIG_MODE_WFM, kHz(220)}, /*  wide  */
        RIG_FLT_END,
    },

    .priv = (void *)& ar5k_priv_caps,

    .rig_init =  NULL,
    .rig_cleanup =  NULL,
    .rig_open =  NULL,
    .rig_close =  aor_close,

    .set_freq =  aor_set_freq,
    .get_freq =  aor_get_freq,
    .set_mode =  aor_set_mode,
    .get_mode =  aor_get_mode,
    .set_vfo =  aor_set_vfo,
    .get_vfo =  aor_get_vfo,

    .set_level =  aor_set_level,
    .get_level =  aor_get_level,
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
 * ar5000a rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/5000.htm
 *
 * TODO: retrieve BW info, and rest of commands
 */
const struct rig_caps ar5000a_caps =
{
    RIG_MODEL(RIG_MODEL_AR5000A),
    .model_name = "AR5000A",
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
    .has_set_func =  AR5000_FUNC_ALL,
    .has_get_level =  AR5000_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(AR5000_LEVEL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,                /* FIXME: CTCSS list */
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 10, 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   10,
    .chan_desc_sz =  8,
    .vfo_ops =  AR5000_VFO_OPS,
    .scan_ops =  AR5000_SCAN_OPS,
    .str_cal = AR5000_STR_CAL,

    .chan_list =  {
        {   0,  999, RIG_MTYPE_MEM, AR5000_MEM_CAP },
        RIG_CHAN_END,
    },


    .rx_range_list1 =  {
        {kHz(10), MHz(3000), AR5000_MODES, -1, -1, AR5000_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {kHz(10), MHz(3000), AR5000_MODES, -1, -1, AR5000_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no tx range, this is a scanner! */

    .tuning_steps =  {
        {AR5000_MODES, 1},
        {AR5000_MODES, 10},
        {AR5000_MODES, 50},
        {AR5000_MODES, 100},
        {AR5000_MODES, 500},
        {AR5000_MODES, kHz(1)},
        {AR5000_MODES, kHz(5)},
        {AR5000_MODES, kHz(6.25)},
        {AR5000_MODES, kHz(9)},
        {AR5000_MODES, kHz(10)},
        {AR5000_MODES, kHz(12.5)},
        {AR5000_MODES, kHz(20)},
        {AR5000_MODES, kHz(25)},
        {AR5000_MODES, kHz(30)},
        {AR5000_MODES, kHz(50)},
        {AR5000_MODES, kHz(100)},
        {AR5000_MODES, kHz(500)},
#if 0
        {AR5000_MODES, 0},  /* any tuning step */
#endif
        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_SAL | RIG_MODE_SAH | RIG_MODE_CW, kHz(3)},
        {RIG_MODE_CW, Hz(500)},   /* narrow */
        {RIG_MODE_AM | RIG_MODE_SAM, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_SAM, kHz(3)},  /* narrow */
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_SAM, kHz(15)},
        {RIG_MODE_FM, kHz(6)},    /* narrow */
        {RIG_MODE_FM, kHz(30)},   /*  wide  */
        {RIG_MODE_WFM, kHz(110)},
        {RIG_MODE_WFM, kHz(30)},  /* narrow */
        {RIG_MODE_WFM, kHz(220)}, /*  wide  */
        RIG_FLT_END,
    },

    .priv = (void *)& ar5k_priv_caps,

    .rig_init =  NULL,
    .rig_cleanup =  NULL,
    .rig_open =  NULL,
    .rig_close =  aor_close,

    .set_freq =  aor_set_freq,
    .get_freq =  aor_get_freq,
    .set_mode =  aor_set_mode,
    .get_mode =  aor_get_mode,
    .set_vfo =  aor_set_vfo,
    .get_vfo =  aor_get_vfo,

    .set_level =  aor_set_level,
    .get_level =  aor_get_level,
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

};

/*
 * Function definitions below
 */


/*
 * modes in use by the "MD" command of AR5000
 */
#define AR5K_FM  '0'
#define AR5K_AM  '1'
#define AR5K_LSB '2'
#define AR5K_USB '3'
#define AR5K_CW  '4'
#define AR5K_SAM '5'
#define AR5K_SAL '6'
#define AR5K_SAH '7'

int format5k_mode(RIG *rig, char *buf, int buf_len, rmode_t mode,
                  pbwidth_t width)
{
    int aormode;

    switch (mode)
    {
    case RIG_MODE_AM:  aormode = AR5K_AM; break;

    case RIG_MODE_WFM:
    case RIG_MODE_FM:  aormode = AR5K_FM; break;

    case RIG_MODE_LSB: aormode = AR5K_LSB; break;

    case RIG_MODE_USB: aormode = AR5K_USB; break;

    case RIG_MODE_CW:  aormode = AR5K_CW; break;

    case RIG_MODE_SAM: aormode = AR5K_SAM; break;

    case RIG_MODE_SAL: aormode = AR5K_SAL; break;

    case RIG_MODE_SAH: aormode = AR5K_SAH; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        int aorwidth;

        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        switch (width)
        {
        case 500:    aorwidth = '0'; break;

        case s_kHz(3):   aorwidth = '1'; break;

        case s_kHz(6):   aorwidth = '2'; break;

        case s_kHz(15):  aorwidth = '3'; break;

        case s_kHz(30):  aorwidth = '4'; break;

        case s_kHz(110): aorwidth = '5'; break;

        case s_kHz(220): aorwidth = '6'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported width %d\n",
                      __func__, (int)width);
            return -RIG_EINVAL;
        }

        SNPRINTF(buf, buf_len, "MD%c BW%c", aormode, aorwidth);
        return strlen(buf);
    }
    else
    {
        SNPRINTF(buf,  buf_len, "MD%c", aormode);
        return strlen(buf);
    }
}



int parse5k_aor_mode(RIG *rig, char aormode, char aorwidth, rmode_t *mode,
                     pbwidth_t *width)
{
    switch (aormode)
    {
    case AR5K_FM:   *mode = RIG_MODE_FM; break;

    case AR5K_AM:   *mode = RIG_MODE_AM; break;

    case AR5K_LSB:  *mode = RIG_MODE_LSB; break;

    case AR5K_USB:  *mode = RIG_MODE_USB; break;

    case AR5K_CW:   *mode = RIG_MODE_CW; break;

    case AR5K_SAM:  *mode = RIG_MODE_SAM; break;

    case AR5K_SAL:  *mode = RIG_MODE_SAL; break;

    case AR5K_SAH:  *mode = RIG_MODE_SAH; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, aormode);
        return -RIG_EPROTO;
    }

    switch (aorwidth)
    {
    case '0':   *width = 500; break;

    case '1':   *width = s_kHz(3); break;

    case '2':   *width = s_kHz(6); break;

    case '3':   *width = s_kHz(15); break;

    case '4':   *width = s_kHz(30); break;

    case '5':   *width = s_kHz(110); break;

    case '6':   *width = s_kHz(220); break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported width %d\n",
                  __func__, aorwidth);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

