/*
 *  Hamlib FLIR PTU rotor backend - main file
 *  Copyright (c) 2022 by Andreas Mueller (DC1MIL)
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
#include "register.h"
#include "idx_builtin.h"

#include "flir.h"

#define FLIR_FUNC 0
#define FLIR_LEVEL ROT_LEVEL_SPEED
#define FLIR_PARM 0

#define FLIR_STATUS (ROT_STATUS_MOVING | ROT_STATUS_MOVING_AZ | ROT_STATUS_MOVING_LEFT | ROT_STATUS_MOVING_RIGHT | \
        ROT_STATUS_MOVING_EL | ROT_STATUS_MOVING_UP | ROT_STATUS_MOVING_DOWN | \
        ROT_STATUS_LIMIT_UP | ROT_STATUS_LIMIT_DOWN | ROT_STATUS_LIMIT_LEFT | ROT_STATUS_LIMIT_RIGHT)

//static int simulating = 0;  // do we need rotator emulation for debug?

struct flir_priv_data
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

    float_t resolution_pp; 
    float_t resolution_tp; 
};

// static const struct confparams flir_ext_levels[] =
// {
//     {
//         TOK_EL_ROT_MAGICLEVEL, "MGL", "Magic level", "Magic level, as an example",
//         NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
//     },
//     {
//         TOK_EL_ROT_MAGICFUNC, "MGF", "Magic func", "Magic function, as an example",
//         NULL, RIG_CONF_CHECKBUTTON
//     },
//     {
//         TOK_EL_ROT_MAGICOP, "MGO", "Magic Op", "Magic Op, as an example",
//         NULL, RIG_CONF_BUTTON
//     },
//     {
//         TOK_EL_ROT_MAGICCOMBO, "MGC", "Magic combo", "Magic combo, as an example",
//         "VALUE1", RIG_CONF_COMBO, { .c = { .combostr = { "VALUE1", "VALUE2", "NONE", NULL } } }
//     },
//     { RIG_CONF_END, NULL, }
// };

// static const struct confparams flir_ext_funcs[] =
// {
//     {
//         TOK_EL_ROT_MAGICEXTFUNC, "MGEF", "Magic ext func", "Magic ext function, as an example",
//         NULL, RIG_CONF_CHECKBUTTON
//     },
//     { RIG_CONF_END, NULL, }
// };

// static const struct confparams flir_ext_parms[] =
// {
//     {
//         TOK_EP_ROT_MAGICPARM, "MGP", "Magic parm", "Magic parameter, as an example",
//         NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, .001 } }
//     },
//     { RIG_CONF_END, NULL, }
// };

/* cfgparams are configuration item generally used by the backend's open() method */
// static const struct confparams flir_cfg_params[] =
// {
//     {
//         TOK_CFG_ROT_MAGICCONF, "mcfg", "Magic conf", "Magic parameter, as an example",
//         "ROTATOR", RIG_CONF_STRING, { }
//     },
//     { RIG_CONF_END, NULL, }
// };

static int flir_request(ROT *rot, char *request, char *response,
             uint32_t *resp_size)
{
    int return_value = -RIG_EINVAL;
    int retry_read = 0;
    unsigned char cmd_ok;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig_flush(&rot->state.rotport);

        if (request)
        {
            return_value = write_block(&rot->state.rotport, (unsigned char *) request,
                                       strlen(request));
            if (return_value != RIG_OK)
            {
                return return_value;
            }
        }
        //Is a direct request expected?
        if (response != NULL)
        {
            while(retry_read < rot->state.rotport.retry)
            {
                memset(response, 0, resp_size);
                resp_size = read_string(&rot->state.rotport, response, resp_size,
                        "\r\n", sizeof("\r\n"), 0, 1);
                if(resp_size > 0)
                {
                    if(response[0] == '*')
                    {
                        rig_debug(RIG_DEBUG_VERBOSE, "accepted command %s\n", request);
                        return RIG_OK;
                    }
                    else
                    {
                        rig_debug(RIG_DEBUG_VERBOSE, "NOT accepted command %s\n", request);
                        return -RIG_ERJCTED;
                    }
                        
                }
                retry_read++;
            }
            rig_debug(RIG_DEBUG_VERBOSE, "timeout for command %s\n", request);
            return -RIG_ETIMEOUT;        
        }

    return return_value;
};

