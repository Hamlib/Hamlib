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
#include <string.h>  /* String function definitions */
#include <math.h>
#include <sys/time.h>

#include "hamlib/rotator.h"
#include "dummy_common.h"
#include "register.h"
#include "idx_builtin.h"

#include "rot_dummy.h"

#define DUMMY_ROT_FUNC 0
#define DUMMY_ROT_LEVEL ROT_LEVEL_SPEED
#define DUMMY_ROT_PARM 0

#define DUMMY_ROT_STATUS (ROT_STATUS_MOVING | ROT_STATUS_MOVING_AZ | ROT_STATUS_MOVING_LEFT | ROT_STATUS_MOVING_RIGHT | \
        ROT_STATUS_MOVING_EL | ROT_STATUS_MOVING_UP | ROT_STATUS_MOVING_DOWN | \
        ROT_STATUS_LIMIT_UP | ROT_STATUS_LIMIT_DOWN | ROT_STATUS_LIMIT_LEFT | ROT_STATUS_LIMIT_RIGHT)

static int simulating = 0;  // do we need rotator emulation for debug?

struct dummy_rot_priv_data
{
    azimuth_t az;
    elevation_t el;

    struct timeval tv;  /* time last az/el update */
    azimuth_t target_az;
    elevation_t target_el;
    rot_status_t status;

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
        TOK_EL_ROT_MAGICLEVEL, "MGL", "Magic level", "Magic level, as an example",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
    },
    {
        TOK_EL_ROT_MAGICFUNC, "MGF", "Magic func", "Magic function, as an example",
        NULL, RIG_CONF_CHECKBUTTON
    },
    {
        TOK_EL_ROT_MAGICOP, "MGO", "Magic Op", "Magic Op, as an example",
        NULL, RIG_CONF_BUTTON
    },
    {
        TOK_EL_ROT_MAGICCOMBO, "MGC", "Magic combo", "Magic combo, as an example",
        "VALUE1", RIG_CONF_COMBO, { .c = { .combostr = { "VALUE1", "VALUE2", "NONE", NULL } } }
    },
    { RIG_CONF_END, NULL, }
};

static const struct confparams dummy_ext_funcs[] =
{
    {
        TOK_EL_ROT_MAGICEXTFUNC, "MGEF", "Magic ext func", "Magic ext function, as an example",
        NULL, RIG_CONF_CHECKBUTTON
    },
    { RIG_CONF_END, NULL, }
};

static const struct confparams dummy_ext_parms[] =
{
    {
        TOK_EP_ROT_MAGICPARM, "MGP", "Magic parm", "Magic parameter, as an example",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
    },
    { RIG_CONF_END, NULL, }
};

/* cfgparams are configuration item generally used by the backend's open() method */
static const struct confparams dummy_cfg_params[] =
{
    {
        TOK_CFG_ROT_MAGICCONF, "mcfg", "Magic conf", "Magic parameter, as an example",
        "ROTATOR", RIG_CONF_STRING, { }
    },
    { RIG_CONF_END, NULL, }
};


static int dummy_rot_init(ROT *rot)
{
    struct dummy_rot_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot->state.priv = (struct dummy_rot_priv_data *)
                      calloc(1, sizeof(struct dummy_rot_priv_data));

    if (!rot->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rot->state.priv;

    priv->ext_funcs = alloc_init_ext(dummy_ext_funcs);

    if (!priv->ext_funcs)
    {
        return -RIG_ENOMEM;
    }

    priv->ext_levels = alloc_init_ext(dummy_ext_levels);

    if (!priv->ext_levels)
    {
        return -RIG_ENOMEM;
    }

    priv->ext_parms = alloc_init_ext(dummy_ext_parms);

    if (!priv->ext_parms)
    {
        return -RIG_ENOMEM;
    }

    rot->state.rotport.type.rig = RIG_PORT_NONE;

    priv->az = priv->el = 0;

    priv->target_az = priv->target_el = 0;

    priv->magic_conf = strdup("ROTATOR");

    return RIG_OK;
}

static int dummy_rot_cleanup(ROT *rot)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    free(priv->ext_funcs);
    free(priv->ext_levels);
    free(priv->ext_parms);
    free(priv->magic_conf);

    if (rot->state.priv)
    {
        free(rot->state.priv);
    }

    rot->state.priv = NULL;

    return RIG_OK;
}

