/* SPDX-License-Identifier: LGPL-2.1-or-later */

#ifndef DMS_DISPLAY_H
#define DMS_DISPLAY_H

struct dms_display
{
    int degrees;
    int minutes;
    double seconds;
};

/* Round nonnegative DMS components to units per second, with ties upward. */
struct dms_display dms_round_for_display(int degrees, int minutes,
        double seconds,
        unsigned int units_per_second);

#endif
