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
 * \addtogroup amp
 * @{
 */

/**
 * \file ext.c
 * \brief Extension request parameter interface
 *
 * An open-ended set of extension parameters and levels are available for each
 * amp, as provided in the ampcaps extparms and extlevels lists.  These
 * provide a way to work with amp-specific functions that don't fit into the
 * basic "virtual amp" of Hamlib.  See amplifiers/kpa.c for an example.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include <hamlib/amplifier.h>

#include "token.h"


/**
 * \param amp The amp handle
 * \param cfunc callback function of each extlevel
 * \param data cookie to be passed to \a cfunc callback
 * \brief Executes cfunc on all the elements stored in the extlevels table
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.  A zero value means a normal end of iteration, and a
 * negative value an abnormal end, which will be the return value of
 * amp_ext_level_foreach.
 */
int HAMLIB_API amp_ext_level_foreach(AMP *amp,
                                     int (*cfunc)(AMP *,
                                                  const struct confparams *,
                                                  amp_ptr_t),
                                     amp_ptr_t data)
{
    const struct confparams *cfp;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = amp->caps->extlevels; cfp && cfp->name; cfp++)
    {
        ret = (*cfunc)(amp, cfp, data);

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
 * \param amp The amp handle
 * \param cfunc callback function of each extparm
 * \param data cookie to be passed to \a cfunc callback
 * \brief Executes cfunc on all the elements stored in the extparms table
 *
 * The callback \a cfunc is called until it returns a value which is not
 * strictly positive.  A zero value means a normal end of iteration, and a
 * negative value an abnormal end, which will be the return value of
 * amp_ext_parm_foreach.
 */
int HAMLIB_API amp_ext_parm_foreach(AMP *amp,
                                    int (*cfunc)(AMP *,
                                                 const struct confparams *,
                                                 amp_ptr_t),
                                    amp_ptr_t data)
{
    const struct confparams *cfp;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = amp->caps->extparms; cfp && cfp->name; cfp++)
    {
        ret = (*cfunc)(amp, cfp, data);

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
 * \param amp
 * \param name
 * \brief lookup ext token by its name, return pointer to confparams struct.
 *
 * Lookup extlevels table first, then fall back to extparms.
 *
 * Returns NULL if nothing found
 *
 * TODO: should use Lex to speed it up, strcmp hurts!
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
 * \param amp
 * \param token
 * \brief lookup ext token, return pointer to confparams struct.
 *
 * lookup extlevels table first, then fall back to extparms.
 *
 * Returns NULL if nothing found
 */
const struct confparams * HAMLIB_API amp_ext_lookup_tok(AMP *amp, token_t token)
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
 * \param amp
 * \param name
 * \brief Simple lookup returning token id assicated with name
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
