/*
 *  Hamlib Kenwood backend - IC-10 header
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ic10.h,v 1.1 2004-05-02 17:13:33 fillods Exp $
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

int ic10_set_vfo(RIG *rig, vfo_t vfo);
int ic10_get_vfo(RIG *rig, vfo_t *vfo);
int ic10_set_split_vfo(RIG *rig, vfo_t vfo , split_t split, vfo_t txvfo);
int ic10_get_split_vfo(RIG *rig, vfo_t vfo , split_t *split, vfo_t *txvfo);
int ic10_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

int ic10_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int ic10_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int ic10_get_mem(RIG *rig, vfo_t vfo, int *ch);
int ic10_set_channel(RIG *rig, const channel_t *chan);
int ic10_get_channel(RIG *rig, channel_t *chan);
int ic10_decode_event (RIG *rig);

#define IC10_CHANNEL_CAPS \
		.freq=1,\
		.tx_freq=1,\
		.mode=1,\
		.tx_mode=1

#endif /* _IC10_H */
