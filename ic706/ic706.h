/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ic706.h - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an IC-706 using the "CI-V" interface.
 *
 *
 *    $Id: ic706.h,v 1.1 2000-09-14 00:44:21 f4cfe Exp $  
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

#ifndef _IC706_H
#define _IC706_H 1


struct ic706_priv_data {
		unsigned char trx_civ_address;	/* the remote equipment's CI-V address*/
		unsigned char ctrl_civ_address;	/* the controller's CI-V address */
		int civ_731_mode; /* Off: freqs on 5 bytes, On: freqs on 4 bytes */
};

static int ic706_init(RIG *rig);
static int ic706_cleanup(RIG *rig);
static int ic706_set_freq_main_vfo_hz(struct rig_state *rig_s, freq_t freq, rig_mode_t mode);

#endif /* _IC706_H */

