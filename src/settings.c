/**
 * \addtogroup rig
 * @{
 */

/**
 * \file settings.c
 * \brief func/level/parm interface
 * \author Stephane Fillod
 * \date 2000-2010
 *
 * Hamlib interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib Interface - func/level/parm
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

#include <hamlib/config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>   /* Error number definitions */

#include <hamlib/rig.h>
#include "cal.h"
#include "misc.h"


#ifndef DOC_HIDDEN

#  define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)

#endif /* !DOC_HIDDEN */


/**
 * \brief set a radio level setting
 * \param rig   The rig handle
 * \param vfo   The target VFO
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
 * \sa rig_has_set_level(), rig_get_level()
 */
int HAMLIB_API rig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->set_level == NULL || !rig_has_set_level(rig, level))
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_LEVEL)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        return caps->set_level(rig, vfo, level, val);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->set_level(rig, vfo, level, val);
    caps->set_vfo(rig, curr_vfo);
    return retcode;
}


/**
 * \brief get the value of a level
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param level The level setting
 * \param val   The location where to store the value of \a level
 *
 *  Retrieves the value of a \a level.
 *  The level value \a val can be a float or an integer. See #value_t
 *  for more information.
 *
 *      RIG_LEVEL_STRENGTH: \a val is an integer, representing the S Meter
 *      level in dB relative to S9, according to the ideal S Meter scale.
 *      The ideal S Meter scale is as follow: S0=-54, S1=-48, S2=-42, S3=-36,
 *      S4=-30, S5=-24, S6=-18, S7=-12, S8=-6, S9=0, +10=10, +20=20,
 *      +30=30, +40=40, +50=50 and +60=60. This is the responsibility
 *      of the backend to return values calibrated for this scale.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_has_get_level(), rig_set_level()
 */
int HAMLIB_API rig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !val)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->get_level == NULL || !rig_has_get_level(rig, level))
    {
        return -RIG_ENAVAIL;
    }

    /*
     * Special case(frontend emulation): calibrated S-meter reading
     */
    if (level == RIG_LEVEL_STRENGTH
            && (caps->has_get_level & RIG_LEVEL_STRENGTH) == 0
            && rig_has_get_level(rig, RIG_LEVEL_RAWSTR)
            && rig->state.str_cal.size)
    {

        value_t rawstr;

        retcode = rig_get_level(rig, vfo, RIG_LEVEL_RAWSTR, &rawstr);

        if (retcode != RIG_OK)
        {
            return retcode;
        }

        val->i = (int)rig_raw2val(rawstr.i, &rig->state.str_cal);
        return RIG_OK;
    }


    if ((caps->targetable_vfo & RIG_TARGETABLE_LEVEL)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {

        return caps->get_level(rig, vfo, level, val);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->get_level(rig, vfo, level, val);
    caps->set_vfo(rig, curr_vfo);
    return retcode;
}


/**
 * \brief set a radio parameter
 * \param rig   The rig handle
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
 * \sa rig_has_set_parm(), rig_get_parm()
 */
int HAMLIB_API rig_set_parm(RIG *rig, setting_t parm, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    if (rig->caps->set_parm == NULL || !rig_has_set_parm(rig, parm))
    {
        return -RIG_ENAVAIL;
    }

    return rig->caps->set_parm(rig, parm, val);
}


/**
 * \brief get the value of a parameter
 * \param rig   The rig handle
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
 * \sa rig_has_get_parm(), rig_set_parm()
 */
int HAMLIB_API rig_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !val)
    {
        return -RIG_EINVAL;
    }

    if (rig->caps->get_parm == NULL || !rig_has_get_parm(rig, parm))
    {
        return -RIG_ENAVAIL;
    }

    return rig->caps->get_parm(rig, parm, val);
}


/**
 * \brief check retrieval ability of level settings
 * \param rig   The rig handle
 * \param level The level settings
 *
 *  Checks if a rig is capable of *getting* a level setting.
 *  Since the \a level is an OR'ed bitwise argument, more than
 *  one level can be checked at the same time.
 *
 *  EXAMPLE: if (rig_has_get_level(my_rig, RIG_LVL_STRENGTH)) disp_Smeter();
 *
 * \return a bit map of supported level settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rig_has_set_level(), rig_get_level()
 */
setting_t HAMLIB_API rig_has_get_level(RIG *rig, setting_t level)
{
    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return 0;
    }

    return (rig->state.has_get_level & level);
}


/**
 * \brief check settable ability of level settings
 * \param rig   The rig handle
 * \param level The level settings
 *
 *  Checks if a rig can *set* a level setting.
 *  Since the \a level is an OR'ed bitwise argument, more than
 *  one level can be check at the same time.
 *
 *  EXAMPLE: if (rig_has_set_level(my_rig, RIG_LVL_RFPOWER)) crank_tx();
 *
 * \return a bit map of supported level settings that can be set,
 * otherwise 0 if none supported.
 *
 * \sa rig_has_get_level(), rig_set_level()
 */
