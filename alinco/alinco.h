/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * alinco.h - Copyright (C) 2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to a Alinco radio.
 *
 *
 *    $Id: alinco.h,v 1.5 2001-07-01 11:46:17 f4cfe Exp $  
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

#ifndef _ALINCO_H
#define _ALINCO_H 1

#if defined(__CYGWIN__)
#  undef HAMLIB_DLL
#  include <hamlib/rig.h>
#  include <cal.h>
#  define HAMLIB_DLL
#  include <hamlib/rig_dll.h>
#else
#  include <hamlib/rig.h>
#  include <cal.h>
#endif

struct alinco_priv_caps {
	cal_table_t str_cal;
};

int alinco_set_vfo(RIG *rig, vfo_t vfo);
int alinco_get_vfo(RIG *rig, vfo_t *vfo);
int alinco_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int alinco_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int alinco_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int alinco_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int alinco_set_split(RIG *rig, vfo_t vfo, split_t split);
int alinco_get_split(RIG *rig, vfo_t vfo, split_t *split);
int alinco_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int alinco_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int alinco_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int alinco_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int alinco_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int alinco_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int alinco_set_parm(RIG *rig, vfo_t vfo, setting_t parm, value_t val);
int alinco_get_parm(RIG *rig, vfo_t vfo, setting_t parm, value_t *val);
int alinco_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int alinco_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
int alinco_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int alinco_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int alinco_set_mem(RIG *rig, vfo_t vfo, int ch);
int alinco_get_mem(RIG *rig, vfo_t vfo, int *ch);

extern const struct rig_caps dx77_caps;

extern HAMLIB_EXPORT(int) init_alinco(void *be_handle);


#endif /* _ALINCO_H */

