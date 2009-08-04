/*
 *  Hamlib PCR backend - main header
 *  Copyright (c) 2001-2009 by Stephane Fillod
 *
 *	$Id: pcr.h,v 1.15 2009-02-06 17:31:33 fillods Exp $
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

#ifndef _PCR_H
#define _PCR_H 1

#include "hamlib/rig.h"
#include "token.h"

/* ext_level's tokens */
#define TOK_EL_ANL  TOKEN_BACKEND(1)


#define BACKEND_VER		"0.6"

#define PCR_MAX_CMD_LEN		32

struct pcr_priv_data {
	freq_t last_freq;
	rmode_t last_mode;

	int last_filter;
	int last_shift;
	int last_att;
	int last_agc;
	tone_t last_ctcss_sql;

	float	volume;
	float	squelch;

	int auto_update;
	int raw_level;

	char info[100];
	char cmd_buf[PCR_MAX_CMD_LEN];

	int protocol;
	int firmware;
	int country;
	int options;

	int sync;

	powerstat_t power;
};

extern const tone_t pcr_ctcss_list[];

int pcr_init(RIG *rig);
int pcr_cleanup(RIG *rig);
int pcr_open(RIG *rig);
int pcr_close(RIG *rig);
int pcr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int pcr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int pcr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int pcr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
const char *pcr_get_info(RIG *rig);


/* Added - G0WCW */

int pcr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int pcr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

int pcr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int pcr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int pcr_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val);

int pcr_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
int pcr_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
int pcr_set_trn(RIG * rig, int trn);
int pcr_decode_event(RIG *rig);
int pcr_set_powerstat(RIG * rig, powerstat_t status);
int pcr_get_powerstat(RIG * rig, powerstat_t *status);

/* ------------------------------------------------------------------ */

// int pcr_get_param(RIG *rig, setting_t parm, value_t *val);
// int pcr_set_param(RIG *rig, setting_t parm, value_t *val);

extern const struct rig_caps pcr1000_caps;
extern const struct rig_caps pcr100_caps;
extern const struct rig_caps pcr1500_caps;

#endif /* _PCR_H */
