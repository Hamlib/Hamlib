/*
 *  Hamlib SmartSDR slices: split, VFO selection and VFO operations
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

/* Which slice this rig drives, which one transmits, and the Hamlib calls that */
/* are answered by choosing between them.                                      */

#ifndef SMARTSDR_SLICE_H
#define SMARTSDR_SLICE_H

#include <hamlib/rig.h>

#include "smartsdr_priv.h"

/* Record in_use/tx/frequency for any slice, so the transmit slice is known. */
extern void smartsdr_slice_info_absorb(struct smartsdr_priv_data *priv,
                                       int slicenum, const char *line);

/* The slice currently designated to transmit, or -1 if none is known. */
extern int smartsdr_tx_slice(const struct smartsdr_priv_data *priv);

/* Classic operating values, derived from the tracked properties. */
extern freq_t smartsdr_slice_freq(const struct smartsdr_priv_data *priv);
extern void smartsdr_slice_set_freq(struct smartsdr_priv_data *priv, freq_t hz);
extern rmode_t smartsdr_slice_mode(const struct smartsdr_priv_data *priv);
extern pbwidth_t smartsdr_slice_width(const struct smartsdr_priv_data *priv);
extern int smartsdr_slice_ptt(const struct smartsdr_priv_data *priv);

/* Main is this rig's slice, Sub is the transmit slice; A/B are accepted as
 * aliases. Returns 1 for Sub, 0 for Main, -1 if the VFO is not one of them. */
extern int smartsdr_vfo_is_sub(vfo_t vfo);

/* The slice behind the Sub VFO, creating one if create is non-zero. */
extern int smartsdr_sub_slice(RIG *rig, int create);

/* Ask the radio for a new slice on the given frequency and return its number.
 * The radio picks the index itself; it cannot be requested. */
extern int smartsdr_slice_create(RIG *rig, double freq_hz, const char *ant,
                                 const char *mode);

/* Create the transmit slice and hand it the transmitter. */
extern int smartsdr_create_split_slice(RIG *rig);

/* Make sure the slice this rig is bound to exists, creating one when the
 * configuration allows it. */
extern int smartsdr_ensure_slice(RIG *rig);

/* Remove a slice created because the configured one was absent. */
extern void smartsdr_release_opened_slice(RIG *rig);

/* Split, expressed through whichever slice carries tx=1. */
extern int smartsdr_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                  vfo_t *tx_vfo);
extern int smartsdr_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                  vfo_t tx_vfo);
extern int smartsdr_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
extern int smartsdr_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
extern int smartsdr_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                   pbwidth_t width);
extern int smartsdr_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                                   pbwidth_t *width);

extern int smartsdr_set_vfo(RIG *rig, vfo_t vfo);
extern int smartsdr_get_vfo(RIG *rig, vfo_t *vfo);
extern int smartsdr_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

#endif /* SMARTSDR_SLICE_H */
