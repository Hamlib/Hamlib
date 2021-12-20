/*
 *  Hamlib Interface - rig state cache routines
 *  Copyright (c) 2000-2012 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
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

#ifndef _CACHE_H
#define _CACHE_H

#include <hamlib/rig.h>

int rig_set_cache_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int rig_set_cache_freq(RIG *rig, vfo_t vfo, freq_t freq);
void rig_cache_show(RIG *rig, const char *func, int line);

#endif
