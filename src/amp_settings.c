/*
 *  Hamlib Interface - amplifier func/level/parm
 *  Copyright (c) 2000-2010 by Stephane Fillod
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
 * \brief Amplifier function/level/parameter interface.
 *
 * \file amp_settings.c
 *
 * \author Stephane Fillod
 * \date 2000-2010
 * \author Mikael Nousiainen OH3BHX
 * \date 2026
 *
 * This Hamlib interface is a frontend implementing wrapper functions.
 */

#include "hamlib/config.h"

#include "hamlib/rig.h"
#include "hamlib/amplifier.h"
#include "hamlib/amp_state.h"

#define CHECK_AMP_ARG(a) (!(a) || !(a)->caps || !(a)->state.comm_state)


/**
 * \brief Set the value of a requested level.
 *
 * \param amp The #AMP handle.
 * \param level The requested level.
 * \param val The variable to store the \a level value.
 *
 * Set the \a val corresponding to the \a level.
 *
 * \note \a val can be any type defined by #value_t.
 *
 * \return RIG_OK if the operation was successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The query was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_level() capability is not available.
 *
 * \sa amp_set_ext_level()
 */
int HAMLIB_API amp_set_level(AMP *amp, setting_t level, value_t val)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->set_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->set_level(amp, level, val);
}

/**
 * \brief Query the value of a requested level.
 *
 * \param amp The #AMP handle.
 * \param level The requested level.
 * \param val The variable to store the \a level value.
 *
 * Query the \a val corresponding to the \a level.
 *
 * \note \a val can be any type defined by #value_t.
 *
 * \return RIG_OK if the operation was successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The query was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_level() capability is not available.
 *
 * \sa amp_get_ext_level()
 */
int HAMLIB_API amp_get_level(AMP *amp, setting_t level, value_t *val)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp) || !val)
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->get_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->get_level(amp, level, val);
}

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

    return (AMPSTATE(amp)->has_set_level & level);
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

    return (AMPSTATE(amp)->has_get_level & level);
}

/**
 * \brief Set a amplifier parameter to a given value.
 *
 * \param amp The #AMP handle.
 * \param parm The parameter to set.
 * \param val The value of the parameter.
 *
 *  Sets \a parm to \a val.
 *
 * \note \a val can be any type defined by #value_t.
 * \note As this function calls amp_has_set_parm(), this may be considered a
 * higher level API.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The parameter was set successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#set_parm() capability is not available.
 *
 * \sa amp_has_set_parm(), amp_get_parm()
 */
int HAMLIB_API amp_set_parm(AMP *amp, setting_t parm, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->set_parm == NULL || !amp_has_set_parm(amp, parm))
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->set_parm(amp, parm, val);
}


/**
 * \brief Query the value of a requested amplifier parameter.
 *
 * \param amp The #AMP handle.
 * \param parm The requested parameter.
 * \param val The variable to store the \a parm value.
 *
 * Query the \a val corresponding to the \a parm.
 *
 * \note \a val can be any type defined by #value_t.
 * \note As this function calls amp_has_get_parm(), this may be considered a
 * higher level API.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The parameter was queried successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_parm() capability is not available.
 *
 * \sa amp_has_get_parm(), amp_set_parm()
 */
int HAMLIB_API amp_get_parm(AMP *amp, setting_t parm, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp) || !val)
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->get_parm == NULL || !amp_has_get_parm(amp, parm))
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->get_parm(amp, parm, val);
}


/**
 * \brief Check which amplifier parameter settings can be queried.
 *
 * \param amp The #AMP handle.
 * \param parm The parameter settings bitmap.
 *
 * Checks if a amplifier is capable of *getting* a parameter setting.  Since
 * \a parm is an OR'ed bitwise argument, more than one parameter can be
 * checked at the same time.
 *
 * EXAMPLE:
 * \code
 * if (amp_has_get_parm(my_amp, AMP_PARM_NONE))
 *     my_get_parms(&parms);
 * \endcode
 *
 * \note As this function is called by amp_get_parm(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported parameter settings that can be retrieved,
 * otherwise 0 if none supported or \a amp is NULL or inconsistent.
 *
 * \sa amp_get_parm(), amp_has_set_parm()
 */
