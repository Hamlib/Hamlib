/*
 *  Hamlib Kenwood backend - IC-10 header
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ic10.h,v 1.4 2005-04-10 21:57:13 fillods Exp $
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

#ifndef _IC10_H
#define _IC10_H 1

#define IC10_VER	"0.6"

int ic10_cmd_trim (char *data, int data_len);
int ic10_transaction (RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len);
int ic10_set_vfo(RIG *rig, vfo_t vfo);
int ic10_get_vfo(RIG *rig, vfo_t *vfo);
int ic10_set_split_vfo(RIG *rig, vfo_t vfo , split_t split, vfo_t txvfo);
int ic10_get_split_vfo(RIG *rig, vfo_t vfo , split_t *split, vfo_t *txvfo);
int ic10_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int ic10_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ic10_set_ant(RIG *rig, vfo_t vfo, ant_t ant);
int ic10_get_ant(RIG *rig, vfo_t vfo, ant_t *ant);
int ic10_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int ic10_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int ic10_set_parm(RIG *rig, setting_t parm, value_t val);
int ic10_get_parm(RIG *rig, setting_t parm, value_t *val);
int ic10_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int ic10_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int ic10_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int ic10_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int ic10_set_mem(RIG *rig, vfo_t vfo, int ch);
int ic10_get_mem(RIG *rig, vfo_t vfo, int *ch);
int ic10_set_channel(RIG *rig, const channel_t *chan);
int ic10_get_channel(RIG *rig, channel_t *chan);
int ic10_set_powerstat(RIG *rig, powerstat_t status);
int ic10_get_powerstat(RIG *rig, powerstat_t *status);
int ic10_set_trn(RIG *rig, int trn);
int ic10_get_trn(RIG *rig, int *trn);
int ic10_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int ic10_scan(RIG * rig, vfo_t vfo, scan_t scan, int ch);
const char* ic10_get_info(RIG *rig);
int ic10_decode_event (RIG *rig);

#define IC10_CHANNEL_CAPS \
		.freq=1,\
		.tx_freq=1,\
		.mode=1,\
		.tx_mode=1

#endif /* _IC10_H */
