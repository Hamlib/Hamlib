/*
 *  Hamlib Dummy backend - main file
 *  Copyright (c) 2001-2009 by Stephane Fillod
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

#include "hamlib/amplifier.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "amp_dummy.h"

#if 0
static const struct confparams dummy_amp_ext_levels[] =
{
    {
        AMP_LEVEL_SWR, "SWR", "SWR", "SWR",
        NULL, RIG_CONF_NUMERIC, { .n = { 1, 99, .1 } }
    }
};
#endif

#define AMP_LEVELS (AMP_LEVEL_SWR|AMP_LEVEL_PF|AMP_LEVEL_NH|AMP_LEVEL_PWR_INPUT|AMP_LEVEL_PWR_FWD|AMP_LEVEL_PWR_REFLECTED|AMP_LEVEL_PWR_PEAK|AMP_LEVEL_FAULT)

struct dummy_amp_priv_data
{
    freq_t freq;
    powerstat_t powerstat;
};


static int dummy_amp_init(AMP *amp)
{
    struct dummy_amp_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    amp->state.priv = (struct dummy_amp_priv_data *)
                      calloc(1, sizeof(struct dummy_amp_priv_data));

    if (!amp->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = amp->state.priv;

    amp->state.ampport.type.rig = RIG_PORT_NONE;

    priv->freq = 0;

    return RIG_OK;
}

static int dummy_amp_cleanup(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (amp->state.priv)
    {
        free(amp->state.priv);
    }

    amp->state.priv = NULL;

    return RIG_OK;
}

static int dummy_amp_open(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int dummy_amp_close(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int dummy_amp_reset(AMP *amp, amp_reset_t reset)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (reset)
    {
    case AMP_RESET_MEM:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Reset memory\n", __func__);
        break;

    case AMP_RESET_FAULT:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Reset fault\n", __func__);
        break;

    case AMP_RESET_AMP:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Reset amplifier\n", __func__);
        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Reset unknown=%d\n", __func__, reset);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
^ON1 turns amp on
^ON0 turns amp off
^OP0 amp in standby
^OP1 amp in operate

^PWF which gets forward power from amp. Reply would be ^PWFnnnn format

^PWK which gets peak forward power. Response is ^PWKnnnn

^PWR peak reflected power. Response is ^PWRnnnn

^SW gets SWR. Response is ^SWnnn. Example return of ^SW025 would be 2.5:1

Also a way to display faults (there are commands)
*/

static int dummy_amp_get_freq(AMP *amp, freq_t *freq)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       amp->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    *freq = priv->freq;
    return RIG_OK;
}

static int dummy_amp_set_freq(AMP *amp, freq_t freq)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       amp->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    priv->freq = freq;
    return RIG_OK;
}

static const char *dummy_amp_get_info(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return "Dummy amplifier";
}

static int dummy_amp_get_level(AMP *amp, setting_t level, value_t *val)
{
    static int flag = 1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    flag = !flag;

    // values toggle between two expected min/~max values for dev purposes
    switch (level)
    {
    case AMP_LEVEL_SWR:
        rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_SWR\n", __func__);
        val->f = flag == 0 ? 1.0 : 99.0;
        return RIG_OK;

    case AMP_LEVEL_PF:
        rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_PF\n", __func__);
        val->f = flag == 0 ? 0 :  2701.2;
        return RIG_OK;

    case AMP_LEVEL_NH:
        rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_UH\n", __func__);
        val->i = flag == 0 ? 0 : 8370;
        return RIG_OK;

    case AMP_LEVEL_PWR_INPUT:
        rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_PWRINPUT\n", __func__);
        val->i = flag == 0 ? 0 : 1499 ;
        return RIG_OK;

    case AMP_LEVEL_PWR_FWD:
        rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_PWRFWD\n", __func__);
        val->i = flag == 0 ? 0 : 1499 ;
        return RIG_OK;

    case AMP_LEVEL_PWR_REFLECTED:
        rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_PWRREFLECTED\n", __func__);
        val->i = flag == 0 ? 0 : 1499 ;
        return RIG_OK;

    case AMP_LEVEL_PWR_PEAK:
        rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_PWRPEAK\n", __func__);
        val->i = flag == 0 ? 0 : 1500 ;
        return RIG_OK;

    case AMP_LEVEL_FAULT:
        rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_FAULT\n", __func__);
        val->s = flag == 0 ? "No Fault" : "SWR too high"; // SWR too high for KPA1500
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s Unknown AMP_LEVEL=%s\n", __func__,
                  rig_strlevel(level));
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s flag=%d\n", __func__, flag);
    return -RIG_EINVAL;
}

