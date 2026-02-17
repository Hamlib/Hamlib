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

#include <stdlib.h>

#include "hamlib/amplifier.h"
#include "register.h"

#include "idx_builtin.h"
#include "bandplan.h"
#include "dummy_common.h"
#include "amp_dummy.h"

#define DUMMY_AMP_INPUTS (RIG_ANT_1 | RIG_ANT_2)
#define DUMMY_AMP_ANTS (RIG_ANT_1 | RIG_ANT_2 | RIG_ANT_3 | RIG_ANT_4)

#define DUMMY_AMP_AMP_OPS (AMP_OP_TUNE | AMP_OP_BAND_UP | AMP_OP_BAND_DOWN | AMP_OP_L_NH_UP | AMP_OP_L_NH_DOWN | AMP_OP_C_PF_UP | AMP_OP_C_PF_DOWN)

#define DUMMY_AMP_GET_FUNCS (AMP_FUNC_TUNER)
#define DUMMY_AMP_SET_FUNCS (AMP_FUNC_TUNER)
#define DUMMY_AMP_GET_LEVELS (AMP_LEVEL_SWR | AMP_LEVEL_SWR_TUNER | AMP_LEVEL_PWR | AMP_LEVEL_PWR_FWD | AMP_LEVEL_PWR_REFLECTED | AMP_LEVEL_PWR_PEAK | AMP_LEVEL_FAULT | AMP_LEVEL_WARNING | AMP_LEVEL_VD_METER | AMP_LEVEL_ID_METER)
#define DUMMY_AMP_SET_LEVELS (AMP_LEVEL_PWR)
#define DUMMY_AMP_GET_PARMS (AMP_PARM_BACKLIGHT)
#define DUMMY_AMP_SET_PARMS (AMP_PARM_BACKLIGHT)

struct dummy_amp_priv_data
{
    freq_t freq;
    powerstat_t powerstat;
    ant_t input;
    ant_t antenna;

    amp_status_t status;

    setting_t funcs;
    value_t levels[RIG_SETTING_MAX];
    value_t parms[RIG_SETTING_MAX];

    struct ext_list *ext_funcs;
    struct ext_list *ext_levels;
    struct ext_list *ext_parms;

    char *magic_conf;
};

static const struct confparams dummy_ext_levels[] =
{
    {
        TOK_EL_AMP_MAGICLEVEL, "MGL", "Magic level", "Magic level, as an example",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001f } }
    },
    {
        TOK_EL_AMP_MAGICFUNC, "MGF", "Magic func", "Magic function, as an example",
        NULL, RIG_CONF_CHECKBUTTON
    },
    {
        TOK_EL_AMP_MAGICOP, "MGO", "Magic Op", "Magic Op, as an example",
        NULL, RIG_CONF_BUTTON
    },
    {
        TOK_EL_AMP_MAGICCOMBO, "MGC", "Magic combo", "Magic combo, as an example",
        "VALUE1", RIG_CONF_COMBO, { .c = { .combostr = { "VALUE1", "VALUE2", "NONE", NULL } } }
    },
    { RIG_CONF_END, NULL, }
};

static const struct confparams dummy_ext_funcs[] =
{
    {
        TOK_EL_AMP_MAGICEXTFUNC, "MGEF", "Magic ext func", "Magic ext function, as an example",
        NULL, RIG_CONF_CHECKBUTTON
    },
    { RIG_CONF_END, NULL, }
};

static const struct confparams dummy_ext_parms[] =
{
    {
        TOK_EP_AMP_MAGICPARM, "MGP", "Magic parm", "Magic parameter, as an example",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001f } }
    },
    { RIG_CONF_END, NULL, }
};

/* cfgparams are configuration item generally used by the backend's open() method */
static const struct confparams dummy_cfg_params[] =
{
    {
        TOK_CFG_AMP_MAGICCONF, "mcfg", "Magic conf", "Magic parameter, as an example",
        "AMPLIFIER", RIG_CONF_STRING, { }
    },
    { RIG_CONF_END, NULL, }
};

