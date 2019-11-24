/*
 *  Hamlib Interface - rotator configuration interface
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

/**
 * \addtogroup rotator
 * @{
 */

/**
 * \brief Rotator Configuration Interface
 * \file rot_conf.c
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include <hamlib/rotator.h>

#include "rot_conf.h"
#include "token.h"


/*
 * Configuration options available in the rot->state struct.
 */
static const struct confparams rotfrontend_cfg_params[] =
{
    {
        TOK_PATHNAME, "rot_pathname", "Rig path name",
        "Path name to the device file of the rotator",
        "/dev/rotator", RIG_CONF_STRING,
    },
    {
        TOK_WRITE_DELAY, "write_delay", "Write delay",
        "Delay in ms between each byte sent out",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 1000, 1 } }
    },
    {
        TOK_POST_WRITE_DELAY, "post_write_delay", "Post write delay",
        "Delay in ms between each command sent out",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 1000, 1 } }
    },
    {
        TOK_TIMEOUT, "timeout", "Timeout", "Timeout in ms",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 10000, 1 } }
    },
    {
        TOK_RETRY, "retry", "Retry", "Max number of retry",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } }
    },

    {
        TOK_MIN_AZ, "min_az", "Minimum azimuth",
        "Minimum rotator azimuth in degrees",
        "-180", RIG_CONF_NUMERIC, { .n = { -360, 360, .001 } }
    },
    {
        TOK_MAX_AZ, "max_az", "Maximum azimuth",
        "Maximum rotator azimuth in degrees",
        "180", RIG_CONF_NUMERIC, { .n = { -360, 360, .001 } }
    },
    {
        TOK_MIN_EL, "min_el", "Minimum elevation",
        "Minimum rotator elevation in degrees",
        "0", RIG_CONF_NUMERIC, { .n = { -90, 180, .001 } }
    },
    {
        TOK_MAX_EL, "max_el", "Maximum elevation",
        "Maximum rotator elevation in degrees",
        "90", RIG_CONF_NUMERIC, { .n = { -90, 180, .001 } }
    },
    {
        TOK_SOUTH_ZERO, "south_zero", "South is zero deg",
        "Adjust azimuth 180 degrees for south oriented rotators",
        "0", RIG_CONF_CHECKBUTTON,
    },

    { RIG_CONF_END, NULL, }
};


static const struct confparams rotfrontend_serial_cfg_params[] =
{
    {
        TOK_SERIAL_SPEED, "serial_speed", "Serial speed",
        "Serial port baud rate",
        "0", RIG_CONF_NUMERIC, { .n = { 300, 115200, 1 } }
    },
    {
        TOK_DATA_BITS, "data_bits", "Serial data bits",
        "Serial port data bits",
        "8", RIG_CONF_NUMERIC, { .n = { 5, 8, 1 } }
    },
    {
        TOK_STOP_BITS, "stop_bits", "Serial stop bits",
        "Serial port stop bits",
        "1", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } }
    },
    {
        TOK_PARITY, "serial_parity", "Serial parity",
        "Serial port parity",
        "None", RIG_CONF_COMBO, { .c = {{ "None", "Odd", "Even", "Mark", "Space", NULL }} }
    },
    {
        TOK_HANDSHAKE, "serial_handshake", "Serial handshake",
        "Serial port handshake",
        "None", RIG_CONF_COMBO, { .c = {{ "None", "XONXOFF", "Hardware", NULL }} }
    },

    { RIG_CONF_END, NULL, }
};


/**
 * \brief Set rotator state info from alpha input
 * \param rot
 * \param token TOK_... specifying which info to set
 * \param val input
 * \return RIG_OK or < 0 error
 *
 * assumes rot!=NULL, val!=NULL
 */
