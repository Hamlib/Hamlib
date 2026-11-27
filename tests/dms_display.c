/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <math.h>

#include "dms_display.h"

struct dms_display dms_round_for_display(int degrees, int minutes,
        double seconds,
        unsigned int units_per_second)
{
    struct dms_display result;
    double units = floor(seconds * units_per_second + 0.5);

    if (units >= 60.0 * units_per_second)
    {
        units = 0;
        ++minutes;

        if (minutes == 60)
        {
            minutes = 0;
            ++degrees;
        }
    }

    result.degrees = degrees;
    result.minutes = minutes;
    result.seconds = units / units_per_second;
    return result;
}
