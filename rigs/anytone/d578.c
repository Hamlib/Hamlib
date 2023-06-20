// ---------------------------------------------------------------------------
//   Anytone D578UVIII
// ---------------------------------------------------------------------------
//
//  d578.c
//
//  Created by Michael Black W9MDB
//  Copyright © 2023, Michael Black W9MDB.
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public
//   License along with this library; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#include "anytone.h"

#define D578_VFO (RIG_VFO_A|RIG_VFO_B)
#define D578_MODES (RIG_MODE_USB|RIG_MODE_AM)

const struct rig_caps anytone_d578_caps =
{
    RIG_MODEL(RIG_MODEL_ATD578UVIII),
    .model_name         =  "D578A",
    .mfg_name           =  "AnyTone",
    .version            =  BACKEND_VER ".0",
    .copyright          =  "Michael Black W9MDB: GNU LGPL",
    .status             =  RIG_STATUS_ALPHA,
    .rig_type           =  RIG_TYPE_TRANSCEIVER,
    .ptt_type           =  RIG_PTT_RIG,
    .dcd_type           =  RIG_DCD_NONE,
    .port_type          =  RIG_PORT_SERIAL,
    .serial_rate_min    =  115200,
    .serial_rate_max    =  115200,
    .serial_data_bits   =  8,
    .serial_stop_bits   =  1,
    .serial_parity      =  RIG_PARITY_NONE,
    .serial_handshake   =  RIG_HANDSHAKE_NONE,
    .write_delay        =  0,
    .post_write_delay   =  0,
    .timeout            =  1000,
    .retry              =  3,

    .level_gran         =  {},
    .parm_gran          =  {},
    .ctcss_list         =  NULL,
    .dcs_list           =  NULL,
    .targetable_vfo     =  RIG_TARGETABLE_NONE,
    .transceive         =  0,

    .rx_range_list1 =
    {
        { MHz(108), MHz(174), D578_MODES, -1, -1, D578_VFO },
        { MHz(144), MHz(148), D578_MODES, -1, -1, D578_VFO },
        { MHz(222), MHz(225), D578_MODES, -1, -1, D578_VFO },
        { MHz(420), MHz(450), D578_MODES, -1, -1, D578_VFO },
        RIG_FRNG_END,
    },

    .tx_range_list1 =
    {
        { MHz(144), MHz(148), D578_MODES, W(1), W(55), D578_VFO },
        { MHz(222), MHz(225), D578_MODES, W(1), W(40), D578_VFO },
        { MHz(420), MHz(450), D578_MODES, W(1), W(25), D578_VFO },
        RIG_FRNG_END,
    },

    .rig_init           =  anytone_init,
    .rig_cleanup        =  anytone_cleanup,
    .rig_open           =  anytone_open,
    .rig_close          =  anytone_close,

    .get_vfo            =  anytone_get_vfo,
    .set_vfo            =  anytone_set_vfo,

    .get_ptt            =  anytone_get_ptt,
    .set_ptt            =  anytone_set_ptt,

    .set_freq           =  anytone_set_freq,
    .get_freq           =  anytone_get_freq,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

// ---------------------------------------------------------------------------
//    END OF FILE
// ---------------------------------------------------------------------------