static int dummy_amp_init(AMP *amp)
{
    struct dummy_amp_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    AMPSTATE(amp)->priv = (struct dummy_amp_priv_data *)
                          calloc(1, sizeof(struct dummy_amp_priv_data));

    if (!AMPSTATE(amp)->priv)
    {
        return -RIG_ENOMEM;
    }

    priv = AMPSTATE(amp)->priv;

    priv->ext_funcs = alloc_init_ext(dummy_ext_funcs);

    if (!priv->ext_funcs)
    {
        free(AMPSTATE(amp)->priv);
        AMPSTATE(amp)->priv = NULL;
        return -RIG_ENOMEM;
    }

    priv->ext_levels = alloc_init_ext(dummy_ext_levels);

    if (!priv->ext_levels)
    {
        free(priv->ext_funcs);
        free(AMPSTATE(amp)->priv);
        AMPSTATE(amp)->priv = NULL;
        return -RIG_ENOMEM;
    }

    priv->ext_parms = alloc_init_ext(dummy_ext_parms);

    if (!priv->ext_parms)
    {
        free(priv->ext_levels);
        free(priv->ext_funcs);
        free(AMPSTATE(amp)->priv);
        AMPSTATE(amp)->priv = NULL;
        return -RIG_ENOMEM;
    }

    AMPPORT(amp)->type.rig = RIG_PORT_NONE;

    priv->freq = 0;
    priv->input = RIG_ANT_1;
    priv->antenna = RIG_ANT_1;

    priv->magic_conf = strdup("AMPLIFIER");

    return RIG_OK;
}

static int dummy_amp_cleanup(AMP *amp)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            AMPSTATE(amp)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (AMPSTATE(amp)->priv)
    {
        free(priv->ext_funcs);
        free(priv->ext_levels);
        free(priv->ext_parms);
        free(priv->magic_conf);
        free(AMPSTATE(amp)->priv);
    }

    AMPSTATE(amp)->priv = NULL;

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

static int dummy_set_conf(AMP *amp, hamlib_token_t token, const char *val)
{
    struct dummy_amp_priv_data *priv;

    priv = (struct dummy_amp_priv_data *) amp->state.priv;

    switch (token)
    {
    case TOK_CFG_AMP_MAGICCONF:
        if (val)
        {
            free(priv->magic_conf);
            priv->magic_conf = strdup(val);
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


static int dummy_get_conf2(AMP *amp, hamlib_token_t token, char *val, int val_len)
{
    struct dummy_amp_priv_data *priv;

    priv = (struct dummy_amp_priv_data *) amp->state.priv;

    switch (token)
    {
    case TOK_CFG_AMP_MAGICCONF:
        SNPRINTF(val, val_len, "%s", priv->magic_conf);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int dummy_get_conf(AMP *amp, hamlib_token_t token, char *val)
{
    return dummy_get_conf2(amp, token, val, 128);
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
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            AMPSTATE(amp)->priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    *freq = priv->freq;
    return RIG_OK;
}

static int dummy_amp_set_freq(AMP *amp, freq_t freq)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       AMPSTATE(amp)->priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    priv->freq = freq;
    return RIG_OK;
}

static int dummy_amp_get_input(AMP *amp, ant_t *input)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    *input = priv->input;
    return RIG_OK;
}

static int dummy_amp_set_input(AMP *amp, ant_t input)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       amp->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    priv->input = input;
    return RIG_OK;
}

static int dummy_amp_get_ant(AMP *amp, ant_t *ant)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    *ant = priv->antenna;
    return RIG_OK;
}

static int dummy_amp_set_ant(AMP *amp, ant_t ant)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       amp->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    priv->antenna = ant;
    return RIG_OK;
}

static const char *dummy_amp_get_info(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return "Dummy amplifier";
}

static int dummy_set_level(AMP *amp, setting_t level, value_t val)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    int idx;
    char lstr[32];

    idx = rig_setting2idx(level);

    if (idx >= RIG_SETTING_MAX)
    {
        return -RIG_EINVAL;
    }

    priv->levels[idx] = val;

    if (AMP_LEVEL_IS_FLOAT(level))
    {
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              amp_strlevel(level), lstr);

    return RIG_OK;
}

