/*
 * Hamlib backend library for the Elecraft KPA1500 command set.
 *
 * kpa1500.c - (C) Michael Black W9MDB 2019
 *
 * This shared library provides an API for communicating
 * to an Elecraft KPA1500 amplifier.
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

#include "register.h"

#include "kpa.h"


struct kpa_priv_data *kpa1500_priv;
/*
 * API local implementation
 *
 */

/*
 * Private helper function prototypes
 */

//static int kpa1500_send_priv_cmd(AMP *amp, const char *cmd);
//static int kpa1500_flush_buffer(AMP *amp);

/* *************************************
 *
 * Separate model capabilities
 *
 * *************************************
 */


/*
 * Elecraft KPA1500
 */

const struct amp_caps kpa1500_amp_caps =
{
    AMP_MODEL(AMP_MODEL_ELECRAFT_KPA1500),
    .model_name =   "KPA1500",
    .mfg_name =     "Elecraft",
    .version =      "20220710.0",
    .copyright =    "LGPL",
    .status =     RIG_STATUS_ALPHA,
    .amp_type =     AMP_TYPE_OTHER,
    .port_type =    RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  230400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =    0,
    .post_write_delay = 0,
    .timeout =      2000,
    .retry =      2,
    .has_get_level = AMP_LEVEL_SWR | AMP_LEVEL_NH | AMP_LEVEL_PF | AMP_LEVEL_PWR_INPUT | AMP_LEVEL_PWR_FWD | AMP_LEVEL_PWR_REFLECTED | AMP_LEVEL_FAULT,
    .has_set_level = 0,

    .amp_open = NULL,
    .amp_init = kpa_init,
    .amp_close = kpa_close,
    .reset = kpa_reset,
    .get_info = kpa_get_info,
    .get_powerstat = kpa_get_powerstat,
    .set_powerstat = kpa_set_powerstat,
    .set_freq = kpa_set_freq,
    .get_freq = kpa_get_freq,
    .get_level = kpa_get_level,
};


/* ************************************
 *
 * API functions
 *
 * ************************************
 */

/*
 *
 */

#if 0 // not implemented yet
/*
 * Send command string to amplifier
 */

static int kpa1500_send_priv_cmd(AMP *amp, const char *cmdstr)
{
    struct amp_state *rs;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp)
    {
        return -RIG_EINVAL;
    }

    rs = &amp->state;
    err = write_block(&rs->ampport, cmdstr, strlen(cmdstr));

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}
#endif

/*
 * Initialize backend
 */

DECLARE_INITAMP_BACKEND(kpa1500)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    amp_register(&kpa1500_amp_caps);

    return RIG_OK;
}
