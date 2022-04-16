/*
 *  Hamlib CI-V backend - description of IC-F8101
 *  Copyright (c) 2000-2010 by Stephane Fillod
 *  Copyright (c) 2021 by Michael Black W9MDB
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

static int icf8101_rig_open(RIG *rig)
{
    // turn on VFO mode
    icom_set_vfo(rig, RIG_VFO_A);
    return icom_rig_open(rig);
}

static int icf8101_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    int freq_len = 5;
    int ack_len;
    unsigned char freqbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    vfo_t vfo_save = rig->state.current_vfo;

    if (vfo != vfo_save)
    {
        rig_set_vfo(rig, vfo);
    }

    to_bcd(freqbuf, freq, freq_len * 2);
    retval = icom_transaction(rig, 0x1a, 0x35, freqbuf, freq_len, ackbuf,
                              &ack_len);

    if (vfo != vfo_save)
    {
        rig_set_vfo(rig, vfo_save);
    }

    return retval;
}

static int icf8101_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    unsigned char modebuf[2];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s, mode=%s, width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    switch (mode)
    {
    case RIG_MODE_LSB:  modebuf[0] = 0x00; modebuf[1] = 0x00; break;

    case RIG_MODE_USB:  modebuf[0] = 0x00; modebuf[1] = 0x01; break;

    case RIG_MODE_AM:   modebuf[0] = 0x00; modebuf[1] = 0x02; break;

    case RIG_MODE_CW:   modebuf[0] = 0x00; modebuf[1] = 0x03; break;

    case RIG_MODE_RTTY: modebuf[0] = 0x00; modebuf[1] = 0x04; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown mode of '%s\n", __func__,
                  rig_strrmode(mode));
        return (-RIG_EINVAL);
    }

    retval = icom_transaction(rig, 0x1A, 0x36, modebuf, 2, NULL, 0);
    return retval;
}

static int icf8101_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width)
{
    int retval = icom_get_mode(rig, vfo, mode, width);
    unsigned char modebuf[MAXFRAMELEN];
    int modebuf_len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));
    retval = icom_transaction(rig, 0x1A, 0x34, NULL, 0, modebuf, &modebuf_len);
    dump_hex(modebuf, modebuf_len);

    switch (modebuf[1])
    {
    case 0x00: *mode = RIG_MODE_LSB; break;

    case 0x01: *mode = RIG_MODE_USB; break;

    case 0x02: *mode = RIG_MODE_AM; break;

    case 0x03: *mode = RIG_MODE_CW; break;

    case 0x04: *mode = RIG_MODE_RTTY; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown mode response=0x%02x\n", __func__,
                  modebuf[1]);
    }

    return retval;
}

/*
 * This function does the special bandwidth coding for IC-F8101
 * (1 - normal, 2 - narrow)
 */
static int icf8101_r2i_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
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


#define ICF8101_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_RTTY)

#define ICF8101_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

#define ICF8101_SCAN_OPS (RIG_SCAN_MEM)

#define ICF8101_FUNC_ALL     (RIG_FUNC_NB| \
                            RIG_FUNC_NR| \
                            RIG_FUNC_ANF| \
                            RIG_FUNC_TONE| \
                            RIG_FUNC_COMP| \
                            RIG_FUNC_VOX| \
                            RIG_FUNC_FBKIN| \
                            RIG_FUNC_AFC)

#define ICF8101_LEVEL_ALL    (RIG_LEVEL_AF| \
                            RIG_LEVEL_RF| \
                            RIG_LEVEL_SQL| \
                            RIG_LEVEL_IF| \
                            RIG_LEVEL_NR| \
                            RIG_LEVEL_RFPOWER| \
                            RIG_LEVEL_MICGAIN| \
                            RIG_LEVEL_ATT| \
                            RIG_LEVEL_PREAMP)

#define ICF8101_STR_CAL UNKNOWN_IC_STR_CAL  /* FIXME */

static const struct icom_priv_caps icf8101_priv_caps =
{
    0x8A,           /* default address */
    0,              /* 731 mode */
    1,              /* no XCHG to avoid display flicker */
    NULL,
    .r2i_mode = icf8101_r2i_mode
};

