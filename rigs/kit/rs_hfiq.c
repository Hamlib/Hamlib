/*
 *  Hamlib rs-hfiq backend - main file
 *  Copyright (c) 2017 by Volker Schroer
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
/*
 *
 * For information about the controls see
 * https://sites.google.com/site/rshfiqtransceiver/home/technical-data/interface-commands
 *
 */
#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"

#define RSHFIQ_INIT_RETRY 5

#define RSHFIQ_LEVEL_ALL (RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_TEMP_METER)

int rshfiq_version_major, rshfiq_version_minor;

static int rshfiq_open(RIG *rig)
{
    int retval;
    int flag;
    char versionstr[20];
    char stopset[2];
    stopset[0] = '\r';
    stopset[1] = '\n';

    rig_debug(RIG_DEBUG_TRACE, "%s: Port = %s\n", __func__,
              rig->state.rigport.pathname);
    rig->state.rigport.timeout = 2000;
    rig->state.rigport.retry = 1;
    retval = serial_open(&rig->state.rigport);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = ser_get_dtr(&rig->state.rigport, &flag);

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: DTR: %d\n", __func__, flag);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Could not get DTR\n", __func__);
    }

    if (flag == 0)
    {
        flag = 1; // Set DTR
        retval = ser_set_dtr(&rig->state.rigport, flag);

        if (retval == RIG_OK)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: set DTR\n", __func__);
        }
    }

    //There is a delay between when the port is open and the RS-HFIQ can receive and respond.
    //Make a few attempts at getting the version string just in case the RS-HFIQ has to catch up first.
    retval = -RIG_ETIMEOUT;

    for (int init_retry_count = 0; (init_retry_count < RSHFIQ_INIT_RETRY)
            && (retval == -RIG_ETIMEOUT); init_retry_count++)
    {
        rig_flush(&rig->state.rigport);
        SNPRINTF(versionstr, sizeof(versionstr), "*w\r");
        rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s\n", __func__, versionstr);
        retval = write_block(&rig->state.rigport, (unsigned char *) versionstr,
                             strlen(versionstr));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = read_string(&rig->state.rigport, (unsigned char *) versionstr, 20,
                             stopset, 2, 0, 1);
    }

    if (retval <= 0)
    {
        return retval;
    }

    rig_flush(&rig->state.rigport);

    versionstr[retval] = 0;
    rig_debug(RIG_DEBUG_TRACE, "%s: Rigversion = %s\n", __func__, versionstr);

    if (!strstr(versionstr, "RS-HFIQ"))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: Invalid Rigversion: %s\n", __func__, versionstr);
        return -RIG_ECONF;
    }

    retval = sscanf(versionstr, "RS-HFIQ FW %d.%d", &rshfiq_version_major,
                    &rshfiq_version_minor);

    if (retval <= 0)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: Failed to parse RS-HFIQ firmware version string. Defaulting to 2.0.\n",
                  __func__);
        rshfiq_version_major = 2;
        rshfiq_version_minor = 0;
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "RS-HFIQ returned firmware version major=%d minor=%d\n", rshfiq_version_major,
              rshfiq_version_minor);

    if (rshfiq_version_major < 4)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: RS-HFIQ firmware major version is less than 4. RFPOWER_METER support will be unavailable.\n",
                  __func__);
    }

    return RIG_OK;
}

static int rshfiq_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char fstr[9];
    char cmdstr[15];
    int retval;

    SNPRINTF(fstr, sizeof(fstr), "%lu", (unsigned long int)(freq));
    rig_debug(RIG_DEBUG_TRACE, "%s called: %s %s\n", __func__,
              rig_strvfo(vfo), fstr);

    rig_flush(&rig->state.rigport);

    SNPRINTF(cmdstr, sizeof(cmdstr), "*f%lu\r", (unsigned long int)(freq));

    retval = write_block(&rig->state.rigport, (unsigned char *) cmdstr,
                         strlen(cmdstr));

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}
static int rshfiq_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char cmdstr[15];
    char stopset[2];
    int retval;
    rig_flush(&rig->state.rigport);

    stopset[0] = '\r';
    stopset[1] = '\n';

    SNPRINTF(cmdstr, sizeof(cmdstr), "*f?\r");

    rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s\n", __func__, cmdstr);

    retval = write_block(&rig->state.rigport, (unsigned char *) cmdstr,
                         strlen(cmdstr));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = read_string(&rig->state.rigport, (unsigned char *) cmdstr, 9,
                         stopset, 2, 0, 1);

    if (retval <= 0)
    {
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: reply = %s\n", __func__, cmdstr);

    cmdstr[retval] = 0;
    *freq = atoi(cmdstr);

    if (*freq == 0)   // fldigi interprets zero frequency as error
    {
        *freq = 1;    // so return 1 ( freq= 1Hz )
    }

    return RIG_OK;
}

