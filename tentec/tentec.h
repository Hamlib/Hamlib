/*
 *  Hamlib Tentec backend - main header
 *  Copyright (c) 2001-2004 by Stephane Fillod
 *
 *	$Id: tentec.h,v 1.8 2004-05-03 22:34:14 fillods Exp $
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

#ifndef _TENTEC_H
#define _TENTEC_H 1

#include <hamlib/rig.h>

struct tentec_priv_data {
	rmode_t mode;		/* detection mode */
	freq_t freq;		/* tuned frequency */
	pbwidth_t width;	/* filter bandwidth in Hz */
	int cwbfo;		/* BFO frequency: 0 [0-2000Hz] */
	float lnvol;		/* line-out volume: 30 [0..63] */
	float spkvol;		/* speaker volume: 30 [0..63] */
	float agc;		/* AGC: medium*/

	/* calculated by tentec_tuning_factor_calc() */
	int ctf; /* Coarse Tune Factor */
	int ftf; /* Fine Tune Factor */
	int btf; /* Bfo Tune Factor, btval is ignored by RX-320 in AM MODE */
};

int tentec_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len);

int tentec_init(RIG *rig);
int tentec_init(RIG *rig);
int tentec_cleanup(RIG *rig);
int tentec_trx_open(RIG *rig);
int tentec_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int tentec_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int tentec_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int tentec_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int tentec_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int tentec_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
const char* tentec_get_info(RIG *rig);

extern const struct rig_caps rx320_caps;
extern const struct rig_caps rx340_caps;
extern const struct rig_caps rx350_caps;
extern const struct rig_caps tt516_caps;
extern const struct rig_caps tt538_caps;
extern const struct rig_caps tt550_caps;
extern const struct rig_caps tt565_caps;


#endif /* _TENTEC_H */
