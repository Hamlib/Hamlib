        // Once included these values can be overridden in the back-end
        // Known variants are PREAMP, ATT, NB, CWPITCH, IF, NOTCHF, VOXDELAY, BKINDL, BKIN_DLYMS, RFPOWER_METER(255 or 100), RFPOWER_METER_WATTS(255 or 100)
        // cppcheck-suppress *
        /* raw data */
        [LVL_RAWSTR]        = { .min = { .i = 0 },     .max = { .i = 255 } },
        /* levels with dB units */
#if !defined(NO_LVL_PREAMP)
        [LVL_PREAMP]        = { .min = { .i = 0 },     .max = { .i = 20 },   .step = { .i = 10 } },
#endif
#if !defined(NO_LVL_ATT)
        [LVL_ATT]           = { .min = { .i = 0 },     .max = { .i = 10 },   .step = { .i = 10 } },
#endif
        [LVL_STRENGTH]      = { .min = { .i = 0 },     .max = { .i = 60 },   .step = { .i = 0 } },
        [LVL_NB]            = { .min = { .f = 0 },     .max = { .f = 10 },   .step = { .f = 1 } },
        /* levels with WPM units */
        [LVL_KEYSPD]        = { .min = { .i = 8 },     .max = { .i = 50 },   .step = { .i = 1 } },
        /* levels with Hz units */
#if !defined(NO_LVL_CWPITCH)
        [LVL_CWPITCH]       = { .min = { .i = 300 },   .max = { .i = 800 }, .step = { .i = 10 } },
#endif
        /* levels with time units */
#if !defined(NO_LVL_CWPITCH)
        [LVL_VOXDELAY]      = { .min = { .i = 0 },     .max = { .i = 255 },  .step = { .i = 50 } },
#endif
        /* levels with 0-1 values -- increment based on rig's range */
        [LVL_NR]            = { .min = { .f = 0 },     .max = { .f = 1 },    .step = { .f = 1.0f/10.0f } },
        [LVL_AF]            = { .min = { .f = 0 },     .max = { .f = 1.0 },  .step = { .f = 1.0f/255.0f } },
        [LVL_RF]            = { .min = { .f = 0 },     .max = { .f = 1.0 },  .step = { .f = 1.0f/255.0f } },
        [LVL_RFPOWER]       = { .min = { .f = .05 },   .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
        [LVL_RFPOWER_METER] = { .min = { .f = .0 },    .max = { .f = 1 },    .step = { .f = 1.0f/255.0f } },
        [LVL_RFPOWER_METER_WATTS] = { .min = { .f = .0 },    .max = { .f = 100 },    .step = { .f = 1.0f/255.0f } },
        [LVL_SQL]           = { .min = { .f = 0 },     .max = { .f = 1.0 },  .step = { .f = 1.0f/100.0f } },
        [LVL_MICGAIN]       = { .min = { .f = .0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
        [LVL_MONITOR_GAIN]  = { .min = { .f = .0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
        [LVL_COMP]          = { .min = { .f = .0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
        [LVL_VOXGAIN]       = { .min = { .f = .0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
        [LVL_ALC]           = { .min = { .f = .0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
        [LVL_SWR]           = { .min = { .f = 1.0 },   .max = { .f = 99.9 }, .step = { .f = 1.0f/10.0f  } },

