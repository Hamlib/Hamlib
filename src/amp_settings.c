/*
 *  Hamlib Interface - amplifier func/level/parm
 *  Copyright (c) 2000-2010 by Stephane Fillod
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
 * \addtogroup amplifier
 * @{
 */

/**
 * \brief Amplifier function/level/parameter interface.
 *
 * \file amp_settings.c
 *
 * \author Stephane Fillod
 * \date 2000-2010
 * \author Mikael Nousiainen
 * \date 2020
 *
 * This Hamlib interface is a frontend implementing wrapper functions.
 */

#include <hamlib/config.h>

#include <hamlib/rig.h>
#include <hamlib/amplifier.h>


/**
 * \brief Check which level settings can be set.
 *
 * \param amp   The #AMP handle.
 * \param level The level settings bitmap.
 *
 * Checks if an amplifier is capable of *setting* a level setting.  Since the
 * \a level is an OR'ed bitwise argument, more than one level can be checked
 * at the same time.
 *
 * EXAMPLE:
 * \code
 * if (amp_has_set_level(my_amp, AMP_LVL_PWR))
 *     my_disp_PWR();
 * \endcode
 *
 * \return A bit map of supported level settings that can be retrieved,
 * otherwise 0 if none supported or \a amp is NULL or inconsistent.
 *
 * \sa amp_has_set_level(), amp_set_level()
 */
setting_t HAMLIB_API amp_has_set_level(AMP *amp, setting_t level)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return 0;
    }

    return (amp->state.has_set_level & level);
}

/**
 * \brief Check which level settings can be queried.
 *
 * \param amp   The #AMP handle.
 * \param level The level settings bitmap.
 *
 * Checks if an amplifier is capable of *getting* a level setting.  Since the
 * \a level is an OR'ed bitwise argument, more than one level can be checked
 * at the same time.
 *
 * EXAMPLE:
 * \code
 * if (amp_has_get_level(my_amp, AMP_LVL_SWR))
 *     my_disp_SWR();
 * \endcode
 *
 * \return A bit map of supported level settings that can be retrieved,
 * otherwise 0 if none supported or \a amp is NULL or inconsistent.
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
