/*
 *  Hamlib Tentec backend - Argonaut, Jupiter, RX-350 header
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: tentec2.h,v 1.1 2003-05-12 22:29:59 fillods Exp $
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

#ifndef _TENTEC2_H
#define _TENTEC2_H 1

#include <hamlib/rig.h>

/*
 * Mem caps to be checked..
 */
#define TT_MEM_CAP {        \
	.freq = 1,      \
	.mode = 1,      \
	.width = 1,     \
	.tx_freq = 1,	\
	.tx_mode = 1,	\
	.tx_width = 1,	\
	.split = 1,	\
}

int tentec2_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int tentec2_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int tentec2_set_vfo(RIG *rig, vfo_t vfo);
int tentec2_get_vfo(RIG *rig, vfo_t *vfo);
int tentec2_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int tentec2_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int tentec2_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
int tentec2_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
int tentec2_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int tentec2_reset(RIG *rig, reset_t reset);
const char* tentec2_get_info(RIG *rig);

#endif /* _TENTEC2_H */
