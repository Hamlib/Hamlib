/*
 *  Hamlib Tuners backend - main header
 *  Copyright (c) 2004 by Stephane Fillod
 *
 *	$Id: tuner.h,v 1.1 2004-09-12 21:30:21 fillods Exp $
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

#ifndef _TUNER_H
#define _TUNER_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/*
 * So far, only Linux has Video4Linux support through ioctl :)
 * until someone port it to some other OS...
 */
#ifdef HAVE_LINUX_IOCTL_H
#define V4L_IOCTL
#endif

#include "hamlib/rig.h"

extern const struct rig_caps v4l_caps;

#endif /* _TUNER_H */
