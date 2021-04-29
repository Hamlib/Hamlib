/*
 *  Hamlib Interface - CTCSS and DCS tables header
 *  Copyright (c) 2000-2009 by Stephane Fillod and Frank Singleton
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

#ifndef _TONES_H
#define _TONES_H 1


#include <hamlib/rig.h>     /* and implicitly rig_dll.h */


/*
 * 52 CTCSS sub-audible tones
 */
#define FULL_CTCSS_LIST \
     600,  670,  693,  719,  744,  770,  797,  825,  854,  885,  915, \
     948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1200, 1230, 1273, \
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679, \
    1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995, \
    2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541, \
    0,

static const tone_t static_full_ctcss_list[] =
{
    FULL_CTCSS_LIST
};

#define FULL_CTCSS_LIST_COUNT 52


/*
 * 50 CTCSS sub-audible tones, from 67.0Hz to 254.1Hz
 *
 * \note Don't even think about changing a bit of this array, several
 * backends depend on it. If you need to, create a copy for your
 * own caps. --SF
 */
#define COMMON_CTCSS_LIST \
     670,  693,  719,  744,  770,  797,  825,  854,  885,  915, \
     948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, \
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679, \
    1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995, \
    2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541, \
    0,

static const tone_t static_common_ctcss_list[] =
{
    COMMON_CTCSS_LIST
};

#define COMMON_CTCSS_LIST_COUNT 50


/*
 * 104 DCS codes
 */
#define COMMON_DCS_LIST \
          23,  25,  26,  31,  32,  36,  43,  47,       51,  53, \
     54,  65,  71,  72,  73,  74, 114, 115, 116, 122, 125, 131, \
    132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, \
    212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, \
    263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343, \
    346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432, \
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, \
    523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, \
    662, 664, 703, 712, 723, 731, 732, 734, 743, 754, \
    0,

static const tone_t static_common_dcs_list[] =
{
    COMMON_DCS_LIST
};

#define COMMON_DCS_LIST_COUNT 104

/*
 * 106 DCS codes
 */
#define FULL_DCS_LIST \
     17,  23,  25,  26,  31,  32,  36,  43,  47,  50,  51,  53, \
     54,  65,  71,  72,  73,  74, 114, 115, 116, 122, 125, 131, \
    132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, \
    212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, \
    263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343, \
    346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432, \
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, \
    523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, \
    662, 664, 703, 712, 723, 731, 732, 734, 743, 754, \
    0,

static const tone_t static_full_dcs_list[] =
{
    FULL_DCS_LIST
};

#define FULL_DCS_LIST_COUNT 106

/*
 * These arrays cannot be shared on Win32 systems,
 * because DLL's vars don't have constant address.
 */
#if (defined(_WIN32) || defined(__CYGWIN__)) // && !defined(IN_HAMLIB)
#define common_ctcss_list (tone_t*)static_common_ctcss_list
#define full_ctcss_list (tone_t*)static_full_ctcss_list
#define full_dcs_list (tone_t*)static_full_dcs_list
#define common_dcs_list (tone_t*)static_common_dcs_list

#else

extern HAMLIB_EXPORT_VAR(tone_t) full_ctcss_list[];
extern HAMLIB_EXPORT_VAR(tone_t) common_ctcss_list[];
extern HAMLIB_EXPORT_VAR(tone_t) full_dcs_list[];
extern HAMLIB_EXPORT_VAR(tone_t) common_dcs_list[];

#endif

#endif /* _TONES_H */
