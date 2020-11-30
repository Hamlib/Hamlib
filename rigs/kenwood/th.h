/*
 *  Hamlib Kenwood backend - TH handheld header
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
#ifndef __TH_H__
#define __TH_H__ 1

#include "idx_builtin.h"

#define TH_VER "20200701"

extern int th_transaction (RIG *rig, const char *cmdstr, char *data, size_t datasize);
extern int th_get_vfo_char(RIG *rig, vfo_t *vfo, char *vfoch);

extern int th_decode_event (RIG *rig);
extern int th_set_freq (RIG *rig, vfo_t vfo, freq_t freq);
extern int th_get_freq (RIG *rig, vfo_t vfo, freq_t *freq);
extern int th_set_mode (RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
extern int th_get_mode (RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
extern int th_set_vfo(RIG *rig, vfo_t vfo);
extern int th_get_vfo(RIG *rig, vfo_t *vfo);
extern int tm_set_vfo_bc2 (RIG *rig, vfo_t vfo);
extern int th_set_split_vfo (RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo);
extern int th_get_split_vfo (RIG *rig, vfo_t vfo, split_t *split, vfo_t *txvfo);
extern int th_set_trn(RIG *rig, int trn);
extern int th_get_trn (RIG *rig, int *trn);
extern int th_set_powerstat (RIG *rig, powerstat_t status);
extern int th_get_powerstat (RIG *rig, powerstat_t *status);
extern int th_set_func (RIG *rig, vfo_t vfo, setting_t func, int status);
extern int th_get_func (RIG *rig, vfo_t vfo, setting_t func, int *status);
extern int th_set_parm (RIG *rig, setting_t parm, value_t val);
extern int th_get_parm (RIG *rig, setting_t parm, value_t *val);
extern int th_get_level (RIG *rig, vfo_t vfo, setting_t level, value_t *val);
extern int th_set_level (RIG *rig, vfo_t vfo, setting_t level, value_t val);
extern int th_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
extern int th_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
extern int th_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
extern int th_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
extern int th_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
extern int th_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code);
extern const char *th_get_info(RIG *rig);
extern int th_set_mem(RIG *rig, vfo_t vfo, int ch);
extern int th_get_mem(RIG *rig, vfo_t vfo, int *ch);
extern int th_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
extern int th_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
extern int th_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
extern int th_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);
extern int th_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);
extern int th_set_ant (RIG * rig, vfo_t vfo, ant_t ant, value_t option);
extern int th_get_ant (RIG * rig, vfo_t vfo, ant_t dummy, value_t *option, ant_t * ant_curr, ant_t *ant_tx, ant_t *ant_rx);
extern int th_reset(RIG *rig, reset_t reset);
extern int th_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);


#define TH_CHANNEL_CAPS \
.freq=1,\
.tx_freq=1,\
.split=1,\
.mode=1,\
.width=1,\
.tuning_step=1,\
.rptr_shift=1,\
.rptr_offs=1,\
.funcs=RIG_FUNC_REV,\
.ctcss_tone=1,\
.ctcss_sql=1,\
.channel_desc=1


#endif /* __TH_H__ */
/* end of file */
