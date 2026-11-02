/*
 *  Hamlib SmartSDR levels, functions and antenna selection
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

/* The Hamlib levels, functions and antenna ports a SmartSDR rig answers. */

#ifndef SMARTSDR_LEVEL_H
#define SMARTSDR_LEVEL_H

#include <hamlib/rig.h>

extern int smartsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
extern int smartsdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
extern int smartsdr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
extern int smartsdr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
extern int smartsdr_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);
extern int smartsdr_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                            ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);

#endif /* SMARTSDR_LEVEL_H */