static int dummy_amp_set_powerstat(AMP *amp, powerstat_t status)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       amp->state.priv;

    switch (status)
    {
    case RIG_POWER_OFF:
        rig_debug(RIG_DEBUG_VERBOSE, "%s called POWER_OFF\n", __func__);
        break;

    case RIG_POWER_ON:
        rig_debug(RIG_DEBUG_VERBOSE, "%s called POWER_ON\n", __func__);
        break;

    case RIG_POWER_STANDBY:
        rig_debug(RIG_DEBUG_VERBOSE, "%s called POWER_STANDBY\n", __func__);
        break;

    case RIG_POWER_OPERATE:
        rig_debug(RIG_DEBUG_VERBOSE, "%s called POWER_OPERATE\n", __func__);
        break;

    case RIG_POWER_UNKNOWN:
        rig_debug(RIG_DEBUG_VERBOSE, "%s called POWER_UNKNOWN\n", __func__);
        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s called invalid power status=%d\n",
                  __func__, status);
        return -RIG_EINVAL;
        break;
    }

    priv->powerstat = status;

    return RIG_OK;
}


static int dummy_amp_get_powerstat(AMP *amp, powerstat_t *status)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       amp->state.priv;

    *status = priv->powerstat;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

#if 0 // not implemented yet
static int dummy_amp_get_ext_level(AMP *amp, token_t token, value_t *val)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       amp->state.priv;
    const struct confparams *cfp;
    struct ext_list *elp;

    cfp = amp_ext_lookup_tok(amp, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case AMP_LEVEL_SWR:
        break;

    default:
        return -RIG_EINVAL;
    }

    elp = find_ext(curr->ext_levels, token);

    if (!elp)
    {
        return -RIG_EINTERNAL;
    }

    /* load value */
    *val = elp->val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              cfp->name);

    return RIG_OK;
}
#endif



/*
 * Dummy amplifier capabilities.
 */

const struct amp_caps dummy_amp_caps =
{
    AMP_MODEL(AMP_MODEL_DUMMY),
    .model_name =     "Dummy",
    .mfg_name =       "Hamlib",
    .version =        "20200112.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .amp_type =       AMP_TYPE_OTHER,
    .port_type =      RIG_PORT_NONE,

    .has_get_level = AMP_LEVELS,
    .has_set_level = RIG_LEVEL_SET(AMP_LEVELS),

    .priv =  NULL,  /* priv */

    .amp_init =     dummy_amp_init,
    .amp_cleanup =  dummy_amp_cleanup,
    .amp_open =     dummy_amp_open,
    .amp_close =    dummy_amp_close,

    .get_freq =     dummy_amp_get_freq,
    .set_freq =     dummy_amp_set_freq,
    .get_info =     dummy_amp_get_info,
    .get_level =    dummy_amp_get_level,

    .set_powerstat = dummy_amp_set_powerstat,
    .get_powerstat = dummy_amp_get_powerstat,

    .reset =    dummy_amp_reset,

};

DECLARE_INITAMP_BACKEND(dummy)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    amp_register(&dummy_amp_caps);
    amp_register(&netampctl_caps);

    return RIG_OK;
}
