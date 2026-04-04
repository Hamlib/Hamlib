/*
 *  Harris PRC-138 support developed by Antonio Regazzoni - HB9RBS
 *  Tested with three different PRC138 versions with following motherboard
 *  Firmware (module 01A) : 8211A, 8214D, 8214F.
 *  04 april 2026
 *            
 *
 *  Copyright (c) 2004-2010 by Stephane Fillod
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

#ifndef _PRC138_H
#define _PRC138_H

#include <hamlib/rig.h>

#define PRC138_MODES (RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_CW | RIG_MODE_AM)
#define PRC138_VFO   (RIG_VFO_A)

extern struct rig_caps prc138_caps;

#endif