int frontrot_set_conf(ROT *rot, token_t token, const char *val)
{
    struct rot_state *rs;
    int val_i;

    rs = &rot->state;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (token)
    {
    case TOK_PATHNAME:
        strncpy(rs->rotport.pathname, val, FILPATHLEN - 1);
        break;

    case TOK_WRITE_DELAY:
        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->rotport.write_delay = val_i;
        break;

    case TOK_POST_WRITE_DELAY:
        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->rotport.post_write_delay = val_i;
        break;

    case TOK_TIMEOUT:
        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->rotport.timeout = val_i;
        break;

    case TOK_RETRY:
        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->rotport.retry = val_i;
        break;

    case TOK_SERIAL_SPEED:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->rotport.parm.serial.rate = val_i;
        break;

    case TOK_DATA_BITS:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->rotport.parm.serial.data_bits = val_i;
        break;

    case TOK_STOP_BITS:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->rotport.parm.serial.stop_bits = val_i;
        break;

    case TOK_PARITY:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "None"))
        {
            rs->rotport.parm.serial.parity = RIG_PARITY_NONE;
        }
        else if (!strcmp(val, "Odd"))
        {
            rs->rotport.parm.serial.parity = RIG_PARITY_ODD;
        }
        else if (!strcmp(val, "Even"))
        {
            rs->rotport.parm.serial.parity = RIG_PARITY_EVEN;
        }
        else if (!strcmp(val, "Mark"))
        {
            rs->rotport.parm.serial.parity = RIG_PARITY_MARK;
        }
        else if (!strcmp(val, "Space"))
        {
            rs->rotport.parm.serial.parity = RIG_PARITY_SPACE;
        }
        else
        {
            return -RIG_EINVAL;
        }

        break;

    case TOK_HANDSHAKE:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "None"))
        {
            rs->rotport.parm.serial.handshake = RIG_HANDSHAKE_NONE;
        }
        else if (!strcmp(val, "XONXOFF"))
        {
            rs->rotport.parm.serial.handshake = RIG_HANDSHAKE_XONXOFF;
        }
        else if (!strcmp(val, "Hardware"))
        {
            rs->rotport.parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
        }
        else
        {
            return -RIG_EINVAL;
        }

        break;

    case TOK_MIN_AZ:
        rs->min_az = atof(val);
        break;

    case TOK_MAX_AZ:
        rs->max_az = atof(val);
        break;

    case TOK_MIN_EL:
        rs->min_el = atof(val);
        break;

    case TOK_MAX_EL:
        rs->max_el = atof(val);
        break;
    
    case TOK_SOUTH_ZERO:
        rs->south_zero = atoi(val);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/**
 * \brief Get data from rotator state in alpha form
 * \param rot non-null
 * \param token TOK_... specifying which data to get
 * \param val result non-null
 * \return RIG_OK or < 0 if error
 */
int frontrot_get_conf(ROT *rot, token_t token, char *val)
{
    struct rot_state *rs;
    const char *s;

    rs = &rot->state;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (token)
    {
    case TOK_PATHNAME:
        strcpy(val, rs->rotport.pathname);
        break;

    case TOK_WRITE_DELAY:
        sprintf(val, "%d", rs->rotport.write_delay);
        break;

    case TOK_POST_WRITE_DELAY:
        sprintf(val, "%d", rs->rotport.post_write_delay);
        break;

    case TOK_TIMEOUT:
        sprintf(val, "%d", rs->rotport.timeout);
        break;

    case TOK_RETRY:
        sprintf(val, "%d", rs->rotport.retry);
        break;

    case TOK_SERIAL_SPEED:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        sprintf(val, "%d", rs->rotport.parm.serial.rate);
        break;

    case TOK_DATA_BITS:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        sprintf(val, "%d", rs->rotport.parm.serial.data_bits);
        break;

    case TOK_STOP_BITS:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        sprintf(val, "%d", rs->rotport.parm.serial.stop_bits);
        break;

    case TOK_PARITY:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        switch (rs->rotport.parm.serial.parity)
        {
        case RIG_PARITY_NONE:
            s = "None";
            break;

        case RIG_PARITY_ODD:
            s = "Odd";
            break;

        case RIG_PARITY_EVEN:
            s = "Even";
            break;

        case RIG_PARITY_MARK:
            s = "Mark";
            break;

        case RIG_PARITY_SPACE:
            s = "Space";
            break;

        default:
            return -RIG_EINVAL;
        }

        strcpy(val, s);
        break;

    case TOK_HANDSHAKE:
        if (rs->rotport.type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        switch (rs->rotport.parm.serial.handshake)
        {
        case RIG_HANDSHAKE_NONE:
            s = "None";
            break;

        case RIG_HANDSHAKE_XONXOFF:
            s = "XONXOFF";
            break;

        case RIG_HANDSHAKE_HARDWARE:
            s = "Hardware";
            break;

        default:
            return -RIG_EINVAL;
        }

        strcpy(val, s);
        break;

    case TOK_MIN_AZ:
        sprintf(val, "%f", rs->min_az);
        break;

    case TOK_MAX_AZ:
        sprintf(val, "%f", rs->max_az);
        break;

    case TOK_MIN_EL:
        sprintf(val, "%f", rs->min_el);
        break;

    case TOK_MAX_EL:
        sprintf(val, "%f", rs->max_el);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/**
 * \brief Executes cfunc on all the elements stored in the conf table
 * \param rot non-null
 * \param cfunc function(..)
 * \param data
 *
 * start first with backend conf table, then finish with frontend table
 */
int HAMLIB_API rot_token_foreach(ROT *rot,
                                 int (*cfunc)(const struct confparams *,
                                              rig_ptr_t),
                                 rig_ptr_t data)
{
    const struct confparams *cfp;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = rotfrontend_cfg_params; cfp->name; cfp++)
    {
        if ((*cfunc)(cfp, data) == 0)
        {
            return RIG_OK;
        }
    }

    if (rot->caps->port_type == RIG_PORT_SERIAL)
    {
        for (cfp = rotfrontend_serial_cfg_params; cfp->name; cfp++)
        {
            if ((*cfunc)(cfp, data) == 0)
            {
                return RIG_OK;
            }
        }
    }

    for (cfp = rot->caps->cfgparams; cfp && cfp->name; cfp++)
    {
        if ((*cfunc)(cfp, data) == 0)
        {
            return RIG_OK;
        }
    }

    return RIG_OK;
}


/**
 * \brief lookup conf token by its name, return pointer to confparams struct.
 * \param rot
 * \param name
 * \return confparams or NULL
 *
 * lookup backend config table first, then fall back to frontend.
 * TODO: should use Lex to speed it up, strcmp hurts!
 */
const struct confparams * HAMLIB_API rot_confparam_lookup(ROT *rot,
                                                          const char *name)
{
    const struct confparams *cfp;
    token_t token;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called lookup=%s\n", __func__, name);

    if (!rot || !rot->caps)
    {
        return NULL;
    }

    /* 0 returned for invalid format */
    token = strtol(name, NULL, 0);

    for (cfp = rot->caps->cfgparams; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name) || token == cfp->token)
        {
            return cfp;
        }
    }

    for (cfp = rotfrontend_cfg_params; cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name) || token == cfp->token)
        {
            return cfp;
        }
    }

    if (rot->caps->port_type == RIG_PORT_SERIAL)
    {
        for (cfp = rotfrontend_serial_cfg_params; cfp->name; cfp++)
        {
            if (!strcmp(cfp->name, name) || token == cfp->token)
            {
                return cfp;
            }
        }
    }

    return NULL;
}


