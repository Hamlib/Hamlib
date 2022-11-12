/*
 *  Hamlib CI-V backend - description of IC-E92D/IC-92AD and variations
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "icom.h"
#include "frame.h"
#include "icom_defs.h"

/* TODO: DV (GMSK 4.8 kbps voice) */
#define IC92D_MODES (RIG_MODE_FM)
#define IC92D_MODES_TX (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define IC92D_FUNC_ALL (RIG_FUNC_MUTE|RIG_FUNC_MON|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_LOCK|RIG_FUNC_AFC)

#define IC92D_LEVEL_ALL (RIG_LEVEL_AF|RIG_LEVEL_SQL|RIG_LEVEL_RFPOWER|RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_RAWSTR)

#define IC92D_PARM_ALL (RIG_PARM_BEEP|RIG_PARM_BACKLIGHT)

#define IC92D_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define IC92D_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define IC92D_SCAN_OPS (RIG_SCAN_VFO|RIG_SCAN_MEM)

/*
 * FIXME: real measurement
 */
#define IC92D_STR_CAL   UNKNOWN_IC_STR_CAL

/* FIXME */
#define IC92D_MEM_CAP  {    \
        .freq = 1,  \
        .mode = 1,  \
        .width = 1, \
        .rptr_offs = 1, \
        .rptr_shift = 1, \
        .funcs = IC92D_FUNC_ALL, \
        .levels = RIG_LEVEL_SET(IC92D_LEVEL_ALL), \
}

static const char *ic92d_get_info(RIG *rig);

/* FIXME: tuning step sub-commands */
const struct ts_sc_list ic92d_ts_sc_list[] =
{
    { kHz(5), 0x00 },
    { kHz(6.25), 0x01 },
    { kHz(8.33), 0x02 },
    { kHz(9), 0x03 },
    { kHz(10), 0x04 },
    { kHz(12.5), 0x05 },
    { kHz(15), 0x06 },
    { kHz(20), 0x07 },
    { kHz(25), 0x08 },
    { kHz(30), 0x09 },
    { kHz(50), 0x0a },
    { kHz(100), 0x0b },
    { kHz(125), 0x0c },
    { kHz(200), 0x0d },
    { 0, 0 },
};


/*
 */
static const struct icom_priv_caps ic92d_priv_caps =
{
    0x01,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic92d_ts_sc_list,
    .serial_full_duplex = 1
};

const struct rig_caps ic92d_caps =
{
    RIG_MODEL(RIG_MODEL_IC92D),
    .model_name = "IC-92D", /* IC-E92D/IC-92AD */
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_HANDHELD,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  38400,
    .serial_rate_max =  38400,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  IC92D_FUNC_ALL,
    .has_set_func =  IC92D_FUNC_ALL,
    .has_get_level =  IC92D_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(IC92D_LEVEL_ALL),
    .has_get_parm =  IC92D_PARM_ALL,
    .has_set_parm =  IC92D_PARM_ALL,
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  full_dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 10, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC92D_VFO_OPS,
    .scan_ops =  IC92D_SCAN_OPS,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   26,
    .chan_desc_sz =  8,

    /* The IC-E92D has a total 1304 memory channels with 26 memory banks.
     * The VFO A has 800 regular channels, 50 scan edges and 2 call channels,
     * while the VFO B has 400 regular, 50 scan edges and 2 call channels.
     */
    .chan_list =  {
        {    1, 1200, RIG_MTYPE_MEM, IC92D_MEM_CAP },
        { 1201, 1300, RIG_MTYPE_EDGE, IC92D_MEM_CAP },
        { 1301, 1304, RIG_MTYPE_CALL, IC92D_MEM_CAP },
        RIG_CHAN_END,
    },

    /* IC-E92D */
    .rx_range_list1 =   {
        {kHz(495), MHz(999.99), RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_WFM, -1, -1, RIG_VFO_A},
        {MHz(118), MHz(174), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_B}, // TODO: MODE_DV
        {MHz(350), MHz(470), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_B}, // TODO: MODE_DV
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {MHz(144), MHz(146) - 1, IC92D_MODES_TX, mW(100), W(5), IC92D_VFO_ALL},
        {MHz(430), MHz(440) - 1, IC92D_MODES_TX, mW(100), W(5), IC92D_VFO_ALL},
        RIG_FRNG_END,
    },

    /* IC-92AD */
    .rx_range_list2 =   {
        {kHz(495), MHz(999.99), RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_WFM, -1, -1, RIG_VFO_A},
        {MHz(118), MHz(174), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_B}, // TODO: MODE_DV
        {MHz(350), MHz(470), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_B}, // TODO: MODE_DV
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {MHz(144), MHz(148) - 1, IC92D_MODES_TX, mW(100), W(5), IC92D_VFO_ALL},
        {MHz(430), MHz(440) - 1, IC92D_MODES_TX, mW(100), W(5), IC92D_VFO_ALL},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC92D_MODES, kHz(5)},
        {IC92D_MODES, kHz(6.25)},
        {IC92D_MODES, kHz(8.33)},
        {IC92D_MODES, kHz(9)},
        {IC92D_MODES, kHz(10)},
        {IC92D_MODES, 12500},
        {IC92D_MODES, kHz(15)},
        {IC92D_MODES, kHz(20)},
        {IC92D_MODES, kHz(25)},
        {IC92D_MODES, kHz(50)},
        {IC92D_MODES, kHz(100)},
        {IC92D_MODES, kHz(125)},
        {IC92D_MODES, kHz(200)},
        RIG_TS_END,
    },
    /* FIXME: mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(9)}, /* N-FM & AM */
        {RIG_MODE_WFM, kHz(230)},
        RIG_FLT_END,
    },
    .str_cal = IC92D_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic92d_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .get_info =  ic92d_get_info,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const char *ic92d_get_info(RIG *rig)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[16];
    int ack_len, retval;
    static char info[64];

    rs = &rig->state;
    priv = (struct icom_priv_data *)rs->priv;

    // 018019fd

    priv->re_civ_addr = 0x01;

    retval = icom_transaction(rig, C_RD_TRXID, -1,
                              NULL, 0, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    if (ack_len <= 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), "
                  "len=%d\n", __func__, ackbuf[0], ack_len);
        return NULL;
    }

    SNPRINTF(info, sizeof(info), "ID %02x%02x%02x\n", ackbuf[1], ackbuf[2],
             ackbuf[3]);

    return info;
}

