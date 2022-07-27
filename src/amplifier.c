/*
 *  Hamlib Interface - main file
 *  Copyright (c) 2000-2012 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
 *  Copyright (C) 2019-2020 by Michael Black
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
 * \file src/amplifier.c
 * \brief Amplifier interface
 * \author Stephane Fillod
 * \date 2000-2012
 * \author Frank Singleton
 * \date 2000-2003
 * \author Michael Black
 * \date 2019-2020
 *
 * This Hamlib interface is a frontend implementing the amplifier wrapper
 * functions.
 */


/**
 * \page amp Amplifier interface
 *
 * An amplifier can be any kind of external power amplifier that is capable of
 * CAT type control.
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <hamlib/amplifier.h>
#include "serial.h"
#include "parallel.h"
#include "usb_port.h"
#include "network.h"
#include "token.h"

//! @cond Doxygen_Suppress
#define CHECK_AMP_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)
//! @endcond

/*
 * Data structure to track the opened amp (by amp_open)
 */
//! @cond Doxygen_Suppress
struct opened_amp_l
{
    AMP *amp;
    struct opened_amp_l *next;
};
//! @endcond
static struct opened_amp_l *opened_amp_list = { NULL };


/*
 * track which amp is opened (with amp_open)
 * needed at least for transceive mode
 */
static int add_opened_amp(AMP *amp)
{
    struct opened_amp_l *p;
    p = (struct opened_amp_l *)calloc(1, sizeof(struct opened_amp_l));

    if (!p)
    {
        return -RIG_ENOMEM;
    }

    p->amp = amp;
    p->next = opened_amp_list;
    opened_amp_list = p;
    return RIG_OK;
}


static int remove_opened_amp(const AMP *amp)
{
    struct opened_amp_l *p, *q;
    q = NULL;

    for (p = opened_amp_list; p; p = p->next)
    {
        if (p->amp == amp)
        {
            if (q == NULL)
            {
                opened_amp_list = opened_amp_list->next;
            }
            else
            {
                q->next = p->next;
            }

            free(p);
            return RIG_OK;
        }

        q = p;
    }

    return -RIG_EINVAL; /* Not found in list ! */
}


#ifdef XXREMOVEDXX
/**
 * \brief Executess cfunc() on each #AMP handle.
 *
 * \param cfunc The function to be executed on each #AMP handle.
 * \param data Data pointer to be passed to cfunc()
 *
 * Calls cfunc() function for each #AMP handle.  The contents of the opened
 * #AMP table is processed in random order according to a function pointed to
 * by \a cfunc, which is called with two arguments, the first pointing to the
 * #AMP handle, the second to a data pointer \a data.
 *
 * If \a data is not needed, then it can be set to NULL.  The processing of
 * the opened amp table is stopped when cfunc() returns 0.
 * \internal
 *
 * \return always RIG_OK.
 */
int foreach_opened_amp(int (*cfunc)(AMP *, rig_ptr_t), rig_ptr_t data)
{
    struct opened_amp_l *p;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (p = opened_amp_list; p; p = p->next)
    {
        if ((*cfunc)(p->amp, data) == 0)
        {
            return RIG_OK;
        }
    }

    return RIG_OK;
}
#endif


/**
 * \brief Allocate a new #AMP handle.
 *
 * \param amp_model The amplifier model for this new handle.
 *
 * Allocates a new #AMP handle and initializes the associated data
 * for \a amp_model (see amplist.h or `ampctl -l`).
 *
 * \return Pointer to the #AMP handle otherwise NULL if memory allocation
 * failed or \a amp_model is unknown, e.g. backend autoload failed.
 *
 * \sa amp_cleanup(), amp_open()
 */
