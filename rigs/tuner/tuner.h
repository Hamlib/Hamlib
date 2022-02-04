/*
 *  Hamlib Tuners backend - main header
 *  Copyright (c) 2004-2011 by Stephane Fillod
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

#ifndef _TUNER_H
#define _TUNER_H 1

#include "hamlib/config.h"
/*
 * So far, only Linux has Video4Linux support through ioctl :)
 * until someone port it to some other OS...
 */
#ifdef HAVE_LINUX_IOCTL_H
#define V4L_IOCTL
#endif

#include "hamlib/rig.h"

extern const struct rig_caps v4l_caps;
extern const struct rig_caps v4l2_caps;

#endif /* _TUNER_H */
