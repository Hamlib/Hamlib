/*
 *  Hamlib Alinco backend - main header
 *  Copyright (c) 2001-2003 by Stephane Fillod
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

#ifndef _ALINCO_H
#define _ALINCO_H 1

#include <hamlib/rig.h>
#include <tones.h>

int alinco_set_vfo(RIG *rig, vfo_t vfo);
int alinco_get_vfo(RIG *rig, vfo_t *vfo);
int alinco_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int alinco_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int alinco_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int alinco_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int alinco_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
int alinco_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
int alinco_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int alinco_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int alinco_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int alinco_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int alinco_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int alinco_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int alinco_set_parm(RIG *rig, setting_t parm, value_t val);
int alinco_get_parm(RIG *rig, setting_t parm, value_t *val);
int alinco_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int alinco_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
int alinco_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int alinco_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int alinco_set_mem(RIG *rig, vfo_t vfo, int ch);
int alinco_get_mem(RIG *rig, vfo_t vfo, int *ch);

extern const struct rig_caps dx77_caps;

#endif /* _ALINCO_H */

