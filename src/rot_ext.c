/*
 *  Hamlib Interface - rotator ext parameter interface
 *  Copyright (c) 2020 by Mikael Nousiainen
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
 * \file rot_ext.c
 * \brief Extension request parameter interface for rotators
 *
 * An open-ended set of extension parameters, functions and levels are available
 * for each rotator, as provided in the rotcaps extparms, extfuncs and extlevels lists.
 * These provide a way to work with rotator-specific functions that don't fit into the
 * basic "virtual rotator" of Hamlib.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include <hamlib/rotator.h>

#include "token.h"

static int rot_has_ext_token(ROT *rot, token_t token)
{
    int *ext_tokens = rot->caps->ext_tokens;
    int i;

    if (ext_tokens == NULL)
    {
        // Assume that all listed extfuncs/extlevels/extparms are supported if
        // the ext_tokens list is not defined to preserve backwards-compatibility
        return 1;
    }

    for (i = 0; ext_tokens[i] != TOK_BACKEND_NONE; i++)
    {
        if (ext_tokens[i] == token)
        {
            return 1;
        }
    }

    return 0;
}


/**
 * \param rot The rotator handle
 * \param cfunc callback function of each extfunc
 * \param data cookie to be passed to \a cfunc callback
 * \brief Executes cfunc on all the elements stored in the extfuncs table
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.  A zero value means a normal end of iteration, and a
 * negative value an abnormal end, which will be the return value of
 * rot_ext_func_foreach.
 */
int HAMLIB_API rot_ext_func_foreach(ROT *rot,
                                    int (*cfunc)(ROT *,
                                            const struct confparams *,
                                            rig_ptr_t),
                                    rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = rot->caps->extfuncs; cfp && cfp->name; cfp++)
    {
        if (!rot_has_ext_token(rot, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(rot, cfp, data);

        if (ret == 0)
        {
            return RIG_OK;
        }

        if (ret < 0)
        {
            return ret;
        }
    }

    return RIG_OK;
}


/**
 * \param rot The rotator handle
 * \param cfunc callback function of each extlevel
 * \param data cookie to be passed to \a cfunc callback
 * \brief Executes cfunc on all the elements stored in the extlevels table
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.  A zero value means a normal end of iteration, and a
 * negative value an abnormal end, which will be the return value of
 * rot_ext_level_foreach.
 */
int HAMLIB_API rot_ext_level_foreach(ROT *rot,
                                     int (*cfunc)(ROT *,
                                             const struct confparams *,
                                             rig_ptr_t),
                                     rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = rot->caps->extlevels; cfp && cfp->name; cfp++)
    {
        if (!rot_has_ext_token(rot, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(rot, cfp, data);

        if (ret == 0)
        {
            return RIG_OK;
        }

        if (ret < 0)
        {
            return ret;
        }
    }

    return RIG_OK;
}


/**
 * \param rot The rotator handle
 * \param cfunc callback function of each extparm
 * \param data cookie to be passed to \a cfunc callback
 * \brief Executes cfunc on all the elements stored in the extparms table
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.  A zero value means a normal end of iteration, and a
 * negative value an abnormal end, which will be the return value of
 * rot_ext_parm_foreach.
 */
int HAMLIB_API rot_ext_parm_foreach(ROT *rot,
                                    int (*cfunc)(ROT *,
                                            const struct confparams *,
                                            rig_ptr_t),
                                    rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = rot->caps->extparms; cfp && cfp->name; cfp++)
    {
        if (!rot_has_ext_token(rot, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(rot, cfp, data);

        if (ret == 0)
        {
            return RIG_OK;
        }

        if (ret < 0)
        {
            return ret;
        }
    }

    return RIG_OK;
}


/**
 * \param rot
 * \param name
 * \brief lookup ext token by its name, return pointer to confparams struct.
 *
 * Lookup extlevels table, then extfuncs, then extparms.
 *
 * Returns NULL if nothing found
 *
 * TODO: should use Lex to speed it up, strcmp hurts!
 */
const struct confparams *HAMLIB_API rot_ext_lookup(ROT *rot, const char *name)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return NULL;
    }

    for (cfp = rot->caps->extlevels; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name))
        {
            return cfp;
        }
    }

    for (cfp = rot->caps->extfuncs; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name))
        {
            return cfp;
        }
    }

    for (cfp = rot->caps->extparms; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name))
        {
            return cfp;
        }
    }

    return NULL;
}

/**
 * \param rot
 * \param token
 * \brief lookup ext token, return pointer to confparams struct.
 *
 * lookup extlevels table first, then extfuncs, then fall back to extparms.
 *
 * Returns NULL if nothing found
 */
const struct confparams *HAMLIB_API rot_ext_lookup_tok(ROT *rot, token_t token)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return NULL;
    }

    for (cfp = rot->caps->extlevels; cfp && cfp->token; cfp++)
    {
        if (cfp->token == token)
        {
            return cfp;
        }
    }

    for (cfp = rot->caps->extfuncs; cfp && cfp->token; cfp++)
    {
        if (cfp->token == token)
        {
            return cfp;
        }
    }

    for (cfp = rot->caps->extparms; cfp && cfp->token; cfp++)
    {
        if (cfp->token == token)
        {
            return cfp;
        }
    }

    return NULL;
}


/**
 * \param rot
 * \param name
 * \brief Simple lookup returning token id associated with name
 */
token_t HAMLIB_API rot_ext_token_lookup(ROT *rot, const char *name)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    cfp = rot_ext_lookup(rot, name);

    if (!cfp)
    {
        return RIG_CONF_END;
    }

    return cfp->token;
}

/** @} */
