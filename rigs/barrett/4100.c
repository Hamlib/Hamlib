/*
 *  Hamlib Barrett 4100 backend - main file
 *  Derived from 4050 backend
 *  Copyright (c) 2017-2024 by Michael Black W9MDB
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

#include <stdio.h>

#include <hamlib/rig.h>
#include "misc.h"

#include "barrett.h"

#define MAXCMDLEN 32

//#define BARRETT4100 VFOS (RIG_VFO_A|RIG_VFO_MEM) // VFO_MEM eventually?
#define BARRETT4100 VFOS (RIG_VFO_A)

#define BARRETT4100_MODES (RIG_MODE_CW | RIG_MODE_SSB)

// Levels eventually
//#define BARRETT4100_LEVELS (RIG_LEVEL_AGC|RIG_LEVEL_STRENGTH)

// Functions eventually
//#define BARRETT4100_FUNCTIONS (RIG_FUNC_TUNER)

extern int barret950_get_freq(RIG *rig, vfo_t vfo, freq_t freq);

/*
 * barrett4100_get_info
 */
static const char *barrett4100_get_info(RIG *rig)
{
    char *response = NULL;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = barrett_transaction(rig, "M:MIB GM", 0, &response);

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: error=\"%s\", result=\"%s\"\n", __func__,
                  strerror(retval), response);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "MIB GM: %s\n", response);
    }

    retval = barrett_transaction(rig, "M:FF GM", 0, &response);

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: error=\"%s\", result=\"%s\"\n", __func__,
                  strerror(retval), response);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "FF GM: %s\n", response);
    }

    retval = barrett_transaction(rig, "M:FF BWA", 0, &response);

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: error=\"%s\", result=\"%s\"\n", __func__,
                  strerror(retval), response);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "FF BWA: %s\n", response);
    }

    retval = barrett_transaction(rig, "M:FF GRFA", 0, &response);

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: error=\"%s\", result=\"%s\"\n", __func__,
                  strerror(retval), response);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "M:FF GRFA: %s\n", response);
    }

    return response;
}

static int barrett4100_open(RIG *rig)
{
    int retval;
    char *response;
    ENTERFUNC;
    retval = barrett_transaction2(rig, "M:REMOTE SENTER2", 0, &response);

    if (retval != RIG_OK || response[0] != 's')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: REMOTE SENTER2 error: got %s\n", __func__,
                  response);
    }

    barrett4100_get_info(rig);
    RETURNFUNC(RIG_OK);
}

static int barrett4100_close(RIG *rig)
{
    char *response;
    int retval = barrett_transaction2(rig, "M:REMOTE SENTER0", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
    }

    return rig_close(rig);
}

int barrett4100_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char *response;
    int retval = barrett_transaction2(rig, "M:FF SRF%.0f GRF", freq, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
        freq_t freq2 = 0;
        int n = sscanf(response, "s gRF%lf", &freq2);

        if (n == 1)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: freq set to %.0f\n", __func__, freq2);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse s gRF\n", __func__);
        }
    }
    retval = barrett_transaction2(rig, "M:FF STF%.0f GTF", freq, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
        freq_t freq2 = 0;
        int n = sscanf(response, "s gTF%lf", &freq2);

        if (n == 1)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: freq set to %.0f\n", __func__, freq2);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse s gTF\n", __func__);
        }
    }

    return retval;
}

int barrett4100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char *response;
    int retval = barrett_transaction2(rig, "M:FF GRF", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
    }
    else
    {
        sscanf(response, "gRFA1,%*d,%lf,%*d", freq);
    }

    return retval;
}

int barrett4100_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char *response;
    int retval = barrett_transaction2(rig, "M:FF SRPTT%d GRPTT", ptt, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d); response=%s\n", __func__, __LINE__,
              response);
    return retval;
}
int barrett4100_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char *response;
    int retval = barrett_transaction2(rig, "M:FF GRPTT", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): result=%s\n", __func__, __LINE__, response);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d); response=%s\n", __func__, __LINE__,
              response);
    return retval;
}

struct rig_caps barrett4100_caps =
{
    RIG_MODEL(RIG_MODEL_BARRETT_4100),
    .model_name =       "4100",
    .mfg_name =         "Barrett",
    .version =          BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_BETA,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_NETWORK,
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

//    .has_get_func =     BARRETT4100_FUNCTIONS,
//    .has_set_func =     BARRETT4100_FUNCTIONS,
//    .has_get_level =    BARRETT4100_LEVELS,
    .has_set_level =    RIG_LEVEL_AGC,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .transceive =       RIG_TRN_RIG,
    .rx_range_list1 = {{
            .startf = kHz(10), .endf = MHz(30), .modes = BARRETT4100_MODES,
            .low_power = -1, .high_power = -1, BARRETT4100_MODES, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 = {RIG_FRNG_END,},
    .tx_range_list1 = {RIG_FRNG_END,},
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {BARRETT4100_MODES, 1}, {BARRETT4100_MODES, RIG_TS_ANY}, RIG_TS_END, },
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
    .rig_open = barrett4100_open,
    .rig_close = barrett4100_close,

    .set_freq = barrett4100_set_freq,
    .get_freq = barrett4100_get_freq,
//    .set_mode = barrett_set_mode,
//    .get_mode = barrett_get_mode,

//    .set_level =    barrett_set_level,
//    .get_level =    barrett_get_level,

    .get_info =     barrett4100_get_info,
    .set_ptt =      barrett4100_set_ptt,
    .get_ptt =      barrett4100_get_ptt,
//    .set_split_freq =   barrett_set_split_freq,
//    .set_split_vfo =    barrett_set_split_vfo,
//    .get_split_vfo =    barrett_get_split_vfo,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
