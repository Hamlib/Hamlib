/*
 *  Hamlib Interface - calibration header
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: cal.h,v 1.4 2001-07-13 19:08:15 f4cfe Exp $
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

#ifndef _CAL_H
#define _CAL_H 1

#include <hamlib/rig.h>


/* add rig_set_cal(cal_table), rig_get_calstat(rawmin,rawmax,cal_table), */

#define MAX_CAL_LENGTH 32

/*
 * cal_table_t is a data type suited to hold linear calibration
 * cal_table_t.size tell the number of plot cal_table_t.table contains
 * If a value is below or equal to cal_table_t.table[0].raw, 
 * rig_raw2val() will return cal_table_t.table[0].val
 * If a value is greater or equal to cal_table_t.table[cal_table_t.size-1].raw, 
 * rig_raw2val() will return cal_table_t.table[cal_table_t.size-1].val
 */
struct cal_cell {
	int raw;
	int val;
};
struct cal_table {
	int size;
	struct cal_cell table[MAX_CAL_LENGTH];
};

typedef struct cal_table cal_table_t;

#define EMPTY_STR_CAL { 0, { { 0, 0 }, } }

extern HAMLIB_EXPORT(float) rig_raw2val(int rawval, const cal_table_t *cal);

#endif /* _CAL_H */
