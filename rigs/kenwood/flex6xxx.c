/*
 *  Hamlib FlexRadio backend - 6K series rigs
 *  Copyright (c) 2002-2009 by Stephane Fillod
 *  Copyright (C) 2010,2011,2012,2013 by Nate Bargmann, n0nb@arrl.net
 *  Copyright (C) 2013 by Steve Conklin AI4QR, steve@conklinhouse.com
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <hamlib/rig.h>
#include "kenwood.h"
#include "bandplan.h"
#include "flex.h"
#include "token.h"
#include "cal.h"

#define F6K_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)

#define F6K_FUNC_ALL (RIG_FUNC_VOX)

#define F6K_LEVEL_ALL (RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW|RIG_LEVEL_KEYSPD)

#define F6K_VFO (RIG_VFO_A|RIG_VFO_B)
#define F6K_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define F6K_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)


static rmode_t flex_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_NONE,
    [1] = RIG_MODE_LSB,
    [2] = RIG_MODE_USB,
    [3] = RIG_MODE_CW,
    [4] = RIG_MODE_FM,
    [5] = RIG_MODE_AM,
    [6] = RIG_MODE_PKTLSB,
    [7] = RIG_MODE_NONE,
    [8] = RIG_MODE_NONE,
    [9] = RIG_MODE_PKTUSB
};

static struct kenwood_priv_caps f6k_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .mode_table = flex_mode_table,
};

#define DSP_BW_NUM 8

static int dsp_bw_ssb[DSP_BW_NUM] =
{
    4000, 3300, 2900, 2700, 2400, 2100, 1800, 1600
};

static int dsp_bw_am[DSP_BW_NUM] =
{
    20000, 16000, 14000, 12000, 10000, 8000, 6000, 5600
};

static int dsp_bw_cw[DSP_BW_NUM] =
{
    3000, 1500, 1000, 800, 400, 250, 100, 50
};

static int dsp_bw_dig[DSP_BW_NUM] =
{
    3000, 2000, 1500, 1000, 600, 300, 150, 100
};

/* Private helper functions */

static int flex6k_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char modebuf[10];
    int index;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!mode || !width)
    {
        return -RIG_EINVAL;
    }

    retval = kenwood_safe_transaction(rig, "MD", modebuf, 6, 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *mode = kenwood2rmode(modebuf[2] - '0', caps->mode_table);

    if ((vfo == RIG_VFO_VFO) || (vfo == RIG_VFO_CURR))
    {
        vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting VFO to current\n", __func__);
    }

    /*
     * The Flex CAT interface does not support FW for reading filter width,
     * so use the ZZFI or ZZFJ command
     */
    switch (vfo)
    {
    case RIG_VFO_A:
        retval = kenwood_safe_transaction(rig, "ZZFI", modebuf, 10, 6);
        break;

    case RIG_VFO_B:
        retval = kenwood_safe_transaction(rig, "ZZFJ", modebuf, 10, 6);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    index = atoi(&modebuf[4]);

    if (index >= DSP_BW_NUM)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "flex6k_get_mode: unexpected ZZF[IJ] answer, index=%d\n", index);
        return -RIG_ERJCTED;
    }

    switch (*mode)
    {
    case RIG_MODE_AM:
        *width = dsp_bw_am[index];
        break;

    case RIG_MODE_CW:
        *width = dsp_bw_cw[index];
        break;

    case RIG_MODE_USB:
    case RIG_MODE_LSB:
        *width = dsp_bw_ssb[index];
        break;

    //case RIG_MODE_FM:
    //*width = 3000; /* not supported yet, needs followup */
    //break;
    case RIG_MODE_PKTLSB:
    case RIG_MODE_PKTUSB:
        *width = dsp_bw_dig[index];
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s, setting default BW\n",
                  __func__, rig_strrmode(*mode));
        *width = 3000;
        break;
    }

    return RIG_OK;
}

