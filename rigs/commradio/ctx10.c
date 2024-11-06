
#include <stdlib.h>

#include "hamlib/rig.h"
#include "bandplan.h"
#include "commradio.h"

/*
 * The CTX-10 has only one VFO, but can be set into some sort of "split" mode
 * where the screen shows two different frequencies.
 * So far I have not figured out how to access these via serial.
 */
#define CTX10_VFO (RIG_VFO_A)

#define CTX10_RX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM)
#define CTX10_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB)

struct rig_caps ctx10_caps =
{
    RIG_MODEL(RIG_MODEL_CTX10),
    .model_name = "CTX-10",
    .mfg_name = "Commradio",
    .version = "20240809.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_NONE,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 3000000,
    .serial_rate_max = 3000000,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 1000,
    .retry = 3,
    .has_get_func = RIG_FUNC_NONE,
    .has_set_func = RIG_FUNC_NONE,
    .has_get_level = RIG_LEVEL_NONE,
    .has_set_level = RIG_LEVEL_NONE,
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
//  .level_gran = {},
//  .parm_gran = {},
    .ctcss_list = NULL,
    .dcs_list = NULL,
//  .preamp = { RIG_DBLST_END, },
//  .attenuator = { RIG_DBLST_END, },
//  .max_rit = Hz(0),
//  .max_xit = Hz(0),
//  .max_ifshift = Hz(0),
    .targetable_vfo = 0,
    .vfo_ops = (RIG_OP_FROM_VFO | RIG_OP_TO_VFO),
    .scan_ops = RIG_SCAN_NONE,
    .transceive = RIG_TRN_OFF,
    .bank_qty = 0,
    .chan_desc_sz = 0,
    .chan_list = { RIG_CHAN_END, },
    .rx_range_list1 = {
        {kHz(150), MHz(30), CTX10_RX_MODES, -1, -1, CTX10_VFO, 0},
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        FRQ_RNG_80m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
        FRQ_RNG_60m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
        FRQ_RNG_40m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
        FRQ_RNG_30m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
        FRQ_RNG_20m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
        FRQ_RNG_17m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
        FRQ_RNG_15m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
        FRQ_RNG_12m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
        FRQ_RNG_10m_REGION2(CTX10_TX_MODES, W(1), W(10), CTX10_VFO, 0),
    },
    .tuning_steps = {
        {CTX10_RX_MODES, 10},
        RIG_TS_END,
    },
//  .async_data_supported = 1, //TODO: Revisit this
//  .decode_event = commradio_decode_event,
    .rig_init = commradio_init,
//  .rig_cleanup = commradio_cleanup,
//  .rig_open = commradio_rig_open,
//  .rig_close = commradio_rig_close,
    .get_freq = commradio_get_freq,
    .set_freq = commradio_set_freq,
    .get_mode = commradio_get_mode,
    .set_mode = commradio_set_mode,
    .get_vfo = commradio_get_vfo,
    .set_vfo = commradio_set_vfo,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

