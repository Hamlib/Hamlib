/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icom.h - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICOM using the "CI-V" interface.
 *
 *
 *    $Id: icom.h,v 1.4 2000-10-08 21:39:58 f4cfe Exp $  
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


struct icom_priv_data {
	unsigned char re_civ_addr;	/* the remote equipment's CI-V address*/
	int civ_731_mode; /* Off: freqs on 10 digits, On: freqs on 8 digits */
};

int icom_init(RIG *rig);
int icom_cleanup(RIG *rig);
int icom_set_freq(RIG *rig, freq_t freq);
int icom_get_freq(RIG *rig, freq_t *freq);
int icom_set_mode(RIG *rig, rmode_t mode);
int icom_get_mode(RIG *rig, rmode_t *mode);
int icom_set_vfo(RIG *rig, vfo_t vfo);
int icom_get_strength(RIG *rig, int *strength);
int icom_decode_event(RIG *rig);

extern const struct rig_caps ic706_caps;
extern const struct rig_caps ic706mkii_caps;
extern const struct rig_caps ic706mkiig_caps;

extern int init_icom(void *be_handle);


#endif /* _ICOM_H */

