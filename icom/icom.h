/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icom.h - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICOM using the "CI-V" interface.
 *
 *
 *    $Id: icom.h,v 1.12 2001-02-14 23:55:54 f4cfe Exp $  
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#ifndef _ICOM_H
#define _ICOM_H 1

#include <hamlib/rig.h>

/*
 * defines used by comp_cal_str in rig.c 
 * STR_CAL_LENGTH is the lenght of the S Meter calibration table
 * STR_CAL_S0 is the value in dB of the lowest value (not even in table)
 */
#define STR_CAL_LENGTH 16
#define STR_CAL_S0 -54


struct ts_sc_list {
	shortfreq_t ts;	/* tuning step */
	unsigned char sc;	/* sub command */
};

struct icom_priv_data {
	unsigned char re_civ_addr;	/* the remote equipment's CI-V address*/
	int civ_731_mode; /* Off: freqs on 10 digits, On: freqs on 8 digits */
	const struct ts_sc_list *ts_sc_list;
	int str_cal_raw[STR_CAL_LENGTH];
	int str_cal_db[STR_CAL_LENGTH];
};

int icom_init(RIG *rig);
int icom_cleanup(RIG *rig);
int icom_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int icom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int icom_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int icom_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int icom_set_vfo(RIG *rig, vfo_t vfo);
int icom_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
int icom_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift);
int icom_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs);
int icom_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs);
int icom_set_split_freq(RIG *rig, vfo_t vfo, freq_t rx_freq, freq_t tx_freq);
int icom_get_split_freq(RIG *rig, vfo_t vfo, freq_t *rx_freq, freq_t *tx_freq);
int icom_set_split(RIG *rig, vfo_t vfo, split_t split);
int icom_get_split(RIG *rig, vfo_t vfo, split_t *split);
int icom_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
int icom_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
int icom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int icom_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int icom_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int icom_set_bank(RIG *rig, vfo_t vfo, int bank);
int icom_set_mem(RIG *rig, vfo_t vfo, int ch);
int icom_mv_ctl(RIG *rig, vfo_t vfo, mv_op_t op);
int icom_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int icom_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int icom_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int icom_set_channel(RIG *rig, const channel_t *chan);
int icom_get_channel(RIG *rig, channel_t *chan);
int icom_set_poweron(RIG *rig);
int icom_set_poweroff(RIG *rig);
int icom_decode_event(RIG *rig);

extern const struct rig_caps ic706_caps;
extern const struct rig_caps ic706mkii_caps;
extern const struct rig_caps ic706mkiig_caps;
extern const struct rig_caps icr8500_caps;

extern int init_icom(void *be_handle);


#endif /* _ICOM_H */

