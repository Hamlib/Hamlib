    .mfg_name =       "Flex-radio",
    .version =        "20240519.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_ALPHA,
    .rig_type =       RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =      0,
    .ptt_type =       RIG_PTT_RIG,
    .port_type =      RIG_PORT_NETWORK,

    .has_get_func =   SMARTSDR_FUNC,
    .has_set_func =   SMARTSDR_FUNC,
    .has_get_level =  SMARTSDR_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(SMARTSDR_LEVEL),
    .has_get_parm =    SMARTSDR_PARM,
    .has_set_parm =    RIG_PARM_SET(SMARTSDR_PARM),
    .chan_list =   {
        RIG_CHAN_END,
    },
    .scan_ops =    RIG_SCAN_NONE,
    .vfo_ops =     RIG_OP_NONE,
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { RIG_DBLST_END, },
    .preamp =      { 14, RIG_DBLST_END, },

    .rx_range_list1 =  { {
            .startf = kHz(30), .endf = MHz(54), .modes = SMARTSDR_MODES,
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

    .tuning_steps =  { {SMARTSDR_MODES, 1},
        RIG_TS_END,
    },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },
    .priv =  NULL,    /* priv */

    .rig_init =     smartsdr_init,
    .rig_open =     smartsdr_open,
    .rig_close =    smartsdr_close,
    .rig_cleanup =  smartsdr_cleanup,

    .set_freq =     smartsdr_set_freq,
    .get_freq =     smartsdr_get_freq,
    .set_ptt  =     smartsdr_set_ptt,
//  .reset    =     smartsdr_reset,
//  .set_level =     smartsdr_set_level,
//  .set_func =     _set_func,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
