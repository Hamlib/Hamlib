/*
 *  Hamlib PCR backend - main header
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: pcr.h,v 1.9 2003-04-16 22:30:41 fillods Exp $
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


struct pcr_priv_data {
	freq_t last_freq;
	rmode_t last_mode;
	int last_filter;
};

extern const int pcr1_ctcss_list[];

int pcr_init(RIG *rig);
int pcr_cleanup(RIG *rig);
int pcr_open(RIG *rig);
int pcr_close(RIG *rig);
int pcr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int pcr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int pcr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int pcr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
const char *pcr_get_info(RIG *rig);


/*Added - G0WCW ----------------------------------------------------- */

int pcr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int pcr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

int pcr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int pcr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);

int pcr_set_comm_rate(RIG *rig, int baud_rate);
int pcr_check_ok(RIG *rig);

int pcr_set_volume(RIG *rig, int level);
int pcr_set_squelch(RIG *rig, int level);
int pcr_set_IF_shift(RIG *rig, int shift);
int pcr_set_AGC(RIG *rig, int level);                // J45xx
int pcr_set_NB(RIG *rig, int level);                 // J46xx
int pcr_set_Attenuator(RIG *rig, int level);         // J47xx

int pcr_set_BFO(RIG *rig, int shift);                // J4Axx
int pcr_set_VSC(RIG *rig, int level);                // J50xx
int pcr_set_DSP(RIG *rig, int state);                // J80xx
int pcr_set_DSP_state(RIG *rig, int state);          // J8100=off J8101=on
int pcr_set_DSP_noise_reducer(RIG *rig, int state);  // J82xx
int pcr_set_DSP_auto_notch(RIG *rig, int state);     // J83xx
/* ------------------------------------------------------------------ */

// int pcr_get_param(RIG *rig, setting_t parm, value_t *val);
// int pcr_set_param(RIG *rig, setting_t parm, value_t *val);


extern const struct rig_caps pcr1000_caps;
extern const struct rig_caps pcr100_caps;


#endif /* _PCR_H */