static int dummy_rot_open(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (rot->caps->rot_model == ROT_MODEL_DUMMY)
    {
        simulating = 1;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: dummy rotator so simulating speed\n",
                  __func__);
    }

    return RIG_OK;
}

static int dummy_rot_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int dummy_set_conf(ROT *rot, token_t token, const char *val)
{
    struct dummy_rot_priv_data *priv;

    priv = (struct dummy_rot_priv_data *)rot->state.priv;

    switch (token)
    {
    case TOK_CFG_ROT_MAGICCONF:
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


static int dummy_get_conf2(ROT *rot, token_t token, char *val, int val_len)
{
    struct dummy_rot_priv_data *priv;

    priv = (struct dummy_rot_priv_data *)rot->state.priv;

    switch (token)
    {
    case TOK_CFG_ROT_MAGICCONF:
        SNPRINTF(val, val_len, "%s", priv->magic_conf);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int dummy_get_conf(ROT *rot, token_t token, char *val)
{
    return dummy_get_conf2(rot, token, val, 128);
}



static int dummy_rot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);

    if (simulating)
    {
        priv->target_az = az;
        priv->target_el = el;
        gettimeofday(&priv->tv, NULL);
    }
    else
    {
        priv->az = priv->target_az = az;
        priv->el = priv->target_az = el;
    }


    return RIG_OK;
}

static void dummy_rot_simulate_rotation(ROT *rot)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    struct timeval tv;
    unsigned elapsed; /* ms */

    gettimeofday(&tv, NULL);

    elapsed = (tv.tv_sec - priv->tv.tv_sec) * 1000 +
              (tv.tv_usec - priv->tv.tv_usec) / 1000;

    /*
     * Simulate rotation speed of 360 deg per minute
     */
#define DEG_PER_MS (360./60/1000)

    if (fabs(priv->target_az - priv->az) / DEG_PER_MS <= elapsed)
    {
        /* target reached */
        priv->az = priv->target_az;
        priv->status &= ~(ROT_STATUS_MOVING_AZ | ROT_STATUS_MOVING_LEFT |
                          ROT_STATUS_MOVING_RIGHT);
    }
    else
    {
        if (priv->az < priv->target_az)
        {
            priv->az += (azimuth_t)elapsed * DEG_PER_MS;
            priv->status |= ROT_STATUS_MOVING_AZ | ROT_STATUS_MOVING_RIGHT;
        }
        else
        {
            priv->az -= (azimuth_t)elapsed * DEG_PER_MS;
            priv->status |=  ROT_STATUS_MOVING_AZ | ROT_STATUS_MOVING_LEFT;
        }
    }

    if (fabs(priv->target_el - priv->el) / DEG_PER_MS <= elapsed)
    {
        /* target reached */
        priv->el = priv->target_el;
        priv->status &= ~(ROT_STATUS_MOVING_EL | ROT_STATUS_MOVING_UP |
                          ROT_STATUS_MOVING_DOWN);
    }
    else
    {
        if (priv->el < priv->target_el)
        {
            priv->el += (elevation_t)elapsed * DEG_PER_MS;
            priv->status |= ROT_STATUS_MOVING_EL | ROT_STATUS_MOVING_UP;
        }
        else
        {
            priv->el -= (elevation_t)elapsed * DEG_PER_MS;
            priv->status |= ROT_STATUS_MOVING_EL | ROT_STATUS_MOVING_DOWN;
        }
    }

    if (priv->status & (ROT_STATUS_MOVING_AZ | ROT_STATUS_MOVING_EL))
    {
        priv->status |= ROT_STATUS_MOVING;
    }
    else
    {
        priv->status &= ~(ROT_STATUS_MOVING);
    }

    priv->tv = tv;
}


/*
 * Get position of rotor, simulating slow rotation
 */
static int dummy_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (simulating && priv->az == priv->target_az &&
            priv->el == priv->target_el)
    {
        *az = priv->az;
        *el = priv->el;
        return RIG_OK;
    }

    if (simulating)
    {
        dummy_rot_simulate_rotation(rot);
    }

    *az = priv->az;
    *el = priv->el;

    return RIG_OK;
}


