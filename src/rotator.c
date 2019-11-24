/*
 *  Hamlib Interface - main file
 *  Copyright (c) 2000-2012 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
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
 * \file src/rotator.c
 * \brief Rotator interface
 * \author Stephane Fillod
 * \date 2000-2012
 *
 * Hamlib interface is a frontend implementing rotator wrapper functions.
 */


/**
 * \page rot Rotator interface
 *
 * Rotator can be any kind of azimuth or azimuth and elevation controlled
 * antenna system.
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

#include <hamlib/rotator.h>
#include "serial.h"
#include "parallel.h"
#include "usb_port.h"
#include "network.h"
#include "rot_conf.h"
#include "token.h"


#ifndef DOC_HIDDEN

#if defined(WIN32) && !defined(__CYGWIN__)
#  define DEFAULT_SERIAL_PORT "\\\\.\\COM1"
#elif BSD
#  define DEFAULT_SERIAL_PORT "/dev/cuaa0"
#elif MACOSX
#  define DEFAULT_SERIAL_PORT "/dev/cu.usbserial"
#else
#  define DEFAULT_SERIAL_PORT "/dev/ttyS0"
#endif

#if defined(WIN32)
#  define DEFAULT_PARALLEL_PORT "\\\\.\\$VDMLPT1"
#elif defined(HAVE_DEV_PPBUS_PPI_H)
#  define DEFAULT_PARALLEL_PORT "/dev/ppi0"
#else
#  define DEFAULT_PARALLEL_PORT "/dev/parport0"
#endif

#define CHECK_ROT_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)


/*
 * Data structure to track the opened rot (by rot_open)
 */
struct opened_rot_l
{
    ROT *rot;
    struct opened_rot_l *next;
};
static struct opened_rot_l *opened_rot_list = { NULL };


/*
 * track which rot is opened (with rot_open)
 * needed at least for transceive mode
 */
static int add_opened_rot(ROT *rot)
{
    struct opened_rot_l *p;
    p = (struct opened_rot_l *)malloc(sizeof(struct opened_rot_l));

    if (!p)
    {
        return -RIG_ENOMEM;
    }

    p->rot = rot;
    p->next = opened_rot_list;
    opened_rot_list = p;
    return RIG_OK;
}


