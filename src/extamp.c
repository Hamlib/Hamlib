/*
 *  Hamlib Interface - extra parameter interface for amplifiers
 *  Copyright (c) 2000-2008 by Stephane Fillod
 *  Derived from ext.c
 *  Copyright (c) 2019 by Michael Black W9MDB
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
 * \file extamp.c
 * \brief Amplifier extension parameters and levels interface.
 *
 * \author Michael Black
 * \date 2019
 *
 * An open-ended set of extension parameters and levels are available for each
 * amplifier, as provided in the amp_caps::extparms and amp_caps::extlevels
 * lists.  These provide a way to work with amplifier-specific functions that
 * don't fit into the basic "virtual amplifier" of Hamlib.  See
 * `amplifiers/elecraft/kpa.c` for an example.
 */

#include <hamlib/config.h>

#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#include <hamlib/amplifier.h>

#include "token.h"


/**
 * \brief Executes \a cfunc on all the elements stored in the amp_caps::extlevels
 * extension levels table.
 *
 * \param amp The #AMP handle.
 * \param cfunc Callback function of each amp_caps::extlevels.
 * \param data Cookie to be passed to the callback function \a cfunc.
 *
 * The callback function \a cfunc is called until it returns a value which is
 * not strictly positive.
 *
 * \returns A zero value which means a normal end of iteration, or a
 * **negative value** which means an abnormal end,
 *
 * \retval RIG_OK All extension levels elements successfully processed.
 * \retval RIG_EINVAL \a amp or \a cfunc is NULL or inconsistent.
 */
int HAMLIB_API amp_ext_level_foreach(AMP *amp,
                                     int (*cfunc)(AMP *,
                                             const struct confparams *,
                                             amp_ptr_t),
                                     amp_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = amp->caps->extlevels; cfp && cfp->name; cfp++)
    {
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
 * \param cfunc Callback function of each amp_caps::extparms.
 * \param data Cookie to be passed to the callback function \a cfunc.
 *
 * The callback function \a cfunc is called until it returns a value which is not
 * strictly positive.
 *
 * \returns A zero value which means a normal end of iteration, or a
 * **negative value** which means an abnormal end.
 *
 * \retval RIG_OK All extension parameters elements successfully processed.
 * \retval RIG_EINVAL \a amp or \a cfunc is NULL or inconsistent.
 */
int HAMLIB_API amp_ext_parm_foreach(AMP *amp,
                                    int (*cfunc)(AMP *,
                                            const struct confparams *,
                                            amp_ptr_t),
                                    amp_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = amp->caps->extparms; cfp && cfp->name; cfp++)
    {
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
 * \brief Lookup an extension levels or parameters token by its name and return
 * a pointer to the containing #confparams structure member.
 *
 * \param amp The #AMP handle.
 * \param name The extension levels or parameters token name.
 *
 * Searches the amp_caps::extlevels table and then the amp_caps::extparms
 * table for the token by its name.
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
 * \brief Search for an extension levels or parameters token by its constant
 * value and return a pointer to the #confparams structure member.
 *
 * \param amp The #AMP handle.
 * \param token The token value (constant).
 *
 * Searches the amp_caps::extlevels table first and then the
 * amp_caps::extparms for the token by its constant value.
 *
 * \return A pointer to the containing #confparams structure member or NULL if
 * nothing found or if \a amp is NULL or inconsistent.
 */
const struct confparams *HAMLIB_API amp_ext_lookup_tok(AMP *amp, token_t token)
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
 * \return The token ID or RIG_CONF_END if there is a lookup failure.
 *
 * \sa amp_ext_lookup()
 */
token_t HAMLIB_API amp_ext_token_lookup(AMP *amp, const char *name)
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
