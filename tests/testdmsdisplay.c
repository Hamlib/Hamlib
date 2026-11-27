/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <math.h>
#include <stdio.h>

#include "dms_display.h"
#include "hamlib/rotator.h"

static int check_display(int degrees, int minutes, double seconds,
                         unsigned int scale, int expected_degrees,
                         int expected_minutes, double expected_seconds)
{
    struct dms_display result = dms_round_for_display(degrees, minutes, seconds,
        scale);

    if (result.degrees != expected_degrees ||
            result.minutes != expected_minutes ||
            !isfinite(result.seconds) ||
            fabs(result.seconds - expected_seconds) > 1e-10)
    {
        fprintf(stderr, "DMS display: %d %d %.9f at scale %u gave %d %d %.9f\n",
                degrees, minutes, seconds, scale, result.degrees, result.minutes,
                result.seconds);
        return 1;
    }

    return 0;
}

int main(void)
{
    int degrees, minutes, sw;
    double seconds;

    if (check_display(10, 11, 59.99999964, 1000000, 10, 12, 0) ||
            check_display(10, 11, 59.9999994, 1000000, 10, 11, 59.999999) ||
            check_display(12, 59, 59.99999964, 1000000, 13, 0, 0) ||
            check_display(10, 11, 59.99964, 100, 10, 12, 0) ||
            check_display(10, 11, 59.994, 100, 10, 11, 59.99) ||
            check_display(10, 11, 12.125, 100, 10, 11, 12.13) ||
            check_display(10, 11, 59.99964, 1000000, 10, 11, 59.99964) ||
            check_display(179, 59, 59.999, 100, 180, 0, 0))
    {
        return 1;
    }

    if (dec2dms(10.2, &degrees, &minutes, &seconds, &sw) != 0 || sw != 0 ||
            check_display(degrees, minutes, seconds, 1000000, 10, 12, 0) ||
            dec2dms(-10.2, &degrees, &minutes, &seconds, &sw) != 0 || sw != 1 ||
            check_display(degrees, minutes, seconds, 1000000, 10, 12, 0))
    {
        fprintf(stderr, "decimal-degree conversion did not round as expected\n");
        return 1;
    }

    return 0;
}
