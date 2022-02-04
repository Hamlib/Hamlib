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
 * \brief Rotator extension parameters and levels interface.
 *
 * \author Mikael Nousiainen
 * \date 2020
 *
 * An open-ended set of extension parameters, functions and levels are
 * available for each rotator, as provided in the rot_caps::extparms,
 * rot_caps::extfuncs and rot_caps::extlevels lists.  These provide a way to
 * work with rotator-specific functions that don't fit into the basic "virtual
 * rotator" of Hamlib.
 */

#include <hamlib/config.h>

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
 * \brief Executes \a cfunc on all the elements stored in the
 * rot_caps::extfuncs table.
 *
 * \param rot The #ROT handle.
 * \param cfunc Callback function of each rot_caps::extfunc.
 * \param data Cookie to be passed to the callback function \a cfunc.
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.
 *
 * \returns A zero value means a normal end of iteration, or a **negative
 * value** which means an abnormal end.
 *
 * \retval RIG_OK All extension functions elements successfully processed.
 * \retval RIG_EINVAL \a rot or \a cfunc is NULL or inconsistent.
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
 * \brief Executes \a cfunc on all the elements stored in the
 * rot_caps::extlevels extension levels table.
 *
 * \param rot The #ROT handle.
 * \param cfunc Callback function of each rot_caps::extlevels.
 * \param data Cookie to be passed to the callback function \a cfunc.
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.
 *
 * \returns A zero value which means a normal end of iteration, or a
 * **negative value** which means an abnormal end.
 *
 * \retval RIG_OK All extension levels elements successfully processed.
 * \retval RIG_EINVAL \a rot or \a cfunc is NULL or inconsistent.
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
 * \brief Executes \a cfunc on all the elements stored in the
 * rot_caps::extparms extension parameters table.
 *
 * \param rot The #ROT handle.
 * \param cfunc callback function of each rot_caps::extparms.
 * \param data Cookie to be passed to the callback function \a cfunc.
 *
 * The callback function \a cfunc is called until it returns a value which is not
 * strictly positive.
 *
 * \returns A zero value which means a normal end of iteration, or a
 * **negative value** which means an abnormal end.
 *
 * \retval RIG_OK All extension parameters elements successfully processed.
 * \retval RIG_EINVAL \a rot or \a cfunc is NULL or inconsistent.
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
 * \brief Lookup an extension functions, levels, or parameters token by its
 * name and return a pointer to the containing #confparams structure member.
 *
 * \param rot The #ROT handle.
 * \param name The extension functions, levels, or parameters token name.
 *
 * Searches the rot_caps::extlevels, rot_caps::extfuncs and the
 * rot_caps::extparms tables in order for the token by its name.
 *
 * \note As this function is called by rot_ext_token_lookup(), it can be
 * considered a lower level API.
 *
 * \return A pointer to the containing #confparams structure member or NULL if
 * nothing found or if \a rot is NULL or inconsistent.
 *
 * \sa rot_ext_token_lookup()
 *
 * \todo Should use Lex to speed it up, strcmp() hurts!
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
 * \brief Searches for an extension levels, functions, or parameters token by
 * its constant value and return a pointer to the #confparams structure
 * member.
 *
 * \param rot The #ROT handle.
 * \param token The token value (constant).
 *
 * Searches the rot_caps::extlevels, rot_caps::extfuncs, and the
 * rot_caps::extparms tables in order for the token by its constant value.
 *
 * \return A pointer to the containing #confparams structure member or NULL if
 * nothing found or if \a rot is NULL or inconsistent.
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
 * \brief Simple search returning the extension token ID associated with
 * \a name.
 *
 * \param rot The #ROT handle.
 * \param name The token name string to search.
 *
 * \note As this function calls rot_ext_lookup(), it can be considered a
 * higher level API.
 *
 * \return The token ID or RIG_CONF_END if there is a lookup failure.
 *
 * \sa rot_ext_lookup()
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
