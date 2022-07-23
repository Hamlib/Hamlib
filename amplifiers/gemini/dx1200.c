/*
 * Hamlib backend library for the Gemini DX1200 and HF-1K command set.
 *
 * dx1200.c - (C) Michael Black W9MDB 2022
 *
 * This shared library provides an API for communicating
 * to Gemini amplifiers.
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

#include "gemini.h"


struct gemini_priv_data *gemini_priv;
/*
 * API local implementation
 *
 */

/*
 * Private helper function prototypes
 */

//static int gemini_send_priv_cmd(AMP *amp, const char *cmd);
//static int gemini_flush_buffer(AMP *amp);

/* *************************************
 *
 * Separate model capabilities
 *
 * *************************************
 */


/*
 * Gemini
 */

const struct amp_caps gemini_amp_caps =
{
    AMP_MODEL(AMP_MODEL_GEMINI_DX1200),
    .model_name =   "DX1200/HF-1K",
    .mfg_name =     "Gemini",
    .version =      "20220710.0",
    .copyright =    "LGPL",
    .status =     RIG_STATUS_ALPHA,
    .amp_type =     AMP_TYPE_OTHER,
    .port_type =    RIG_PORT_NETWORK,
    .write_delay =    0,
    .post_write_delay = 0,
    .timeout =      2000,
    .retry =      2,
    .has_get_level = AMP_LEVEL_SWR | AMP_LEVEL_PWR_FWD | AMP_LEVEL_PWR_PEAK | AMP_LEVEL_FAULT | AMP_LEVEL_PWR,
    .has_set_level = AMP_LEVEL_PWR,

    .amp_open = NULL,
    .amp_init = gemini_init,
    .amp_close = gemini_close,
    .reset = gemini_reset,
    .get_info = gemini_get_info,
    .get_powerstat = gemini_get_powerstat,
    .set_powerstat = gemini_set_powerstat,
    .get_freq = gemini_get_freq,
    .set_freq = gemini_set_freq,
    .get_level = gemini_get_level,
    .set_level = gemini_set_level,
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

static int gemini_send_priv_cmd(AMP *amp, const char *cmdstr)
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

DECLARE_INITAMP_BACKEND(gemini)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    amp_register(&gemini_amp_caps);

    return RIG_OK;
}
