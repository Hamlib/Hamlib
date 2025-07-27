/*
 *  Hamlib Interface - Rotator state structure
 *  Copyright (c) 2000-2025 The Hamlib Group
 *  Copyright (c) 2025 George Baltz
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

#ifndef _ROT_STATE_H
#define _ROT_STATE_H 1

__BEGIN_DECLS

/**
 * \addtogroup rotator
 * @{
 */


/**
 * \brief Hamlib rotator data structures.
 *
 * \file rot_state.h
 *
 * This file contains the live data state structure of the rotator.
 */

/**
 * \brief Rotator state structure
 *
 * \struct rot_state
 *
 * This structure contains live data, as well as a copy of capability fields
 * that may be updated, i.e. customized while the #ROT handle is instantiated.
 *
 * It is fine to move fields around, as this kind of structure should not be
 * initialized like rot_caps are.
 */
struct rot_state {
    /*
     * overridable fields
     */
    azimuth_t min_az;       /*!< Lower limit for azimuth (overridable). */
    azimuth_t max_az;       /*!< Upper limit for azimuth (overridable). */
    elevation_t min_el;     /*!< Lower limit for elevation (overridable). */
    elevation_t max_el;     /*!< Upper limit for elevation (overridable). */
    int south_zero;         /*!< South is zero degrees. */
    azimuth_t az_offset;    /*!< Offset to be applied to azimuth. */
    elevation_t el_offset;  /*!< Offset to be applied to elevation. */

    setting_t has_get_func;     /*!< List of get functions. */
    setting_t has_set_func;     /*!< List of set functions. */
    setting_t has_get_level;    /*!< List of get levels. */
    setting_t has_set_level;    /*!< List of set levels. */
    setting_t has_get_parm;     /*!< List of get parameters. */
    setting_t has_set_parm;     /*!< List of set parameters. */

    rot_status_t has_status;    /*!< Supported status flags. */

    gran_t level_gran[RIG_SETTING_MAX]; /*!< Level granularity. */
    gran_t parm_gran[RIG_SETTING_MAX];  /*!< Parameter granularity. */

    /*
     * non overridable fields, internal use
     */
    hamlib_port_t_deprecated rotport_deprecated;  /*!< Rotator port (internal use). Deprecated */
    hamlib_port_t_deprecated rotport2_deprecated;  /*!< 2nd Rotator port (internal use). Deprecated */

    int comm_state;         /*!< Comm port state, i.e. opened or closed. */
    rig_ptr_t priv;         /*!< Pointer to private rotator state data. */
    rig_ptr_t obj;          /*!< Internal use by hamlib++ for event handling. */

    int current_speed;      /*!< Current speed 1-100, to be used when no change to speed is requested. */
    hamlib_port_t rotport;  /*!< Rotator port (internal use). */
    hamlib_port_t rotport2;  /*!< 2nd Rotator port (internal use). */
    rig_ptr_t *pstrotator_handler_priv_data; /*!< PstRotator private data. */
    deferred_config_header_t config_queue;   /*!< Que for deferred processing. */
};

__END_DECLS

#if defined(IN_HAMLIB)
#define ROTSTATE(r) (&(r)->state)
#endif
/** Macro for application access to rot_state data structure using the #ROT
 * handle
 *
 * Example code.
 * ```
 * ROT *my_rot;
 *
 * //Instantiate a rotator
 * my_rot = rot_init(ROT_MODEL_DUMMY); // your rot (rotator) model.
 *
 * const struct rot_state *my_rs = HAMLIB_ROTSTATE(my_rot);
 * ```
 */
#define HAMLIB_ROTSTATE(r) ((struct rot_state *)rot_data_pointer(r, RIG_PTRX_ROTSTATE))

#endif /* _ROT_STATE_H */

/** @} */
