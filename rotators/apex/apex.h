/*
 *  Hamlib Rotator backend - Apex rotators
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

#ifndef _ROT_APEX_H
#define _ROT_APEX_H 1

#include "rotator.h"

extern const struct rot_caps apex_shared_loop_rot_caps;

extern float apex_azimuth;

extern int apex_open(ROT *rot);
extern const char *apex_get_info(ROT *rot);

#endif /* _ROT_APEX_H */