static int dummy_get_level(AMP *amp, setting_t level, value_t *val)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    static int flag = 1;
    int idx;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    flag = !flag;

    // values toggle between two expected min/~max values for dev purposes
    switch (level)
    {
        case AMP_LEVEL_SWR:
            rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_SWR\n", __func__);
            val->f = flag == 0 ? 1.0f : 99.0f;
            return RIG_OK;

        case AMP_LEVEL_PF:
            rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_PF\n", __func__);
            val->f = flag == 0 ? 0 :  2701.2f;
            return RIG_OK;

        case AMP_LEVEL_NH:
            rig_debug(RIG_DEBUG_VERBOSE, "%s AMP_LEVEL_NH\n", __func__);
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
            break;
    }

    idx = rig_setting2idx(level);

    if (idx >= RIG_SETTING_MAX)
    {
        return -RIG_EINVAL;
    }

    *val = priv->levels[idx];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
            amp_strlevel(level));

    rig_debug(RIG_DEBUG_VERBOSE, "%s flag=%d\n", __func__, flag);

    return RIG_OK;
}

static int dummy_set_ext_level(AMP *amp, hamlib_token_t token, value_t val)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    char lstr[64];
    const struct confparams *cfp;
    struct ext_list *elp;

    cfp = amp_ext_lookup_tok(amp, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EL_AMP_MAGICLEVEL:
    case TOK_EL_AMP_MAGICFUNC:
    case TOK_EL_AMP_MAGICOP:
    case TOK_EL_AMP_MAGICCOMBO:
        break;

    default:
        return -RIG_EINVAL;
    }

    switch (cfp->type)
    {
    case RIG_CONF_STRING:
        SNPRINTF(lstr, sizeof(lstr), "%s", val.s);
        break;

    case RIG_CONF_COMBO:
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
        break;

    case RIG_CONF_NUMERIC:
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
        break;

    case RIG_CONF_CHECKBUTTON:
        SNPRINTF(lstr, sizeof(lstr), "%s", val.i ? "ON" : "OFF");
        break;

    case RIG_CONF_BUTTON:
        lstr[0] = '\0';
        break;

    default:
        return -RIG_EINTERNAL;
    }

    elp = find_ext(priv->ext_levels, token);

    if (!elp)
    {
        return -RIG_EINTERNAL;
    }

    /* store value */
    elp->val = val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              cfp->name, lstr);

    return RIG_OK;
}

static int dummy_get_ext_level(AMP *amp, hamlib_token_t token, value_t *val)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
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
    case TOK_EL_AMP_MAGICLEVEL:
    case TOK_EL_AMP_MAGICFUNC:
    case TOK_EL_AMP_MAGICOP:
    case TOK_EL_AMP_MAGICCOMBO:
        break;

    default:
        return -RIG_EINVAL;
    }

    elp = find_ext(priv->ext_levels, token);

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


static int dummy_set_ext_func(AMP *amp, hamlib_token_t token, int status)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
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
    case TOK_EL_AMP_MAGICEXTFUNC:
        break;

    default:
        return -RIG_EINVAL;
    }

    switch (cfp->type)
    {
    case RIG_CONF_CHECKBUTTON:
        break;

    case RIG_CONF_BUTTON:
        break;

    default:
        return -RIG_EINTERNAL;
    }

    elp = find_ext(priv->ext_funcs, token);

    if (!elp)
    {
        return -RIG_EINTERNAL;
    }

    /* store value */
    elp->val.i = status;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %d\n", __func__,
              cfp->name, status);

    return RIG_OK;
}


static int dummy_get_ext_func(AMP *amp, hamlib_token_t token, int *status)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
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
    case TOK_EL_AMP_MAGICEXTFUNC:
        break;

    default:
        return -RIG_EINVAL;
    }

    elp = find_ext(priv->ext_funcs, token);

    if (!elp)
    {
        return -RIG_EINTERNAL;
    }

    /* load value */
    *status = elp->val.i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              cfp->name);

    return RIG_OK;
}


