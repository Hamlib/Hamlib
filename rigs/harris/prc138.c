#include "harris.h"
#include "prc138.h"

/**
 * Harris PRC-138 Capabilities
 */
struct rig_caps prc138_caps = {
    .rig_model          = RIG_MODEL_PRC138,
    .model_name         = "PRC-138",
    .mfg_name           = "Harris",
    .version            = "1.0.6",
    .port_type          = RIG_PORT_SERIAL,
    
    /* Frequency Ranges with correct brace nesting */
    .rx_range_list1 = {
        { 
            .startf = kHz(1600), .endf = MHz(60), .modes = PRC138_MODES, 
            .low_power = -1, .high_power = -1, .vfo = PRC138_VFO 
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        { 
            .startf = kHz(1600), .endf = MHz(60), .modes = PRC138_MODES, 
            .low_power = W(1), .high_power = W(20), .vfo = PRC138_VFO 
        },
        RIG_FRNG_END,
    },
    .tuning_steps =  { {RIG_MODE_ALL, 100}, {RIG_MODE_ALL, RIG_TS_ANY}, {0, 0} },    
    
    /* Serial Parameters */
    .serial_rate_min    = 75,
    .serial_rate_max    = 9600,
    .serial_data_bits   = 8,
    .serial_stop_bits   = 1,
    .serial_parity      = RIG_PARITY_NONE,
    .serial_handshake   = RIG_HANDSHAKE_NONE,
    .write_delay        = 0, //50
    .post_write_delay   = 50, //100
    .timeout            = 200, //200
    .retry              = 0, //3
    
    /* Passband filters definition directly inside caps to fix pointer warning */
    .filters = {
        /* SSB (USB/LSB): 2.7 (def), 1.5, 2.0, 2.4, 3.0 kHz */
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(1.5)},
        {RIG_MODE_SSB, kHz(2.0)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(3.0)},

        /* CW: 1.0 (def), 0.35, 0.68, 1.5 kHz */
        {RIG_MODE_CW,  kHz(1.0)},
        {RIG_MODE_CW,  350},
        {RIG_MODE_CW,  680},
        {RIG_MODE_CW,  kHz(1.5)},

        /* AME: 6.0 (def), 3.0, 4.0, 5.0 kHz */
        {RIG_MODE_AM,  kHz(6.0)},
        {RIG_MODE_AM,  kHz(3.0)},
        {RIG_MODE_AM,  kHz(4.0)},
        {RIG_MODE_AM,  kHz(5.0)},

        RIG_FLT_END,
    },

    /* Capabilities */
    .ptt_type           = RIG_PTT_RIG,
    .has_get_level      = RIG_LEVEL_SQL | RIG_LEVEL_RFPOWER | RIG_LEVEL_AGC | RIG_LEVEL_RF | RIG_LEVEL_PREAMP,
    .has_set_level      = RIG_LEVEL_SQL | RIG_LEVEL_RFPOWER | RIG_LEVEL_AGC | RIG_LEVEL_RF | RIG_LEVEL_PREAMP,
    .has_set_func       = RIG_FUNC_SQL,

    /* Function Callbacks */
    .rig_init           = harris_init,
    .rig_cleanup        = harris_cleanup,
    .rig_open           = harris_open,
    .rig_close          = harris_close,
    .set_freq           = harris_set_freq,
    .get_freq           = harris_get_freq,
    .set_mode           = harris_set_mode,
    .get_mode           = harris_get_mode,
    .set_ptt            = harris_set_ptt,
    .get_ptt            = harris_get_ptt,
    .set_level          = harris_set_level,
    .get_level          = harris_get_level,
    .set_func           = harris_set_func,
};
