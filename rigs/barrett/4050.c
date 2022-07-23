/*
 *  Hamlib Barrett 4050 backend - main file
 *  Copyright (c) 2017-2022 by Michael Black W9MDB
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

#include <stdio.h>

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "token.h"
#include "register.h"

#include "barrett.h"

#define MAXCMDLEN 32

#define BARRETT4050_VFOS (RIG_VFO_A|RIG_VFO_MEM)

#define BARRETT4050_MODES (RIG_MODE_AM | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_SSB)

#define BARRETT4050_LEVELS (RIG_LEVEL_NONE)

#define BARRETT4050_FUNCTIONS (RIG_FUNC_TUNER)

extern int barret950_get_freq(RIG *rig, vfo_t vfo, freq_t freq);

/*
 * barrett4050_get_level
 */
static int barrett4050_get_level(RIG *rig, vfo_t vfo, setting_t level,
                                 value_t *val)
{
    return -RIG_ENIMPL;
}

/*
 * barrett4050_get_info
 */
static const char *barrett4050_get_info(RIG *rig)
{
    char *response = NULL;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = barrett_transaction(rig, "IV", 0, &response);

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: result=%s\n", __func__, response);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "Software Version %s\n", response);
    }

    return response;
}

static int barrett4050_open(RIG *rig)
{
    int retval;
    char *response;
    struct barrett_priv_data *priv = rig->state.priv;
    ENTERFUNC;
    barrett4050_get_info(rig);
    retval = barrett_transaction(rig, "IDC9999", 0, &response);

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: channel 9999 info=%s\n", __func__, response);
        priv->channel_base = 9990;
    }

    retval = barrett_transaction(rig, "XC9999", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
    }

    retval = barrett_transaction(rig, "IC", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
    }

    RETURNFUNC(RIG_OK);
}

const struct rig_caps barrett4050_caps =
{
    RIG_MODEL(RIG_MODEL_BARRETT_4050),
    .model_name =       "4050",
    .mfg_name =         "Barrett",
    .version =          BACKEND_VER ".0f",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_XONXOFF,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          500,
    .retry =            3,

    .has_get_func =     BARRETT4050_FUNCTIONS,
    .has_set_func =     BARRETT4050_FUNCTIONS,
    .has_get_level =    BARRETT4050_LEVELS,
    .has_set_level =    RIG_LEVEL_NONE,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .transceive =       RIG_TRN_RIG,
    .rx_range_list1 = {{
            .startf = kHz(1600), .endf = MHz(30), .modes = BARRETT4050_MODES,
            .low_power = -1, .high_power = -1, BARRETT4050_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 = {RIG_FRNG_END,},
    .tx_range_list1 = {RIG_FRNG_END,},
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {BARRETT4050_MODES, 1}, {BARRETT4050_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .filters = {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_AM, kHz(8)},
        {RIG_MODE_AM, kHz(2.4)},
        RIG_FLT_END,
    },
    .priv = NULL,

    .rig_init =     barrett_init,
    .rig_cleanup =  barrett_cleanup,
    .rig_open = barrett4050_open,

    // Barrett said eeprom is good for 1M writes so channelized should be OK for a long time
    // TC command was not implemented as of 2022-01-12 which would be better
    .set_freq = barrett950_set_freq,
    .get_freq = barrett_get_freq,
    .set_mode = barrett_set_mode,
    .get_mode = barrett_get_mode,

    .get_level =    barrett4050_get_level,

    .get_info =     barrett4050_get_info,
    .set_ptt =      barrett_set_ptt,
    .get_ptt =      barrett_get_ptt,
    .set_split_freq =   barrett_set_split_freq,
    .set_split_vfo =    barrett_set_split_vfo,
    .get_split_vfo =    barrett_get_split_vfo,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
