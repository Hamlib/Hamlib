/*
 *  Hamlib Elecraft backend--support extensions to Kenwood commands
 *  Copyright (C) 2010,2011 by Nate Bargmann, n0nb@n0nb.us
 *  Copyright (C) 2013 by Steve Conklin AI4QR, steve@conklinhouse.com
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

#ifndef _FLEXRADIO_H
#define _FLEXRADIO_H 1

#include <hamlib/rig.h>

#define EXT_LEVEL_NONE -1
#define FLEXRADIO_MAX_BUF_LEN 50

struct flexradio_priv_data {
    char info[FLEXRADIO_MAX_BUF_LEN];
    split_t split;		/* current split state */
};

/* Flexradio extension function declarations */
int flexradio_open(RIG *rig);

#endif /* _FLEXRADIO_H */
