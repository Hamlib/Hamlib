/*
 *  Hamlib Interface - amplifier configuration interface
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/**
 * \addtogroup amplifier
 * @{
 */

/**
 * \brief Amplifier Configuration Interface
 * \file amp_conf.c
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#include <hamlib/amplifier.h>

#include "amp_conf.h"
#include "token.h"


/*
 * Configuration options available in the amp->state struct.
 */
static const struct confparams ampfrontend_cfg_params[] =
{
    {
        TOK_PATHNAME, "amp_pathname", "Rig path name",
        "Path name to the device file of the amplifier",
        "/dev/amplifier", RIG_CONF_STRING,
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

    { RIG_CONF_END, NULL, }
};


static const struct confparams ampfrontend_serial_cfg_params[] =
{
#include "serial_cfg_params.h"
};

/** @} */ /* amplifier definitions */


/**
 * \addtogroup amp_internal
 * @{
 */


/**
 * \brief Set an amplifier state value from alpha input.
 *
 * \param amp The #AMP handle.
 * \param token TOK_... specify which value to set.
 * \param val Input.
 *
 * Assumes amp != NULL and val != NULL.
 *
 * \return RIG_OK or a **negative value** error.
 *
 * \retval RIG_OK TOK_... value set successfully.
 * \retval RIG_EINVAL TOK_.. value not set.
 *
 * \sa frontamp_get_conf()
 */
int frontamp_set_conf(AMP *amp, hamlib_token_t token, const char *val)
{
    struct amp_state *rs;
    hamlib_port_t *ampp = AMPPORT(amp);
    int val_i;

    rs = &amp->state;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (token)
    {
    case TOK_PATHNAME:
        strncpy(ampp->pathname, val, HAMLIB_FILPATHLEN - 1);
        strncpy(rs->ampport_deprecated.pathname, val, HAMLIB_FILPATHLEN - 1);
        break;

    case TOK_WRITE_DELAY:
        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        ampp->write_delay = val_i;
        rs->ampport_deprecated.write_delay = val_i;
        break;

    case TOK_POST_WRITE_DELAY:
        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        ampp->post_write_delay = val_i;
        rs->ampport_deprecated.post_write_delay = val_i;
        break;

    case TOK_TIMEOUT:
        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        ampp->timeout = val_i;
        rs->ampport_deprecated.timeout = val_i;
        break;

    case TOK_RETRY:
        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        ampp->retry = val_i;
        rs->ampport_deprecated.retry = val_i;
        break;

    case TOK_SERIAL_SPEED:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        ampp->parm.serial.rate = val_i;
        rs->ampport_deprecated.parm.serial.rate = val_i;
        break;

    case TOK_DATA_BITS:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        ampp->parm.serial.data_bits = val_i;
        rs->ampport_deprecated.parm.serial.data_bits = val_i;
        break;

    case TOK_STOP_BITS:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%d", &val_i))
        {
            return -RIG_EINVAL;
        }

        ampp->parm.serial.stop_bits = val_i;
        rs->ampport_deprecated.parm.serial.stop_bits = val_i;
        break;

    case TOK_PARITY:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "None"))
        {
            val_i = RIG_PARITY_NONE;
        }
        else if (!strcmp(val, "Odd"))
        {
            val_i = RIG_PARITY_ODD;
        }
        else if (!strcmp(val, "Even"))
        {
            val_i = RIG_PARITY_EVEN;
        }
        else if (!strcmp(val, "Mark"))
        {
            val_i = RIG_PARITY_MARK;
        }
        else if (!strcmp(val, "Space"))
        {
            val_i = RIG_PARITY_SPACE;
        }
        else
        {
            return -RIG_EINVAL;
        }

        ampp->parm.serial.parity = val_i;
        rs->ampport_deprecated.parm.serial.parity = val_i;
        break;

    case TOK_HANDSHAKE:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "None"))
        {
            val_i = RIG_HANDSHAKE_NONE;
        }
        else if (!strcmp(val, "XONXOFF"))
        {
            val_i = RIG_HANDSHAKE_XONXOFF;
        }
        else if (!strcmp(val, "Hardware"))
        {
            val_i = RIG_HANDSHAKE_HARDWARE;
        }
        else
        {
            return -RIG_EINVAL;
        }

        ampp->parm.serial.handshake = val_i;
        break;

    case TOK_RTS_STATE:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "Unset"))
        {
            val_i = RIG_SIGNAL_UNSET;
        }
        else if (!strcmp(val, "ON"))
        {
            val_i = RIG_SIGNAL_ON;
        }
        else if (!strcmp(val, "OFF"))
        {
            val_i = RIG_SIGNAL_OFF;
        }
        else
        {
            return -RIG_EINVAL;
        }

        ampp->parm.serial.rts_state = val_i;
        rs->ampport_deprecated.parm.serial.rts_state = val_i;
        break;

    case TOK_DTR_STATE:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "Unset"))
        {
            val_i = RIG_SIGNAL_UNSET;
        }
        else if (!strcmp(val, "ON"))
        {
            val_i = RIG_SIGNAL_ON;
        }
        else if (!strcmp(val, "OFF"))
        {
            val_i = RIG_SIGNAL_OFF;
        }
        else
        {
            return -RIG_EINVAL;
        }

        ampp->parm.serial.dtr_state = val_i;
        rs->ampport_deprecated.parm.serial.dtr_state = val_i;
        break;




