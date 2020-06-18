/*
 *  Hamlib Alinco backend - main header
 *  Copyright (c) 2001-2003 by Stephane Fillod
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

#ifndef _ALINCO_H
#define _ALINCO_H 1

#include <hamlib/rig.h>
#include <tones.h>

#define BACKEND_VER "20200323"

extern const struct rig_caps dx77_caps;
extern const struct rig_caps dxsr8_caps;

#define BUFSZ 32

#define AL "AL"

#endif /* _ALINCO_H */