setting_t HAMLIB_API amp_has_get_parm(AMP *amp, setting_t parm)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return 0;
    }

    return (amp->state.has_get_parm & parm);
}


/**
 * \brief Query the amplifier parameters that may be set.
 *
 * \param amp The #AMP handle.
 * \param parm The parameter settings bitmap.
 *
 * Checks if a amplifier can *set* a parameter setting.  Since \a parm is an
 * OR'ed bitwise argument, more than one parameter can be checked at the same
 * time.
 *
 * EXAMPLE:
 * \code
 * if (amp_has_set_parm(my_rig, AMP_PARM_NONE))
 *     my_set_parm(parameter);
 * \endcode
 *
 * \note As this function is called by amp_set_parm(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported parameter settings that can be set,
 * otherwise 0 if none supported or \a amp is NULL or inconsistent.
 *
 * \sa amp_set_parm(), amp_has_get_parm()
 */
setting_t HAMLIB_API amp_has_set_parm(AMP *amp, setting_t parm)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return 0;
    }

    return (amp->state.has_set_parm & parm);
}


/**
 * \brief Check which amplifier functions can be queried.
 *
 * \param amp The #AMP handle.
 * \param func The functions bitmap.
 *
 * Checks if a amplifier supports a set of functions.  Since \a func is an OR'ed
 * bitwise argument, more than one function can be checked at the same time.
 *
 * EXAMPLE:
 * \code
 * if (amp_has_get_func(my_rig, AMP_FUNC_NONE))
 *     do_something();
 * \endcode
 *
 * \note As this function is called by amp_get_func(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported functions that can be retrieved, otherwise 0
 * if none supported or \a amp is NULL or inconsistent.
 *
 * \sa amp_get_func(), amp_has_set_func()
 */
setting_t HAMLIB_API amp_has_get_func(AMP *amp, setting_t func)
{
    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return 0;
    }

    return (amp->state.has_get_func & func);
}


/**
 * \brief Query support of amplifier functions.
 *
 * \param amp The #AMP handle.
 * \param func The functions bitmap.
 *
 * Checks if a amplifier supports a set of functions.  Since \a func is an OR'ed
 * bitwise argument, more than one function can be checked at the same time.
 *
 * EXAMPLE:
 * \code
 * if (amp_has_set_func(my_rig, AMP_FUNC_NONE))
 *     do_this_func(my_func);
 * \endcode
 *
 * \note As this function is called by amp_set_func(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported functions that can be set, otherwise 0 if
 * none supported or \a amp is NULL or inconsistent.
 *
 * \sa amp_set_func(), amp_has_get_func()
 */
setting_t HAMLIB_API amp_has_set_func(AMP *amp, setting_t func)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return 0;
    }

    return (amp->state.has_set_func & func);
}


/**
 * \brief Activate or deactivate functions of a amplifier.
 *
 * \param amp The #AMP handle.
 * \param func The function to activate or deactivate.
 * \param status The status (On or Off) to set.
 *
 * Activate or deactivate a function of the amplifier.
 *
 * The \a status argument is a value that is not NULL for "activate",
 * "deactivate" otherwise, much as TRUE or FALSE boolean definitions in the C
 * language.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The function was activated or deactivated successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#set_func() capability is not available or
 * \a func is not supported.
 *
 * \sa amp_get_func()
 */
int HAMLIB_API amp_set_func(AMP *amp, setting_t func, int status)
{
    const struct amp_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->set_func == NULL || !amp_has_set_func(amp, func))
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_func(amp, func, status);
}


/**
 * \brief Query the status of functions of the amplifier.
 *
 * \param amp The #AMP handle.
 * \param func The function to query the status.
 * \param status The variable to store the function status.
 *
 * Retrieves the status (On or Off) of a function of the amplifier.  Upon
 * return, \a status will hold the status of the function.  The value pointed
 * to by the \a status argument is not NULL for "On", or "Off" otherwise, much
 * as TRUE or FALSE boolean definitions in the C language.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The function status was queried successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_func() capability is not available or
 * \a func is not supported.
 *
 * \sa amp_set_func()
 */
