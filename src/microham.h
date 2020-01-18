/*
 *  Hamlib Interface - Microham device support for POSIX systems
 *  Copyright (c) 2017 by Christoph van Wüllen
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

//
// declaration of functions implemented in microham.c
//
//

extern int  uh_open_radio(int baud, int databits, int stopbits, int rtscts);
extern int  uh_open_ptt();
extern void uh_close_wkey();
extern void uh_set_ptt(int ptt);
extern int  uh_get_ptt();
extern void uh_close_radio();
extern void uh_close_ptt();