setting_t HAMLIB_API rig_has_set_level(RIG *rig, setting_t level)
{
    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return 0;
    }

    return (rig->state.has_set_level & level);
}


/**
 * \brief check retrieval ability of parameter settings
 * \param rig   The rig handle
 * \param parm  The parameter settings
 *
 *  Checks if a rig is capable of *getting* a parm setting.
 *  Since the \a parm is an OR'ed bitwise argument, more than
 *  one parameter can be checked at the same time.
 *
 *  EXAMPLE: if (rig_has_get_parm(my_rig, RIG_PARM_ANN)) good4you();
 *
 * \return a bit map of supported parameter settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rig_has_set_parm(), rig_get_parm()
 */
setting_t HAMLIB_API rig_has_get_parm(RIG *rig, setting_t parm)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return 0;
    }

    return (rig->state.has_get_parm & parm);
}


/**
 * \brief check settable ability of parameter settings
 * \param rig   The rig handle
 * \param parm  The parameter settings
 *
 *  Checks if a rig can *set* a parameter setting.
 *  Since the \a parm is an OR'ed bitwise argument, more than
 *  one parameter can be check at the same time.
 *
 *  EXAMPLE: if (rig_has_set_parm(my_rig, RIG_PARM_ANN)) announce_all();
 *
 * \return a bit map of supported parameter settings that can be set,
 * otherwise 0 if none supported.
 *
 * \sa rig_has_get_parm(), rig_set_parm()
 */
setting_t HAMLIB_API rig_has_set_parm(RIG *rig, setting_t parm)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return 0;
    }

    return (rig->state.has_set_parm & parm);
}


/**
 * \brief check ability of radio functions
 * \param rig   The rig handle
 * \param func  The functions
 *
 *  Checks if a rig supports a set of functions.
 *  Since the \a func is an OR'ed bitwise argument, more than
 *  one function can be checked at the same time.
 *
 *  EXAMPLE: if (rig_has_get_func(my_rig,RIG_FUNC_FAGC)) disp_fagc_button();
 *
 *  \return a bit map of supported functions,
 *  otherwise 0 if none supported.
 *
 * \sa rig_has_set_func(), rig_get_func()
 */
setting_t HAMLIB_API rig_has_get_func(RIG *rig, setting_t func)
{
    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return 0;
    }

    return (rig->state.has_get_func & func);
}


/**
 * \brief check ability of radio functions
 * \param rig   The rig handle
 * \param func  The functions
 *
 *  Checks if a rig supports a set of functions.
 *  Since the \a func is an OR'ed bitwise argument, more than
 *  one function can be checked at the same time.
 *
 *  EXAMPLE: if (rig_has_set_func(my_rig,RIG_FUNC_FAGC)) disp_fagc_button();
 *
 * \return a bit map of supported functions,
 * otherwise 0 if none supported.
 *
 * \sa rig_set_func(), rig_has_get_func()
 */
setting_t HAMLIB_API rig_has_set_func(RIG *rig, setting_t func)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return 0;
    }

    return (rig->state.has_set_func & func);
}


/**
 * \brief activate/de-activate functions of radio
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param func  The functions to activate
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
 * \sa rig_get_func()
 */
int HAMLIB_API rig_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->set_func == NULL || !rig_has_set_func(rig, func))
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_FUNC)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {

        return caps->set_func(rig, vfo, func, status);
    }
    else
    {
        int targetable = caps->targetable_vfo & RIG_TARGETABLE_FUNC;
        rig_debug(RIG_DEBUG_TRACE, "%s: targetable=%d, vfo=%s, currvfo=%s\n", __func__,
                  targetable, rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->set_func(rig, vfo, func, status);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}


/**
 * \brief get the status of functions of the radio
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param func  The functions to get the status
 * \param status    The location where to store the function status
 *
 *  Retrieves the status (on/off) of a function of the radio.
 *  Upon return, \a status will hold the status of the function,
 *  The value pointer to by the \a status argument is a non null
 *  value for "on", "off" otherwise, much as TRUE/FALSE
 *  definitions in C language.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_func()
 */
int HAMLIB_API rig_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !func)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->get_func == NULL || !rig_has_get_func(rig, func))
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_FUNC)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {

        return caps->get_func(rig, vfo, func, status);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->get_func(rig, vfo, func, status);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}


/**
 * \brief set a radio level extra parameter
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param token The parameter
 * \param val   The value to set the parameter to
 *
 *  Sets an level extra parameter.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_ext_level()
 */
