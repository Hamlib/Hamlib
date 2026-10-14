/*
 *  Hamlib SmartSDR tracked radio properties
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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

/* Reading and writing the radio settings the backend tracks. A property is */
/* named by its SMARTSDR_P_* index, which is all a caller needs to know: the */
/* status key, the object it belongs to and the command that sets it are     */
/* held here.                                                                */

#ifndef SMARTSDR_PROPS_H
#define SMARTSDR_PROPS_H

#include <stddef.h>

#include <hamlib/rig.h>

#include "smartsdr_priv.h"

/* Hold the model still. props, slices and the meter table are written by the
 * control thread as status arrives and by the UDP dispatcher as meter values
 * arrive, so a reader takes this for the moment it needs to copy a value out.
 * The functions that absorb or report a value do not take it themselves: the
 * caller decides how much has to be consistent. Nothing else may be locked
 * while it is held, and no I/O may be done. */
extern void smartsdr_state_lock(struct smartsdr_priv_data *priv);
extern void smartsdr_state_unlock(struct smartsdr_priv_data *priv);

/* Find "key=" in a status line, requiring a space or '|' before it so that
 * e.g. "pan=" does not match inside "audio_pan=". Returns the value start,
 * or NULL when the line does not carry the key. */
extern const char *smartsdr_status_find(const char *line, const char *key);

/* Read a status value as a number, reporting whether the field carried one.
 * strtol and strtod both answer 0 for an empty or non-numeric value, so a
 * truncated "RF_frequency=" would otherwise record a radio tuned to 0 Hz and a
 * truncated "in_use=" would mark the slice unused. Both return 0 and leave
 * *out alone when there is nothing to read. */
extern int smartsdr_status_as_double(const char *v, double *out);
extern int smartsdr_status_as_long(const char *v, long *out);

/* Update tracked properties from one status line of the given object. */
extern void smartsdr_props_absorb(struct smartsdr_priv_data *priv, int object,
                                  const char *line);

/* Whether a property is usable: the radio has reported it and, when an expiry
 * is configured, has done so recently enough. */
extern int smartsdr_prop_valid(const struct smartsdr_priv_data *priv, int idx);

/* Build the command that assigns a property, into a buffer of out_len bytes.
 * Returns 0, or -1 when the index names no property or the command does not
 * fit. */
extern int smartsdr_prop_build_set_cmd(int idx, int slicenum,
                                       const char *value,
                                       char *out, size_t out_len);

/* Send a property assignment and record the accepted value. */
extern int smartsdr_prop_set(RIG *rig, int idx, const char *value);

#endif /* SMARTSDR_PROPS_H */
