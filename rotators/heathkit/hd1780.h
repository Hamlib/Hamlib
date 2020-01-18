/*
 * Hamlib backend library for the Heathkit HD 1780 Intellirotor command set.
 *
 * hd1780.h - (C) Nate Bargmann 2003 (n0nb at arrl.net)
 *            (C) Rob Frohne 2008 (kl7na at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to a Heathkit Intellirotor HD 1780
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

#ifndef _ROT_HD1780_H
#define _ROT_HD1780_H 1

#define AZ_READ_LEN 6

extern const struct rot_caps hd1780_rot_caps;

/*
 * API local implementation
 *
 */

static int hd1780_rot_init(ROT *rot);
static int hd1780_rot_cleanup(ROT *rot);

static int hd1780_rot_set_position(ROT *rot, azimuth_t azimuth, elevation_t elevation);
static int hd1780_rot_get_position(ROT *rot, azimuth_t *azimuth, elevation_t *elevation);



#endif  /* _ROT_HD1780_H */

