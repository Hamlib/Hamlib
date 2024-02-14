#include "mds.h"

struct rig_caps mds_9710_caps =
{
    RIG_MODEL(RIG_MODEL_MDS9710),
    .model_name =       "9710",
    .mfg_name =         "MDS",
    .version =          "20221116.0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_BETA,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  110,
    .serial_rate_max =  38400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_HARDWARE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          1000,
    .retry =            3,

    .has_get_func =     RIG_FUNC_NONE,
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    MDS_LEVELS,
    .has_set_level =    RIG_LEVEL_NONE,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
//  .level_gran =          { [LVL_CWPITCH] = { .step = { .i = 10 } } },
//  .ctcss_list =          common_ctcss_list,
//  .dcs_list =            full_dcs_list,
//  2050 does have channels...not implemented yet as no need yet
//  .chan_list =   {
//                        {   0,  18, RIG_MTYPE_MEM, DUMMY_MEM_CAP },
//                        {  19,  19, RIG_MTYPE_CALL },
//                        {  20,  NB_CHAN-1, RIG_MTYPE_EDGE },
//                        RIG_CHAN_END,
//                 },
// .scan_ops =    DUMMY_SCAN,
// .vfo_ops =     DUMMY_VFO_OP,
    .transceive =       RIG_TRN_RIG,
    .rx_range_list1 = {
        {
            .startf = MHz(800), .endf = MHz(880), .modes = MDS_ALL_MODES,
            .low_power = 0, .high_power = 0, MDS_ALL_MODES, RIG_ANT_1,
        },
        {
            .startf = MHz(880), .endf = MHz(960), .modes = MDS_ALL_MODES,
            .low_power = 0, .high_power = 0, MDS_ALL_MODES, RIG_ANT_1,
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 = {RIG_FRNG_END,},
    .tx_range_list1 = {
        {MHz(380), MHz(530), MDS_ALL_MODES, W(.1), W(5), RIG_VFO_A, RIG_ANT_NONE},
        RIG_FRNG_END,
    },
//    .tx_range_list2 = {RIG_FRNG_END,}
    .tuning_steps =     {
        // Rem: no support for changing tuning step
        {MDS_ALL_MODES, 6250},
        RIG_TS_END,
    },

    .filters =  {
        {MDS_ALL_MODES, RIG_FLT_ANY},
        RIG_FLT_END
    },
    .priv = NULL,

    .rig_init =     mds_init,
    .rig_open =     mds_open,
    .rig_cleanup =  mds_cleanup,

//    .set_conf =     dummy_set_conf,
//    .get_conf =     dummy_get_conf,

    .set_freq = mds_set_freq,
    .get_freq = mds_get_freq,
//    .set_mode = mds_set_mode,
//    .get_mode = mds_get_mode,

//    .set_level =     dummy_set_level,
//    .get_level =    mds_get_level,

    .get_info =     mds_get_info,
    .set_ptt =      mds_set_ptt,
    .get_ptt =      mds_get_ptt,
//  .get_dcd =    dummy_get_dcd,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
