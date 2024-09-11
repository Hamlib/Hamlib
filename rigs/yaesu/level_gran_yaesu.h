        // Once included these values can be overridden in the back-end
        // Known variants are PREAMP, ATT, NB, CWPITCH, IF, NOTCHF, VOXDELAY, BKINDL, BKIN_DLYMS, RFPOWER_METER(255 or 100), RFPOWER_METER_WATTS(255 or 100)
        // cppcheck-suppress *
        /* raw data */
        [LVL_RAWSTR]        = { .min = { .i = 0 },     .max = { .i = 255 } },
        /* levels with dB units */
        [LVL_PREAMP]        = { .min = { .i = 0 },     .max = { .i = 20 },   .step = { .i = 10 } },
        [LVL_ATT]           = { .min = { .i = 0 },     .max = { .i = 12 },   .step = { .i = 0 } },
        [LVL_STRENGTH]      = { .min = { .i = 0 },     .max = { .i = 60 },   .step = { .i = 0 } },
        [LVL_NB]            = { .min = { .f = 0 },     .max = { .f = 10 },    .step = { .f = 1 } },
        /* levels with WPM units */
#if !defined(LVL_KEYSPD)
        [LVL_KEYSPD]  = { .min = { .i = 4 },           .max = { .i = 60 },   .step = { .i = 1 } },
#endif
        /* levels with Hz units */
#if !defined(NO_LVL_CWPITCH)
        [LVL_CWPITCH]       = { .min = { .i = 300 },   .max = { .i = 1050 }, .step = { .i = 10 } },
#endif
        [LVL_IF]            = { .min = { .i = -1200 }, .max = { .i = 1200 }, .step = { .i = 20 } },
#if !defined(NO_LVL_NOTCHF)
        [LVL_NOTCHF]        = { .min = { .i = 1 },     .max = { .i = 3200 }, .step = { .i = 10 } },
#endif
        /* levels with time units */
        [LVL_VOXDELAY]      = { .min = { .i = 3 },     .max = { .i = 300 },  .step = { .i = 1 } },
        [LVL_BKINDL]        = { .min = { .i = 30 },    .max = { .i = 3000 }, .step = { .i = 1 } },
        [LVL_BKIN_DLYMS]    = { .min = { .i = 30 },    .max = { .i = 3000 }, .step = { .i = 1 } },
        /* levels with watt units */
        [LVL_RFPOWER_METER_WATTS] = { .min = { .f = .0 },    .max = { .f = 100 },    .step = { .f = 1.0f/255.0f } },
        /* level with misc units */
        [LVL_SWR]           = { .min = { .f = 0 },     .max = { .f = 5.0 },  .step = { .f = 1.0f/255.0f } },
        [LVL_BAND_SELECT]   = { .min = { .i = 0 },     .max = { .i = 16 },  .step = { .i = 1 } },
        // most recent rigs seem to have 15 as the maximum -- other values can be set in the backend
#if !defined(NO_LVL_NR)
        [LVL_NR]            = { .min = { .f = 0 },     .max = { .f = 1 },  .step = { .f = 1.0f/15.0f } },
#endif
        /* levels with 0-1 values -- increment based on rig's range */
#if !defined(NO_LVL_AF)
        [LVL_AF]            = { .min = { .f = 0 },     .max = { .f = 1 },  .step = { .f = 1.0f/255.0f } },
#endif
#if !defined(NO_LVL_RF)
        [LVL_RF]            = { .min = { .f = 0 },     .max = { .f = 1 },  .step = { .f = 1.0f/255.0f } },
#endif
#if !defined(NO_LVL_RFPOWER)
        [LVL_RFPOWER]       = { .min = { .f = 5.0/255.0 }, .max = { .f = 1 }, .step = { .f = 1.0f/255.0f } },
#endif
        [LVL_RFPOWER_METER] = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/255.0f } },
        [LVL_COMP_METER]    = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/255.0f } },
        [LVL_ID_METER]      = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/255.0f } },
        [LVL_VD_METER]      = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/255.0f } },
#if !defined(NO_LVL_SQL)
        [LVL_SQL]           = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/255.0f } },
#endif
#if !defined(NO_LVL_MICGAIN)
        [LVL_MICGAIN]       = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/255.0f } },
#endif
#if !defined(NO_LVL_MONITOR_GAIN)
        [LVL_MONITOR_GAIN]  = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/255.0f } },
#endif
#if !defined(NO_LVL_COMP)
        [LVL_COMP]          = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
#endif
#if !defined(NO_LVL_VOXGAIN)
        [LVL_VOXGAIN]       = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
#endif
        [LVL_ANTIVOX]       = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },
        [LVL_ALC]           = { .min = { .f = 0 },    .max = { .f = 1 },    .step = { .f = 1.0f/100.0f } },

