/*
 *  Hamlib Interface - rotator func/level/parm
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
 * \file rot_settings.c
 * \brief Rotator functions/levels/parameters interface.
 *
 * \author Mikael Nousiainen
 * \date 2020
 *
 * This Hamlib interface is a frontend implementing wrapper functions.
 */


#include <hamlib/config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <hamlib/rig.h>
#include <hamlib/rotator.h>
#include "cal.h"


#ifndef DOC_HIDDEN

#  define CHECK_ROT_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)

#endif /* !DOC_HIDDEN */


/**
 * \brief Set a rotator level to a given value.
 *
 * \param rot The #ROT handle.
 * \param level The level to set.
 * \param val The value of the level.
 *
 * Set \a level to \a val.
 *
 * \note \a val can be any type defined by #value_t.
 * \note As this function calls rot_has_set_level(), this may be considered a
 * higher level API.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK Setting the level was successful.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#set_level() capability is not available.
 *
 * \sa rot_has_set_level(), rot_get_level()
 */
int HAMLIB_API rot_set_level(ROT *rot, setting_t level, value_t val)
{
    const struct rot_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->set_level == NULL || !rot_has_set_level(rot, level))
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_level(rot, level, val);
}


/**
 * \brief Query the value of a requested rotator level.
 *
 * \param rot The #ROT handle.
 * \param level The requested level.
 * \param val The variable to store the \a level value.
 *
 * Query the \a val corresponding to the \a level.
 *
 * \note \a val can be any type defined by #value_t.
 * \note As this function calls rot_has_get_level(), this may be considered a
 * higher level API.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The query was successful.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#get_level() capability is not available.
 *
 * \sa rot_has_get_level(), rot_set_level()
 */
int HAMLIB_API rot_get_level(ROT *rot, setting_t level, value_t *val)
{
    const struct rot_caps *caps;

    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot) || !val)
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->get_level == NULL || !rot_has_get_level(rot, level))
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_level(rot, level, val);
}


/**
 * \brief Set a rotator parameter to a given value.
 *
 * \param rot The #ROT handle.
 * \param parm The parameter to set.
 * \param val The value of the parameter.
 *
 *  Sets \a parm to \a val.
 *
 * \note \a val can be any type defined by #value_t.
 * \note As this function calls rot_has_set_parm(), this may be considered a
 * higher level API.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The parameter was set successfully.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#set_parm() capability is not available.
 *
 * \sa rot_has_set_parm(), rot_get_parm()
 */
int HAMLIB_API rot_set_parm(ROT *rot, setting_t parm, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    if (rot->caps->set_parm == NULL || !rot_has_set_parm(rot, parm))
    {
        return -RIG_ENAVAIL;
    }

    return rot->caps->set_parm(rot, parm, val);
}


/**
 * \brief Query the value of a requested rotator parameter.
 *
 * \param rot The #ROT handle.
 * \param parm The requested parameter.
 * \param val The variable to store the \a parm value.
 *
 * Query the \a val corresponding to the \a parm.
 *
 * \note \a val can be any type defined by #value_t.
 * \note As this function calls rot_has_get_parm(), this may be considered a
 * higher level API.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The parameter was queried successfully.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#get_parm() capability is not available.
 *
 * \sa rot_has_get_parm(), rot_set_parm()
 */
int HAMLIB_API rot_get_parm(ROT *rot, setting_t parm, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot) || !val)
    {
        return -RIG_EINVAL;
    }

    if (rot->caps->get_parm == NULL || !rot_has_get_parm(rot, parm))
    {
        return -RIG_ENAVAIL;
    }

    return rot->caps->get_parm(rot, parm, val);
}


/**
 * \brief Check which rotator level settings can be queried.
 *
 * \param rot The #ROT handle.
 * \param level The level settings bitmap.
 *
 * Checks if a rotator is capable of *getting* a level setting.  Since
 * \a level is an OR'ed bitwise argument, more than one level can be checked
 * at the same time.
 *
 * EXAMPLE:
 * \code
 * if (rot_has_get_level(my_rot, ROT_LEVEL_SPEED))
 *     my_get_speed(&my_speed);
 * \endcode
 *
 * \note As this function is called by rot_get_level(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported level settings that can be retrieved,
 * otherwise 0 if none supported or \a rot is NULL or inconsistent.
 *
 * \sa rot_get_level(), rot_has_set_level()
 */
setting_t HAMLIB_API rot_has_get_level(ROT *rot, setting_t level)
{
    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return 0;
    }

    return (rot->state.has_get_level & level);
}



