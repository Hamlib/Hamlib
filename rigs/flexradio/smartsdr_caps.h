/*
 *  Hamlib FlexRadio backend - capabilities shared by every SmartSDR model
 *  Copyright (c) 2024 by Michael Black W9MDB
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Not a standalone header: this is the body of a struct rig_caps
 *  initialiser, included once per model in smartsdr.c so each slice gets the
 *  same capabilities. It therefore has no include guard on purpose -- it must
 *  expand at every inclusion -- and must contain nothing but field
 *  initialisers.
 */
    .mfg_name =       "FlexRadio",
    .version =        "20260826.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_BETA,
    .rig_type =       RIG_TYPE_TRANSCEIVER,
    /* Both slices are addressable without switching VFOs. */
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type =       RIG_PTT_RIG,
    .port_type =      RIG_PORT_NETWORK,
    .write_delay = 0,
    .post_write_delay = 0,
    .max_rit =        Hz(99999),
    .max_xit =        Hz(99999),
    .timeout = 2000,
    .retry = 2,

    .has_get_func =   SMARTSDR_FUNC,
    .has_set_func =   SMARTSDR_FUNC,
    .has_get_level =  SMARTSDR_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(SMARTSDR_LEVEL),
    .has_get_parm =    SMARTSDR_PARM,
    .has_set_parm =    RIG_PARM_SET(SMARTSDR_PARM),

    /* Ranges an application can trust for a slider or a bounds check. The
     * percentage settings take an integer 0-100 on the wire, so one percent
     * is their real granularity. The CW and VOX limits were read off a
     * FLEX-8400M: speed clamps to 5 and to 100, vox_delay refuses 200, and
     * break_in_delay refuses everything up to 10. Pitch is the range
     * FlexRadio documents -- the radio accepts values outside it without
     * complaint, so probing cannot establish it. */
    .level_gran = {
        [LVL_AF]           = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_RF]           = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_SQL]          = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_NR]           = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_NB]           = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_APF]          = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_BALANCE]      = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_RFPOWER]      = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_MICGAIN]      = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_COMP]         = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_VOXGAIN]      = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },
        [LVL_MONITOR_GAIN] = { .min = { .f = 0 }, .max = { .f = 1 }, .step = { .f = 0.01f } },

        [LVL_KEYSPD]       = { .min = { .i = 5 },   .max = { .i = 100 },  .step = { .i = 1 } },
        [LVL_CWPITCH]      = { .min = { .i = 100 }, .max = { .i = 6000 }, .step = { .i = 10 } },
        [LVL_BKIN_DLYMS]   = { .min = { .i = 30 },  .max = { .i = 2000 }, .step = { .i = 1 } },
        [LVL_VOXDELAY]     = { .min = { .i = 0 },   .max = { .i = 100 },  .step = { .i = 1 } },
        /* The radio's four AGC settings map onto Hamlib enums that are not
         * contiguous: OFF(0), FAST(2), SLOW(3), MEDIUM(5). The range has to
         * reach MEDIUM or an application clamping to it cannot select med. */
        [LVL_AGC]          = { .min = { .i = 0 },   .max = { .i = 5 },    .step = { .i = 1 } },

        /* Reported by the radio; nothing sets these. */
        [LVL_STRENGTH]     = { .min = { .i = -54 }, .max = { .i = 60 },  .step = { .i = 1 } },
        [LVL_SWR]          = { .min = { .f = 1 },   .max = { .f = 99 },  .step = { .f = 0.1f } },
        [LVL_ALC]          = { .min = { .f = 0 },   .max = { .f = 1 },   .step = { .f = 0.01f } },
        [LVL_RFPOWER_METER] = { .min = { .f = 0 },  .max = { .f = 1 },   .step = { .f = 0.01f } },
        [LVL_RFPOWER_METER_WATTS] = { .min = { .f = 0 }, .max = { .f = 100 }, .step = { .f = 0.1f } },
        /* No LVL_ shorthand exists for this one. */
        [setting2idx_builtin(RIG_LEVEL_TEMP_METER)]
                           = { .min = { .f = 0 },   .max = { .f = 100 }, .step = { .f = 0.1f } },
        [LVL_VD_METER]     = { .min = { .f = 0 },   .max = { .f = 20 },  .step = { .f = 0.1f } },
        [LVL_ID_METER]     = { .min = { .f = 0 },   .max = { .f = 30 },  .step = { .f = 0.1f } },
    },

    .chan_list =   {
        RIG_CHAN_END,
    },
    .scan_ops =    RIG_SCAN_NONE,
    /* One scope per rig: this model's slice sees its own panadapter. */
    .spectrum_scopes = {
        { 0, "Panadapter" },
        { -1, NULL },
    },
    .spectrum_modes = {
        RIG_SPECTRUM_MODE_CENTER,
        RIG_SPECTRUM_MODE_NONE,
    },
    .vfo_ops =     RIG_OP_TOGGLE | RIG_OP_CPY,
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { RIG_DBLST_END, },
    .preamp =      { RIG_DBLST_END, },

    .rx_range_list1 =  { {
            .startf = kHz(30), .endf = MHz(100), .modes = SMARTSDR_MODES,
            .low_power = -1, .high_power = -1, SMARTSDR_VFO
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        /* restricted to ham band */
        FRQ_RNG_HF(1, SMARTSDR_MODES, W(1), W(100), SMARTSDR_VFO, SMARTSDR_ANTS),
        FRQ_RNG_6m(1, SMARTSDR_MODES, W(1), W(100), SMARTSDR_VFO, SMARTSDR_ANTS),
        RIG_FRNG_END,
    },

    .rx_range_list2 =  { {
            .startf = kHz(30), .endf = MHz(100), .modes = SMARTSDR_MODES,
            .low_power = -1, .high_power = -1, SMARTSDR_VFO
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        /* restricted to ham band */
        FRQ_RNG_HF(2, SMARTSDR_MODES, W(1), W(100), SMARTSDR_VFO, SMARTSDR_ANTS),
        FRQ_RNG_6m(2, SMARTSDR_MODES, W(1), W(100), SMARTSDR_VFO, SMARTSDR_ANTS),
        RIG_FRNG_END,
    },

    .tuning_steps =  { {SMARTSDR_MODES, Hz(1)},
        {SMARTSDR_MODES, Hz(10)},
        {SMARTSDR_MODES, Hz(50)},
        {SMARTSDR_MODES, Hz(100)},
        {SMARTSDR_MODES, Hz(500)},
        {SMARTSDR_MODES, Hz(1000)},
        {SMARTSDR_MODES, Hz(2000)},
        {SMARTSDR_MODES, Hz(3000)},
        RIG_TS_END,
    },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },
    .priv =  NULL,    /* priv */

    .cfgparams =    smartsdr_config_params,
    .set_conf =     smartsdr_set_conf,
    .get_conf2 =    smartsdr_get_conf2,

    .rig_init =     smartsdr_init,
    .rig_open =     smartsdr_open,
    .rig_close =    smartsdr_close,
    .rig_cleanup =  smartsdr_cleanup,

    .set_freq =     smartsdr_set_freq,
    .get_freq =     smartsdr_get_freq,
    .get_mode=      smartsdr_get_mode,
    .set_mode=      smartsdr_set_mode,
    .set_ptt  =     smartsdr_set_ptt,
    .get_ptt  =     smartsdr_get_ptt,
    .set_level =    smartsdr_set_level,
    .get_level =    smartsdr_get_level,
    .set_func =     smartsdr_set_func,
    .get_func =     smartsdr_get_func,
    .set_ant =      smartsdr_set_ant,
    .get_ant =      smartsdr_get_ant,
    .set_vfo =      smartsdr_set_vfo,
    .get_vfo =      smartsdr_get_vfo,
    .vfo_op =       smartsdr_vfo_op,
    .set_rit =      smartsdr_set_rit,
    .get_rit =      smartsdr_get_rit,
    .set_xit =      smartsdr_set_xit,
    .get_xit =      smartsdr_get_xit,
    .set_ts =       smartsdr_set_ts,
    .get_ts =       smartsdr_get_ts,
    .get_info =     smartsdr_get_info,
    .set_split_vfo = smartsdr_set_split_vfo,
    .get_split_vfo = smartsdr_get_split_vfo,
    .set_split_freq = smartsdr_set_split_freq,
    .get_split_freq = smartsdr_get_split_freq,
    .set_split_mode = smartsdr_set_split_mode,
    .get_split_mode = smartsdr_get_split_mode,
    .send_morse =  smartsdr_send_morse,
    .stop_morse = smartsdr_stop_morse,

    .stream_caps = smartsdr_stream_caps,
    .stream_open =  smartsdr_stream_open,
    .stream_close = smartsdr_stream_close,
    .stream_hardware_time = smartsdr_stream_hardware_time,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS