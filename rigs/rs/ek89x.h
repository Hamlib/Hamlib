/*
 *  Hamlib R&S backend for EK895/896 - main header
 *  Reused from gp2000.c
 *  Copyright (c) 2022 by Michael Black W9MDB
 *  Copyright (c) 2018 by Michael Black W9MDB
 *  Copyright (c) 2009-2010 by Stephane Fillod
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

#ifndef _EK89X_H
#define _EK89X_H 1

#include <hamlib/rig.h>


int ek89x_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int ek89x_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int ek89x_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ek89x_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int ek89x_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int ek89x_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int ek89x_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int ek89x_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int ek89x_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int ek89x_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int ek89x_reset(RIG *rig, reset_t reset);
const char * gek89x_get_info(RIG *rig);

extern const struct rig_caps ek89x_caps;

#endif /* EK89X_H */
