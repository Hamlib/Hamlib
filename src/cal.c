/*
 *  Hamlib Interface - calibration routines
 *  Copyright (c) 2000-2009 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
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

/**
 * \addtogroup rig_internal
 * @{
 */

/**
 * \file cal.c
 * \brief Calibration routines.
 */

#include <hamlib/config.h>

#include <hamlib/rig.h>
#include "cal.h"

/* add rig_set_cal(cal_table), rig_get_calstat(rawmin,rawmax,cal_table), */


/**
 * \brief Convert raw data to a calibrated integer value, according to a
 * calibration table.
 *
 * \param rawval Input value.
 * \param cal Calibration table,
 *
 * cal_table_t is a data type suited to hold linear calibration.
 *
 * cal_table_t.size is the number of plots cal_table_t.table contains.
 *
 * If a value is below or equal to cal_table_t.table[0].raw,
 * rig_raw2val() will return cal_table_t.table[0].val.
 *
 * If a value is greater or equal to
 * cal_table_t.table[cal_table_t.size-1].raw, rig_raw2val() will return
 * cal_table_t.table[cal_table_t.size-1].val.
 *
 * \return Calibrated integer value.
 */
float HAMLIB_API rig_raw2val(int rawval, const cal_table_t *cal)
{
#ifdef WANT_CHEAP_WNO_FP
    int interpolation;
#else
    float interpolation;
#endif
    int i;

    /* ASSERT(cal != NULL) */
    /* ASSERT(cal->size <= HAMLIB_MAX_CAL_LENGTH) */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (cal->size == 0)
    {
        return rawval;
    }

    for (i = 0; i < cal->size; i++)
    {
        if (rawval < cal->table[i].raw)
        {
            break;
        }
    }

    if (i == 0)
    {
        return cal->table[0].val;
    }

    if (i >= cal->size)
    {
        return cal->table[i - 1].val;
    }

    /* catch divide by 0 error */
    if (cal->table[i].raw == cal->table[i - 1].raw)
    {
        return cal->table[i].val;
    }

#ifdef WANT_CHEAP_WNO_FP
    /* cheap, less accurate, but no fp needed */
    interpolation = ((cal->table[i].raw - rawval)
                     * (cal->table[i].val - cal->table[i - 1].val))
                    / (cal->table[i].raw - cal->table[i - 1].raw);

    return cal->table[i].val - interpolation;
#else
    interpolation = ((cal->table[i].raw - rawval)
                     * (float)(cal->table[i].val - cal->table[i - 1].val))
                    / (float)(cal->table[i].raw - cal->table[i - 1].raw);
#endif
    return cal->table[i].val - interpolation;
}


/**
 * \brief Convert raw data to a calibrated floating-point value, according to
 * a calibration table.
 *
 * \param rawval Input value.
 * \param cal Calibration table.
 *
 * cal_table_float_t is a data type suited to hold linear calibration.
 *
 * cal_table_float_t.size tell the number of plot cal_table_t.table contains.
 *
 * If a value is below or equal to cal_table_float_t.table[0].raw,
 * rig_raw2val_float() will return cal_table_float_t.table[0].val.
 *
 * If a value is greater or equal to
 * cal_table_float_t.table[cal_table_float_t.size-1].raw, rig_raw2val_float()
 * will return cal_table_float_t.table[cal_table_float_t.size-1].val.
 *
 * \return calibrated floating-point value
 */
float HAMLIB_API rig_raw2val_float(int rawval, const cal_table_float_t *cal)
{
    float interpolation;
    int i;

    /* ASSERT(cal != NULL) */
    /* ASSERT(cal->size <= HAMLIB_MAX_CAL_LENGTH) */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (cal->size == 0)
    {
        return rawval;
    }

    for (i = 0; i < cal->size; i++)
    {
        if (rawval < cal->table[i].raw)
        {
            break;
        }
    }

    if (i == 0)
    {
        return cal->table[0].val;
    }

    if (i >= cal->size)
    {
        return cal->table[i - 1].val;
    }

    /* catch divide by 0 error */
    if (cal->table[i].raw == cal->table[i - 1].raw)
    {
        return cal->table[i].val;
    }

    interpolation = ((cal->table[i].raw - rawval)
                     * (float)(cal->table[i].val - cal->table[i - 1].val))
                    / (float)(cal->table[i].raw - cal->table[i - 1].raw);

    return cal->table[i].val - interpolation;
}

/** @} */
