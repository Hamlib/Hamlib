/*
 *  Hamlib Interface - CTCSS and DCS tables
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: tone_tbl.h,v 1.2 2001-12-16 11:14:46 fillods Exp $
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

#ifndef _TONE_TBL_H
#define _TONE_TBL_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <hamlib/rig.h>

/*
 * Under win32, dll backends cannot link against exported variables
 * in other dll (or I don't know how).
 * Anyway, we got to trick it
 *
 * The TBL_SCOPE is necessary to the tables are not static 
 * in hamlib (exported).
 * --SF
 */
#if defined(IN_HAMLIB) || defined(__CYGWIN__)

#ifdef __CYGWIN__
#define TBL_SCOPE static
#else
#define TBL_SCOPE 
#endif

/**
 * 52 CTCSS sub-audible tones
 */
TBL_SCOPE const tone_t full_ctcss_list[] = {
		 600,  670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
		 948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1200, 1230, 1273,
		1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
		1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
		2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
		0,
};

/**
 * 50 CTCSS sub-audible tones, from 67.0Hz to 254.1Hz
 *
 * \note Don't even think about changing a bit of this array, several
 * backends depend on it. If you need to, create a copy for your 
 * own caps. --SF
 */
TBL_SCOPE const tone_t common_ctcss_list[] = {
		 670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
		 948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
		1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
		1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
		2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
		0,
};

/**
 * 106 DCS codes
 */
TBL_SCOPE const tone_t full_dcs_list[] = {
		 17,  23,  25,  26,  31,  32,  36,  43,  47,  50,  51,  53, 
		 54,  65,  71,  72,  73,  74, 114, 115, 116, 122, 125, 131, 
		132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, 
		212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, 
		263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343, 
		346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432, 
		445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 
		523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, 
		662, 664, 703, 712, 723, 731, 732, 734, 743, 754, 
		0,
};
#else
extern const HAMLIB_EXPORT_VAR(tone_t) full_ctcss_list[];
extern const HAMLIB_EXPORT_VAR(tone_t) common_ctcss_list[];
extern const HAMLIB_EXPORT_VAR(tone_t) full_dcs_list[];
#endif	/* HAMLIB_DLL || __CYGWIN__ */
	

#endif /* _TONE_TBL_H */