static int flex6k_find_width(rmode_t mode, pbwidth_t width, int *ridx)
{
    int *w_a; // Width array, these are all ordered in descending order!
    int idx = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (mode)
    {
    case RIG_MODE_AM:
        w_a = dsp_bw_am;
        break;

    case RIG_MODE_CW:
        w_a = dsp_bw_cw;
        break;

    case RIG_MODE_USB:
    case RIG_MODE_LSB:
        w_a = dsp_bw_ssb;
        break;

    //case RIG_MODE_FM:
    //*width = 3000; /* not supported yet, needs followup */
    //break;
    case RIG_MODE_PKTLSB:
    case RIG_MODE_PKTUSB:
        w_a = dsp_bw_dig;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    // return the first smaller or equal possibility
    while ((idx < DSP_BW_NUM) && (w_a[idx] > width))
    {
        idx++;
    }

    if (idx >= DSP_BW_NUM)
    {
        idx = DSP_BW_NUM - 1;
    }

    *ridx = idx;
    return RIG_OK;
}

static int flex6k_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char buf[10];
    char kmode;
    int idx;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    kmode = rmode2kenwood(mode, caps->mode_table);

    if (kmode < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    sprintf(buf, "MD%c", '0' + kmode);
    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    if ((vfo == RIG_VFO_VFO) || (vfo == RIG_VFO_CURR))
    {
        vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting VFO to current\n", __func__);
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return err; }

    err = flex6k_find_width(mode, width, &idx);

    if (err != RIG_OK)
    {
        return err;
    }

    /*
     * The Flex CAT interface does not support FW for reading filter width,
     * so use the ZZFI or ZZFJ command
     */
    switch (vfo)
    {
    case RIG_VFO_A:
        sprintf(buf, "ZZFI%02d;", idx);
        break;

    case RIG_VFO_B:
        sprintf(buf, "ZZFJ%02d;", idx);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}
/*
 * flex6k_get_ptt
 */
int flex6k_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    const char *ptt_cmd;
    int err;
    char response[16];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!ptt)
    {
        return -RIG_EINVAL;
    }

    ptt_cmd = "ZZTX";

    err =  kenwood_transaction(rig, ptt_cmd, response, sizeof(response));

    if (err != RIG_OK)
    {
        return err;
    }

    *ptt = response[4] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

    return RIG_OK;
}

int flex6k_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    const char *ptt_cmd;
    char response[16];
    int err;
    int retry = 3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON_DATA:
    case RIG_PTT_ON_MIC:
    case RIG_PTT_ON:  ptt_cmd = "ZZTX1;ZZTX"; break;

    case RIG_PTT_OFF: ptt_cmd = "ZZTX0;ZZTX"; break;

    default: return -RIG_EINVAL;
    }

    do
    {
        err =  kenwood_transaction(rig, ptt_cmd, response, sizeof(response));

        if (ptt_cmd[4] != response[4])
        {
            rig_debug(RIG_DEBUG_ERR, "%s: %s != %s\n", __func__, ptt_cmd, response);
        }
    }
    while (ptt_cmd[4] != response[4] && --retry);

    return err;
}

/*
 * F6K rig capabilities.
 */
