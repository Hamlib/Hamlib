/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * winradio.h - Copyright (C) 2001 pab@users.sourceforge.net
 * Derived from hamlib code (C) 2000,2001 Stephane Fillod.
 *
 * This shared library supports winradio receivers through the
 * /dev/winradio API.
 *
 *
 *		$Id: winradio.h,v 1.3 2001-06-02 18:07:45 f4cfe Exp $
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

#ifndef _WINRADIO_H
#define _WINRADIO_H 1

#include <hamlib/rig.h>

int wr_rig_init(RIG *rig);
int wr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int wr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int wr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int wr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int wr_set_powerstat(RIG *rig, powerstat_t status);
int wr_get_powerstat(RIG *rig, powerstat_t *status);
int wr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int wr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int wr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int wr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
const char *wr_get_info(RIG *rig);

extern const struct rig_caps wr1000_caps;
extern const struct rig_caps wr1500_caps;
extern const struct rig_caps wr1550_caps;
extern const struct rig_caps wr3100_caps;
extern const struct rig_caps wr3150_caps;
extern const struct rig_caps wr3500_caps;
extern const struct rig_caps wr3700_caps;

extern int init_winradio(void *be_handle);

#endif /* _WINRADIO_H */
