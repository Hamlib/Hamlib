/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 *
 * ft757gx.h - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-757GX using the "CAT" interface
 * box (FIF-232C) or similar (max232 + some capacitors :-)
 *
 *
 *    $Id: ft757gx.h,v 1.2 2004-08-08 19:15:31 fillods Exp $  
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */


#ifndef _FT757GX_H
#define _FT757GX_H 1

#define FT757GX_STATUS_UPDATE_DATA_LENGTH      75

#define FT757GX_PACING_INTERVAL                5 
#define FT757GX_PACING_DEFAULT_VALUE           0 
#define FT757GX_WRITE_DELAY                    50


/* Sequential fast writes confuse my FT757GX without this delay */

#define FT757GX_POST_WRITE_DELAY               5


/* Rough safe value for default timeout */

#define FT757GX_DEFAULT_READ_TIMEOUT  345 * ( 3 + (FT757GX_PACING_INTERVAL * FT757GX_PACING_DEFAULT_VALUE))



#endif /* _FT757GX_H */
