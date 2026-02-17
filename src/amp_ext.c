/*
 *  Hamlib Interface - extra parameter interface for amplifiers
 *  Copyright (c) 2000-2008 by Stephane Fillod
 *  Derived from ext.c and rot_ext.c
 *  Copyright (c) 2019 by Michael Black W9MDB
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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
 * \addtogroup amplifier
 * @{
 */

/**
 * \file amp_ext.c
 * \brief Amplifier extension parameters and levels interface.
 *
 * \author Mikael Nousiainen OH3BHX
 * \date 2026
 *
 * An open-ended set of extension parameters, functions and levels are
 * available for each amplifier, as provided in the amp_caps::extparms,
 * amp_caps::extfuncs and amp_caps::extlevels lists.  These provide a way to
 * work with amplifier-specific functions that don't fit into the basic "virtual
 * amplifier" of Hamlib.
 */

#include "hamlib/config.h"

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "hamlib/amplifier.h"

#include "token.h"

static int amp_has_ext_token(AMP *amp, token_t token)
{
    const int *ext_tokens = amp->caps->ext_tokens;
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
 * amp_caps::extfuncs table.
 *
 * \param amp The #AMP handle.
 * \param cfunc Callback function of each amp_caps::extfunc.
 * \param data Cookie to be passed to the callback function \a cfunc.
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.
 *
 * \returns A zero value means a normal end of iteration, or a **negative
 * value** which means an abnormal end.
 *
 * \retval RIG_OK All extension functions elements successfully processed.
 * \retval RIG_EINVAL \a amp or \a cfunc is NULL or inconsistent.
 */
int HAMLIB_API amp_ext_func_foreach(AMP *amp,
                                    int (*cfunc)(AMP *,
                                            const struct confparams *,
                                            rig_ptr_t),
                                    rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = amp->caps->extfuncs; cfp && cfp->name; cfp++)
    {
        if (!amp_has_ext_token(amp, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(amp, cfp, data);

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
 * amp_caps::extlevels extension levels table.
 *
 * \param amp The #AMP handle.
 * \param cfunc Callback function of each amp_caps::extlevels.
 * \param data Cookie to be passed to the callback function \a cfunc.
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.
 *
 * \returns A zero value which means a normal end of iteration, or a
 * **negative value** which means an abnormal end.
 *
 * \retval RIG_OK All extension levels elements successfully processed.
 * \retval -RIG_EINVAL \a amp or \a cfunc is NULL or inconsistent.
 */
int HAMLIB_API amp_ext_level_foreach(AMP *amp,
                                     int (*cfunc)(AMP *,
                                             const struct confparams *,
                                             rig_ptr_t),
                                     rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = amp->caps->extlevels; cfp && cfp->name; cfp++)
    {
        if (!amp_has_ext_token(amp, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(amp, cfp, data);

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
 * amp_caps::extparms extension parameters table.
 *
 * \param amp The #AMP handle.
 * \param cfunc callback function of each amp_caps::extparms.
 * \param data Cookie to be passed to the callback function \a cfunc.
 *
 * The callback function \a cfunc is called until it returns a value which is not
 * strictly positive.
 *
 * \returns A zero value which means a normal end of iteration, or a
 * **negative value** which means an abnormal end.
 *
 * \retval RIG_OK All extension parameters elements successfully processed.
 * \retval -RIG_EINVAL \a amp or \a cfunc is NULL or inconsistent.
 */
int HAMLIB_API amp_ext_parm_foreach(AMP *amp,
                                    int (*cfunc)(AMP *,
                                            const struct confparams *,
                                            rig_ptr_t),
                                    rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = amp->caps->extparms; cfp && cfp->name; cfp++)
    {
        if (!amp_has_ext_token(amp, cfp->token))
        {
            continue;
        }

        int ret = (*cfunc)(amp, cfp, data);

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
 * \param amp The #AMP handle.
 * \param name The extension functions, levels, or parameters token name.
 *
 * Searches the amp_caps::extlevels, amp_caps::extfuncs and the
 * amp_caps::extparms tables in order for the token by its name.
 *
 * \note As this function is called by amp_ext_token_lookup(), it can be
 * considered a lower level API.
 *
 * \return A pointer to the containing #confparams structure member or NULL if
 * nothing found or if \a amp is NULL or inconsistent.
 *
 * \sa amp_ext_token_lookup()
 *
 * \todo Should use Lex to speed it up, strcmp() hurts!
 */
const struct confparams *HAMLIB_API amp_ext_lookup(AMP *amp, const char *name)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return NULL;
    }

    for (cfp = amp->caps->extlevels; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name))
        {
            return cfp;
        }
    }

    for (cfp = amp->caps->extfuncs; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name))
        {
            return cfp;
        }
    }

    for (cfp = amp->caps->extparms; cfp && cfp->name; cfp++)
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
 * \param amp The #AMP handle.
 * \param token The token value (constant).
 *
 * Searches the amp_caps::extlevels, amp_caps::extfuncs, and the
 * amp_caps::extparms tables in order for the token by its constant value.
 *
 * \return A pointer to the containing #confparams structure member or NULL if
 * nothing found or if \a amp is NULL or inconsistent.
 */
const struct confparams *HAMLIB_API amp_ext_lookup_tok(AMP *amp,
        hamlib_token_t token)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return NULL;
    }

    for (cfp = amp->caps->extlevels; cfp && cfp->token; cfp++)
    {
        if (cfp->token == token)
        {
            return cfp;
        }
    }

    for (cfp = amp->caps->extfuncs; cfp && cfp->token; cfp++)
    {
        if (cfp->token == token)
        {
            return cfp;
        }
    }

    for (cfp = amp->caps->extparms; cfp && cfp->token; cfp++)
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
 * \param amp The #AMP handle.
 * \param name The token name string to search.
 *
 * \note As this function calls amp_ext_lookup(), it can be considered a
 * higher level API.
 *
 * \return The token ID or #RIG_CONF_END if there is a lookup failure.
 *
 * \sa amp_ext_lookup()
 */
hamlib_token_t HAMLIB_API amp_ext_token_lookup(AMP *amp, const char *name)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    cfp = amp_ext_lookup(amp, name);

    if (!cfp)
    {
        return RIG_CONF_END;
    }

    return cfp->token;
}

/** @} */