const struct rig_caps f6k_caps =
{
    RIG_MODEL(RIG_MODEL_F6K),
    .model_name =       "6xxx",
    .mfg_name =     "FlexRadio",
    .version =      "20130717.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_BETA,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_NETWORK,
    // The combination of timeout and retry is important
    // We need at least 3 seconds to do profile switches
    // Hitting the timeout is OK as long as we retry
    // Previous note showed FA/FB may take up to 500ms on band change
    .timeout =      300,
    .retry =        13,

    .has_get_func =     RIG_FUNC_NONE, /* has VOX but not implemented here */
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    F6K_LEVEL_ALL,
    .has_set_level =    F6K_LEVEL_ALL,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran =       {},     /* FIXME: granularity */
    .parm_gran =        {},
    //.extlevels =      elecraft_ext_levels,
    //.extparms =       kenwood_cfg_params,
    .post_write_delay = 20,
    .preamp =       { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .vfo_ops =      RIG_OP_NONE,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(30), MHz(77), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        {MHz(135), MHz(165), F6K_MODES, -1, - 1, F6K_VFO, F6K_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_6m(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_2m(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(30), MHz(77), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        { MHz(135), MHz(165), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_6m(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_2m(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {F6K_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(3.3)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(1.6)},
        {RIG_MODE_SSB, kHz(4.0)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW, kHz(0.4)},
        {RIG_MODE_CW, kHz(1.5)},
        {RIG_MODE_CW, Hz(50)},
        {RIG_MODE_CW, kHz(3.0)},
        {RIG_MODE_CW, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(1.5)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(3.0)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(0.1)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(14)},
        {RIG_MODE_AM, kHz(5.6)},
        {RIG_MODE_AM, kHz(20.0)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& f6k_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     flexradio_open,
    .rig_close =        kenwood_close,
    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     flex6k_set_mode,
    .get_mode =     flex6k_get_mode,
    .set_vfo =      kenwood_set_vfo,
    .get_vfo =      kenwood_get_vfo_if,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .get_ptt =      kenwood_get_ptt,
    .set_ptt =      kenwood_set_ptt,
    // TODO copy over kenwood_[set|get]_level and modify to handle DSP filter values
    // correctly - use actual values instead of indices
    .set_level =        kenwood_set_level,
    .get_level =        kenwood_get_level,
    //.set_ant =       kenwood_set_ant_no_ack,
    //.get_ant =       kenwood_get_ant,
};

/*
 * PowerSDR rig capabilities.
 */
const struct rig_caps powersdr_caps =
{
    RIG_MODEL(RIG_MODEL_POWERSDR),
    .model_name =       "PowerSDR",
    .mfg_name =     "FlexRadio",
    .version =      "20200528.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_BETA,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0, /* ms */
    // The combination of timeout and retry is important
    // We need at least 3 seconds to do profile switches
    // Hitting the timeout is OK as long as we retry
    // Previous note showed FA/FB may take up to 500ms on band change
    .timeout =      250,
    .retry =        3,

    .has_get_func =     RIG_FUNC_NONE, /* has VOX but not implemented here */
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    F6K_LEVEL_ALL,
    .has_set_level =    F6K_LEVEL_ALL,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran =       {},     /* FIXME: granularity */
    .parm_gran =        {},
    //.extlevels =      elecraft_ext_levels,
    //.extparms =       kenwood_cfg_params,
    .post_write_delay = 20,
    .preamp =       { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .vfo_ops =      RIG_OP_NONE,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(30), MHz(77), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        {MHz(135), MHz(165), F6K_MODES, -1, - 1, F6K_VFO, F6K_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_6m(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_2m(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(30), MHz(77), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        { MHz(135), MHz(165), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_6m(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_2m(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {F6K_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(3.3)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(1.6)},
        {RIG_MODE_SSB, kHz(4.0)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW, kHz(0.4)},
        {RIG_MODE_CW, kHz(1.5)},
        {RIG_MODE_CW, Hz(50)},
        {RIG_MODE_CW, kHz(3.0)},
        {RIG_MODE_CW, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(1.5)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(3.0)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(0.1)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(14)},
        {RIG_MODE_AM, kHz(5.6)},
        {RIG_MODE_AM, kHz(20.0)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& f6k_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     flexradio_open,
    .rig_close =        kenwood_close,
    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     flex6k_set_mode,
    .get_mode =     flex6k_get_mode,
    .set_vfo =      kenwood_set_vfo,
    .get_vfo =      kenwood_get_vfo_if,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .get_ptt =      flex6k_get_ptt,
    .set_ptt =      flex6k_set_ptt,
    // TODO copy over kenwood_[set|get]_level and modify to handle DSP filter values
    // correctly - use actual values instead of indices
    .set_level =        kenwood_set_level,
    .get_level =        kenwood_get_level,
    //.set_ant =       kenwood_set_ant_no_ack,
    //.get_ant =       kenwood_get_ant,
};
