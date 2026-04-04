/*
 *  Harris PRC-138 support developed by Antonio Regazzoni - HB9RBS
 *  Tested with three different PRC138 versions with following motherboard
 *  Firmware (module 01A) : 8211A, 8214D, 8214F.
 *  04 april 2026
 *            
 *
 *  Copyright (c) 2004-2010 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
 
#ifndef _HARRIS_H
#define _HARRIS_H

#include <hamlib/rig.h>

struct harris_priv_data {
    freq_t current_freq;
    rmode_t current_mode;
};

/* Prototypes for prc138.c */
int harris_init(RIG *rig);
int harris_cleanup(RIG *rig);
int harris_open(RIG *rig);
int harris_close(RIG *rig);
int harris_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int harris_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int harris_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int harris_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int harris_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int harris_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int harris_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int harris_get_bw(RIG *rig, vfo_t vfo, mode_t mode, pbwidth_t *width);
int harris_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int harris_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);

#endif
