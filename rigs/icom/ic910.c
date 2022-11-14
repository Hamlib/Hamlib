/*
 *  Hamlib CI-V backend - description of IC-910 (VHF/UHF All-Mode Transceiver)
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
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
#include "misc.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "idx_builtin.h"

/*
 * It seems some IC910 out there have weird firmware. Uncomment the following
 * if your modes are wrong, and please report to hamlib-developer maillist
 * with firmware number. That'd be interesting to have a word from Icom
 * on this subject, and if firmware updates are possible.
 */
#ifdef HAVE_WEIRD_IC910_MODES
static int ic910_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    /* FIX: The IC-910 has "Set FM" = 4, which is RTTY in for other radios */
    if (mode == RIG_MODE_FM)
    {
        mode = RIG_MODE_RTTY;
    }

    return icom_set_mode(rig, vfo, mode, width);
}

static int ic910_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    /* FIX: The IC-910 has "Set FM" = 4, which is RTTY in for other radios */
    int retval = icom_get_mode(rig, vfo, mode, width);

    if (*mode == RIG_MODE_RTTY)
    {
        *mode = RIG_MODE_FM;
    }

    return retval;
}
#endif /* HAVE_WEIRD_IC910_MODES */

/*
 * This function does the special bandwidth coding for IC-910
 * (1 - normal, 2 - narrow)
 */
static int ic910_r2i_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
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


#define IC910_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_FM)

#define IC910_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_FM)

#define IC910_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MEM|RIG_VFO_MAIN_A|RIG_VFO_MAIN_B|RIG_VFO_SUB_A|RIG_VFO_SUB_B)

#define IC910_SCAN_OPS (RIG_SCAN_MEM)

#define IC910_VFO_OPS      (RIG_OP_FROM_VFO| \
                            RIG_OP_TO_VFO| \
                            RIG_OP_CPY| \
                            RIG_OP_MCL| \
                            RIG_OP_XCHG)

#define IC910_FUNC_ALL     (RIG_FUNC_NB| \
                            RIG_FUNC_NR| \
                            RIG_FUNC_ANF| \
                            RIG_FUNC_TONE| \
                            RIG_FUNC_TSQL| \
                            RIG_FUNC_COMP| \
                            RIG_FUNC_VOX| \
                            RIG_FUNC_FBKIN| \
                            RIG_FUNC_AFC| \
                            RIG_FUNC_SATMODE| \
                            RIG_FUNC_SCOPE)

#define IC910_LEVEL_ALL    (RIG_LEVEL_AF| \
                            RIG_LEVEL_RF| \
                            RIG_LEVEL_SQL| \
                            RIG_LEVEL_IF| \
                            RIG_LEVEL_NR| \
                            RIG_LEVEL_CWPITCH| \
                            RIG_LEVEL_RFPOWER| \
                            RIG_LEVEL_MICGAIN| \
                            RIG_LEVEL_KEYSPD| \
                            RIG_LEVEL_COMP| \
                            RIG_LEVEL_VOXGAIN| \
                            RIG_LEVEL_VOXDELAY| \
                            RIG_LEVEL_ANTIVOX| \
                            RIG_LEVEL_ATT| \
                            RIG_LEVEL_PREAMP)

#define IC910_STR_CAL UNKNOWN_IC_STR_CAL  /* FIXME */

static const struct icom_priv_caps ic910_priv_caps =
{
    0x60,           /* default address */
    0,              /* 731 mode */
    1,              /* no XCHG to avoid display flicker */
    ic910_ts_sc_list,
    .r2i_mode = ic910_r2i_mode
};

int ic910_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    switch (func)
    {
    case RIG_FUNC_SCOPE:
        return icom_set_raw(rig, C_CTL_MEM, S_MEM_BANDSCOPE, 0, NULL, 1,
                            status ? 1 : 0);

    case RIG_FUNC_SATMODE:
        return icom_set_raw(rig, C_CTL_MEM, S_MEM_SATMODE910, 0, NULL, 1,
                            status ? 1 : 0);

    default:
        return icom_set_func(rig, vfo, func, status);
    }
}

int ic910_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    switch (func)
    {
    case RIG_FUNC_SCOPE:
        return icom_get_raw(rig, C_CTL_MEM, S_MEM_BANDSCOPE, 0, NULL, status);

    case RIG_FUNC_SATMODE:
        return icom_get_raw(rig, C_CTL_MEM, S_MEM_SATMODE910, 0, NULL, status);

    default:
        return icom_get_func(rig, vfo, func, status);
    }
}

int ic910_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_VOXDELAY:
        return icom_set_level_raw(rig, level, C_CTL_MEM, S_MEM_VOXDELAY, 0, NULL, 1,
                                  val);

    default:
        return icom_set_level(rig, vfo, level, val);
    }
}

int ic910_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_VOXDELAY:
        return icom_get_level_raw(rig, level, C_CTL_MEM, S_MEM_VOXDELAY, 0, NULL, val);

    default:
        return icom_get_level(rig, vfo, level, val);
    }
}