int HAMLIB_API amp_get_func(AMP *amp, setting_t func, int *status)
{
    const struct amp_caps *caps;

    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp) || !func)
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->get_func == NULL || !amp_has_get_func(amp, func))
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_func(amp, func, status);
}


/**
 * \brief Set a amplifier extension level to a given value.
 *
 * \param amp The #AMP handle.
 * \param token The extension level token.
 * \param val The value of the extension level.
 *
 * Set extension level \a token to \a val.
 *
 * \note \a val can be any type defined by #value_t.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension level was set successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#set_ext_level() capability is not available.
 *
 * \sa amp_get_ext_level()
 */
int HAMLIB_API amp_set_ext_level(AMP *amp, token_t token, value_t val)
{
    const struct amp_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->set_ext_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_ext_level(amp, token, val);
}


/**
 * \brief Query the value of a requested amplifier extension level.
 *
 * \param amp The #AMP handle.
 * \param token The extension level token.
 * \param val The location where to store the value of \a token.
 *
 * Query the \a val corresponding to the extension level \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension level was queried successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_ext_level() capability is not available.
 *
 * \sa amp_set_ext_level()
 */
int HAMLIB_API amp_get_ext_level(AMP *amp, token_t token, value_t *val)
{
    const struct amp_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp) || !val)
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->get_ext_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_ext_level(amp, token, val);
}


/**
 * \brief Activate or deactivate extension functions of a amplifier.
 *
 * \param amp The #AMP handle.
 * \param token The extension function to activate or deactivate.
 * \param status The status (On or Off) to set.
 *
 * Activate or deactivate an extension function of the amplifier.
 *
 * The \a status argument is a value that is not NULL for "activate",
 * "deactivate" otherwise, much as TRUE or FALSE boolean definitions in the C
 * language.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension function status was set successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_ext_func() capability is not available.
 *
 * \sa amp_get_ext_func()
 */
int HAMLIB_API amp_set_ext_func(AMP *amp, token_t token, int status)
{
    const struct amp_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->set_ext_func == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_ext_func(amp, token, status);
}


/**
 * \brief Query the status of extension functions of a amplifier.
 *
 * \param amp The #AMP handle.
 * \param token The extension function to query the status.
 * \param status The variable to store the extension function status.
 *
 * Retrieves the status (On or Off) of an extension function of the amplifier.
 * Upon return, \a status will hold the status of the extension function.  The
 * value pointed to by the \a status argument is not NULL for "On", or "Off"
 * otherwise, much as TRUE or FALSE boolean definitions in the C language.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension function status was queried successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_ext_func() capability is not available or
 * \a token is not supported.
 *
 * \sa amp_set_ext_func()
 */
int HAMLIB_API amp_get_ext_func(AMP *amp, token_t token, int *status)
{
    const struct amp_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp) || !status)
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->get_ext_func == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_ext_func(amp, token, status);
}


/**
 * \brief Set a amplifier extension parameter to a given value.
 *
 * \param amp The #AMP handle.
 * \param token The extension parameter token.
 * \param val The value of the extension parameter.
 *
 * Set an extension parameter \a token to \a val.
 *
 * \note \a val can be any type defined by #value_t.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension parameter was set successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#set_ext_parm() capability is not available.
 *
 * \sa amp_get_ext_parm()
 */
int HAMLIB_API amp_set_ext_parm(AMP *amp, token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->set_ext_parm == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->set_ext_parm(amp, token, val);
}


/**
 * \brief Query the value of a requested amplifier extension parameter.
 *
 * \param amp The #AMP handle.
 * \param token The extension parameter to query the status.
 * \param val The variable to store the extension parameter status.
 *
 * Query the \a val corresponding to the extension parameter \a token.
 *
 * \note \a val can be any type defined by #value_t.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension parameter was queried successfully.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_ext_parm() capability is not available.
 *
 * \sa amp_set_ext_parm()
 */
int HAMLIB_API amp_get_ext_parm(AMP *amp, token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp) || !val)
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->get_ext_parm == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->get_ext_parm(amp, token, val);
}

/*! @} */
