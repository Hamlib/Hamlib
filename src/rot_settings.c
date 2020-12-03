/**
 * \addtogroup rotator
 * @{
 */

/**
 * \file rot_settings.c
 * \brief rotator func/level/parm interface
 * \author Mikael Nousiainen
 * \date 2020
 *
 * Hamlib interface is a frontend implementing wrapper functions.
 */

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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
 * \brief set a rotator level setting
 * \param rig   The rotator handle
 * \param level The level setting
 * \param val   The value to set the level setting to
 *
 * Sets the level of a setting.
 * The level value \a val can be a float or an integer. See #value_t
 * for more information.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief get the value of a level
 * \param rig   The rotator handle
 * \param level The level setting
 * \param val   The location where to store the value of \a level
 *
 *  Retrieves the value of a \a level.
 *  The level value \a val can be a float or an integer. See #value_t
 *  for more information.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief set a rotator parameter
 * \param rig   The rotator handle
 * \param parm  The parameter
 * \param val   The value to set the parameter
 *
 *  Sets a parameter.
 *  The parameter value \a val can be a float or an integer. See #value_t
 *  for more information.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief get the value of a parameter
 * \param rig   The rotator handle
 * \param parm  The parameter
 * \param val   The location where to store the value of \a parm
 *
 *  Retrieves the value of a \a parm.
 *  The parameter value \a val can be a float or an integer. See #value_t
 *  for more information.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief check retrieval ability of level settings
 * \param rig   The rotator handle
 * \param level The level settings
 *
 *  Checks if a rotator is capable of *getting* a level setting.
 *  Since the \a level is an OR'ed bitwise argument, more than
 *  one level can be checked at the same time.
 *
 *  EXAMPLE: if (rot_has_get_level(my_rig, RIG_LEVEL_SPEED))
 *
 * \return a bit map of supported level settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rot_has_set_level(), rot_get_level()
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
 * \brief check settable ability of level settings
 * \param rig   The rotator handle
 * \param level The level settings
 *
 *  Checks if a rotator can *set* a level setting.
 *  Since the \a level is an OR'ed bitwise argument, more than
 *  one level can be check at the same time.
 *
 *  EXAMPLE: if (rot_has_set_level(my_rig, ROT_LEVEL_SPEED))
 *
 * \return a bit map of supported level settings that can be set,
 * otherwise 0 if none supported.
 *
 * \sa rot_has_get_level(), rot_set_level()
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
 * \brief check retrieval ability of parameter settings
 * \param rig   The rotator handle
 * \param parm  The parameter settings
 *
 *  Checks if a rotator is capable of *getting* a parm setting.
 *  Since the \a parm is an OR'ed bitwise argument, more than
 *  one parameter can be checked at the same time.
 *
 *  EXAMPLE: if (rot_has_get_parm(my_rig, ROT_PARM_NONE))
 *
 * \return a bit map of supported parameter settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rot_has_set_parm(), rot_get_parm()
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
 * \brief check settable ability of parameter settings
 * \param rig   The rotator handle
 * \param parm  The parameter settings
 *
 *  Checks if a rotator can *set* a parameter setting.
 *  Since the \a parm is an OR'ed bitwise argument, more than
 *  one parameter can be check at the same time.
 *
 *  EXAMPLE: if (rot_has_set_parm(my_rig, RIG_PARM_NONE))
 *
 * \return a bit map of supported parameter settings that can be set,
 * otherwise 0 if none supported.
 *
 * \sa rot_has_get_parm(), rot_set_parm()
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
 * \brief check ability of rotator functions
 * \param rig   The rotator handle
 * \param func  The functions
 *
 *  Checks if a rotator supports a set of functions.
 *  Since the \a func is an OR'ed bitwise argument, more than
 *  one function can be checked at the same time.
 *
 *  EXAMPLE: if (rot_has_get_func(my_rig, RIG_FUNC_NONE))
 *
 *  \return a bit map of supported functions,
 *  otherwise 0 if none supported.
 *
 * \sa rot_has_set_func(), rot_get_func()
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
 * \brief check ability of rotator functions
 * \param rig   The rotator handle
 * \param func  The functions
 *
 *  Checks if a rotator supports a set of functions.
 *  Since the \a func is an OR'ed bitwise argument, more than
 *  one function can be checked at the same time.
 *
 *  EXAMPLE: if (rot_has_set_func(my_rig, RIG_FUNC_NONE))
 *
 * \return a bit map of supported functions,
 * otherwise 0 if none supported.
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
 * \brief activate/de-activate functions of rotator
 * \param rig   The rotator handle
 * \param func  The function to activate
 * \param status    The status (on or off) to set to
 *
 * Activate/de-activate a function of the radio.
 *
 * The \a status argument is a non null value for "activate",
 * "de-activate" otherwise, much as TRUE/FALSE definitions in C language.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief get the status of functions of the rotator
 * \param rig   The rotator handle
 * \param func  The function to get the status
 * \param status    The location where to store the function status
 *
 *  Retrieves the status (on/off) of a function of the rotator.
 *  Upon return, \a status will hold the status of the function,
 *  The value pointer to by the \a status argument is a non null
 *  value for "on", "off" otherwise, much as TRUE/FALSE
 *  definitions in C language.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief set a rotator level extra parameter
 * \param rig   The rotator handle
 * \param token The parameter
 * \param val   The value to set the parameter to
 *
 *  Sets an level extra parameter.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief get the value of a level extra parameter
 * \param rig   The rotator handle
 * \param token The parameter
 * \param val   The location where to store the value of \a token
 *
 *  Retrieves the value of a level extra parameter associated with \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief set a rotator function extra parameter
 * \param rig   The rotator handle
 * \param token The parameter
 * \param status The value to set the parameter to
 *
 *  Sets a function extra parameter.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief get the value of a function extra parameter
 * \param rig   The rotator handle
 * \param token The parameter
 * \param status The location where to store the value of \a token
 *
 *  Retrieves the value of a function extra parameter associated with \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief set a rotator parm extra parameter
 * \param rig   The rotator handle
 * \param token The parameter
 * \param val   The value to set the parameter to
 *
 *  Sets an parm extra parameter.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
 * \brief get the value of a parm extra parameter
 * \param rig   The rotator handle
 * \param token The parameter
 * \param val   The location where to store the value of \a token
 *
 *  Retrieves the value of a parm extra parameter associated with \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
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