/**
 * \brief Simple lookup returning token id associated with name
 * \param rot
 * \param name
 * \return token enum
 */
token_t HAMLIB_API rot_token_lookup(ROT *rot, const char *name)
{
    const struct confparams *cfp;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called lookup %s\n", __func__, name);

    cfp = rot_confparam_lookup(rot, name);

    if (!cfp)
    {
        return RIG_CONF_END;
    }

    return cfp->token;
}


/**
 * \brief set a rotator configuration parameter
 * \param rot   The rot handle
 * \param token The parameter
 * \param val   The value to set the parameter to
 *
 *  Sets a configuration parameter.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rot_get_conf()
 */
int HAMLIB_API rot_set_conf(ROT *rot, token_t token, const char *val)
{
    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return -RIG_EINVAL;
    }

    if (rig_need_debug(RIG_DEBUG_VERBOSE))
    {
        const struct confparams *cfp;
        char tokenstr[12];
        sprintf(tokenstr, "%ld", token);
        cfp = rot_confparam_lookup(rot, tokenstr);

        if (!cfp)
        {
            return -RIG_EINVAL;
        }

        rot_debug(RIG_DEBUG_VERBOSE, "%s: %s='%s'\n", __func__, cfp->name, val);
    }

    if (IS_TOKEN_FRONTEND(token))
    {
        return frontrot_set_conf(rot, token, val);
    }

    if (rot->caps->set_conf == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return rot->caps->set_conf(rot, token, val);
}


/**
 * \brief get the value of a configuration parameter
 * \param rot   The rot handle
 * \param token The parameter
 * \param val   The location where to store the value of config \a token
 *
 *  Retrieves the value of a configuration paramter associated with \a token.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rot_set_conf()
 */
int HAMLIB_API rot_get_conf(ROT *rot, token_t token, char *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps || !val)
    {
        return -RIG_EINVAL;
    }

    if (IS_TOKEN_FRONTEND(token))
    {
        return frontrot_get_conf(rot, token, val);
    }

    if (rot->caps->get_conf == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return rot->caps->get_conf(rot, token, val);
}

/** @} */
