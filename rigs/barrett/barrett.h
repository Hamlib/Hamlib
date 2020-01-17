/*
 *  Hamlib Barrett backend - main header
 *  Copyright (c) 2017 by Michael Black W9MDB
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

#ifndef _BARRETT_H
#define _BARRETT_H 1

#include "hamlib/rig.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#define BACKEND_VER "0.2"

#define EOM "\x0d"
#define TRUE 1
#define FALSE 0
// For the current implemented command set 64 is long enough
// This will need a lot more room for some channel commands like IDFA which return all channels
// But that would 9999*41 or 406KB so didn't do that right now
#define BARRETT_DATA_LEN 64

extern const struct rig_caps barrett_caps;

struct barrett_priv_data {
    char cmd_str[BARRETT_DATA_LEN];       /* command string buffer */
    char ret_data[BARRETT_DATA_LEN];      /* returned data--max value, most are less */
    char split;                           /* split on/off */
};

#endif /* _BARRETT_H */
