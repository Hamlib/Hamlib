/*
 * Hamlib backend library for the DCU rotor command set.
 *
 * rotorez.h - (C) Nate Bargmann 2003 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to a Hy-Gain or Yaesu rotor using
 * the Idiom Press Rotor-EZ or RotorCard interface.  It also
 * supports the Hy-Gain DCU-1.
 *
 *
 *    $Id: rotorez.h,v 1.4 2003-02-27 03:50:03 n0nb Exp $
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

#ifndef _ROT_ROTOREZ_H
#define _ROT_ROTOREZ_H 1

#define AZ_READ_LEN 4

extern const struct rot_caps rotorez_rot_caps;
extern const struct rot_caps rotorcard_rot_caps;
extern const struct rot_caps dcu_rot_caps;

extern BACKEND_EXPORT(int) initrots_rotorez(void *be_handle);

/* 
 * API local implementation
 *
 */

static int rotorez_rot_init(ROT *rot);
static int rotorez_rot_cleanup(ROT *rot);

static int rotorez_rot_set_position(ROT *rot, azimuth_t azimuth, elevation_t elevation);
static int rotorez_rot_get_position(ROT *rot, azimuth_t *azimuth, elevation_t *elevation);

static int rotorez_rot_stop(ROT *rot);

static int rotorez_rot_set_conf(ROT *rot, token_t token, const char *val);

#endif  /* _ROT_ROTOREZ_H */