#if 0

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
#endif

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/**
 * \brief Query data from an amplifier state in alpha form.
 *
 * \param amp The #AMP handle.
 * \param token TOK_... specify which data to query.
 * \param val Result.
 *
 * Assumes amp != NULL and val != NULL.
 *
 * \return RIG_OK or a **negative value** on error.
 *
 * \retval RIG_OK TOK_... value queried successfully.
 * \retval RIG_EINVAL TOK_.. value not queried.
 *
 * \sa frontamp_set_conf()
 */
int frontamp_get_conf2(AMP *amp, hamlib_token_t token, char *val, int val_len)
{
    hamlib_port_t *ampp = AMPPORT(amp);
    const char *s;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (token)
    {
    case TOK_PATHNAME:
        strncpy(val, ampp->pathname, val_len - 1);
        break;

    case TOK_WRITE_DELAY:
        SNPRINTF(val, val_len, "%d", ampp->write_delay);
        break;

    case TOK_POST_WRITE_DELAY:
        SNPRINTF(val, val_len, "%d", ampp->post_write_delay);
        break;

    case TOK_TIMEOUT:
        SNPRINTF(val, val_len, "%d", ampp->timeout);
        break;

    case TOK_RETRY:
        SNPRINTF(val, val_len, "%d", ampp->retry);
        break;

    case TOK_SERIAL_SPEED:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(val, val_len, "%d", ampp->parm.serial.rate);
        break;

    case TOK_DATA_BITS:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(val, val_len, "%d", ampp->parm.serial.data_bits);
        break;

    case TOK_STOP_BITS:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(val, val_len, "%d", ampp->parm.serial.stop_bits);
        break;

    case TOK_PARITY:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        switch (ampp->parm.serial.parity)
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

        strncpy(val, s, val_len - 1);
        break;

    case TOK_HANDSHAKE:
        if (ampp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        switch (ampp->parm.serial.handshake)
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

        strncpy(val, s, val_len);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}
/** @} */ /* amp_internal definitions */


/**
 * \addtogroup amplifier
 * @{
 */


/**
 * \brief Executes cfunc on all the elements stored in the configuration
 * parameters table.
 *
 * \param amp The #AMP handle.
 * \param cfunc Pointer to the callback function(...).
 * \param data Data for the callback function.
 *
 * Start first with backend configuration parameters table, then finish with
 * frontend configuration parameters table.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The \a cfunc action completed successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent or \a cfunc is NULL.
 */
int HAMLIB_API amp_token_foreach(AMP *amp,
                                 int (*cfunc)(const struct confparams *,
                                         rig_ptr_t),
                                 rig_ptr_t data)
{
    const struct confparams *cfp;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = ampfrontend_cfg_params; cfp->name; cfp++)
    {
        if ((*cfunc)(cfp, data) == 0)
        {
            return RIG_OK;
        }
    }

    if (amp->caps->port_type == RIG_PORT_SERIAL)
    {
        for (cfp = ampfrontend_serial_cfg_params; cfp->name; cfp++)
        {
            if ((*cfunc)(cfp, data) == 0)
            {
                return RIG_OK;
            }
        }
    }

    for (cfp = amp->caps->cfgparams; cfp && cfp->name; cfp++)
    {
        if ((*cfunc)(cfp, data) == 0)
        {
            return RIG_OK;
        }
    }

    return RIG_OK;
}


/**
 * \brief Query an amplifier configuration parameter token by its name.
 *
 * \param amp The #AMP handle.
 * \param name Configuration parameter token name string.
 *
 * Use this function to get a pointer to the token in the #confparams
 * structure.  Searches the backend config params table first, then falls back
 * to the frontend config params table.
 *
 * \return A pointer to the token in the #confparams structure or NULL if
 * \a amp is NULL or inconsistent or if \a name is not found (how can the
 * caller know which occurred?).
 *
 * \sa amp_token_lookup()
 *
 * TODO: Should use Lex to speed it up, strcmp() hurts!
 */
const struct confparams *HAMLIB_API amp_confparam_lookup(AMP *amp,
        const char *name)
{
    const struct confparams *cfp;
    hamlib_token_t token;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return NULL;
    }

    /* 0 returned for invalid format */
    token = strtol(name, NULL, 0);

    for (cfp = amp->caps->cfgparams; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name) || token == cfp->token)
        {
            return cfp;
        }
    }

    for (cfp = ampfrontend_cfg_params; cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name) || token == cfp->token)
        {
            return cfp;
        }
    }

    if (amp->caps->port_type == RIG_PORT_SERIAL)
    {
        for (cfp = ampfrontend_serial_cfg_params; cfp->name; cfp++)
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
 * \brief Search for the token ID associated with an amplifier configuration
 * parameter token name.
 *
 * \param amp The #AMP handle.
 * \param name Configuration parameter token name string.
 *
 * Searches the backend and frontend configuration parameters tables for the
 * token ID.
 *
 * \return The token ID value or #RIG_CONF_END if the lookup failed.
 *
 * \sa amp_confparam_lookup()
 */
hamlib_token_t HAMLIB_API amp_token_lookup(AMP *amp, const char *name)
{
    const struct confparams *cfp;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    cfp = amp_confparam_lookup(amp, name);

    if (!cfp)
    {
        return RIG_CONF_END;
    }

    return cfp->token;
}


/**
 * \brief Set an amplifier configuration parameter.
 *
 * \param amp The #AMP handle.
 * \param token The token of the parameter to set.
 * \param val The value to set the parameter to.
 *
 *  Sets an amplifier configuration parameter to \a val.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The parameter was set successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent or \a token is invalid.
 * \retval RIG_ENAVAIL amp_caps#set_conf() capability is not available.
 *
 * \sa amp_get_conf()
 */
int HAMLIB_API amp_set_conf(AMP *amp, hamlib_token_t token, const char *val)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return -RIG_EINVAL;
    }

    if (rig_need_debug(RIG_DEBUG_VERBOSE))
    {
        const struct confparams *cfp;
        char tokenstr[12];
        SNPRINTF(tokenstr, sizeof(tokenstr), "%ld", token);
        cfp = amp_confparam_lookup(amp, tokenstr);

        if (!cfp)
        {
            return -RIG_EINVAL;
        }

        amp_debug(RIG_DEBUG_VERBOSE, "%s: %s='%s'\n", __func__, cfp->name, val);
    }

    if (IS_TOKEN_FRONTEND(token))
    {
        return frontamp_set_conf(amp, token, val);
    }

    if (amp->caps->set_conf == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->set_conf(amp, token, val);
}


/**
 * \brief Query the value of an amplifier configuration parameter.
 *
 * \param amp The #AMP handle.
 * \param token The token of the parameter to query.
 * \param val The location where to store the value of the configuration \a token.
 *
 * Retrieves the value of a configuration parameter associated with \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK Querying the parameter was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_conf() capability is not available.
 *
 * \sa amp_set_conf()
 */
int HAMLIB_API amp_get_conf2(AMP *amp, hamlib_token_t token, char *val, int val_len)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !val)
    {
        return -RIG_EINVAL;
    }

    if (IS_TOKEN_FRONTEND(token))
    {
        return frontamp_get_conf2(amp, token, val, val_len);
    }

    if (amp->caps->get_conf == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->get_conf(amp, token, val);
}

int HAMLIB_API amp_get_conf(AMP *amp, hamlib_token_t token, char *val)
{
    return amp_get_conf2(amp, token, val, 128);
}
/** @} */
