/*
 *  Hamlib Interface - calibration header
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
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

#ifndef _CAL_H
#define _CAL_H 1

#include <hamlib/rig.h>

extern HAMLIB_EXPORT(float) rig_raw2val(int rawval, const cal_table_t *cal);
extern HAMLIB_EXPORT(float) rig_raw2val_float(int rawval, const cal_table_float_t *cal);

#endif /* _CAL_H */
