/*
 * frg100.h - (C) Michael Black <mdblack98@gmail.com> 2016
 *
 * This shared library provides an API for communicating
 * via serial interface to an FRG-100 using the "CAT" interface
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


#define FRG100_OP_DATA_LENGTH           19      /* 0x10 p1=02 return size */
#define FRG100_CMD_UPDATE               0x10

#define FRG100_MIN_CHANNEL      1
#define FRG100_MAX_CHANNEL      200


// Return codes
#define FRG100_CMD_RETCODE_OK           0x00
#define FRG100_CMD_RETCODE_ERROR        0xF0

