/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * winradio.h - Copyright (C) 2001 pab@users.sourceforge.net
 * Derived from hamlib code (C) 2000 Stephane Fillod.
 *
 * This shared library supports winradio receivers through the
 * /dev/winradio API.
 *
 *
 *		$Id: winradio.h,v 1.1 2001-02-07 23:54:14 f4cfe Exp $
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

extern const struct rig_caps wr1500_caps;

extern int init_winradio(void *be_handle);

#endif /* _WINRADIO_H */
