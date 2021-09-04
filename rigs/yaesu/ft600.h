/*
 * hamlib - (C) Frank Singleton 2000-2003 (vk3fcs@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2009
 *
 * ft600.h -(C) Kārlis Millers YL3ALK 2019
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-600 using the "CAT" interface.
 * The starting point for this code was Chris Karpinsky's ft100 implementation.
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

#ifndef _FT600_H
#define _FT600_H 1

#define FT600_STATUS_UPDATE_DATA_LENGTH      19
#define FT600_METER_INFO_LENGTH              5

#define FT600_WRITE_DELAY                    5
#define FT600_POST_WRITE_DELAY               200
#define FT600_DEFAULT_READ_TIMEOUT           2000
#endif /* _FT600_H */
