/**
 * \addtogroup amplifier
 * @{
 */

/**
 * \file amp_settings.c
 * \brief amplifiter func/level/parm interface
 * \author Stephane Fillod
 * \date 2000-2010
 *
 * Hamlib interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib Interface - amplifier func/level/parm
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>

#include <hamlib/rig.h>
#include <hamlib/amplifier.h>


/**
 * \brief check retrieval ability of level settings
 * \param amp   The amp handle
 * \param level The level settings
 *
 *  Checks if an amp is capable of *getting* a level setting.
 *  Since the \a level is an OR'ed bitwise argument, more than
 *  one level can be checked at the same time.
 *
 *  EXAMPLE: if (amp_has_get_level(my_amp, AMP_LVL_SWR)) disp_SWR();
 *
 * \return a bit map of supported level settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa amp_has_set_level(), amp_get_level()
 */
setting_t HAMLIB_API amp_has_get_level(AMP *amp, setting_t level)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return 0;
    }

    return (amp->state.has_get_level & level);
}

/*! @} */