static int flir_init(ROT *rot)
{
    struct flir_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot->state.priv = (struct flir_priv_data *)
                      calloc(1, sizeof(struct flir_priv_data));

    if (!rot->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rot->state.priv;

    // priv->ext_funcs = alloc_init_ext(flir_ext_funcs);

    // if (!priv->ext_funcs)
    // {
    //     return -RIG_ENOMEM;
    // }

    // priv->ext_levels = alloc_init_ext(flir_ext_levels);

    // if (!priv->ext_levels)
    // {
    //     return -RIG_ENOMEM;
    // }

    // priv->ext_parms = alloc_init_ext(flir_ext_parms);

    // if (!priv->ext_parms)
    // {
    //     return -RIG_ENOMEM;
    // }

    //rot->state.rotport.type.rig = RIG_PORT_SERIAL;
    //flir_request(rot, "r\n", sizeof("r\n"), NULL, NULL);

    priv->az = priv->el = 0;

    priv->target_az = priv->target_el = 0;

    priv->magic_conf = strdup("ROTATOR");

    priv->resolution_pp = 92.5714;
    priv->resolution_tp = 46.2857;

    return RIG_OK;
}

static int flir_cleanup(ROT *rot)
{
    struct flir_priv_data *priv = (struct flir_priv_data *)
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

static int flir_open(ROT *rot)
{
    struct flir_priv_data *priv;
    char return_str[MAXBUF];
    float_t resolution_pp, resolution_tp;
    int return_value = RIG_OK;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = rot->state.priv;

    // Disable ECHO
    return_value = flir_request(rot, "ED\n", NULL, MAXBUF);

    // Disable Verbose Mode
    return_value = flir_request(rot, "FT\n", return_str, MAXBUF);

    // Get PAN resolution in arcsecs
    if(flir_request(rot, "PR\n", return_str, MAXBUF) == RIG_OK)
    {
        sscanf(return_str, "* %f", &resolution_pp);
        rig_debug(RIG_DEBUG_VERBOSE, "PAN resolution: %f arcsecs per position\n", resolution_pp);
        priv->resolution_pp = resolution_pp;
    }
    else
    {
        return_value = -RIG_EPROTO;
    }
    // Get TILT resolution in arcsecs
    if(flir_request(rot, "TR\n", return_str, MAXBUF) == RIG_OK)
    {
        sscanf(return_str, "* %f", &resolution_tp);
        rig_debug(RIG_DEBUG_VERBOSE, "TILT resolution: %f arcsecs per position\n", resolution_tp);
        priv->resolution_tp = resolution_tp;
    }
    else
    {
        return_value = -RIG_EPROTO;
    }

    return return_value;
}

static int flir_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return RIG_OK;
}

// static int flir_set_conf(ROT *rot, token_t token, const char *val)
// {
//     struct flir_priv_data *priv;

//     priv = (struct flir_priv_data *)rot->state.priv;

//     switch (token)
//     {
//     case TOK_CFG_ROT_MAGICCONF:
//         if (val)
//         {
//             free(priv->magic_conf);
//             priv->magic_conf = strdup(val);
//         }

//         break;

//     default:
//         return -RIG_EINVAL;
//     }

//     return RIG_OK;
// }


// static int flir_get_conf2(ROT *rot, token_t token, char *val, int val_len)
// {
//     struct flir_priv_data *priv;

//     priv = (struct flir_priv_data *)rot->state.priv;

//     switch (token)
//     {
//     case TOK_CFG_ROT_MAGICCONF:
//         SNPRINTF(val, val_len, "%s", priv->magic_conf);
//         break;

//     default:
//         return -RIG_EINVAL;
//     }

//     return RIG_OK;
// }

// static int flir_get_conf(ROT *rot, token_t token, char *val)
// {
//     return flir_get_conf2(rot, token, val, 128);
// }



static int flir_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    int32_t t_pan_positions, t_tilt_positions;
    char return_str[MAXBUF];
    char cmd_str[MAXBUF];
    struct flir_priv_data *priv = (struct flir_priv_data *)
                                       rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %.2f %.2f\n", __func__,
              az, el);

    priv->target_az = az;
    priv->target_el = el;

    t_pan_positions = (az * 3600) / priv->resolution_pp;
    t_tilt_positions = - ((90.0 - el) * 3600) / priv->resolution_tp;

    sprintf(cmd_str, "PP%d TP%d\n", t_pan_positions, t_tilt_positions);

    return flir_request(rot, cmd_str, return_str, MAXBUF);
}