int HAMLIB_API rig_set_ext_level(RIG *rig,
                                 vfo_t vfo,
                                 token_t token,
                                 value_t val)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->set_ext_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_LEVEL)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {

        return caps->set_ext_level(rig, vfo, token, val);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->set_ext_level(rig, vfo, token, val);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}


/**
 * \brief get the value of a level extra parameter
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param token The parameter
 * \param val   The location where to store the value of \a token
 *
 *  Retrieves the value of a level extra parameter associated with \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_ext_level()
 */
int HAMLIB_API rig_get_ext_level(RIG *rig,
                                 vfo_t vfo,
                                 token_t token,
                                 value_t *val)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !val)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->get_ext_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_LEVEL)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {

        return caps->get_ext_level(rig, vfo, token, val);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->get_ext_level(rig, vfo, token, val);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}

/**
 * \brief set a radio function extra parameter
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param token The parameter
 * \param status The value to set the parameter to
 *
 *  Sets a function extra parameter.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_ext_func()
 */
int HAMLIB_API rig_set_ext_func(RIG *rig,
                                vfo_t vfo,
                                token_t token,
                                int status)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->set_ext_func == NULL)
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_FUNC)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {

        return caps->set_ext_func(rig, vfo, token, status);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->set_ext_func(rig, vfo, token, status);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}


/**
 * \brief get the value of a function extra parameter
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param token The parameter
 * \param status The location where to store the value of \a token
 *
 *  Retrieves the value of a function extra parameter associated with \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_ext_func()
 */
int HAMLIB_API rig_get_ext_func(RIG *rig,
                                vfo_t vfo,
                                token_t token,
                                int *status)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !status)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->get_ext_func == NULL)
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_FUNC)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {

        return caps->get_ext_func(rig, vfo, token, status);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->get_ext_func(rig, vfo, token, status);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}


/**
 * \brief set a radio parm extra parameter
 * \param rig   The rig handle
 * \param token The parameter
 * \param val   The value to set the parameter to
 *
 *  Sets an parm extra parameter.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_ext_parm()
 */
int HAMLIB_API rig_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    if (rig->caps->set_ext_parm == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return rig->caps->set_ext_parm(rig, token, val);
}


/**
 * \brief get the value of a parm extra parameter
 * \param rig   The rig handle
 * \param token The parameter
 * \param val   The location where to store the value of \a token
 *
 *  Retrieves the value of a parm extra parameter associated with \a token.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_ext_parm()
 */
int HAMLIB_API rig_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !val)
    {
        return -RIG_EINVAL;
    }

    if (rig->caps->get_ext_parm == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return rig->caps->get_ext_parm(rig, token, val);
}


/**
 * \brief basically convert setting_t expressed 2^n to n
 * \param s The setting to convert to
 *
 *  Converts a setting_t value expressed by 2^n to the value of n.
 *
 * \return the index such that 2^n is the setting, otherwise 0
 * if the setting was not found.
 */
int HAMLIB_API rig_setting2idx(setting_t s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        if (s & rig_idx2setting(i))
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: idx=%d\n", __func__, i);
            return i;
        }
    }

    return 0;
}

#include <unistd.h>  /* UNIX standard function definitions */

#if 0
#include <hamlib/config.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include <math.h>

#include <hamlib/rig.h>
#endif


#define SETTINGS_FILE "hamlib_settings"
char settings_file[4096];

