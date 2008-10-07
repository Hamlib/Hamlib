/*
 *  Hamlib Uniden backend - uniden_digital header
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
 *	$Id: uniden_digital.h,v 1.1 2008-10-07 18:58:08 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _UNIDEN_DIGITAL_H
#define _UNIDEN_DIGITAL_H 1

#include "hamlib/rig.h"

#define BACKEND_DIGITAL_VER	"0.3"

int uniden_digital_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int uniden_digital_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);


#endif /* _UNIDEN_DIGITAL_H */