/*
 * Get position of rotor
 */
static int flir_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    int return_value = RIG_OK;
    char return_str[MAXBUF];
    int32_t pan_positions, tilt_positions;
    
    struct flir_priv_data *priv = (struct flir_priv_data *)
                                       rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    if(flir_request(rot, "PP\n", return_str, MAXBUF) == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "PP Return String: %s\n", return_str);
        sscanf(return_str, "* %d", &pan_positions);
        priv->az = (pan_positions * priv->resolution_pp) / 3600;
    }
    else
    {
        return_value = -RIG_EPROTO;
    }
    
        if(flir_request(rot, "TP\n", return_str, MAXBUF) == RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "TP Return String: %s\n", return_str);
        sscanf(return_str, "* %d", &tilt_positions);
        priv->el = 90.0 + ((tilt_positions * priv->resolution_tp) / 3600);
    }
    else
    {
        return_value = -RIG_EPROTO;
    }

    *az = priv->az;
    *el = priv->el;

    return return_value;
}


static int flir_stop(ROT *rot)
{
    int return_value = RIG_OK;
    char return_str[MAXBUF];

    struct flir_priv_data *priv = (struct flir_priv_data *)
                                       rot->state.priv;
    azimuth_t az;
    elevation_t el;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return_value = flir_request(rot, "H\n", NULL, MAXBUF);
    // Wait 2s until rotor has stopped (Needs to be refactored)
    hl_usleep(2000000);

    return_value = flir_get_position(rot, &az, &el);

    priv->target_az = priv->az = az;
    priv->target_el = priv->el = el;

    return return_value;
}


static int flir_park(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Assume park Position is 0,90 */
    flir_set_position(rot, 0, 90);

    return RIG_OK;
}

static int flir_reset(ROT *rot, rot_reset_t reset)
{
    int return_value = RIG_OK;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    if (reset != 0)
    {
        return_value = flir_request(rot, "r\n", NULL, NULL);
        // After Reset: Disable Hard Limits
        if(return_value == RIG_OK)
        {
            return_value = flir_request(rot, "LD\n", NULL, MAXBUF);
        }
    }
    return return_value;
}

