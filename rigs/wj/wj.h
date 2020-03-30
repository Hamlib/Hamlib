/*
 *  Hamlib Watkins-Johnson backend - main header
 *  Copyright (c) 2004 by Stephane Fillod
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

#ifndef _WJ_H
#define _WJ_H 1

#include <hamlib/rig.h>

#define BACKEND_VER "20040912"

#define TOK_RIGID TOKEN_BACKEND(1)

extern const struct confparams wj_cfg_params[];

struct wj_priv_data {
	unsigned receiver_id;
	freq_t freq;
	rmode_t mode;
	pbwidth_t width;
	value_t agc;
	value_t rfgain;
	value_t ifshift;
	value_t rawstr;
};

int wj_set_conf(RIG *rig, token_t token, const char *val);
int wj_get_conf(RIG *rig, token_t token, char *val);
int wj_init(RIG *rig);
int wj_cleanup(RIG *rig);
int wj_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int wj_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int wj_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int wj_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int wj_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int wj_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

extern const struct rig_caps wj8888_caps;

#endif /* _WJ_H */