/**
 * \brief Query the rotator levels that may be set.
 *
 * \param rot The #ROT handle.
 * \param level The level settings bitmap.
 *
 * Checks if a rotator can *set* a level setting.  Since \a level is an OR'ed
 * bitwise argument, more than one level can be checked at the same time.
 *
 * EXAMPLE:
 * \code
 * if (rot_has_set_level(my_rot, ROT_LEVEL_SPEED))
 *     my_set_speed(MEDIUM);
 * \endcode
 *
 * \note As this function is called by rot_set_level(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported level settings that can be set, otherwise 0
 * if none supported or \a rot is NULL or inconsistent.
 *
 * \sa rot_set_level(), rot_has_get_level()
 */
setting_t HAMLIB_API rot_has_set_level(ROT *rot, setting_t level)
{
    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return 0;
    }

    return (rot->state.has_set_level & level);
}


/**
 * \brief Check which rotator parameter settings can be queried.
 *
 * \param rot The #ROT handle.
 * \param parm The parameter settings bitmap.
 *
 * Checks if a rotator is capable of *getting* a parameter setting.  Since
 * \a parm is an OR'ed bitwise argument, more than one parameter can be
 * checked at the same time.
 *
 * EXAMPLE:
 * \code
 * if (rot_has_get_parm(my_rot, ROT_PARM_NONE))
 *     my_get_parms(&parms);
 * \endcode
 *
 * \note As this function is called by rot_get_parm(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported parameter settings that can be retrieved,
 * otherwise 0 if none supported or \a rot is NULL or inconsistent.
 *
 * \sa rot_get_parm(), rot_has_set_parm()
 */
setting_t HAMLIB_API rot_has_get_parm(ROT *rot, setting_t parm)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return 0;
    }

    return (rot->state.has_get_parm & parm);
}


/**
 * \brief Query the rotator parameters that may be set.
 *
 * \param rot The #ROT handle.
 * \param parm The parameter settings bitmap.
 *
 * Checks if a rotator can *set* a parameter setting.  Since \a parm is an
 * OR'ed bitwise argument, more than one parameter can be checked at the same
 * time.
 *
 * EXAMPLE:
 * \code
 * if (rot_has_set_parm(my_rig, ROT_PARM_NONE))
 *     my_set_parm(parameter);
 * \endcode
 *
 * \note As this function is called by rot_set_parm(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported parameter settings that can be set,
 * otherwise 0 if none supported or \a rot is NULL or inconsistent.
 *
 * \sa rot_set_parm(), rot_has_get_parm()
 */
setting_t HAMLIB_API rot_has_set_parm(ROT *rot, setting_t parm)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return 0;
    }

    return (rot->state.has_set_parm & parm);
}


/**
 * \brief Check which rotator functions can be queried.
 *
 * \param rot The #ROT handle.
 * \param func The functions bitmap.
 *
 * Checks if a rotator supports a set of functions.  Since \a func is an OR'ed
 * bitwise argument, more than one function can be checked at the same time.
 *
 * EXAMPLE:
 * \code
 * if (rot_has_get_func(my_rig, RIG_FUNC_NONE))
 *     do_something();
 * \endcode
 *
 * \note As this function is called by rot_get_func(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported functions that can be retrieved, otherwise 0
 * if none supported or \a rot is NULL or inconsistent.
 *
 * \sa rot_get_func(), rot_has_set_func()
 */
setting_t HAMLIB_API rot_has_get_func(ROT *rot, setting_t func)
{
    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return 0;
    }

    return (rot->state.has_get_func & func);
}


/**
 * \brief Query support of rotator functions.
 *
 * \param rot The #ROT handle.
 * \param func The functions bitmap.
 *
 * Checks if a rotator supports a set of functions.  Since \a func is an OR'ed
 * bitwise argument, more than one function can be checked at the same time.
 *
 * EXAMPLE:
 * \code
 * if (rot_has_set_func(my_rig, RIG_FUNC_NONE))
 *     do_this_func(my_func);
 * \endcode
 *
 * \note As this function is called by rot_set_func(), this may be considered
 * a lower level API.
 *
 * \return A bit map of supported functions that can be set, otherwise 0 if
 * none supported or \a rot is NULL or inconsistent.
 *
 * \sa rot_set_func(), rot_has_get_func()
 */
setting_t HAMLIB_API rot_has_set_func(ROT *rot, setting_t func)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return 0;
    }

    return (rot->state.has_set_func & func);
}


/**
 * \brief Activate or deactivate functions of a rotator.
 *
 * \param rot The #ROT handle.
 * \param func The function to activate or deactivate.
 * \param status The status (On or Off) to set.
 *
 * Activate or deactivate a function of the rotator.
 *
 * The \a status argument is a value that is not NULL for "activate",
 * "deactivate" otherwise, much as TRUE or FALSE boolean definitions in the C
 * language.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The function was activated or deactivated successfully.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#set_func() capability is not available or
 * \a func is not supported.
 *
 * \sa rot_get_func()
 */