const struct rig_caps ic910_caps =
{
    RIG_MODEL(RIG_MODEL_IC910),
    .model_name =   "IC-910",
    .mfg_name =   "Icom",
    .version =    BACKEND_VER ".1",
    .copyright =    "LGPL",
    .status =   RIG_STATUS_STABLE,
    .rig_type =   RIG_TYPE_TRANSCEIVER,
    .ptt_type =   RIG_PTT_RIG,
    .dcd_type =   RIG_DCD_RIG,
    .port_type =    RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  19200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =    0,
    .post_write_delay = 0,
    .timeout =    1000,
    .retry =    3,
    .has_get_func =   IC910_FUNC_ALL,
    .has_set_func =   IC910_FUNC_ALL | RIG_FUNC_RESUME,
    .has_get_level =  IC910_LEVEL_ALL | (RIG_LEVEL_RAWSTR),
    .has_set_level =  IC910_LEVEL_ALL,
    .has_get_parm =   RIG_PARM_NONE,
    .has_set_parm =   RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
    },
    .parm_gran =    {},
    .ctcss_list =  common_ctcss_list,

    .dcs_list =   NULL,
    .preamp =   { 20, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =    Hz(0),    /* SSB,CW: +-1.0kHz  FM: +-5.0kHz */
    .max_xit =    Hz(0),
    .max_ifshift =    Hz(0),    /* 1.2kHz manual knob */
//    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .vfo_ops =    IC910_VFO_OPS,
    .scan_ops =   IC910_SCAN_OPS,
    .transceive =   RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =   0,

    .chan_list = {
        {   1,  99, RIG_MTYPE_MEM  },
        { 100, 105, RIG_MTYPE_EDGE },
        { 106, 106, RIG_MTYPE_CALL },
        RIG_CHAN_END,
    },

    .rx_range_list1 = { /* USA */
        {MHz(144), MHz(148), IC910_MODES, 0, 0, IC910_VFO_ALL},
        {MHz(430), MHz(450), IC910_MODES, 0, 0, IC910_VFO_ALL},
        {MHz(1240), MHz(1300), IC910_MODES, 0, 0, IC910_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        {MHz(144), MHz(148), IC910_MODES, W(5), W(100), IC910_VFO_ALL},
        {MHz(430), MHz(450), IC910_MODES, W(5), W(75), IC910_VFO_ALL},
        {MHz(1240), MHz(1300), IC910_MODES, 0, 0, IC910_VFO_ALL},
        RIG_FRNG_END,
    },

    .rx_range_list2 = { /* Europe */
        {MHz(144), MHz(146), IC910_MODES, 0, 0, IC910_VFO_ALL},
        {MHz(430), MHz(440), IC910_MODES, 0, 0, IC910_VFO_ALL},
        {MHz(1240), MHz(1300), IC910_MODES, 0, 0, IC910_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 = {
        {MHz(144), MHz(146), IC910_MODES, W(5), W(100), IC910_VFO_ALL},
        {MHz(430), MHz(440), IC910_MODES, W(5), W(75), IC910_VFO_ALL},
        {MHz(1240), MHz(1300), IC910_MODES, 0, 0, IC910_VFO_ALL},
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {RIG_MODE_SSB | RIG_MODE_CW, 1},
        {RIG_MODE_SSB | RIG_MODE_CW, 10},
        {RIG_MODE_SSB | RIG_MODE_CW, 50},
        {RIG_MODE_SSB | RIG_MODE_CW, 100},
        {RIG_MODE_FM, kHz(0.1)},
        {RIG_MODE_FM, kHz(5)},
        {RIG_MODE_FM, kHz(6.25)},
        {RIG_MODE_FM, kHz(10)},
        {RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_FM, kHz(20)},
        {RIG_MODE_FM, kHz(25)},
        {RIG_MODE_FM, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_CW | RIG_MODE_SSB, kHz(2.3)}, /* builtin */
        {RIG_MODE_CW, Hz(600)},     /* with optional FL-132/Fl133 CW filters */
        {RIG_MODE_FM, kHz(15)},     /* builtin */
        {RIG_MODE_FM, kHz(6)},      /* builtin */
        RIG_FLT_END,
    },
    .str_cal =    IC910_STR_CAL,

    .priv =     &ic910_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =    icom_cleanup,

    .cfgparams =    icom_cfg_params,
    .set_conf =   icom_set_conf,
    .get_conf =   icom_get_conf,

    .get_freq =   icom_get_freq,
    .set_freq =   icom_set_freq,

#ifdef HAVE_WEIRD_IC910_MODES
    .get_mode =   ic910_get_mode,
    .set_mode =   ic910_set_mode,
#else
    .get_mode =   icom_get_mode,
    .set_mode =   icom_set_mode,
#endif

    .set_ptt = icom_set_ptt,
    .get_ptt = icom_get_ptt,
    .set_vfo = icom_set_vfo,
    .get_vfo = icom_get_vfo,
    .get_ts =  icom_get_ts,
    .set_ts =  icom_set_ts,
    .get_func =  ic910_get_func,
    .set_func =  ic910_set_func,
    .get_level =  ic910_get_level,
    .set_level =  ic910_set_level,

    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .get_dcd =  icom_get_dcd,
    .decode_event =  icom_decode_event,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,
    .set_split_vfo = icom_set_split_vfo,
    .set_split_freq = icom_set_split_freq,
    .get_split_freq = icom_get_split_freq,
    .set_split_mode = icom_set_split_mode,
    .get_split_mode = icom_get_split_mode,
    .set_split_freq_mode =  icom_set_split_freq_mode,
    .get_split_freq_mode =  icom_get_split_freq_mode,
    .set_ctcss_tone =  icom_set_ctcss_tone,
    .get_ctcss_tone =  icom_get_ctcss_tone,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_rptr_shift =  icom_set_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
