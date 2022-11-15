/*
 *  Hamlib R&S backend - XK2100 description
 *  Copyright (c) 2009-2010 by Stephane Fillod
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

#include <hamlib/rig.h>
#include "gp2000.h"

#define XK2100_MODES (RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_FM)

#define XK2100_FUNC (RIG_FUNC_SQL)

/* #define XK2100_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_AGC|  \
 *                      RIG_LEVEL_RF|RIG_LEVEL_AF|RIG_LEVEL_STRENGTH)
 */

#define XK2100_LEVEL_ALL (RIG_LEVEL_SQL|\
                        RIG_LEVEL_AF)

#define XK2100_PARM_ALL (RIG_PARM_NONE)

#define XK2100_VFO (RIG_VFO_A)

#define XK2100_VFO_OPS (RIG_OP_NONE)

#define XK2100_ANTS (RIG_ANT_1)

#define XK2100_MEM_CAP {    \
        .freq = 1,      \
        .mode = 1,      \
        .width = 1,     \
        .ant = 1,     \
        .funcs = XK2100_FUNC, \
        .levels = RIG_LEVEL_SET(XK2100_LEVEL_ALL), \
        .channel_desc=1, \
        .flags = RIG_CHFLAG_SKIP, \
}


/*
 * XK2100 rig capabilities.
 *
 * Had to use NONE for flow control and set RTS high
 * We are not using address mode since we're on RS232 for now
 * If using RS485 should add address capability
 *
 * TODO
 *  - set/get_channels
 */

const struct rig_caps xk2100_caps =
{
    RIG_MODEL(RIG_MODEL_XK2100),
    .model_name = "XK2100",
    .mfg_name = "Rohde&Schwarz",
    .version = BACKEND_VER ".0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    // Need to set RTS on for some reason
    // And HANDSHAKE_NONE even though HARDWARE is what is called for
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 38400, /* 7E1, RTS/CTS */
    .serial_data_bits = 7,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_EVEN,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 200,
    .retry = 3,

    .has_get_func = XK2100_FUNC,
    .has_set_func = XK2100_FUNC,
    .has_get_level = XK2100_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(XK2100_LEVEL_ALL),
    .has_get_parm = XK2100_PARM_ALL,
    .has_set_parm = RIG_PARM_SET(XK2100_PARM_ALL),
    .level_gran = {},
    .parm_gran = {},
    .ctcss_list = NULL,
    .dcs_list = NULL,
    .preamp = {RIG_DBLST_END},
    .attenuator = {32, RIG_DBLST_END},
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
    .targetable_vfo = 0,
    .transceive = RIG_TRN_RIG,
    .bank_qty = 0,
    .chan_desc_sz = 7,        /* FIXME */
    .vfo_ops = XK2100_VFO_OPS,

    .chan_list = {
        {0, 99, RIG_MTYPE_MEM, XK2100_MEM_CAP},
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        {
            kHz(1500), MHz(30), XK2100_MODES, -1, -1, XK2100_VFO,
            XK2100_ANTS
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {
        {
            kHz(1500), MHz(30), XK2100_MODES, -1, -1, XK2100_VFO,
            XK2100_ANTS
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},

    .tuning_steps =     {
        // Rem: no support for changing tuning step
        {RIG_MODE_ALL, 1},
        RIG_TS_END,
    },

    /*
    .tuning_steps =  {
         {XK2100_MODES,1},
         {XK2100_MODES,10},
         {XK2100_MODES,100},
         {XK2100_MODES,1000},
         RIG_TS_END,
        },
        */

    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_WFM, kHz(150)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(15)},
        {XK2100_MODES, kHz(2.4)},
        {XK2100_MODES, kHz(1.5)},
        {XK2100_MODES, Hz(150)},
        {XK2100_MODES, Hz(300)},
        {XK2100_MODES, Hz(600)},
        {XK2100_MODES, kHz(6)},
        {XK2100_MODES, kHz(9)},
        {XK2100_MODES, kHz(15)},
        {XK2100_MODES, kHz(30)},
        {XK2100_MODES, kHz(50)},
        {XK2100_MODES, kHz(120)},
        RIG_FLT_END,
    },
    .priv = NULL,

    .set_ptt = gp2000_set_ptt,
    .get_ptt = gp2000_get_ptt,
    .set_freq = gp2000_set_freq,
    .get_freq = gp2000_get_freq,
    .set_mode =  gp2000_set_mode,
    .get_mode =  gp2000_get_mode,
    .set_level =  gp2000_set_level,
    .get_level =  gp2000_get_level,
//.set_func =  gp2000_set_func,
//.get_func =  gp2000_get_func,
    .get_info =  gp2000_get_info,

#if 0
    /* TODO */
    .rig_open = gp2000_rig_open,
    .set_channel = gp2000_set_channel,
    .get_channel = gp2000_get_channel,
#endif
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */
