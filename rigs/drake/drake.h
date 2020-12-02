/*
 *  Hamlib Drake backend - main header
 *  Copyright (c) 2001-2004 by Stephane Fillod
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

#ifndef _DRAKE_H
#define _DRAKE_H 1

#include <hamlib/rig.h>

#define BACKEND_VER "20200319"

struct drake_priv_data {
	int curr_ch;
};

int drake_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int drake_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int drake_set_vfo(RIG *rig, vfo_t vfo);
int drake_get_vfo(RIG *rig, vfo_t *vfo);
int drake_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int drake_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int drake_init(RIG *rig);
int drake_cleanup(RIG *rig);
int drake_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);
int drake_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option, ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);
int drake_set_mem(RIG *rig, vfo_t vfo, int ch);
int drake_get_mem(RIG *rig, vfo_t vfo, int *ch);
int drake_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan);
int drake_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);
int drake_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int drake_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int drake_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int drake_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int drake_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int drake_set_powerstat (RIG * rig, powerstat_t status);
int drake_get_powerstat (RIG * rig, powerstat_t *status);
const char *drake_get_info(RIG *rig);

extern const struct rig_caps r8a_caps;
extern const struct rig_caps r8b_caps;

#endif /* _DRAKE_H */

