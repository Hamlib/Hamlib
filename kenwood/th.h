/*
 *  Hamlib Kenwood backend - TH handheld header
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: th.h,v 1.4 2003-03-24 23:08:28 fillods Exp $
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
#ifndef __TH_H__
#define __TH_H__ 1

extern int th_transaction (RIG *rig, const char *cmdstr, char *data, size_t datasize);
extern int th_decode_event (RIG *rig);
extern int th_set_freq (RIG *rig, vfo_t vfo, freq_t freq);
extern int th_get_freq (RIG *rig, vfo_t vfo, freq_t *freq);
extern int th_set_mode (RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
extern int th_get_mode (RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
extern int th_set_vfo(RIG *rig, vfo_t vfo);
extern int th_get_vfo(RIG *rig, vfo_t *vfo);
extern int th_set_trn(RIG *rig, int trn);
extern int th_get_trn (RIG *rig, int *trn);
extern int th_get_func (RIG *rig, vfo_t vfo, setting_t func, int *status);
extern int th_get_parm (RIG *rig, setting_t parm, value_t *val);
extern int th_get_level (RIG *rig, vfo_t vfo, setting_t level, value_t *val);
extern int th_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
extern int th_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
extern const char *th_get_info(RIG *rig);
extern int th_set_mem(RIG *rig, vfo_t vfo, int ch);
extern int th_get_mem(RIG *rig, vfo_t vfo, int *ch);

#endif /* __TH_H__ */
/* end of file */
