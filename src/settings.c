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
#include <hamlib/amplifier.h>
#include "cal.h"


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
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
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

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
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
 *      +30=30, +40=40, +50=50 and +60=60. This is the responsability
 *      of the backend to return values calibrated for this scale.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_has_get_level(), rig_set_level()
 */
int HAMLIB_API rig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
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


    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
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
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
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
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
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
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return 0;
    }

    return (rig->state.has_get_level & level);
}


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
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
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
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_func()
 */
int HAMLIB_API rig_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
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

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
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
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
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

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
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
 * \brief set a radio parm extra parameter
 * \param rig   The rig handle
 * \param token The parameter
 * \param val   The value to set the parameter to
 *
 *  Sets an parm extra parameter.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
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
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
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
            return i;
        }
    }

    return 0;
}

/*! @} */