AMP *HAMLIB_API amp_init(amp_model_t amp_model)
{
    AMP *amp;
    const struct amp_caps *caps;
    struct amp_state *rs;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    amp_check_backend(amp_model);

    caps = amp_get_caps(amp_model);

    if (!caps)
    {
        return NULL;
    }

    /*
     * okay, we've found it. Allocate some memory and set it to zeros,
     * and especially the initialize the callbacks
     */
    amp = calloc(1, sizeof(AMP));

    if (amp == NULL)
    {
        /*
         * FIXME: how can the caller know it's a memory shortage,
         *        and not "amp not found" ?
         */
        return NULL;
    }

    /* caps is const, so we need to tell compiler
       that we know what we are doing */
    amp->caps = (struct amp_caps *) caps;

    /*
     * populate the amp->state
     */
    /**
     * \todo Read the Preferences here!
     */
    rs = &amp->state;

    rs->comm_state = 0;
    rs->ampport.type.rig = caps->port_type; /* default from caps */

    rs->ampport.write_delay = caps->write_delay;
    rs->ampport.post_write_delay = caps->post_write_delay;
    rs->ampport.timeout = caps->timeout;
    rs->ampport.retry = caps->retry;
    rs->has_get_level = caps->has_get_level;

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        // Dont' think we need a default port here
        //strncpy(rs->ampport.pathname, DEFAULT_SERIAL_PORT, HAMLIB_FILPATHLEN - 1);
        rs->ampport.parm.serial.rate = caps->serial_rate_max;   /* fastest ! */
        rs->ampport.parm.serial.data_bits = caps->serial_data_bits;
        rs->ampport.parm.serial.stop_bits = caps->serial_stop_bits;
        rs->ampport.parm.serial.parity = caps->serial_parity;
        rs->ampport.parm.serial.handshake = caps->serial_handshake;
        break;

    case RIG_PORT_NETWORK:
    case RIG_PORT_UDP_NETWORK:
        strncpy(rs->ampport.pathname, "127.0.0.1:4531", HAMLIB_FILPATHLEN - 1);
        break;

    default:
        strncpy(rs->ampport.pathname, "", HAMLIB_FILPATHLEN - 1);
    }

    rs->ampport.fd = -1;

    /*
     * let the backend a chance to setup his private data
     * This must be done only once defaults are setup,
     * so the backend init can override amp_state.
     */
    if (caps->amp_init != NULL)
    {
        int retcode = caps->amp_init(amp);

        if (retcode != RIG_OK)
        {
            amp_debug(RIG_DEBUG_VERBOSE,
                      "%s: backend_init failed!\n",
                      __func__);
            /* cleanup and exit */
            free(amp);
            return NULL;
        }
    }

    // Now we have to copy our new rig state hamlib_port structure to the deprecated one
    // Clients built on older 4.X versions will use the old structure
    // Clients built on newer 4.5 versions will use the new structure
    memcpy(&amp->state.ampport_deprecated, &amp->state.ampport,
           sizeof(amp->state.ampport_deprecated));

    return amp;
}


/**
 * \brief Open the communication channel to the amplifier.
 *
 * \param amp The #AMP handle of the amplifier to be opened.
 *
 * Opens the communication channel to an amplifier for which the #AMP handle
 * has been passed.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK Communication channel successfully opened.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 *
 * \sa amp_init(), amp_close()
 */
