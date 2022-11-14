/*
 *  Hamlib Tentec backend - Argonaut, Jupiter, RX-350 header
 *  Copyright (c) 2001-2003 by Stephane Fillod
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

#ifndef _TENTEC2_H
#define _TENTEC2_H 1

#include <hamlib/rig.h>
#include <idx_builtin.h>

// The include order will determine which BACKEND_VER is used
// tentec.h may also be included and the last include is the BACKEND_VER used
#undef BACKEND_VER
#define BACKEND_VER "20191208"

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
int tentec2_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int tentec2_reset(RIG *rig, reset_t reset);
const char* tentec2_get_info(RIG *rig);

#endif /* _TENTEC2_H */
