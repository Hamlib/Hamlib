/*
 *  Hamlib miniVNA backend - main file
 *  Copyright (c) 2001-2008 by Stephane Fillod
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"


#define DDS_RATIO 10.73741824

static int miniVNA_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char fstr[20];
    char cmdstr[40];
    int retval;

    sprintf_freq(fstr, sizeof(fstr), freq);
    rig_debug(RIG_DEBUG_TRACE, "%s called: %s %s\n", __func__,
              rig_strvfo(vfo), fstr);

    rig_flush(&rig->state.rigport);

    sprintf(cmdstr, "0\r%lu\r1\r0\r", (unsigned long int)(freq * DDS_RATIO));

    retval = write_block(&rig->state.rigport, cmdstr, strlen(cmdstr));

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

const struct rig_caps miniVNA_caps =
{
    RIG_MODEL(RIG_MODEL_MINIVNA),
    .model_name =     "miniVNA",
    .mfg_name =       "mRS",
    .version =        "20190817.0",
    .copyright =   "LGPL",
    .status =         RIG_STATUS_ALPHA,
    .rig_type =       RIG_TYPE_TUNER,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =  115200,
    .serial_rate_max =  115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  1,
    .timeout =  1000,
    .retry =  3,

    .rx_range_list1 =  { {.startf = kHz(100), .endf = MHz(180), .modes = RIG_MODE_NONE, .low_power = -1, .high_power = -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   { {.startf = kHz(100), .endf = MHz(180), .modes = RIG_MODE_NONE, .low_power = -1, .high_power = -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tuning_steps =  { {RIG_MODE_NONE, 1}, RIG_TS_END, },

    .set_freq =     miniVNA_set_freq,
};