int HAMLIB_API amp_open(AMP *amp)
{
    const struct amp_caps *caps;
    struct amp_state *rs;
    int status;
    int net1, net2, net3, net4, port;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;
    rs = &amp->state;

    if (rs->comm_state)
    {
        return -RIG_EINVAL;
    }

    rs->ampport.fd = -1;

    // determine if we have a network address
    if (sscanf(rs->ampport.pathname, "%d.%d.%d.%d:%d", &net1, &net2, &net3, &net4,
               &port) == 5)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: using network address %s\n", __func__,
                  rs->ampport.pathname);
        rs->ampport.type.rig = RIG_PORT_NETWORK;
    }

    switch (rs->ampport.type.rig)
    {
    case RIG_PORT_SERIAL:
        status = serial_open(&rs->ampport);

        if (status != 0)
        {
            return status;
        }

        break;

    case RIG_PORT_PARALLEL:
        status = par_open(&rs->ampport);

        if (status < 0)
        {
            return status;
        }

        break;

    case RIG_PORT_DEVICE:
        status = open(rs->ampport.pathname, O_RDWR, 0);

        if (status < 0)
        {
            return -RIG_EIO;
        }

        rs->ampport.fd = status;
        break;

    case RIG_PORT_USB:
        status = usb_port_open(&rs->ampport);

        if (status < 0)
        {
            return status;
        }

        break;

    case RIG_PORT_NONE:
    case RIG_PORT_RPC:
        break;  /* ez :) */

    case RIG_PORT_NETWORK:
    case RIG_PORT_UDP_NETWORK:
        /* FIXME: default port */
        status = network_open(&rs->ampport, 4531);

        if (status < 0)
        {
            return status;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    add_opened_amp(amp);

    rs->comm_state = 1;

    /*
     * Maybe the backend has something to initialize
     * In case of failure, just close down and report error code.
     */
    if (caps->amp_open != NULL)
    {
        status = caps->amp_open(amp);

        if (status != RIG_OK)
        {
            memcpy(&amp->state.ampport_deprecated, &amp->state.ampport,
                   sizeof(amp->state.ampport_deprecated));
            return status;
        }
    }

    memcpy(&amp->state.ampport_deprecated, &amp->state.ampport,
           sizeof(amp->state.ampport_deprecated));

    return RIG_OK;
}


/**
 * \brief Close the communication channel to the amplifier.
 *
 * \param amp The #AMP handle of the amplifier to be closed.
 *
 * Closes the communication channel to an amplifier for which the #AMP
 * handle has been passed by argument that was previously opened with
 * amp_open().
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK Communication channel successfully closed.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 *
 * \sa amp_cleanup(), amp_open()
 */
int HAMLIB_API amp_close(AMP *amp)
{
    const struct amp_caps *caps;
    struct amp_state *rs;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        amp_debug(RIG_DEBUG_ERR, "%s: NULL ptr? amp=%p, amp->caps=%p\n", __func__, amp,
                  amp->caps);
        return -RIG_EINVAL;
    }

    caps = amp->caps;
    rs = &amp->state;

    if (!rs->comm_state)
    {
        amp_debug(RIG_DEBUG_ERR, "%s: comm_state=0? rs=%p, rs->comm_state=%d\n",
                  __func__, rs, rs->comm_state);
        return -RIG_EINVAL;
    }

    /*
     * Let the backend say 73s to the amp.
     * and ignore the return code.
     */
    if (caps->amp_close)
    {
        caps->amp_close(amp);
    }


    if (rs->ampport.fd != -1)
    {
        switch (rs->ampport.type.rig)
        {
        case RIG_PORT_SERIAL:
            ser_close(&rs->ampport);
            break;

        case RIG_PORT_PARALLEL:
            par_close(&rs->ampport);
            break;

        case RIG_PORT_USB:
            usb_port_close(&rs->ampport);
            break;

        case RIG_PORT_NETWORK:
        case RIG_PORT_UDP_NETWORK:
            network_close(&rs->ampport);
            break;

        default:
            close(rs->ampport.fd);
        }

        rs->ampport.fd = -1;
    }

    remove_opened_amp(amp);

    rs->comm_state = 0;

    return RIG_OK;
}


/**
 * \brief Release an #AMP handle and free associated memory.
 *
 * \param amp The #AMP handle to be released.
 *
 * Releases an #AMP handle for which the communications channel has been
 * closed with amp_close().
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK #AMP handle successfully released.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 *
 * \sa amp_init(), amp_close()
 */
int HAMLIB_API amp_cleanup(AMP *amp)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return -RIG_EINVAL;
    }

    /*
     * check if they forgot to close the amp
     */
    if (amp->state.comm_state)
    {
        amp_close(amp);
    }

    /*
     * basically free up the priv struct
     */
    if (amp->caps->amp_cleanup)
    {
        amp->caps->amp_cleanup(amp);
    }

    free(amp);

    return RIG_OK;
}


/**
 * \brief Reset the amplifier.
 *
 * \param amp The #AMP handle.
 * \param reset The reset operation to perform.
 *
 * Perform a reset of the amplifier.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The reset command was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#reset() capability is not available.
 */
int HAMLIB_API amp_reset(AMP *amp, amp_reset_t reset)
{
    const struct amp_caps *caps;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->reset == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->reset(amp, reset);
}


/**
 * \brief Query the operating frequency of the amplifier.
 *
 * \param amp The #AMP handle.
 * \param freq The variable to store the operating frequency.
 *
 * Retrieves the operating frequency from the amplifier.
 *
 * \return RIG_OK if the operation was successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The query was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_freq() capability is not available.
 *
 * \sa amp_set_freq()
 */
