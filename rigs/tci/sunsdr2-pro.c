/*
 *  Hamlib TCI 2.0 backend — SunSDR2 PRO capabilities
 *  Copyright (c) 2026 by Jeff Francis N0GQ <gjfrancis@protonmail.com>
 *
 *  This file is the per-radio capabilities table for the Expert
 *  Electronics SunSDR2 PRO talking TCI 2.0 over WebSocket.  All protocol
 *  work lives in the generic tci2.c; this file only describes the
 *  hardware's RF envelope, filter widths, and TX power limits, then
 *  wires Hamlib's callback table to the tci2_* entry points.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include "hamlib/config.h"

#include "hamlib/rig.h"
#include "tci2.h"

struct rig_caps sunsdr2_pro_caps =
{
    RIG_MODEL(RIG_MODEL_SUNSDR2_PRO),
    .model_name     = "SunSDR2 PRO",
    .mfg_name       = "Expert Electronics",
    .version        = "20260630.0",
    .copyright      = "LGPL",
    .status         = RIG_STATUS_BETA,
    .rig_type       = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .ptt_type       = RIG_PTT_RIG_MICDATA,
    .dcd_type       = RIG_DCD_NONE,
    .port_type      = RIG_PORT_NETWORK,
    .timeout        = 2000,
    .retry          = 3,

    .has_get_func   = TCI2_FUNCS,
    .has_set_func   = TCI2_FUNCS,
    .has_get_level  = TCI2_LEVELS_GET,
    .has_set_level  = RIG_LEVEL_SET(TCI2_LEVELS_SET),
    .has_get_parm   = RIG_PARM_NONE,
    .has_set_parm   = RIG_PARM_NONE,

    /* SunSDR2 PRO: HF + 6 m RX, HF + 6 m TX at 15 W out.
     * Datasheet RX range is 0.09–65 MHz; TX is the amateur bands within
     * 1.8–54 MHz at the rig's licensed limits.  We publish a generous
     * envelope and let the server clamp it. */
    .rx_range_list1 =
    {
        { kHz(1), MHz(65), TCI2_MODES, -1, -1, TCI2_VFOS, RIG_ANT_NONE },
        RIG_FRNG_END,
    },
    .tx_range_list1 =
    {
        { kHz(1800), MHz(54), TCI2_MODES, mW(1), W(15), TCI2_VFOS, RIG_ANT_NONE },
        RIG_FRNG_END,
    },
    .tuning_steps =
    {
        { TCI2_MODES, 1 },
        { TCI2_MODES, RIG_TS_ANY },
        RIG_TS_END,
    },
    .filters =
    {
        { RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | \
          RIG_MODE_RTTYR, kHz(2.7) },
        { RIG_MODE_AM  | RIG_MODE_SAM | RIG_MODE_DSB, kHz(6) },
        { RIG_MODE_FM  | RIG_MODE_FMN, kHz(12) },
        { RIG_MODE_WFM, kHz(180) },
        { TCI2_MODES, RIG_FLT_ANY },
        RIG_FLT_END,
    },

    .cfgparams      = tci2_cfg_params,
    .set_conf       = tci2_set_conf,
    .get_conf       = tci2_get_conf,

    .priv = NULL,

    .rig_init       = tci2_init,
    .rig_open       = tci2_open,
    .rig_close      = tci2_close,
    .rig_cleanup    = tci2_cleanup,

    .set_freq       = tci2_set_freq,
    .get_freq       = tci2_get_freq,
    .set_mode       = tci2_set_mode,
    .get_mode       = tci2_get_mode,
    .set_vfo        = tci2_set_vfo,
    .get_vfo        = tci2_get_vfo,
    .set_ptt        = tci2_set_ptt,
    .get_ptt        = tci2_get_ptt,
    .set_split_vfo  = tci2_set_split_vfo,
    .get_split_vfo  = tci2_get_split_vfo,
    .set_split_freq = tci2_set_split_freq,
    .get_split_freq = tci2_get_split_freq,
    .set_rit        = tci2_set_rit,
    .get_rit        = tci2_get_rit,
    .set_xit        = tci2_set_xit,
    .get_xit        = tci2_get_xit,
    .set_level      = tci2_set_level,
    .get_level      = tci2_get_level,
    .set_func       = tci2_set_func,
    .get_func       = tci2_get_func,
    .power2mW       = tci2_power2mW,
    .mW2power       = tci2_mW2power,
    .send_morse     = tci2_send_morse,
    .stop_morse     = tci2_stop_morse,
    .wait_morse     = tci2_wait_morse,
    .get_info       = tci2_get_info,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