HAMLIB_EXPORT(int) rig_settings_get_path(char *path, int pathlen)
{
    char cwd[4096];

    if (getcwd(cwd, 4096) == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: getcwd: %s\n", __func__, strerror(errno));
        return -RIG_EINTERNAL;
    }

#ifdef APPLE
//#include "TargetConditionals.h"
#ifdef TARGET_OS_IPHONE
// iOS
#elif TARGET_IPHONE_SIMULATOR
// iOS Simulator
#elif TARGET_OS_MAC
// Other kinds of Mac OS
#else
// Unsupported platform
#endif
#endif

    char *xdgpath = getenv("XDG_CONFIG_HOME");
    char *home = getenv("HOME");
    snprintf(path, pathlen, "%s/.config", home);

    if (xdgpath)
    {
        snprintf(path, pathlen - 1, "%s/%s/%s", xdgpath, cwd, HAMLIB_SETTINGS_FILE);
    }
    else if (home && access(path, F_OK) != -1)
    {
        snprintf(path, pathlen - 1, "%s/.config/%s", home, HAMLIB_SETTINGS_FILE);
    }
    else if (home)
    {
        // we add a leading period to hide the file
        snprintf(path, pathlen - 1, "%s/.%s", home, HAMLIB_SETTINGS_FILE);
    }
    else
    {
        snprintf(path, pathlen - 1, ".%s", HAMLIB_SETTINGS_FILE);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: path=%s\n", __func__, path);
    return RIG_OK;
}

/**
 * \brief Save setting parameter
 * \param setting
 * \param value
 * \return RIG_OK or error
 *
 * \sa rig_setting_load()
 */
HAMLIB_EXPORT(int) rig_settings_save(char *setting, void *value,
                                     settings_value_t valuetype)
{
    FILE *fp;
    FILE *fptmp;
    char path[4096];
    char buf[4096];
    char *cvalue = (char *)value;
    int *ivalue = (int *)value;
    int n = 0;
    long *lvalue = (long *) value;
    float *fvalue = (float *) value;
    double *dvalue = (double *) value;
    char *vformat = "Unknown format??";
    char template[64];

    rig_settings_get_path(path, sizeof(path));
    fp = fopen(path, "r");

    if (fp == NULL)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: %s not found\n", __func__,  path);
        return -RIG_EIO;
    }

    strcpy(template, "hamlib_settings_XXXXXX");

    switch (valuetype)
    {
    case e_CHAR: cvalue = (char *)value; vformat = "%s=%s\n"; break;

    case e_INT: ivalue = (int *)value; vformat = "%s=%d\n"; break;

    case e_LONG: lvalue = (long *)value; vformat = "%s=%l\n"; break;

    case e_FLOAT: fvalue = (float *)value; vformat = "%s=%f\n"; break;

    case e_DOUBLE: dvalue = (double *)value; vformat = "%s=%f\n"; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown valuetype=%d\n", __func__, valuetype);
    }

    if (fp == NULL)
    {
        // first time for this file
        fp = fopen(settings_file, "w");

        switch (valuetype)
        {
        case e_CHAR: fprintf(fp, vformat, setting, cvalue); break;

        case e_INT: fprintf(fp, vformat, setting, *ivalue); break;

        case e_LONG: fprintf(fp, vformat, setting, *lvalue); break;

        case e_FLOAT: fprintf(fp, vformat, setting, *fvalue); break;

        case e_DOUBLE: fprintf(fp, vformat, setting, *dvalue); break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unknown valuetype=%d\n", __func__, valuetype);
        }

        fclose(fp);
        return RIG_OK;
    }

    int fd = mkstemp(template);
    close(fd);
    fptmp = fopen(template, "w");

    if (fptmp == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error opening for write %s: %s\n", __func__,
                  template, strerror(errno));
        fclose(fp);
        return -RIG_EIO;
    }

    while (fgets(buf, sizeof(buf), fp))
    {
        char *tmp = strdup(buf);
        char *s = strtok(tmp, "=");

        if (buf[0] == '#')
        {
            fprintf(fptmp, "%s\n", buf);
            continue;
        }

        if (s == NULL)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse setting from '%s'\n", __func__,
                      strtok(buf, "\r\n"));
            fclose(fp);
            fclose(fptmp);
            return -RIG_EINTERNAL;
        }

        ++n;

        char *v = strtok(NULL, "\r\n");

        if (v == NULL)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse value from '%s'\n", __func__,
                      strtok(buf, "\r\n"));
            fclose(fp);
            fclose(fptmp);
            return -RIG_EINTERNAL;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: parsing setting %s=%s\n", __func__, s, v);
        fprintf(fptmp, vformat, s, value);
    }

    fclose(fp);
    fclose(fptmp);
    remove(settings_file);
    rename(template, settings_file);
    rig_debug(RIG_DEBUG_TRACE, "%s: %d settings read\n", __func__, n);
    return -RIG_ENIMPL;
}

HAMLIB_EXPORT(int) rig_settings_load(char *setting, void *value,
                                     settings_value_t valuetype)
{
    return -RIG_ENIMPL;
}

HAMLIB_EXPORT(int) rig_settings_load_all(char *settings_file)
{
    FILE *fp;
    char buf[4096];
    char settingstmp[4096];

    if (settings_file == NULL)
    {
        rig_settings_get_path(settingstmp, sizeof(settingstmp));
        settings_file = settingstmp;
    }

    fp = fopen(settings_file, "r");

    if (fp == NULL)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: settings_file (%s): %s\n", __func__,
                  settings_file, strerror(errno));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: opened %s\n", __func__, settings_file);

    while (fgets(buf, sizeof(buf), fp))
    {
        char *s = strtok(buf, "=");
        char *v = strtok(NULL, "\r\n");

        if (strcmp(s, "sharedkey") == 0)
        {
            char *sharedkey = strdup(v);
            rig_debug(RIG_DEBUG_TRACE, "%s: settings_file=%s, shared_key=%s\n", __func__,
                      settings_file, sharedkey);
            free(sharedkey);
        }
    }

    return RIG_OK;
}


/*! @} */
