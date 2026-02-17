/*
 *  Hamlib Interface - main file
 *  Copyright (c) 2000-2012 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
 *  Copyright (C) 2019-2020 by Michael Black
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

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
 * \author Mikael Nousiainen OH3BHX
 * \date 2026
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

#include "hamlib/config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "hamlib/amplifier.h"
#include "hamlib/port.h"
#include "hamlib/amp_state.h"
#include "serial.h"
#include "parallel.h"
#include "usb_port.h"
#include "network.h"

//! @cond Doxygen_Suppress
#define CHECK_AMP_ARG(r) (!(r) || !(r)->caps || !AMPSTATE(r)->comm_state)
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
 * \brief Executes cfunc() on each #AMP handle.
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
    hamlib_port_t *ap;

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
    rs = AMPSTATE(amp);

    //TODO allocate and link new ampport
    // For now, use the embedded one
    ap = AMPPORT(amp);

    rs->comm_state = 0;
    ap->type.rig = caps->port_type; /* default from caps */

    ap->write_delay = caps->write_delay;
    ap->post_write_delay = caps->post_write_delay;
    ap->timeout = caps->timeout;
    ap->retry = caps->retry;
    rs->has_get_level = caps->has_get_level;

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        // Don't think we need a default port here
        //strncpy(ap->pathname, DEFAULT_SERIAL_PORT, HAMLIB_FILPATHLEN - 1);
        ap->parm.serial.rate = caps->serial_rate_max;   /* fastest ! */
        ap->parm.serial.data_bits = caps->serial_data_bits;
        ap->parm.serial.stop_bits = caps->serial_stop_bits;
        ap->parm.serial.parity = caps->serial_parity;
        ap->parm.serial.handshake = caps->serial_handshake;
        break;

    case RIG_PORT_NETWORK:
    case RIG_PORT_UDP_NETWORK:
        strncpy(ap->pathname, "127.0.0.1:4531", HAMLIB_FILPATHLEN - 1);
        break;

    default:
        strncpy(ap->pathname, "", HAMLIB_FILPATHLEN - 1);
    }

    ap->fd = -1;

    rs->has_get_func = caps->has_get_func;
    rs->has_set_func = caps->has_set_func;
    rs->has_get_level = caps->has_get_level;
    rs->has_set_level = caps->has_set_level;
    rs->has_get_parm = caps->has_get_parm;
    rs->has_set_parm = caps->has_set_parm;

    rs->has_status = caps->has_status;
    rs->amp_ops = caps->amp_ops;

    memcpy(rs->level_gran, caps->level_gran, sizeof(gran_t)*RIG_SETTING_MAX);
    memcpy(rs->parm_gran, caps->parm_gran, sizeof(gran_t)*RIG_SETTING_MAX);

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
    memcpy(&rs->ampport_deprecated, ap,
           sizeof(rs->ampport_deprecated));

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
 * \retval -RIG_EINVAL \a amp is NULL or inconsistent.
 *
 * \sa amp_init(), amp_close()
 */
int HAMLIB_API amp_open(AMP *amp)
{
    const struct amp_caps *caps;
    struct amp_state *rs;
    hamlib_port_t *ap = AMPPORT(amp);
    int status;
    int net1, net2, net3, net4, port;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;
    rs = AMPSTATE(amp);

    if (rs->comm_state)
    {
        return -RIG_EINVAL;
    }

    ap->fd = -1;

    // determine if we have a network address
    if (sscanf(ap->pathname, "%d.%d.%d.%d:%d", &net1, &net2, &net3, &net4,
               &port) == 5)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: using network address %s\n", __func__,
                  ap->pathname);
        ap->type.rig = RIG_PORT_NETWORK;
    }

    switch (ap->type.rig)
    {
    case RIG_PORT_SERIAL:
        status = serial_open(ap);

        if (status != 0)
        {
            return status;
        }

        break;

    case RIG_PORT_PARALLEL:
        status = par_open(ap);

        if (status < 0)
        {
            return status;
        }

        break;

    case RIG_PORT_DEVICE:
        status = open(ap->pathname, O_RDWR, 0);

        if (status < 0)
        {
            return -RIG_EIO;
        }

        ap->fd = status;
        break;

    case RIG_PORT_USB:
        status = usb_port_open(ap);

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
        status = network_open(ap, 4531);

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
            memcpy(&rs->ampport_deprecated, ap,
                   sizeof(rs->ampport_deprecated));
            return status;
        }
    }

    if (ap->parm.serial.dtr_state == RIG_SIGNAL_ON)
    {
        ser_set_dtr(ap, 1);
    }
    else
    {
        ser_set_dtr(ap, 0);
    }

    if (ap->parm.serial.rts_state == RIG_SIGNAL_ON)
    {
        ser_set_rts(ap, 1);
    }
    else
    {
        ser_set_rts(ap, 0);
    }

    memcpy(&rs->ampport_deprecated, ap,
           sizeof(rs->ampport_deprecated));

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
 * \retval -RIG_EINVAL \a amp is NULL or inconsistent.
 *
 * \sa amp_cleanup(), amp_open()
 */
