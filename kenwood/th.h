/*
 *  Hamlib Kenwood backend - TH handheld header
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: th.h,v 1.1 2001-11-09 15:42:11 f4cfe Exp $
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


#ifndef _TH_H
#define _TH_H 1

int th_set_vfo(RIG *rig, vfo_t vfo);
int th_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int th_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int th_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int th_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int th_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int th_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);

#endif /* _TH_H */