static int remove_opened_rot(ROT *rot)
{
    struct opened_rot_l *p, *q;
    q = NULL;

    for (p = opened_rot_list; p; p = p->next)
    {
        if (p->rot == rot)
        {
            if (q == NULL)
            {
                opened_rot_list = opened_rot_list->next;
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
#endif /* !DOC_HIDDEN */


/**
 * \brief execs cfunc() on each opened rot
 * \param cfunc The function to be executed on each rot
 * \param data  Data pointer to be passed to cfunc()
 *
 *  Calls cfunc() function for each opened rot.  The contents of the opened
 *  rot table is processed in random order according to a function pointed to
 *  by \a cfunc, whic is called with two arguments, the first pointing to the
 *  #ROT handle, the second to a data pointer \a data.
 *
 *  If \a data is not needed, then it can be set to NULL.  The processing of
 *  the opened rot table is stopped when cfunc() returns 0.
 * \internal
 *
 * \return always RIG_OK.
 */
int foreach_opened_rot(int (*cfunc)(ROT *, rig_ptr_t), rig_ptr_t data)
{
    struct opened_rot_l *p;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (p = opened_rot_list; p; p = p->next)
    {
        if ((*cfunc)(p->rot, data) == 0)
        {
            return RIG_OK;
        }
    }

    return RIG_OK;
}


/**
 * \brief allocate a new #ROT handle
 * \param rot_model The rot model for this new handle
 *
 * Allocates a new #ROT handle and initializes the associated data
 * for \a rot_model.
 *
 * \return a pointer to the #ROT handle otherwise NULL if memory allocation
 * failed or \a rot_model is unknown (e.g. backend autoload failed).
 *
 * \sa rot_cleanup(), rot_open()
 */
ROT * HAMLIB_API rot_init(rot_model_t rot_model)
{
    ROT *rot;
    const struct rot_caps *caps;
    struct rot_state *rs;
    int retcode;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_check_backend(rot_model);

    caps = rot_get_caps(rot_model);

    if (!caps)
    {
        return NULL;
    }

    /*
     * okay, we've found it. Allocate some memory and set it to zeros,
     * and especially the initialize the callbacks
     */
    rot = calloc(1, sizeof(ROT));

    if (rot == NULL)
    {
        /*
         * FIXME: how can the caller know it's a memory shortage,
         *        and not "rot not found" ?
         */
        return NULL;
    }

    /* caps is const, so we need to tell compiler
       that we know what we are doing */
    rot->caps = (struct rot_caps *) caps;

    /*
     * populate the rot->state
     * TODO: read the Preferences here!
     */
    rs = &rot->state;

    rs->comm_state = 0;
    rs->rotport.type.rig = caps->port_type; /* default from caps */

    rs->rotport.write_delay = caps->write_delay;
    rs->rotport.post_write_delay = caps->post_write_delay;
    rs->rotport.timeout = caps->timeout;
    rs->rotport.retry = caps->retry;

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        strncpy(rs->rotport.pathname, DEFAULT_SERIAL_PORT, FILPATHLEN - 1);
        rs->rotport.parm.serial.rate = caps->serial_rate_max;   /* fastest ! */
        rs->rotport.parm.serial.data_bits = caps->serial_data_bits;
        rs->rotport.parm.serial.stop_bits = caps->serial_stop_bits;
        rs->rotport.parm.serial.parity = caps->serial_parity;
        rs->rotport.parm.serial.handshake = caps->serial_handshake;
        break;

    case RIG_PORT_PARALLEL:
        strncpy(rs->rotport.pathname, DEFAULT_PARALLEL_PORT, FILPATHLEN - 1);
        break;

    case RIG_PORT_NETWORK:
    case RIG_PORT_UDP_NETWORK:
        strncpy(rs->rotport.pathname, "127.0.0.1:4533", FILPATHLEN - 1);
        break;

    default:
        strncpy(rs->rotport.pathname, "", FILPATHLEN - 1);
    }

    rs->min_el = caps->min_el;
    rs->max_el = caps->max_el;
    rs->min_az = caps->min_az;
    rs->max_az = caps->max_az;

    rs->rotport.fd = -1;

    /*
     * let the backend a chance to setup his private data
     * This must be done only once defaults are setup,
     * so the backend init can override rot_state.
     */
    if (caps->rot_init != NULL)
    {
        retcode = caps->rot_init(rot);

        if (retcode != RIG_OK)
        {
            rot_debug(RIG_DEBUG_VERBOSE,
                      "%s: backend_init failed!\n",
                      __func__);
            /* cleanup and exit */
            free(rot);
            return NULL;
        }
    }

    return rot;
}


/**
 * \brief open the communication to the rot
 * \param rot   The #ROT handle of the rotator to be opened
 *
 * Opens communication to a rotator which \a ROT handle has been passed
 * by argument.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \retval RIG_EINVAL   \a rot is NULL or unconsistent.
 * \retval RIG_ENIMPL   port type communication is not implemented yet.
 *
 * \sa rot_init(), rot_close()
 */
int HAMLIB_API rot_open(ROT *rot)
{
    const struct rot_caps *caps;
    struct rot_state *rs;
    int status;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;
    rs = &rot->state;

    if (rs->comm_state)
    {
        return -RIG_EINVAL;
    }

    rs->rotport.fd = -1;

    switch (rs->rotport.type.rig)
    {
    case RIG_PORT_SERIAL:
        status = serial_open(&rs->rotport);

        if (status != 0)
        {
            return status;
        }

        break;

    case RIG_PORT_PARALLEL:
        status = par_open(&rs->rotport);

        if (status < 0)
        {
            return status;
        }

        break;

    case RIG_PORT_DEVICE:
        status = open(rs->rotport.pathname, O_RDWR, 0);

        if (status < 0)
        {
            return -RIG_EIO;
        }

        rs->rotport.fd = status;
        break;

    case RIG_PORT_USB:
        status = usb_port_open(&rs->rotport);

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
        status = network_open(&rs->rotport, 4533);

        if (status < 0)
        {
            return status;
        }

        break;

    default:
        return -RIG_EINVAL;
    }


    add_opened_rot(rot);

    rs->comm_state = 1;

    /*
     * Maybe the backend has something to initialize
     * In case of failure, just close down and report error code.
     */
    if (caps->rot_open != NULL)
    {
        status = caps->rot_open(rot);

        if (status != RIG_OK)
        {
            return status;
        }
    }

    return RIG_OK;
}


/**
 * \brief close the communication to the rot
 * \param rot   The #ROT handle of the rotator to be closed
 *
 * Closes communication to a rotator which \a ROT handle has been passed
 * by argument that was previously open with rot_open().
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rot_cleanup(), rot_open()
 */
int HAMLIB_API rot_close(ROT *rot)
{
    const struct rot_caps *caps;
    struct rot_state *rs;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;
    rs = &rot->state;

    if (!rs->comm_state)
    {
        return -RIG_EINVAL;
    }

    /*
     * Let the backend say 73s to the rot.
     * and ignore the return code.
     */
    if (caps->rot_close)
    {
        caps->rot_close(rot);
    }


    if (rs->rotport.fd != -1)
    {
        switch (rs->rotport.type.rig)
        {
        case RIG_PORT_SERIAL:
            ser_close(&rs->rotport);
            break;

        case RIG_PORT_PARALLEL:
            par_close(&rs->rotport);
            break;

        case RIG_PORT_USB:
            usb_port_close(&rs->rotport);
            break;

        case RIG_PORT_NETWORK:
        case RIG_PORT_UDP_NETWORK:
            network_close(&rs->rotport);
            break;

        default:
            close(rs->rotport.fd);
        }

        rs->rotport.fd = -1;
    }

    remove_opened_rot(rot);

    rs->comm_state = 0;

    return RIG_OK;
}


/**
 * \brief release a rot handle and free associated memory
 * \param rot   The #ROT handle of the radio to be closed
 *
 * Releases a rot struct which port has eventualy been closed already
 * with rot_close().
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rot_init(), rot_close()
 */
int HAMLIB_API rot_cleanup(ROT *rot)
{
    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return -RIG_EINVAL;
    }

    /*
     * check if they forgot to close the rot
     */
    if (rot->state.comm_state)
    {
        rot_close(rot);
    }

    /*
     * basically free up the priv struct
     */
    if (rot->caps->rot_cleanup)
    {
        rot->caps->rot_cleanup(rot);
    }

    free(rot);

    return RIG_OK;
}


/**
 * \brief set the azimuth and elevation of the rotator
 * \param rot   The rot handle
 * \param azimuth   The azimuth to set to
 * \param elevation The elevation to set to
 *
 * Sets the azimuth and elevation of the rotator.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rot_get_position()
 */
int HAMLIB_API rot_set_position(ROT *rot,
                                azimuth_t azimuth,
                                elevation_t elevation)
{
    const struct rot_caps *caps;
    const struct rot_state *rs;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called az=%.02f el=%.02f\n", __func__, azimuth, elevation);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;
    rs = &rot->state;

    rot_debug(RIG_DEBUG_VERBOSE, "%s: south_zero=%d \n", __func__, rs->south_zero);

    if (rs->south_zero) {
        azimuth += azimuth >=180 ? -180: 180;
        rot_debug(RIG_DEBUG_TRACE,"%s: south adj to az=%.2f\n", __func__, azimuth);
    }

    if (azimuth < rs->min_az
        || azimuth > rs->max_az
        || elevation < rs->min_el
        || elevation > rs->max_el)
    {
        rot_debug(RIG_DEBUG_TRACE,"%s: range problem az=%.02f(min=%.02f,max=%.02f), el=%02f(min=%.02f,max=%02f)\n", __func__, azimuth,rs->min_az,rs->max_az,elevation,rs->min_el,rs->max_el);
        return -RIG_EINVAL;
    }

    if (caps->set_position == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->set_position(rot, azimuth, elevation);
}


/**
 * \brief get the azimuth and elevation of the rotator
 * \param rot   The rot handle
 * \param azimuth   The location where to store the current azimuth
 * \param elevation The location where to store the current elevation
 *
 *  Retrieves the current azimuth and elevation of the rotator.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 * \sa rot_set_position()
 */
int HAMLIB_API rot_get_position(ROT *rot,
                                azimuth_t *azimuth,
                                elevation_t *elevation)
{
    const struct rot_caps *caps;
    const struct rot_state *rs;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot) || !azimuth || !elevation)
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;
    rs = &rot->state;

    if (caps->get_position == NULL)
    {
        return -RIG_ENAVAIL;
    }

    int retval = caps->get_position(rot, azimuth, elevation);
    if (retval != RIG_OK) return retval;

    rot_debug(RIG_DEBUG_VERBOSE,"%s: got az=%.2f, el=%.2f\n", __func__, *azimuth, *elevation);

    if (rs->south_zero) {
        *azimuth += *azimuth >=180 ? -180: 180;
        rot_debug(RIG_DEBUG_VERBOSE,"%s: south adj to az=%.2f\n", __func__, azimuth);
    }
    return RIG_OK;
}


/**
 * \brief park the antenna
 * \param rot   The rot handle
 *
 *  Park the antenna.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rot_park(ROT *rot)
{
    const struct rot_caps *caps;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->park == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->park(rot);
}


/**
 * \brief stop the rotator
 * \param rot   The rot handle
 *
 *  Stop the rotator.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rot_stop(ROT *rot)
{
    const struct rot_caps *caps;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->stop == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->stop(rot);
}


/**
 * \brief reset the rotator
 * \param rot   The rot handle
 * \param reset The reset operation to perform
 *
 *  Resets the rotator.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rot_reset(ROT *rot, rot_reset_t reset)
{
    const struct rot_caps *caps;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->reset == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->reset(rot, reset);
}


/**
 * \brief move the rotator in the specified direction
 * \param rot   The rot handle
 * \param direction   Direction of movement
 * \param speed   Speed of movement
 *
 * Move the rotator in the specified direction. The speed is a value
 * between 1 and 100.
 */
int HAMLIB_API rot_move(ROT *rot, int direction, int speed)
{
    const struct rot_caps *caps;

    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    if (caps->move == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return caps->move(rot, direction, speed);
}


/**
 * \brief get general information from the rotator
 * \param rot   The rot handle
 *
 * Retrieves some general information from the rotator.
 * This can include firmware revision, exact model name, or just nothing.
 *
 * \return a pointer to static memory containing the ASCIIZ string
 * if the operation has been sucessful, otherwise NULL if an error occured
 * or get_info not part of capabilities.
 */
const char * HAMLIB_API rot_get_info(ROT *rot)
{
    rot_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_ROT_ARG(rot))
    {
        return NULL;
    }

    if (rot->caps->get_info == NULL)
    {
        return NULL;
    }

    return rot->caps->get_info(rot);
}

/*! @} */
