/*
 *  Hamlib Racal backend - main header
 *  Copyright (c) 2004-2010 by Stephane Fillod
 *
 *	$Id: racal.h,v 1.1 2004-09-12 21:28:26 fillods Exp $
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

#ifndef _RACAL_H
#define _RACAL_H 1

#include "hamlib/rig.h"

#define TOK_RIGID TOKEN_BACKEND(1)

extern const struct confparams racal_cfg_params[];

struct racal_priv_data {
	unsigned receiver_id;
	int bfo;
	float threshold;	/* attenuation */
};

int racal_set_conf(RIG *rig, token_t token, const char *val);
int racal_get_conf(RIG *rig, token_t token, char *val);
int racal_init(RIG *rig);
int racal_cleanup(RIG *rig);
int racal_open(RIG *rig);
int racal_close(RIG *rig);

int racal_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int racal_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int racal_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int racal_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int racal_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int racal_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int racal_reset(RIG *rig, reset_t reset);
const char* racal_get_info(RIG *rig);

extern const struct rig_caps ra6790_caps;
extern const struct rig_caps ra3702_caps;


#endif	/* _RACAL_H */
