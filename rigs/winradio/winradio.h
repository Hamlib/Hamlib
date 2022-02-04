/*
 *  Hamlib WiNRADiO backend - main header
 *  Copyright (c) 2000-2009 by Stephane Fillod
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

#ifndef _WINRADIO_H
#define _WINRADIO_H 1

#include "hamlib/config.h"

#define BACKEND_VER "20110822"
/*
 * So far, only Linux has Linradio support through ioctl,
 * until someone port it to some other OS...
 */
#ifdef HAVE_LINUX_IOCTL_H
#define WINRADIO_IOCTL
#endif

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
extern const struct rig_caps g303_caps;
extern const struct rig_caps g305_caps;
#if defined( _WIN32) || !defined(OTHER_POSIX)
extern const struct rig_caps g313_caps;
#endif

#endif /* _WINRADIO_H */
