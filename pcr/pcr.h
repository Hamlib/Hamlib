/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * pcr.h - Copyright (C) 2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICOM using the PCR1000 protocol.
 *
 *
 *    $Id: pcr.h,v 1.3 2001-06-15 07:08:37 f4cfe Exp $  
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
int pcr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int pcr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int pcr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int pcr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
const char *pcr_get_info(RIG *rig);


extern const struct rig_caps pcr1000_caps;
extern const struct rig_caps pcr100_caps;

extern HAMLIB_EXPORT(int) init_pcr(void *be_handle);


#endif /* _PCR_H */