static int dummy_rot_stop(ROT *rot)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    azimuth_t az;
    elevation_t el;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    dummy_rot_get_position(rot, &az, &el);

    priv->target_az = priv->az = az;
    priv->target_el = priv->el = el;

    return RIG_OK;
}


static int dummy_rot_park(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Assume home is 0,0 */
    dummy_rot_set_position(rot, 0, 0);

    return RIG_OK;
}

static int dummy_rot_reset(ROT *rot, rot_reset_t reset)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

static int dummy_rot_move(ROT *rot, int direction, int speed)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: Direction = %d, Speed = %d\n", __func__,
              direction, speed);

    switch (direction)
    {
    case ROT_MOVE_UP:
        return dummy_rot_set_position(rot, priv->target_az, 90);

    case ROT_MOVE_DOWN:
        return dummy_rot_set_position(rot, priv->target_az, 0);

    case ROT_MOVE_CCW:
        return dummy_rot_set_position(rot, -180, priv->target_el);

    case ROT_MOVE_CW:
        return dummy_rot_set_position(rot, 180, priv->target_el);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static const char *dummy_rot_get_info(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return "Dummy rotator";
}

static int dummy_set_func(ROT *rot, setting_t func, int status)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %d\n", __func__,
              rot_strfunc(func), status);

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


static int dummy_get_func(ROT *rot, setting_t func, int *status)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;

    *status = (priv->funcs & func) ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              rot_strfunc(func));

    return RIG_OK;
}


static int dummy_set_level(ROT *rot, setting_t level, value_t val)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    int idx;
    char lstr[32];

    idx = rig_setting2idx(level);

    if (idx >= RIG_SETTING_MAX)
    {
        return -RIG_EINVAL;
    }

    priv->levels[idx] = val;

    if (ROT_LEVEL_IS_FLOAT(level))
    {
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              rot_strlevel(level), lstr);

    return RIG_OK;
}


static int dummy_get_level(ROT *rot, setting_t level, value_t *val)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    int idx;

    idx = rig_setting2idx(level);

    if (idx >= RIG_SETTING_MAX)
    {
        return -RIG_EINVAL;
    }

    *val = priv->levels[idx];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              rot_strlevel(level));

    return RIG_OK;
}