static int dummy_set_parm(AMP *amp, setting_t parm, value_t val)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    int idx;
    char pstr[32];

    idx = rig_setting2idx(parm);

    if (idx >= RIG_SETTING_MAX)
    {
        return -RIG_EINVAL;
    }

    if (AMP_PARM_IS_FLOAT(parm))
    {
        SNPRINTF(pstr, sizeof(pstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(pstr, sizeof(pstr), "%d", val.i);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              amp_strparm(parm), pstr);

    priv->parms[idx] = val;

    return RIG_OK;
}


static int dummy_get_parm(AMP *amp, setting_t parm, value_t *val)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    int idx;

    idx = rig_setting2idx(parm);

    if (idx >= RIG_SETTING_MAX)
    {
        return -RIG_EINVAL;
    }

    *val = priv->parms[idx];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s\n", __func__,
              amp_strparm(parm));

    return RIG_OK;
}

static int dummy_set_ext_parm(AMP *amp, hamlib_token_t token, value_t val)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    char lstr[64];
    const struct confparams *cfp;
    struct ext_list *epp;

    cfp = amp_ext_lookup_tok(amp, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EP_AMP_MAGICPARM:
        break;

    default:
        return -RIG_EINVAL;
    }

    switch (cfp->type)
    {
    case RIG_CONF_STRING:
        SNPRINTF(lstr, sizeof(lstr), "%s", val.s);
        break;

    case RIG_CONF_COMBO:
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
        break;

    case RIG_CONF_NUMERIC:
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
        break;

    case RIG_CONF_CHECKBUTTON:
        SNPRINTF(lstr, sizeof(lstr), "%s", val.i ? "ON" : "OFF");
        break;

    case RIG_CONF_BUTTON:
        lstr[0] = '\0';
        break;

    default:
        return -RIG_EINTERNAL;
    }

    epp = find_ext(priv->ext_parms, token);

    if (!epp)
    {
        return -RIG_EINTERNAL;
    }

    /* store value */
    epp->val = val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              cfp->name, lstr);

    return RIG_OK;
}

static int dummy_get_ext_parm(AMP *amp, hamlib_token_t token, value_t *val)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;
    const struct confparams *cfp;
    struct ext_list *epp;

    cfp = amp_ext_lookup_tok(amp, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EP_AMP_MAGICPARM:
        break;

    default:
        return -RIG_EINVAL;
    }

    epp = find_ext(priv->ext_parms, token);

    if (!epp)
    {
        return -RIG_EINTERNAL;
    }

    /* load value */
    *val = epp->val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              cfp->name);

    return RIG_OK;
}


static int dummy_amp_get_status(AMP *amp, amp_status_t *status)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;

    // TODO: simulate more status bits
    priv->status ^= AMP_STATUS_PTT;

    *status = priv->status;

    return RIG_OK;
}


static int dummy_amp_set_powerstat(AMP *amp, powerstat_t status)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       AMPSTATE(amp)->priv;

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
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            AMPSTATE(amp)->priv;

    *status = priv->powerstat;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int dummy_set_func(AMP *amp, setting_t func, int status)
{
    struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
                                       AMPSTATE(amp)->priv;
    const struct confparams *cfp;
    struct ext_list *elp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %d\n", __func__,
              amp_strfunc(func), status);

    if (status)
    {
        priv->funcs |=  func;
    }
    else
    {
        priv->funcs &= ~func;
    }

    return RIG_OK;
}

static int dummy_get_func(AMP *amp, setting_t func, int *status)
{
    const struct dummy_amp_priv_data *priv = (struct dummy_amp_priv_data *)
            amp->state.priv;

    *status = (priv->funcs & func) ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              amp_strfunc(func));

    return RIG_OK;
}

static int dummy_amp_op(AMP *amp, amp_op_t op)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              amp_strampop(op));

    return RIG_OK;
}

/**
 * Dummy amplifier capabilities.
 */