int HAMLIB_API rot_set_func(ROT *rot, setting_t func, int status)
{
    const struct rot_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->set_func == NULL || !rot_has_set_func(rot, func))
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_func(rot, func, status);
}


/**
 * \brief Query the status of functions of the rotator.
 *
 * \param rot The #ROT handle.
 * \param func The function to query the status.
 * \param status The variable to store the function status.
 *
 * Retrieves the status (On or Off) of a function of the rotator.  Upon
 * return, \a status will hold the status of the function.  The value pointed
 * to by the \a status argument is not NULL for "On", or "Off" otherwise, much
 * as TRUE or FALSE boolean definitions in the C language.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The function status was queried successfully.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#get_func() capability is not available or
 * \a func is not supported.
 *
 * \sa rot_set_func()
 */
int HAMLIB_API rot_get_func(ROT *rot, setting_t func, int *status)
{
    const struct rot_caps *caps;

    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot) || !func)
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->get_func == NULL || !rot_has_get_func(rot, func))
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_func(rot, func, status);
}


/**
 * \brief Set a rotator extension level to a given value.
 *
 * \param rot The #ROT handle.
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
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#set_ext_level() capability is not available.
 *
 * \sa rot_get_ext_level()
 */
int HAMLIB_API rot_set_ext_level(ROT *rot, token_t token, value_t val)
{
    const struct rot_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->set_ext_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_ext_level(rot, token, val);
}


/**
 * \brief Query the value of a requested rotator extension level.
 *
 * \param rot The #ROT handle.
 * \param token The extension level token.
 * \param val The location where to store the value of \a token.
 *
 * Query the \a val corresponding to the extension level \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension level was queried successfully.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#get_ext_level() capability is not available.
 *
 * \sa rot_set_ext_level()
 */
int HAMLIB_API rot_get_ext_level(ROT *rot, token_t token, value_t *val)
{
    const struct rot_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot) || !val)
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->get_ext_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_ext_level(rot, token, val);
}


/**
 * \brief Activate or deactivate extension functions of a rotator.
 *
 * \param rot The #ROT handle.
 * \param token The extension function to activate or deactivate.
 * \param status The status (On or Off) to set.
 *
 * Activate or deactivate an extension function of the rotator.
 *
 * The \a status argument is a value that is not NULL for "activate",
 * "deactivate" otherwise, much as TRUE or FALSE boolean definitions in the C
 * language.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension function status was set successfully.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#get_ext_func() capability is not available.
 *
 * \sa rot_get_ext_func()
 */
int HAMLIB_API rot_set_ext_func(ROT *rot, token_t token, int status)
{
    const struct rot_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->set_ext_func == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_ext_func(rot, token, status);
}


/**
 * \brief Query the status of extension functions of a rotator.
 *
 * \param rot The #ROT handle.
 * \param token The extension function to query the status.
 * \param status The variable to store the extension function status.
 *
 * Retrieves the status (On or Off) of an extension function of the rotator.
 * Upon return, \a status will hold the status of the extension function.  The
 * value pointed to by the \a status argument is not NULL for "On", or "Off"
 * otherwise, much as TRUE or FALSE boolean definitions in the C language.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The extension function status was queried successfully.
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#get_ext_func() capability is not available or
 * \a token is not supported.
 *
 * \sa rot_set_ext_func()
 */
int HAMLIB_API rot_get_ext_func(ROT *rot, token_t token, int *status)
{
    const struct rot_caps *caps;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot) || !status)
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->get_ext_func == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_ext_func(rot, token, status);
}


/**
 * \brief Set a rotator extension parameter to a given value.
 *
 * \param rot The #ROT handle.
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
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#set_ext_parm() capability is not available.
 *
 * \sa rot_get_ext_parm()
 */
int HAMLIB_API rot_set_ext_parm(ROT *rot, token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    if (rot->caps->set_ext_parm == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return rot->caps->set_ext_parm(rot, token, val);
}


/**
 * \brief Query the value of a requested rotator extension parameter.
 *
 * \param rot The #ROT handle.
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
 * \retval RIG_EINVAL \a rot is NULL or inconsistent.
 * \retval RIG_ENAVAIL rot_caps#get_ext_parm() capability is not available.
 *
 * \sa rot_set_ext_parm()
 */
int HAMLIB_API rot_get_ext_parm(ROT *rot, token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot) || !val)
    {
        return -RIG_EINVAL;
    }

    if (rot->caps->get_ext_parm == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return rot->caps->get_ext_parm(rot, token, val);
}

/*! @} */
