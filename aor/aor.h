/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * aor.h - Copyright (C) 2000 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an AOR scanner.
 *
 *
 *    $Id: aor.h,v 1.1 2000-11-01 23:23:56 f4cfe Exp $  
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

#ifndef _AOR_H
#define _AOR_H 1


int aor_close(RIG *rig);

int aor_set_freq(RIG *rig, freq_t freq);
int aor_get_freq(RIG *rig, freq_t *freq);
int aor_set_mode(RIG *rig, rmode_t mode);
int aor_get_mode(RIG *rig, rmode_t *mode);

int aor_set_ts(RIG *rig, unsigned long ts);
int aor_set_poweroff(RIG *rig);

extern const struct rig_caps ar8200_caps;

extern int init_aor(void *be_handle);


#endif /* _AOR_H */