static int rshfiq_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char cmdstr[5];
    int retval;

    cmdstr[0] = '*';
    cmdstr[1] = 'x';
    cmdstr[3] = '\r';
    cmdstr[4] = 0;

    if (ptt == RIG_PTT_ON)
    {
        cmdstr[2] = '1';
    }
    else
    {
        cmdstr[2] = '0';
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s\n", __func__, cmdstr);

    retval = write_block(&rig->state.rigport, (unsigned char *) cmdstr,
                         strlen(cmdstr));
    return retval;
}

static int rshfiq_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called. level type =%"PRIll"\n", __func__,
              level);

    char cmdstr[15];
    char stopset[2];
    int retval;

    if (!val)
    {
        return -RIG_EINVAL;
    }

    switch (level)
    {
    //Requires RS-HFIQ firmware version 4 or later
    case RIG_LEVEL_RFPOWER_METER:

        if (rshfiq_version_major <= 3)
        {
            return -RIG_ENAVAIL;
        }

        rig_flush(&rig->state.rigport);

        SNPRINTF(cmdstr, sizeof(cmdstr), "*L\r");

        rig_debug(RIG_DEBUG_TRACE, "RIG_LEVEL_RFPOWER_METER command=%s\n", cmdstr);

        retval = write_block(&rig->state.rigport, (unsigned char *) cmdstr,
                             strlen(cmdstr));

        if (retval != RIG_OK)
        {
            return retval;
        }

        stopset[0] = '\r';
        stopset[1] = '\n';

        retval = read_string(&rig->state.rigport, (unsigned char *) cmdstr, 9,
                             stopset, 2, 0, 1);

        rig_debug(RIG_DEBUG_TRACE, "RIG_LEVEL_RFPOWER_METER reply=%s\n", cmdstr);

        if (retval <= 0)
        {
            return retval;
        }

        cmdstr[retval] = 0;

        //Range is 0-110. Unit is percent
        val->i = atoi(cmdstr);
        val->f = (float)(val->i) / 100;

        rig_debug(RIG_DEBUG_TRACE, "RIG_LEVEL_RFPOWER_METER val=%f\n", val->f);

        return RIG_OK;
        break;

    case RIG_LEVEL_TEMP_METER:

        rig_flush(&rig->state.rigport);

        SNPRINTF(cmdstr, sizeof(cmdstr), "*T\r");

        rig_debug(RIG_DEBUG_TRACE, "RIG_LEVEL_TEMP_METER command=%s\n", cmdstr);

        retval = write_block(&rig->state.rigport, (unsigned char *) cmdstr,
                             strlen(cmdstr));

        if (retval != RIG_OK)
        {
            return retval;
        }

        stopset[0] = '\r';
        stopset[1] = '\n';

        retval = read_string(&rig->state.rigport, (unsigned char *) cmdstr, 9,
                             stopset, 2, 0, 1);

        rig_debug(RIG_DEBUG_TRACE, "RIG_LEVEL_TEMP_METER reply=%s\n", cmdstr);

        if (retval <= 0)
        {
            return retval;
        }

        cmdstr[retval] = 0;

        sscanf(cmdstr, "%d.", &val->i);
        val->f = val->i;

        rig_debug(RIG_DEBUG_TRACE, "RIG_LEVEL_TEMP_METER val=%g\n", val->f);

        return RIG_OK;
        break;
        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Unrecognized RIG_LEVEL_* enum: %"PRIll"\n",
                  __func__, level);
        return -RIG_EDOM;
        break;
    }
}

static int rshfiq_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    *mode = RIG_MODE_IQ;
    return RIG_OK;
}


const struct rig_caps rshfiq_caps =
{
    RIG_MODEL(RIG_MODEL_RSHFIQ),
    .model_name =     "RS-HFIQ",
    .mfg_name =       "HobbyPCB",
    .version =        "20220430.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_TRANSCEIVER,
    .ptt_type =       RIG_PTT_RIG,
    .port_type =      RIG_PORT_SERIAL,
    .dcd_type =       RIG_DCD_NONE,
    .serial_rate_min =  57600,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  1,
    .timeout =  1000,
    .retry =  3,

    .has_get_level = RSHFIQ_LEVEL_ALL,

    .rx_range_list1 =  { {.startf = kHz(3500), .endf = MHz(30), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .rx_range_list2 =  { {.startf = kHz(3500), .endf = MHz(30), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   { {.startf = kHz(3500), .endf = kHz(3800), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        {.startf = kHz(7000), .endf = kHz(7200), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        {.startf = kHz(10100), .endf = kHz(10150), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        {.startf = MHz(14), .endf = kHz(14350), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        {.startf = MHz(21), .endf = kHz(21450), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        {.startf = kHz(24890), .endf = kHz(24990), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        {.startf = MHz(28), .endf = kHz(29700), .modes = RIG_MODE_IQ, .low_power = 0, .high_power = 0, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tuning_steps =  { {RIG_MODE_IQ, Hz(1)}, RIG_TS_END, },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .rig_open =     rshfiq_open,
    .get_freq =     rshfiq_get_freq,
    .set_freq =     rshfiq_set_freq,
    .set_ptt  =     rshfiq_set_ptt,
    .get_level =     rshfiq_get_level,
    .get_mode =     rshfiq_get_mode,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

