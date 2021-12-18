/*
 *  Hamlib JRC backend - main header
 *  Copyright (c) 2001-2009 by Stephane Fillod
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

#ifndef _JRC_H
#define _JRC_H 1

#include <hamlib/rig.h>

#define BACKEND_VER	"20200320"

struct jrc_priv_caps {
	int max_freq_len;
	int info_len;
	int mem_len;
	int pbs_info_len;
	int pbs_len;
	int beep;
	int beep_len;
	const char * cw_pitch;
};

int jrc_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len);
int jrc_open(RIG *rig);
int jrc_close(RIG *rig);
int jrc_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int jrc_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int jrc_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int jrc_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int jrc_set_vfo(RIG *rig, vfo_t vfo);
int jrc_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int jrc_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int jrc_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int jrc_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int jrc_set_parm(RIG *rig, setting_t parm, value_t val);
int jrc_get_parm(RIG *rig, setting_t parm, value_t *val);
int jrc_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int jrc_set_trn(RIG *rig, int trn);
int jrc_set_mem(RIG *rig, vfo_t vfo, int ch);
int jrc_get_mem(RIG *rig, vfo_t vfo, int *ch);
int jrc_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan);
int jrc_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);
int jrc_set_powerstat(RIG *rig, powerstat_t status);
int jrc_get_powerstat(RIG *rig, powerstat_t *status);
int jrc_reset(RIG *rig, reset_t reset);
int jrc_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int jrc_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
int jrc_decode_event(RIG *rig);

extern const struct rig_caps nrd535_caps;
extern const struct rig_caps nrd545_caps;
extern const struct rig_caps nrd525_caps;
extern const struct rig_caps jst145_caps;
extern const struct rig_caps jst245_caps;


#endif /* _JRC_H */

