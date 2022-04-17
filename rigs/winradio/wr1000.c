/*
 *  Hamlib WiNRADiO backend - WR1000 description
 *  Copyright (c) 2001-2004 by Stephane Fillod
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

#include <stdlib.h>

#include <hamlib/rig.h>
#include "winradio.h"


#ifdef WINRADIO_IOCTL

/*
 * Winradio rigs capabilities.
 */

#define WR1000_FUNC  0
#define WR1000_SET_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AF)
#define WR1000_LEVEL (WR1000_SET_LEVEL | RIG_LEVEL_STRENGTH)

#define WR1000_MODES (RIG_MODE_AM | \
                     RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_FM)

const struct rig_caps wr1000_caps =
{
    RIG_MODEL(RIG_MODEL_WR1000),
    .model_name =     "WR-1000",
    .mfg_name =       "Winradio",
    .version =        BACKEND_VER ".0",
    .copyright =   "LGPL",
    .status =         RIG_STATUS_ALPHA,
    .rig_type =       RIG_TYPE_PCRECEIVER,
    .port_type =      RIG_PORT_DEVICE,
    .targetable_vfo =      0,
    .ptt_type =       RIG_PTT_NONE,
    .dcd_type =       RIG_DCD_NONE,
    .has_get_func =   WR1000_FUNC,
    .has_set_func =   WR1000_FUNC,
    .has_get_level =  WR1000_LEVEL,
    .has_set_level =  WR1000_SET_LEVEL,
    .has_get_parm =    RIG_PARM_NONE,
    .has_set_parm =    RIG_PARM_NONE,
    .ctcss_list =      NULL,
    .dcs_list =        NULL,
    .chan_list =   { RIG_CHAN_END, },
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { 20, RIG_DBLST_END, },
    .rx_range_list1 =  { {
            .startf = kHz(500), .endf = MHz(1300), .modes = WR1000_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        {
            .startf = MHz(30), .endf = MHz(1300), .modes = RIG_MODE_WFM,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 = { {
            .startf = kHz(500), .endf = MHz(824), .modes = WR1000_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        {
            .startf = MHz(30), .endf = MHz(824), .modes = RIG_MODE_WFM,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        {
            .startf = MHz(849), .endf = MHz(869),
            .modes = WR1000_MODES | RIG_MODE_WFM,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        {
            .startf = MHz(894), .endf = MHz(1300),
            .modes = WR1000_MODES | RIG_MODE_WFM,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        RIG_FRNG_END,
    },

    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  { {WR1000_MODES | RIG_MODE_WFM, 100},
        RIG_TS_END,
    },

    .filters =       { {RIG_MODE_SSB | RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(17)},
        {RIG_MODE_WFM, kHz(230)},
        RIG_FLT_END,
    },

    .priv =           NULL,   /* priv */

    .rig_init =     wr_rig_init,

    .set_freq =     wr_set_freq,
    .get_freq =     wr_get_freq,
    .set_mode =     wr_set_mode,
    .get_mode =     wr_get_mode,

    .set_powerstat =   wr_set_powerstat,
    .get_powerstat =   wr_get_powerstat,
    .set_level =     wr_set_level,
    .get_level =     wr_get_level,
    .set_func =      NULL,
    .get_func =      NULL,

    .get_info =      wr_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

#endif  /* WINRADIO_IOCTL */
