/*
 *  Hamlib WiNRADiO backend - main header
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: winradio.h,v 1.8 2003-04-16 22:30:43 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _WINRADIO_H
#define _WINRADIO_H 1

#include <hamlib/rig.h>

int wr_rig_init(RIG *rig);
int wr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int wr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int wr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int wr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int wr_set_powerstat(RIG *rig, powerstat_t status);
int wr_get_powerstat(RIG *rig, powerstat_t *status);
int wr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int wr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int wr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int wr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
const char *wr_get_info(RIG *rig);

extern const struct rig_caps wr1000_caps;
extern const struct rig_caps wr1500_caps;
extern const struct rig_caps wr1550_caps;
extern const struct rig_caps wr3100_caps;
extern const struct rig_caps wr3150_caps;
extern const struct rig_caps wr3500_caps;
extern const struct rig_caps wr3700_caps;

#endif /* _WINRADIO_H */
