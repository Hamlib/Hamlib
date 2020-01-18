/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 *
 * ft767gx.h - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 * adapted from ft757gx.h by Steve Conklin
 * This shared library provides an API for communicating
 * via serial interface to an FT-767GX using the "CAT" interface
 * box (FIF-232C) or similar (max232 + some capacitors :-)
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


#ifndef _FT767GX_H
#define _FT767GX_H 1

#define FT767GX_STATUS_UPDATE_DATA_LENGTH      86

#define FT767GX_PACING_INTERVAL                5
#define FT767GX_PACING_DEFAULT_VALUE           0
#define FT767GX_WRITE_DELAY                    50


/* Sequential fast writes confuse my FT767GX without this delay */

#define FT767GX_POST_WRITE_DELAY               5


/* Rough safe value for default timeout */

#define FT767GX_DEFAULT_READ_TIMEOUT  345 * ( 3 + (FT767GX_PACING_INTERVAL * FT767GX_PACING_DEFAULT_VALUE))



#endif /* _FT767GX_H */
