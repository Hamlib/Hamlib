/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * kenwood.h - Copyright (C) 2000,2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to a Kenwood radio.
 *
 *
 *    $Id: kenwood.h,v 1.4 2001-05-22 21:59:26 f4cfe Exp $  
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

#ifndef _KENWOOD_H
#define _KENWOOD_H 1

extern const int kenwood38_ctcss_list[];

int kenwood_set_vfo(RIG *rig, vfo_t vfo);
int kenwood_get_vfo(RIG *rig, vfo_t *vfo);
int kenwood_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int kenwood_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int kenwood_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int kenwood_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int kenwood_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int kenwood_set_ctcss(RIG *rig, vfo_t vfo, unsigned int tone);
int kenwood_get_ctcss(RIG *rig, vfo_t vfo, unsigned int *tone);
int kenwood_set_powerstat(RIG *rig, powerstat_t status);
int kenwood_get_powerstat(RIG *rig, powerstat_t *status);
int kenwood_reset(RIG *rig, reset_t reset);
int kenwood_send_morse(RIG *rig, vfo_t vfo, const char *msg);
int kenwood_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int kenwood_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);

int kenwood_set_trn(RIG *rig, vfo_t vfo, int trn);
int kenwood_get_trn(RIG *rig, vfo_t vfo, int *trn);

extern const struct rig_caps ts870s_caps;

extern int init_kenwood(void *be_handle);


#endif /* _KENWOOD_H */

