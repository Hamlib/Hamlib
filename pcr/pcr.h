/*
 *  Hamlib PCR backend - main header
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *		$Id: pcr.h,v 1.6 2001-12-28 20:28:03 fillods Exp $
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
int pcr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int pcr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int pcr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int pcr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
const char *pcr_get_info(RIG *rig);


extern const struct rig_caps pcr1000_caps;
extern const struct rig_caps pcr100_caps;

extern BACKEND_EXPORT(int) initrigs_pcr(void *be_handle);


#endif /* _PCR_H */

