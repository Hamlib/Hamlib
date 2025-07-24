/*
 *  Hamlib Interface - Amplifier state structure
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

#ifndef _AMP_STATE_H
#define _AMP_STATE_H 1

/**
 * \addtogroup amplifier
 * @{
 */


/**
 * \brief Hamlib amplifier state structure.
 *
 * \file amp_state.h
 *
 * This file contains the live data state structure of the amplifier.
 */


__BEGIN_DECLS
/**
 * \brief Amplifier state structure.
 *
 * \struct amp_state
 *
 * This structure contains live data, as well as a copy of capability fields
 * that may be updated, i.e. customized while the #AMP handle is instantiated.
 *
 * It is fine to move fields around, as this kind of struct should not be
 * initialized like amp_caps are.
 */
struct amp_state
{
  /*
   * overridable fields
   */

  /*
   * non overridable fields, internal use
   */
  //---Start cut here---
  hamlib_port_t_deprecated ampport_deprecated;  /*!< \deprecated Amplifier port (internal use). */
  //---End cut here---

  int comm_state;         /*!< Comm port state, opened/closed. */
  rig_ptr_t priv;         /*!< Pointer to private amplifier state data. */
  rig_ptr_t obj;          /*!< Internal use by hamlib++ for event handling. */

  setting_t has_get_level; /*!< List of get levels. */
  setting_t has_set_level; /*!< List of set levels. */

  gran_t level_gran[RIG_SETTING_MAX]; /*!< Level granularity. */
  gran_t parm_gran[RIG_SETTING_MAX];  /*!< Parameter granularity. */
  hamlib_port_t ampport;  /*!< Amplifier port (internal use). */
};

#if defined(IN_HAMLIB)
#define AMPSTATE(a) (&(a)->state)
#endif

/** Macro for application access to amp_state data structure using the #AMP
 * handle.
 *
 * Example code.
 * ```
 * AMP *my_amp;
 *
 * //Instantiate an amp
 * my_amp = amp_init(AMP_MODEL_DUMMY); // your amp (amplifier) model.
 *
 * const struct amp_state *my_as = HAMLIB_AMPSTATE(my_amp);
 * ```
 */
#define HAMLIB_AMPSTATE(a) ((struct amp_state *)amp_data_pointer(a, RIG_PTRX_AMPSTATE))

__END_DECLS

#endif /* _AMP_STATE_H */

/** @} */
