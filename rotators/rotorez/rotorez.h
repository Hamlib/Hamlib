/*
 * Hamlib backend library for the DCU rotor command set.
 *
 * rotorez.h - (C) Nate Bargmann 2003,2009,2010 (n0nb at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to a Hy-Gain or Yaesu rotor using
 * the Idiom Press Rotor-EZ or RotorCard interface.  It also
 * supports the Hy-Gain DCU-1.
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

#ifndef _ROT_ROTOREZ_H
#define _ROT_ROTOREZ_H 1

#include "token.h"

#define AZ_READ_LEN 4
#define RT21_AZ_LEN 6

extern const struct rot_caps rotorez_rot_caps;
extern const struct rot_caps rotorcard_rot_caps;
extern const struct rot_caps dcu_rot_caps;
extern const struct rot_caps erc_rot_caps;
extern const struct rot_caps rt21_rot_caps;
extern const struct rot_caps yrc1_rot_caps;

/*
 * Tokens used by rotorez_rot_set_conf and the 'C' command in rotctl
 * and rotctld followed by a value of '0' to disable a Rotor-EZ option
 * and '1' to enable it.
 */
#define TOK_ENDPT   TOKEN_BACKEND(1)
#define TOK_JAM     TOKEN_BACKEND(2)
#define TOK_OVRSHT  TOKEN_BACKEND(3)
#define TOK_UNSTICK TOKEN_BACKEND(4)

#endif  /* _ROT_ROTOREZ_H */