static int dummy_set_ext_level(ROT *rot, token_t token, value_t val)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    char lstr[64];
    const struct confparams *cfp;
    struct ext_list *elp;

    cfp = rot_ext_lookup_tok(rot, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EL_ROT_MAGICLEVEL:
    case TOK_EL_ROT_MAGICFUNC:
    case TOK_EL_ROT_MAGICOP:
    case TOK_EL_ROT_MAGICCOMBO:
        break;

    default:
        return -RIG_EINVAL;
    }

    switch (cfp->type)
    {
    case RIG_CONF_STRING:
        strcpy(lstr, val.s);
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

static int dummy_get_ext_level(ROT *rot, token_t token, value_t *val)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    const struct confparams *cfp;
    struct ext_list *elp;

    cfp = rot_ext_lookup_tok(rot, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EL_ROT_MAGICLEVEL:
    case TOK_EL_ROT_MAGICFUNC:
    case TOK_EL_ROT_MAGICOP:
    case TOK_EL_ROT_MAGICCOMBO:
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


static int dummy_set_ext_func(ROT *rot, token_t token, int status)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    const struct confparams *cfp;
    struct ext_list *elp;

    cfp = rot_ext_lookup_tok(rot, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EL_ROT_MAGICEXTFUNC:
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


static int dummy_get_ext_func(ROT *rot, token_t token, int *status)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    const struct confparams *cfp;
    struct ext_list *elp;

    cfp = rot_ext_lookup_tok(rot, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EL_ROT_MAGICEXTFUNC:
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


static int dummy_set_parm(ROT *rot, setting_t parm, value_t val)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    int idx;
    char pstr[32];

    idx = rig_setting2idx(parm);

    if (idx >= RIG_SETTING_MAX)
    {
        return -RIG_EINVAL;
    }

    if (ROT_PARM_IS_FLOAT(parm))
    {
        SNPRINTF(pstr, sizeof(pstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(pstr, sizeof(pstr), "%d", val.i);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              rig_strparm(parm), pstr);

    priv->parms[idx] = val;

    return RIG_OK;
}


static int dummy_get_parm(ROT *rot, setting_t parm, value_t *val)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    int idx;

    idx = rig_setting2idx(parm);

    if (idx >= RIG_SETTING_MAX)
    {
        return -RIG_EINVAL;
    }

    *val = priv->parms[idx];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s\n", __func__,
              rig_strparm(parm));

    return RIG_OK;
}

static int dummy_set_ext_parm(ROT *rot, token_t token, value_t val)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    char lstr[64];
    const struct confparams *cfp;
    struct ext_list *epp;

    cfp = rot_ext_lookup_tok(rot, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EP_ROT_MAGICPARM:
        break;

    default:
        return -RIG_EINVAL;
    }

    switch (cfp->type)
    {
    case RIG_CONF_STRING:
        strcpy(lstr, val.s);
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

static int dummy_get_ext_parm(ROT *rot, token_t token, value_t *val)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;
    const struct confparams *cfp;
    struct ext_list *epp;

    cfp = rot_ext_lookup_tok(rot, token);

    if (!cfp)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_EP_ROT_MAGICPARM:
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


static int dummy_rot_get_status(ROT *rot, rot_status_t *status)
{
    struct dummy_rot_priv_data *priv = (struct dummy_rot_priv_data *)
                                       rot->state.priv;

    if (simulating)
    {
        dummy_rot_simulate_rotation(rot);
    }

    *status = priv->status;

    return RIG_OK;
}


/*
 * Dummy rotator capabilities.
 */
struct rot_caps dummy_rot_caps =
{
    ROT_MODEL(ROT_MODEL_DUMMY),
    .model_name =     "Dummy",
    .mfg_name =       "Hamlib",
    .version =        "20220531.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_NONE,

    .min_az =     -180.,
    .max_az =     450.,
    .min_el =     0.,
    .max_el =     90.,

    .priv =  NULL,    /* priv */

    .has_get_func =   DUMMY_ROT_FUNC,
    .has_set_func =   DUMMY_ROT_FUNC,
    .has_get_level =  DUMMY_ROT_LEVEL,
    .has_set_level =  ROT_LEVEL_SET(DUMMY_ROT_LEVEL),
    .has_get_parm =    DUMMY_ROT_PARM,
    .has_set_parm =    ROT_PARM_SET(DUMMY_ROT_PARM),

    .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    .extlevels =    dummy_ext_levels,
    .extfuncs =     dummy_ext_funcs,
    .extparms =     dummy_ext_parms,
    .cfgparams =    dummy_cfg_params,

    .has_status = DUMMY_ROT_STATUS,

    .rot_init =     dummy_rot_init,
    .rot_cleanup =  dummy_rot_cleanup,
    .rot_open =     dummy_rot_open,
    .rot_close =    dummy_rot_close,

    .set_conf =     dummy_set_conf,
    .get_conf =     dummy_get_conf,

    .set_position =     dummy_rot_set_position,
    .get_position =     dummy_rot_get_position,
    .park =     dummy_rot_park,
    .stop =     dummy_rot_stop,
    .reset =    dummy_rot_reset,
    .move =     dummy_rot_move,

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

    .get_info = dummy_rot_get_info,
    .get_status = dummy_rot_get_status,
};

DECLARE_INITROT_BACKEND(dummy)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&dummy_rot_caps);
    rot_register(&netrotctl_caps);

    return RIG_OK;
}
