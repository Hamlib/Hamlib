/*
 * Hamlib backend library for the SARtek rotor command set.
 *
 * sartek.h - (C) Stephane Fillod 2003
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

#ifndef _ROT_SARTEK_H
#define _ROT_SARTEK_H 1

#define AZ_READ_LEN 4

extern const struct rot_caps sartek_rot_caps;

/*
 * API local implementation
 *
 */

static int sartek_rot_set_position(ROT *rot, azimuth_t azimuth, elevation_t elevation);

static int sartek_rot_stop(ROT *rot);


#endif  /* _ROT_SARTEK_H */
