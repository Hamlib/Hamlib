/*
 *  Hamlib Interface - event handling header
 *  Copyright (c) 2000-2003 by Stephane Fillod and Frank Singleton
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

#ifndef _EVENT_H
#define _EVENT_H 1

#include "hamlib/rig.h"

int rig_poll_routine_start(RIG *rig);
int rig_poll_routine_stop(RIG *rig);

int rig_fire_freq_event(RIG *rig, vfo_t vfo, freq_t freq);
int rig_fire_mode_event(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int rig_fire_vfo_event(RIG *rig, vfo_t vfo);
int rig_fire_ptt_event(RIG *rig, vfo_t vfo, ptt_t ptt);
int rig_fire_dcd_event(RIG *rig, vfo_t vfo, dcd_t dcd);
int rig_fire_pltune_event(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode, pbwidth_t *width);
int rig_fire_spectrum_event(RIG *rig, struct rig_spectrum_line *line);

#endif /* _EVENT_H */