static int flir_move(ROT *rot, int direction, int speed)
{
    struct flir_priv_data *priv = (struct flir_priv_data *)
                                       rot->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: Direction = %d, Speed = %d\n", __func__,
              direction, speed);

    switch (direction)
    {
    case ROT_MOVE_UP:
        return flir_set_position(rot, priv->target_az, 90);

    case ROT_MOVE_DOWN:
        return flir_set_position(rot, priv->target_az, 0);

    case ROT_MOVE_CCW:
        return flir_set_position(rot, -180, priv->target_el);

    case ROT_MOVE_CW:
        return flir_set_position(rot, 180, priv->target_el);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static const char *flir_get_info(ROT *rot)
{
    const char* firmware_str[120];
    const char* info_str[120];
    const char* return_str[256];

    sprintf(return_str, "No Info");

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if(flir_request(rot, "V\n", firmware_str, 120) == RIG_OK && 
        flir_request(rot, "O\n", info_str, 120) == RIG_OK)
    {
        sprintf(return_str, "Firmware: %s Info: %s", firmware_str, info_str);
    }
    //rig_debug(RIG_DEBUG_VERBOSE, "Return String: %s", return_str);
    return *return_str;
}

// static int flir_set_func(ROT *rot, setting_t func, int status)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %d\n", __func__,
//               rot_strfunc(func), status);

//     if (status)
//     {
//         priv->funcs |=  func;
//     }
//     else
//     {
//         priv->funcs &= ~func;
//     }

//     return RIG_OK;
// }


// static int flir_get_func(ROT *rot, setting_t func, int *status)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;

//     *status = (priv->funcs & func) ? 1 : 0;

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
//               rot_strfunc(func));

//     return RIG_OK;
// }


// static int flir_set_level(ROT *rot, setting_t level, value_t val)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     int idx;
//     char lstr[32];

//     idx = rig_setting2idx(level);

//     if (idx >= RIG_SETTING_MAX)
//     {
//         return -RIG_EINVAL;
//     }

//     priv->levels[idx] = val;

//     if (ROT_LEVEL_IS_FLOAT(level))
//     {
//         SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
//     }
//     else
//     {
//         SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
//     }

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
//               rot_strlevel(level), lstr);

//     return RIG_OK;
// }


// static int flir_get_level(ROT *rot, setting_t level, value_t *val)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     int idx;

//     idx = rig_setting2idx(level);

//     if (idx >= RIG_SETTING_MAX)
//     {
//         return -RIG_EINVAL;
//     }

//     *val = priv->levels[idx];

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
//               rot_strlevel(level));

//     return RIG_OK;
// }

// static int flir_set_ext_level(ROT *rot, token_t token, value_t val)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     char lstr[64];
//     const struct confparams *cfp;
//     struct ext_list *elp;

//     cfp = rot_ext_lookup_tok(rot, token);

//     if (!cfp)
//     {
//         return -RIG_EINVAL;
//     }

//     switch (token)
//     {
//     case TOK_EL_ROT_MAGICLEVEL:
//     case TOK_EL_ROT_MAGICFUNC:
//     case TOK_EL_ROT_MAGICOP:
//     case TOK_EL_ROT_MAGICCOMBO:
//         break;

//     default:
//         return -RIG_EINVAL;
//     }

//     switch (cfp->type)
//     {
//     case RIG_CONF_STRING:
//         strcpy(lstr, val.s);
//         break;

//     case RIG_CONF_COMBO:
//         SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
//         break;

//     case RIG_CONF_NUMERIC:
//         SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
//         break;

//     case RIG_CONF_CHECKBUTTON:
//         SNPRINTF(lstr, sizeof(lstr), "%s", val.i ? "ON" : "OFF");
//         break;

//     case RIG_CONF_BUTTON:
//         lstr[0] = '\0';
//         break;

//     default:
//         return -RIG_EINTERNAL;
//     }

//     elp = find_ext(priv->ext_levels, token);

//     if (!elp)
//     {
//         return -RIG_EINTERNAL;
//     }

//     /* store value */
//     elp->val = val;

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
//               cfp->name, lstr);

//     return RIG_OK;
// }

// static int flir_get_ext_level(ROT *rot, token_t token, value_t *val)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     const struct confparams *cfp;
//     struct ext_list *elp;

//     cfp = rot_ext_lookup_tok(rot, token);

//     if (!cfp)
//     {
//         return -RIG_EINVAL;
//     }

//     switch (token)
//     {
//     case TOK_EL_ROT_MAGICLEVEL:
//     case TOK_EL_ROT_MAGICFUNC:
//     case TOK_EL_ROT_MAGICOP:
//     case TOK_EL_ROT_MAGICCOMBO:
//         break;

//     default:
//         return -RIG_EINVAL;
//     }

//     elp = find_ext(priv->ext_levels, token);

//     if (!elp)
//     {
//         return -RIG_EINTERNAL;
//     }

//     /* load value */
//     *val = elp->val;

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
//               cfp->name);

//     return RIG_OK;
// }


// static int flir_set_ext_func(ROT *rot, token_t token, int status)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     const struct confparams *cfp;
//     struct ext_list *elp;

//     cfp = rot_ext_lookup_tok(rot, token);

//     if (!cfp)
//     {
//         return -RIG_EINVAL;
//     }

//     switch (token)
//     {
//     case TOK_EL_ROT_MAGICEXTFUNC:
//         break;

//     default:
//         return -RIG_EINVAL;
//     }

//     switch (cfp->type)
//     {
//     case RIG_CONF_CHECKBUTTON:
//         break;

//     case RIG_CONF_BUTTON:
//         break;

//     default:
//         return -RIG_EINTERNAL;
//     }

//     elp = find_ext(priv->ext_funcs, token);

//     if (!elp)
//     {
//         return -RIG_EINTERNAL;
//     }

//     /* store value */
//     elp->val.i = status;

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %d\n", __func__,
//               cfp->name, status);

//     return RIG_OK;
// }


// static int flir_get_ext_func(ROT *rot, token_t token, int *status)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     const struct confparams *cfp;
//     struct ext_list *elp;

//     cfp = rot_ext_lookup_tok(rot, token);

//     if (!cfp)
//     {
//         return -RIG_EINVAL;
//     }

//     switch (token)
//     {
//     case TOK_EL_ROT_MAGICEXTFUNC:
//         break;

//     default:
//         return -RIG_EINVAL;
//     }

//     elp = find_ext(priv->ext_funcs, token);

//     if (!elp)
//     {
//         return -RIG_EINTERNAL;
//     }

//     /* load value */
//     *status = elp->val.i;

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
//               cfp->name);

//     return RIG_OK;
// }


// static int flir_set_parm(ROT *rot, setting_t parm, value_t val)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     int idx;
//     char pstr[32];

//     idx = rig_setting2idx(parm);

//     if (idx >= RIG_SETTING_MAX)
//     {
//         return -RIG_EINVAL;
//     }

//     if (ROT_PARM_IS_FLOAT(parm))
//     {
//         SNPRINTF(pstr, sizeof(pstr), "%f", val.f);
//     }
//     else
//     {
//         SNPRINTF(pstr, sizeof(pstr), "%d", val.i);
//     }

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
//               rig_strparm(parm), pstr);

//     priv->parms[idx] = val;

//     return RIG_OK;
// }


// static int flir_get_parm(ROT *rot, setting_t parm, value_t *val)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     int idx;

//     idx = rig_setting2idx(parm);

//     if (idx >= RIG_SETTING_MAX)
//     {
//         return -RIG_EINVAL;
//     }

//     *val = priv->parms[idx];

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called %s\n", __func__,
//               rig_strparm(parm));

//     return RIG_OK;
// }

// static int flir_set_ext_parm(ROT *rot, token_t token, value_t val)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     char lstr[64];
//     const struct confparams *cfp;
//     struct ext_list *epp;

//     cfp = rot_ext_lookup_tok(rot, token);

//     if (!cfp)
//     {
//         return -RIG_EINVAL;
//     }

//     switch (token)
//     {
//     case TOK_EP_ROT_MAGICPARM:
//         break;

//     default:
//         return -RIG_EINVAL;
//     }

//     switch (cfp->type)
//     {
//     case RIG_CONF_STRING:
//         strcpy(lstr, val.s);
//         break;

//     case RIG_CONF_COMBO:
//         SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
//         break;

//     case RIG_CONF_NUMERIC:
//         SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
//         break;

//     case RIG_CONF_CHECKBUTTON:
//         SNPRINTF(lstr, sizeof(lstr), "%s", val.i ? "ON" : "OFF");
//         break;

//     case RIG_CONF_BUTTON:
//         lstr[0] = '\0';
//         break;

//     default:
//         return -RIG_EINTERNAL;
//     }

//     epp = find_ext(priv->ext_parms, token);

//     if (!epp)
//     {
//         return -RIG_EINTERNAL;
//     }

//     /* store value */
//     epp->val = val;

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
//               cfp->name, lstr);

//     return RIG_OK;
// }

// static int flir_get_ext_parm(ROT *rot, token_t token, value_t *val)
// {
//     struct flir_priv_data *priv = (struct flir_priv_data *)
//                                        rot->state.priv;
//     const struct confparams *cfp;
//     struct ext_list *epp;

//     cfp = rot_ext_lookup_tok(rot, token);

//     if (!cfp)
//     {
//         return -RIG_EINVAL;
//     }

//     switch (token)
//     {
//     case TOK_EP_ROT_MAGICPARM:
//         break;

//     default:
//         return -RIG_EINVAL;
//     }

//     epp = find_ext(priv->ext_parms, token);

//     if (!epp)
//     {
//         return -RIG_EINTERNAL;
//     }

//     /* load value */
//     *val = epp->val;

//     rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
//               cfp->name);

//     return RIG_OK;
// }


static int flir_get_status(ROT *rot, rot_status_t *status)
{
    struct flir_priv_data *priv = (struct flir_priv_data *)
                                       rot->state.priv;

    // if (simulating)
    // {
    //     flir_simulate_rotation(rot);
    // }

    *status = priv->status;

    return RIG_OK;
}


/*
 * flir rotator capabilities.
 */
struct rot_caps flir_caps =
{
    ROT_MODEL(ROT_MODEL_FLIR),
    .model_name =     "PTU Serial",
    .mfg_name =       "FLIR",
    .version =        "20221126.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_ALPHA,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =   9600,
    .serial_rate_max =   9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =       0,
    .post_write_delay =  300,
    .timeout =           400,
    .retry =             3,

    .min_az =     -180.,
    .max_az =     180.,
    .min_el =     0.,
    .max_el =     90.,



    .priv =  NULL,    /* priv */

    // .has_get_func =   FLIR_FUNC,
    // .has_set_func =   FLIR_FUNC,
    // .has_get_level =  FLIR_LEVEL,
    // .has_set_level =  ROT_LEVEL_SET(FLIR_LEVEL),
    // .has_get_parm =   FLIR_PARM,
    // .has_set_parm =   ROT_PARM_SET(FLIR_PARM),

    // .level_gran =      { [ROT_LVL_SPEED] = { .min = { .i = 1 }, .max = { .i = 4 }, .step = { .i = 1 } } },

    // .extlevels =    flir_ext_levels,
    // .extfuncs =     flir_ext_funcs,
    // .extparms =     flir_ext_parms,
    // .cfgparams =    flir_cfg_params,

    .has_status = FLIR_STATUS,

    .rot_init =     flir_init,
    .rot_cleanup =  flir_cleanup,
    .rot_open =     flir_open,
    .rot_close =    flir_close,

    // .set_conf =     flir_set_conf,
    // .get_conf =     flir_get_conf,

    .set_position =     flir_set_position,
    .get_position =     flir_get_position,
    .park =     flir_park,
    .stop =     flir_stop,
    .reset =    flir_reset,
    .move =     flir_move,

    // .set_func = flir_set_func,
    // .get_func = flir_get_func,
    // .set_level = flir_set_level,
    // .get_level = flir_get_level,
    // .set_parm = flir_set_parm,
    // .get_parm = flir_get_parm,

    // .set_ext_func = flir_set_ext_func,
    // .get_ext_func = flir_get_ext_func,
    // .set_ext_level = flir_set_ext_level,
    // .get_ext_level = flir_get_ext_level,
    // .set_ext_parm = flir_set_ext_parm,
    // .get_ext_parm = flir_get_ext_parm,

    .get_info = flir_get_info,
    .get_status = flir_get_status,
};

DECLARE_INITROT_BACKEND(flir)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rot_register(&flir_caps);
    rot_register(&netrotctl_caps);

    return RIG_OK;
}
