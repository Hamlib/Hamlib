/*
 *  Hamlib CI-V backend - main header
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: icom.h,v 1.52 2003-02-19 23:56:56 fillods Exp $
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

#ifndef _ICOM_H
#define _ICOM_H 1

#include <hamlib/rig.h>
#include <cal.h>
#include <tones.h>

/*
 * defines used by comp_cal_str in rig.c
 * STR_CAL_LENGTH is the lenght of the S Meter calibration table
 * STR_CAL_S0 is the value in dB of the lowest value (not even in table)
 */
#define STR_CAL_LENGTH 16
#define STR_CAL_S0 -54


/*
 * minimal channel caps.
 * If your rig has better/lesser, don't modify this define but clone it,
 * so you don't change other rigs.
 */
#define IC_MIN_MEM_CAP {	\
	.freq = 1,	\
	.mode = 1,	\
	.width = 1,	\
} 

/*
 * common channel caps.
 * If your rig has better/lesser, don't modify this define but clone it,
 * so you don't change other rigs.
 */
#define IC_MEM_CAP {	\
	.freq = 1,	\
	.mode = 1,	\
	.width = 1,	\
	.rptr_shift = 1,	\
	.rptr_offs = 1,	\
	.ctcss_tone = 1,	\
	.ctcss_sql = 1,	\
	.dcs_code = 1,	\
	.dcs_sql = 1,	\
} 

struct ts_sc_list {
	shortfreq_t ts;	/* tuning step */
	unsigned char sc;	/* sub command */
};

struct icom_priv_caps {
	unsigned char re_civ_addr;	/* the remote dlft equipment's CI-V address*/
	int civ_731_mode; /* Off: freqs on 10 digits, On: freqs on 8 digits */
	const struct ts_sc_list *ts_sc_list;
	cal_table_t str_cal;
};


struct icom_priv_data {
	unsigned char re_civ_addr;	/* the remote equipment's CI-V address*/
	int civ_731_mode; /* Off: freqs on 10 digits, On: freqs on 8 digits */
	cal_table_t str_cal;
};

extern const struct ts_sc_list r8500_ts_sc_list[];
extern const struct ts_sc_list ic737_ts_sc_list[];
extern const struct ts_sc_list r75_ts_sc_list[];
extern const struct ts_sc_list r7100_ts_sc_list[];
extern const struct ts_sc_list r9000_ts_sc_list[];
extern const struct ts_sc_list ic756_ts_sc_list[];
extern const struct ts_sc_list ic756pro_ts_sc_list[];
extern const struct ts_sc_list ic706_ts_sc_list[];
extern const struct ts_sc_list ic910_ts_sc_list[];
extern const struct ts_sc_list ic718_ts_sc_list[];

int icom_init(RIG *rig);
int icom_cleanup(RIG *rig);
int icom_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int icom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int icom_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int icom_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int icom_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int icom_set_vfo(RIG *rig, vfo_t vfo);
int icom_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
int icom_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift);
int icom_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs);
int icom_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs);
int icom_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int icom_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int icom_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width);
int icom_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width);
int icom_set_split(RIG *rig, vfo_t vfo, split_t split);
int icom_get_split(RIG *rig, vfo_t vfo, split_t *split);
int icom_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
int icom_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
int icom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int icom_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int icom_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int icom_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int icom_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
int icom_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
int icom_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
int icom_set_bank(RIG *rig, vfo_t vfo, int bank);
int icom_set_mem(RIG *rig, vfo_t vfo, int ch);
int icom_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int icom_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
int icom_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int icom_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int icom_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int icom_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int icr75_set_channel(RIG *rig, const channel_t *chan);
int icr75_get_channel(RIG *rig, channel_t *chan);
int icom_set_conf(RIG *rig, token_t token, const char *val);
int icom_get_conf(RIG *rig, token_t token, char *val);
int icom_set_powerstat(RIG *rig, powerstat_t status);
int icom_get_powerstat(RIG *rig, powerstat_t *status);
int icom_decode_event(RIG *rig);

int optoscan_open(RIG *rig);
int optoscan_close(RIG *rig);
const char* optoscan_get_info(RIG *rig);

extern const struct confparams icom_cfg_params[];

extern const struct rig_caps ic706_caps;
extern const struct rig_caps ic706mkii_caps;
extern const struct rig_caps ic706mkiig_caps;
extern const struct rig_caps ic718_caps;
extern const struct rig_caps ic725_caps;
extern const struct rig_caps ic735_caps;
extern const struct rig_caps ic736_caps;
extern const struct rig_caps ic737_caps;
extern const struct rig_caps ic756_caps;
extern const struct rig_caps ic756pro_caps;
extern const struct rig_caps ic756pro2_caps;
extern const struct rig_caps ic775_caps;
extern const struct rig_caps ic821h_caps;
extern const struct rig_caps ic910_caps;
extern const struct rig_caps ic970_caps;
extern const struct rig_caps icr7000_caps;
extern const struct rig_caps icr8500_caps;
extern const struct rig_caps ic275_caps;
extern const struct rig_caps ic475_caps;

extern const struct rig_caps os456_caps;
extern const struct rig_caps os535_caps;

extern const struct rig_caps omnivip_caps;

extern BACKEND_EXPORT(rig_model_t) proberigs_icom(port_t *p);
extern BACKEND_EXPORT(int) initrigs_icom(void *be_handle);


#endif /* _ICOM_H */

