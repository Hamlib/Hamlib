/*
 *  Hamlib Interface - CTCSS and DCS tables header
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: tones.h,v 1.1 2001-12-19 03:18:44 fillods Exp $
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

#ifndef _TONES_H
#define _TONES_H 1

#include <hamlib/rig.h>		/* and implicitly rig_dll.h */


extern const HAMLIB_EXPORT_VAR(tone_t) full_ctcss_list[];
extern const HAMLIB_EXPORT_VAR(tone_t) common_ctcss_list[];
extern const HAMLIB_EXPORT_VAR(tone_t) full_dcs_list[];


#endif /* _TONES_H */