int icf8101_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    switch (func)
    {
    default:
        return icom_set_func(rig, vfo, func, status);
    }
}

int icf8101_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    switch (func)
    {
    default:
        return icom_get_func(rig, vfo, func, status);
    }
}

int icf8101_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
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

int icf8101_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
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

int icf8101_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    return rig_set_freq(rig, RIG_VFO_B, tx_freq);
}

int icf8101_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    return rig_get_freq(rig, RIG_VFO_B, tx_freq);
}

int icf8101_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t tx_freq,
                                rmode_t mode, pbwidth_t width)
{
    rig_set_freq(rig, RIG_VFO_B, tx_freq);
    return rig_set_mode(rig, RIG_VFO_B, mode, -1);
}

int icf8101_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *tx_freq,
                                rmode_t *mode, pbwidth_t *width)
{
    rig_get_freq(rig, RIG_VFO_B, tx_freq);
    return rig_get_mode(rig, RIG_VFO_B, mode, width);
}



int icf8101_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    unsigned char cmdbuf[4];
    int ack_len;
    unsigned char ackbuf[MAXFRAMELEN];

    cmdbuf[0] = 0x03;
    cmdbuf[1] = 0x17;
    cmdbuf[2] = 0x00;
    cmdbuf[3] = split == RIG_SPLIT_ON;
    return icom_transaction(rig, 0x1a, 0x05, cmdbuf, sizeof(cmdbuf), ackbuf,
                            &ack_len);
}

int icf8101_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int retval;
    int ack_len;
    unsigned char ackbuf[MAXFRAMELEN];
    unsigned char cmdbuf[2];

    cmdbuf[0] = 0x03;
    cmdbuf[1] = 0x17;
    retval = icom_transaction(rig, 0x1a, 0x05, cmdbuf, sizeof(cmdbuf), ackbuf,
                              &ack_len);

    if (retval == RIG_OK && ack_len >= 1)
    {
        dump_hex(ackbuf, ack_len);
        *split = ackbuf[0] == 1;
        *tx_vfo = *split ? RIG_VFO_B : RIG_VFO_A;
    }

    return retval;
}

int icf8101_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char ackbuf[MAXFRAMELEN], pttbuf[2];
    int ack_len = sizeof(ackbuf), retval;

    ENTERFUNC;

    switch (ptt)
    {
    case RIG_PTT_ON:      pttbuf[0] = 0; pttbuf[1] = 1; break;

    case RIG_PTT_ON_MIC:  pttbuf[0] = 0; pttbuf[1] = 1; break;

    case RIG_PTT_ON_DATA: pttbuf[0] = 0; pttbuf[1] = 2; break;

    case RIG_PTT_OFF:     pttbuf[0] = 0; pttbuf[1] = 0; break;

    default: RETURNFUNC(-RIG_EINVAL);
    }

    retval = icom_transaction(rig, C_CTL_MEM, S_MEM_PTT, pttbuf, 2,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((ack_len >= 1 && ackbuf[0] != ACK) && (ack_len >= 2 && ackbuf[1] != NAK))
    {
        //  if we don't get ACK/NAK some serial corruption occurred
        // so we'll call it a timeout for retry purposes
        RETURNFUNC(-RIG_ETIMEOUT);
    }

    if (ack_len != 1 || (ack_len >= 1 && ackbuf[0] != ACK))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    RETURNFUNC(RIG_OK);


}

/*
 * icf8101_get_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icf8101_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    unsigned char pttbuf[MAXFRAMELEN];
    int ptt_len, retval;

    ENTERFUNC;
    retval = icom_transaction(rig, C_CTL_MEM, S_MEM_PTT, NULL, 0,
                              pttbuf, &ptt_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /*
     * pttbuf should contain Cn,Sc,Data area
     */
    ptt_len -= 2;

    if (ptt_len != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong frame len=%d\n",
                  __func__, ptt_len);
        RETURNFUNC(-RIG_ERJCTED);
    }

    if (pttbuf[3] == 0) { *ptt = RIG_PTT_OFF; }
    else if (pttbuf[3] == 1) { *ptt = RIG_PTT_ON_MIC; }
    else if (pttbuf[3] == 2) { *ptt = RIG_PTT_ON_DATA; }

    RETURNFUNC(RIG_OK);
}


