/*
 *  Hamlib Interface - calibration routines
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: cal.c,v 1.4 2001-12-16 11:14:46 fillods Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <hamlib/rig.h>
#include "cal.h"

/* add rig_set_cal(cal_table), rig_get_calstat(rawmin,rawmax,cal_table), */

/*
 * cal_table_t is a data type suited to hold linear calibration
 * cal_table_t.size tell the number of plot cal_table_t.table contains
 * If a value is below or equal to cal_table_t.table[0].raw, 
 * rig_raw2val() will return cal_table_t.table[0].val
 * If a value is greater or equal to cal_table_t.table[cal_table_t.size-1].raw, 
 * rig_raw2val() will return cal_table_t.table[cal_table_t.size-1].val
 */


float rig_raw2val(int rawval, const cal_table_t *cal)
{
#ifdef WANT_CHEAP_WNO_FP
	float interpolation;
#else
	int interpolation;
#endif
	int i;

	/* ASSERT(cal != NULL) */
	/* ASSERT(cal->size <= MAX_CAL_LENGTH) */

	if (cal->size == 0)
			return rawval;

	for (i=0; i<cal->size; i++)
		if (rawval < cal->table[i].raw)
			break;

	if (i==0)
		return cal->table[0].val;

	if (i>=cal->size)
		return cal->table[i-1].val;

	if (cal->table[i].raw == cal->table[i-1].raw)	/* catch divide by 0 error */
		return cal->table[i].val;

#ifdef WANT_CHEAP_WNO_FP
	/* cheap, less accurate, but no fp needed */
	interpolation = ((cal->table[i].raw - rawval) * 
			 (cal->table[i].val - cal->table[i-1].val)) /
			  (cal->table[i].raw - cal->table[i-1].raw);

	return cal->table[i].val - interpolation;
#else
	interpolation = ((cal->table[i].raw - rawval) * 
			 (cal->table[i].val - cal->table[i-1].val)) /
			  (cal->table[i].raw - cal->table[i-1].raw);
#endif

	return cal->table[i].val - interpolation;
}

