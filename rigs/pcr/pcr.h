/*
 *  Hamlib PCR backend - main header
 *  Copyright (c) 2001-2010 by Stephane Fillod
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

#ifndef _PCR_H
#define _PCR_H 1

#include "hamlib/rig.h"
#include "token.h"

/* ext_level's tokens */
#define TOK_EL_ANL  TOKEN_BACKEND(1)
#define TOK_EL_DIVERSITY  TOKEN_BACKEND(2)

#define BACKEND_VER		"20200323"
#define PCR_MAX_CMD_LEN		32

struct pcr_priv_data
{
	struct pcr_rcvr {

	    freq_t last_freq;
	    int last_mode;

	    int last_filter;
	    int last_shift;
	    int last_att;
	    int last_agc;
	    tone_t last_ctcss_sql;
	    tone_t last_dcs_sql;

	    float	volume;
	    float	squelch;

	    unsigned int raw_level;
	    unsigned int squelch_status;

	} main_rcvr, sub_rcvr;

	vfo_t current_vfo;

	int auto_update;

	char info[100];
	char cmd_buf[PCR_MAX_CMD_LEN];
	char reply_buf[PCR_MAX_CMD_LEN];

	int protocol;
	int firmware;
	int country;
	int options;

	int sync;

	powerstat_t power;
};

struct pcr_priv_caps
{
	unsigned int reply_size;
	unsigned int reply_offset;
	unsigned int always_sync;
};

#define pcr_caps(rig) ((struct pcr_priv_caps *)(rig)->caps->priv)

extern tone_t pcr_ctcss_list[];
extern tone_t pcr_dcs_list[];

int pcr_init(RIG *rig);
int pcr_cleanup(RIG *rig);
int pcr_open(RIG *rig);
int pcr_close(RIG *rig);
int pcr_set_vfo(RIG * rig, vfo_t vfo);
int pcr_get_vfo(RIG * rig, vfo_t *vfo);
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
int pcr_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *tone);
int pcr_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t tone);
int pcr_set_trn(RIG * rig, int trn);
int pcr_decode_event(RIG *rig);
int pcr_set_powerstat(RIG * rig, powerstat_t status);
int pcr_get_powerstat(RIG * rig, powerstat_t *status);
int pcr_get_dcd(RIG * rig, vfo_t vfo, dcd_t *dcd);

/* ------------------------------------------------------------------ */

// int pcr_get_param(RIG *rig, setting_t parm, value_t *val);
// int pcr_set_param(RIG *rig, setting_t parm, value_t *val);

extern const struct rig_caps pcr1000_caps;
extern const struct rig_caps pcr100_caps;
extern const struct rig_caps pcr1500_caps;
extern const struct rig_caps pcr2500_caps;

#endif /* _PCR_H */