const struct rig_caps icf8101_caps =
{
    RIG_MODEL(RIG_MODEL_ICF8101),
    .model_name =   "IC-F8101",
    .mfg_name =   "Icom",
    .version =    BACKEND_VER ".2",
    .copyright =    "LGPL",
    .status =   RIG_STATUS_STABLE,
    .rig_type =   RIG_TYPE_TRANSCEIVER,
    .ptt_type =   RIG_PTT_RIG_MICDATA,
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
    .has_get_func =   ICF8101_FUNC_ALL,
    .has_set_func =   ICF8101_FUNC_ALL | RIG_FUNC_RESUME,
    .has_get_level =  ICF8101_LEVEL_ALL | (RIG_LEVEL_RAWSTR),
    .has_set_level =  ICF8101_LEVEL_ALL,
    .has_get_parm =   RIG_PARM_NONE,
    .has_set_parm =   RIG_PARM_NONE,
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
    },
    .tuning_steps = {
        {ICF8101_MODES, 1},
    },
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_SLOW, RIG_AGC_AUTO },
    .parm_gran =    {},
    .ctcss_list =  NULL,

    .dcs_list =   NULL,
    .preamp =   { 20, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, }, // is it really 20dB? */
    .max_rit =    Hz(0),
    .max_xit =    Hz(0),
    .max_ifshift =    Hz(0),    /* there is RTTY shift */
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .scan_ops =   ICF8101_SCAN_OPS,
    .transceive =   RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =   0,

    .chan_list = {
        {   1,  500, RIG_MTYPE_MEM  },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(500), MHz(29.9999), ICF8101_MODES, -1, -1, ICF8101_VFO_ALL, RIG_ANT_CURR, "Other" },
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        { MHz(1.6), MHz(29.9999), ICF8101_MODES, W(1), W(100), ICF8101_VFO_ALL, RIG_ANT_CURR, "Other"},
        RIG_FRNG_END,
    },
    .rx_range_list2 =   {
        {kHz(500), MHz(29.9999), ICF8101_MODES, -1, -1, ICF8101_VFO_ALL, RIG_ANT_CURR, "AUS" },
        RIG_FRNG_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_RTTY, kHz(3.0)}, /* builtin */
        {RIG_MODE_AM, kHz(10)},     /* builtin */
        RIG_FLT_END,
    },
    .str_cal =    ICF8101_STR_CAL,

    .priv =     &icf8101_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =    icom_cleanup,

    .cfgparams =    icom_cfg_params,
    .set_conf =   icom_set_conf,
    .get_conf =   icom_get_conf,

    .get_freq =   icom_get_freq,
    .set_freq =   icf8101_set_freq,

    .get_mode =   icf8101_get_mode,
    .set_mode =   icf8101_set_mode,

    .set_ptt = icf8101_set_ptt,
    .get_ptt = icf8101_get_ptt,
    .set_vfo = icom_set_vfo,
    .get_vfo = icom_get_vfo,
    .get_ts =  icom_get_ts,
    .set_ts =  icom_set_ts,
    .get_func =  icf8101_get_func,
    .set_func =  icf8101_set_func,
    .get_level =  icf8101_get_level,
    .set_level =  icf8101_set_level,

    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .get_dcd =  icom_get_dcd,
    .decode_event =  icom_decode_event,
    .rig_open =  icf8101_rig_open,
    .rig_close =  icom_rig_close,

    .set_split_freq = icf8101_set_split_freq,
    .get_split_freq = icf8101_get_split_freq,
    .set_split_vfo = icf8101_set_split_vfo,
    .get_split_vfo = icf8101_get_split_vfo,
    .set_split_freq_mode = icf8101_set_split_freq_mode,
    .get_split_freq_mode = icf8101_get_split_freq_mode,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