int HAMLIB_API amp_get_freq(AMP *amp, freq_t *freq)
{
    const struct amp_caps *caps;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->get_freq == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_freq(amp, freq);
}


/**
 * \brief Set the operating frequency of the amplifier.
 *
 * \param amp The #AMP handle.
 * \param freq The operating frequency.
 *
 * Set the operating frequency of the amplifier.  Depending on the amplifier
 * this may simply set the bandpass filters, etc.
 *
 * \return RIG_OK if the operation was successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK Setting the frequency was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#set_freq() capability is not available.
 *
 * \sa amp_get_freq()
 */
int HAMLIB_API amp_set_freq(AMP *amp, freq_t freq)
{
    const struct amp_caps *caps;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->set_freq == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_freq(amp, freq);
}


/**
 * \brief Query general information from the amplifier.
 *
 * \param amp The #AMP handle.
 *
 * Retrieves some general information from the amplifier.  This can include
 * firmware revision, exact model name, or just nothing.
 *
 * \return A pointer to static memory containing an ASCII nul terminated
 * string (C string) if the operation has been successful, otherwise NULL if
 * \a amp is NULL or inconsistent or the amp_caps#get_info() capability is not
 * available.
 */
const char *HAMLIB_API amp_get_info(AMP *amp)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return NULL;
    }

    if (amp->caps->get_info == NULL)
    {
        return NULL;
    }

    return amp->caps->get_info(amp);
}


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

    if (CHECK_AMP_ARG(amp))
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
 * \brief Set the value of a requested extension levels token.
 *
 * \param amp The #AMP handle.
 * \param level The requested extension levels token.
 * \param val The variable to set the extension \a level token value.
 *
 * Query the \a val corresponding to the extension \a level token.
 *
 * \return RIG_OK if the operation was successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The query was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#set_ext_level() capability is not available.
 *
 * \sa amp_set_level()
 */
int HAMLIB_API amp_set_ext_level(AMP *amp, token_t level, value_t val)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->set_ext_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->set_ext_level(amp, level, val);
}

/**
 * \brief Query the value of a requested extension levels token.
 *
 * \param amp The #AMP handle.
 * \param level The requested extension levels token.
 * \param val The variable to store the extension \a level token value.
 *
 * Query the \a val corresponding to the extension \a level token.
 *
 * \return RIG_OK if the operation was successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The query was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_ext_level() capability is not available.
 *
 * \sa amp_get_level()
 */
int HAMLIB_API amp_get_ext_level(AMP *amp, token_t level, value_t *val)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->get_ext_level == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->get_ext_level(amp, level, val);
}


/**
 * \brief Turn the amplifier On or Off or toggle the Standby or Operate
 * status.
 *
 * \param amp The #AMP handle
 * \param status The #powerstat_t setting.
 *
 * Turns the amplifier On or Off or toggles the Standby or Operate status.
 * See #RIG_POWER_ON, #RIG_POWER_OFF and #RIG_POWER_OPERATE,
 * #RIG_POWER_STANDBY for the value of \a status.
 *
 * \return RIG_OK if the operation was successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The requested power/standby state was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#set_powerstat() capability is not available.
 *
 * \sa amp_get_powerstat()
 */
int HAMLIB_API amp_set_powerstat(AMP *amp, powerstat_t status)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->set_powerstat == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->set_powerstat(amp, status);
}


/**
 * \brief Query the power or standby status of the amplifier.
 *
 * \param amp The #AMP handle.
 * \param status The variable to store the amplifier \a status.
 *
 * Query the amplifier's power or standby condition.  The value stored in
 * \a status will be one of #RIG_POWER_ON, #RIG_POWER_OFF and
 * #RIG_POWER_OPERATE, #RIG_POWER_STANDBY, or #RIG_POWER_UNKNOWN.
 *
 *\return RIG_OK if the query was successful, otherwise a **negative value**
 * if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK Querying the power/standby state was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_powerstat() capability is not available.
 *
 * \sa amp_set_powerstat()
 */
int HAMLIB_API amp_get_powerstat(AMP *amp, powerstat_t *status)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->get_powerstat == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->get_powerstat(amp, status);
}


/*! @} */
