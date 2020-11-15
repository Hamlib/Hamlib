/*
 *  Hamlib Rotator backend - INDI integration
 *  Copyright (c) 2020 by Norbert Varga HA2NON <nonoo@nonoo.hu>
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

#ifndef _INDI_WRAPPER_H
#define _INDI_WRAPPER_H 1

#include <hamlib/rotator.h>

int indi_wrapper_set_position(ROT *rot, azimuth_t az, elevation_t el);
int indi_wrapper_get_position(ROT *rot, azimuth_t *az, elevation_t *el);
int indi_wrapper_stop(ROT *rot);
int indi_wrapper_park(ROT *rot);
int indi_wrapper_move(ROT *rot, int direction, int speed);
const char *indi_wrapper_get_info(ROT *rot);
int indi_wrapper_open(ROT *rot);
int indi_wrapper_close(ROT *rot);

#endif  // _INDI_WRAPPER_H