int HAMLIB_API amp_close(AMP *amp)
{
    const struct amp_caps *caps;
    struct amp_state *rs;
    hamlib_port_t *ap = AMPPORT(amp);

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (amp == NULL)
    {
        amp_debug(RIG_DEBUG_ERR, "%s: NULL ptr? amp=%p\n", __func__, amp);
        return -RIG_EINVAL;
    }

    if (amp->caps == NULL)
    {
        amp_debug(RIG_DEBUG_ERR, "%s: NULL ptr? amp->caps=%p\n", __func__, amp->caps);
        return -RIG_EINVAL;
    }

    caps = amp->caps;
    rs = AMPSTATE(amp);

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


    if (ap->fd != -1)
    {
        switch (ap->type.rig)
        {
        case RIG_PORT_SERIAL:
            ser_close(ap);
            break;

        case RIG_PORT_PARALLEL:
            par_close(ap);
            break;

        case RIG_PORT_USB:
            usb_port_close(ap);
            break;

        case RIG_PORT_NETWORK:
        case RIG_PORT_UDP_NETWORK:
            network_close(ap);
            break;

        default:
            close(ap->fd);
        }

        ap->fd = -1;
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
 * \retval -RIG_EINVAL \a amp is NULL or inconsistent.
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
    if (AMPSTATE(amp)->comm_state)
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
 * \retval -RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval -RIG_ENAVAIL amp_caps#reset() capability is not available.
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
 * \retval -RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval -RIG_ENAVAIL amp_caps#get_freq() capability is not available.
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
 * \retval -RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval -RIG_ENAVAIL amp_caps#set_freq() capability is not available.
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
 * \retval -RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval -RIG_ENAVAIL amp_caps#set_powerstat() capability is not available.
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
 * \retval -RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval -RIG_ENAVAIL amp_caps#get_powerstat() capability is not available.
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


/**
 * \brief Query status flags of the amplifier.
 *
 * \param amp The #AMP handle.
 * \param status The variable where the status flags will be stored.
 *
 * Query the active status flags from the amplifier.
 *
 * \return RIG_OK if the operation has been successful, otherwise a **negative
 * value** if an error occurred (in which case, cause is set appropriately).
 *
 * \retval RIG_OK The query was successful.
 * \retval RIG_EINVAL \a amp is NULL or inconsistent.
 * \retval RIG_ENAVAIL amp_caps#get_status() capability is not available.
 */
int HAMLIB_API amp_get_status(AMP *amp, amp_status_t *status)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    if (amp->caps->get_status == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->get_status(amp, status);
}


/**
 * \brief check retrieval ability of amp operations
 * \param amp   The #AMP handle
 * \param op    The amp op
 *
 *  Checks if an amplifier is capable of executing an operation.
 *  Since the \a op is an OR'ed bitmap argument, more than
 *  one op can be checked at the same time.
 *
 *  EXAMPLE: if (amp_has_op(my_rig, AMP_OP_TUNE)) disp_tune_btn();
 *
 * \return a bit map mask of supported op settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa amp_op()
 */
amp_op_t HAMLIB_API amp_has_op(AMP *amp, amp_op_t op)
{
    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp || !amp->caps)
    {
        return 0;
    }

    return (amp->state.amp_ops & op);
}


/**
 * \brief perform amplifier operations
 * \param amp   The #AMP handle
 * \param op    The amplifier operation to perform
 *
 *  Performs amplifier operation.
 *  See #amp_op_t for more information.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa amp_has_op()
 */
int HAMLIB_API amp_op(AMP *amp, amp_op_t op)
{
    const struct amp_caps *caps;

    amp_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->amp_op == NULL || amp_has_op(amp, op) == 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: amp_op=%p, has_amp_op=%d\n", __func__,
                  caps->amp_op, amp_has_op(amp, op));
        return -RIG_ENAVAIL;
    }

    return caps->amp_op(amp, op);
}


/**
 * \brief set the input
 * \param amp   The amp handle
 * \param input   The input to select
 *
 *  Select the input connector for RF signal.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa amp_get_input()
 */
int HAMLIB_API amp_set_input(AMP *amp, ant_t input)
{
    const struct amp_caps *caps;

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->set_input == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->set_input(amp, input);
}

/**
 * \brief get the current input
 * \param amp   The amp handle
 * \param ant   The variable to store the current input to.
 *
 *  Retrieves the current input for RF signal.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa amp_set_input()
 */
int HAMLIB_API amp_get_input(AMP *amp, ant_t *input)
{
    const struct amp_caps *caps;

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->get_input == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_input(amp, input);
}


/**
 * \brief set the antenna
 * \param amp   The amp handle
 * \param ant   The antenna to select
 *
 *  Select the antenna connector.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa amp_get_ant()
 */
int HAMLIB_API amp_set_ant(AMP *amp, ant_t ant)
{
    const struct amp_caps *caps;

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->set_ant == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return amp->caps->set_ant(amp, ant);
}


/**
 * \brief get the current antenna
 * \param amp   The amp handle
 * \param ant   The antenna to query option for
 *
 *  Retrieves the current antenna.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa amp_set_ant()
 */
int HAMLIB_API amp_get_ant(AMP *amp, ant_t *ant)
{
    const struct amp_caps *caps;

    if (CHECK_AMP_ARG(amp))
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    if (caps->get_ant == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->get_ant(amp, ant);
}


/**
 * \brief Get the address of amplifier data structure(s)
 *
 * \sa rig_data_pointer(), rot_data_pointer()
 *
 */
void *HAMLIB_API amp_data_pointer(AMP *amp, rig_ptrx_t idx)
{
    switch (idx)
    {
    case RIG_PTRX_AMPPORT:
        return AMPPORT(amp);

    case RIG_PTRX_AMPSTATE:
        return AMPSTATE(amp);

    default:
        amp_debug(RIG_DEBUG_ERR, "%s: Invalid data index=%d\n", __func__, idx);
        return NULL;
    }
}

/*! @} */