const struct amp_caps dummy_amp_caps =
{
    AMP_MODEL(AMP_MODEL_DUMMY),
    .model_name =     "Dummy",
    .mfg_name =       "Hamlib",
    .version =        "20240317.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .amp_type =       AMP_TYPE_OTHER,
    .port_type =      RIG_PORT_NONE,

    .priv =  NULL,  /* priv */

    .has_get_func = DUMMY_AMP_GET_FUNCS,
    .has_set_func = DUMMY_AMP_SET_FUNCS,
    .has_get_level = DUMMY_AMP_GET_LEVELS,
    .has_set_level = DUMMY_AMP_SET_LEVELS,
    .has_get_parm = DUMMY_AMP_GET_PARMS,
    .has_set_parm = DUMMY_AMP_SET_PARMS,

    .level_gran = {
        [AMP_LVL_PWR] = { .min = { .i = 1 }, .max = { .i = 1500 }, .step = { .i = 1 } }
    },

    .extlevels =    dummy_ext_levels,
    .extfuncs =     dummy_ext_funcs,
    .extparms =     dummy_ext_parms,
    .cfgparams =    dummy_cfg_params,

    .amp_ops = DUMMY_AMP_AMP_OPS,
    .has_status = AMP_STATUS_PTT,

    .amp_init =     dummy_amp_init,
    .amp_cleanup =  dummy_amp_cleanup,
    .amp_open =     dummy_amp_open,
    .amp_close =    dummy_amp_close,

    .set_conf =     dummy_set_conf,
    .get_conf =     dummy_get_conf,
    .get_conf2 =    dummy_get_conf2,

    .get_freq =     dummy_amp_get_freq,
    .set_freq =     dummy_amp_set_freq,
    .get_input =    dummy_amp_get_input,
    .set_input =    dummy_amp_set_input,
    .get_ant =      dummy_amp_get_ant,
    .set_ant =      dummy_amp_set_ant,

    .set_func = dummy_set_func,
    .get_func = dummy_get_func,
    .set_level = dummy_set_level,
    .get_level = dummy_get_level,
    .set_parm = dummy_set_parm,
    .get_parm = dummy_get_parm,

    .set_ext_func = dummy_set_ext_func,
    .get_ext_func = dummy_get_ext_func,
    .set_ext_level = dummy_set_ext_level,
    .get_ext_level = dummy_get_ext_level,
    .set_ext_parm = dummy_set_ext_parm,
    .get_ext_parm = dummy_get_ext_parm,

    .get_info = dummy_amp_get_info,
    .get_status = dummy_amp_get_status,

    .set_powerstat = dummy_amp_set_powerstat,
    .get_powerstat = dummy_amp_get_powerstat,

    .amp_op =   dummy_amp_op,
    .reset =    dummy_amp_reset,

    .range_list1 = {
        FRQ_RNG_HF(1, RIG_MODE_ALL, W(1), W(1500), DUMMY_AMP_INPUTS, DUMMY_AMP_ANTS),
        FRQ_RNG_6m(1, RIG_MODE_ALL, W(1), W(1500), DUMMY_AMP_INPUTS, DUMMY_AMP_ANTS),
        FRQ_RNG_60m(1, RIG_MODE_ALL, W(1), W(1500), DUMMY_AMP_INPUTS, DUMMY_AMP_ANTS),
        RIG_FRNG_END,
    },
    .range_list2 = {
        FRQ_RNG_HF(2, RIG_MODE_ALL, W(1), W(1500), DUMMY_AMP_INPUTS, DUMMY_AMP_ANTS),
        FRQ_RNG_6m(2, RIG_MODE_ALL, W(1), W(1500), DUMMY_AMP_INPUTS, DUMMY_AMP_ANTS),
        FRQ_RNG_60m(2, RIG_MODE_ALL, W(1), W(1500), DUMMY_AMP_INPUTS, DUMMY_AMP_ANTS),
        RIG_FRNG_END,
    },
};

DECLARE_INITAMP_BACKEND(dummy)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    amp_register(&dummy_amp_caps);
    amp_register(&netampctl_caps);

    return RIG_OK;
}
