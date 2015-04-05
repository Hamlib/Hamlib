/*
 *  Hamlib Rotator backend - Easycomm interface protocol
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
 *  Copyright (c) 2014 by Alexander Schultze <alexschultze@gmail.com>
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

#ifndef _ROT_EASYCOMM_H
#define _ROT_EASYCOMM_H 1

#include "token.h"

extern const struct rot_caps easycomm1_rot_caps;
extern const struct rot_caps easycomm2_rot_caps;
extern const struct rot_caps easycomm3_rot_caps;

/*
 * Tokens used by rotorez_rot_set_conf and get_conf and the 'C' command in rotctl
 * and rotctld.
 */

#define TOK_GET_CONFIG       TOKEN_BACKEND(1)
#define TOK_SET_CONFIG       TOKEN_BACKEND(2)
#define TOK_GET_STATUS       TOKEN_BACKEND(3)
#define TOK_GET_ERRORS       TOKEN_BACKEND(4)
#define TOK_GET_VERSION      TOKEN_BACKEND(5)
#define TOK_GET_INPUT        TOKEN_BACKEND(6)
#define TOK_SET_OUTPUT       TOKEN_BACKEND(7)
#define TOK_GET_ANALOG_INPUT  TOKEN_BACKEND(8)


#endif /* _ROT_EASYCOMM_H */
