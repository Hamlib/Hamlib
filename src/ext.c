/*
 *  Hamlib Interface - rig ext parameter interface
 *  Copyright (c) 2000-2008 by Stephane Fillod
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
 * \addtogroup rig
 * @{
 */

/**
 * \file ext.c
 * \brief Extension request parameter interface
 *
 * An open-ended set of extension parameters, functions and levels are available
 * for each rig, as provided in the rigcaps extparms, extfuncs and extlevels lists.
 * These provide a way to work with rig-specific functions that don't fit into the
 * basic "virtual rig" of Hamlib.  See icom/ic746.c for an example.
 */

#include <hamlib/config.h>

#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>

#include "token.h"

static int rig_has_ext_token(RIG *rig, token_t token)
{
    int *ext_tokens = rig->caps->ext_tokens;
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
 * \param rig The rig handle
 * \param cfunc callback function of each extfunc
 * \param data cookie to be passed to \a cfunc callback
 * \brief Executes cfunc on all the elements stored in the extfuncs table
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.  A zero value means a normal end of iteration, and a
 * negative value an abnormal end, which will be the return value of
 * rig_ext_func_foreach.
 */
int HAMLIB_API rig_ext_func_foreach(RIG *rig,
                                    int (*cfunc)(RIG *,
                                            const struct confparams *,
                                            rig_ptr_t),
                                    rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = rig->caps->extfuncs; cfp && cfp->name; cfp++)
    {
        if (!rig_has_ext_token(rig, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(rig, cfp, data);

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
 * \param rig The rig handle
 * \param cfunc callback function of each extlevel
 * \param data cookie to be passed to \a cfunc callback
 * \brief Executes cfunc on all the elements stored in the extlevels table
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.  A zero value means a normal end of iteration, and a
 * negative value an abnormal end, which will be the return value of
 * rig_ext_level_foreach.
 */
int HAMLIB_API rig_ext_level_foreach(RIG *rig,
                                     int (*cfunc)(RIG *,
                                             const struct confparams *,
                                             rig_ptr_t),
                                     rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = rig->caps->extlevels; cfp && cfp->name; cfp++)
    {
        if (!rig_has_ext_token(rig, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(rig, cfp, data);

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
 * \param rig The rig handle
 * \param cfunc callback function of each extparm
 * \param data cookie to be passed to \a cfunc callback
 * \brief Executes cfunc on all the elements stored in the extparms table
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.  A zero value means a normal end of iteration, and a
 * negative value an abnormal end, which will be the return value of
 * rig_ext_parm_foreach.
 */
int HAMLIB_API rig_ext_parm_foreach(RIG *rig,
                                    int (*cfunc)(RIG *,
                                            const struct confparams *,
                                            rig_ptr_t),
                                    rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = rig->caps->extparms; cfp && cfp->name; cfp++)
    {
        if (!rig_has_ext_token(rig, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(rig, cfp, data);

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
 * \param rig
 * \param name
 * \brief lookup ext token by its name, return pointer to confparams struct.
 *
 * Lookup extlevels table, then extfuncs, then extparms.
 *
 * Returns NULL if nothing found
 *
 * TODO: should use Lex to speed it up, strcmp hurts!
 */
const struct confparams *HAMLIB_API rig_ext_lookup(RIG *rig, const char *name)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return NULL;
    }

    for (cfp = rig->caps->extlevels; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name))
        {
            return cfp;
        }
    }

    for (cfp = rig->caps->extfuncs; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name))
        {
            return cfp;
        }
    }

    for (cfp = rig->caps->extparms; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name))
        {
            return cfp;
        }
    }

    return NULL;
}

/**
 * \param rig
 * \param token
 * \brief lookup ext token, return pointer to confparams struct.
 *
 * lookup extlevels table first, then extfuncs, then fall back to extparms.
 *
 * Returns NULL if nothing found
 */
const struct confparams *HAMLIB_API rig_ext_lookup_tok(RIG *rig, token_t token)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return NULL;
    }

    for (cfp = rig->caps->extlevels; cfp && cfp->token; cfp++)
    {
        if (cfp->token == token)
        {
            return cfp;
        }
    }

    for (cfp = rig->caps->extfuncs; cfp && cfp->token; cfp++)
    {
        if (cfp->token == token)
        {
            return cfp;
        }
    }

    for (cfp = rig->caps->extparms; cfp && cfp->token; cfp++)
    {
        if (cfp->token == token)
        {
            return cfp;
        }
    }

    return NULL;
}


/**
 * \param rig
 * \param name
 * \brief Simple lookup returning token id associated with name
 */
token_t HAMLIB_API rig_ext_token_lookup(RIG *rig, const char *name)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    cfp = rig_ext_lookup(rig, name);

    if (!cfp)
    {
        return RIG_CONF_END;
    }

    return cfp->token;
}

/** @} */
