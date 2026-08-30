/*
 * Hamlib FT-991 ID meter calibration tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <math.h>
#include <stdio.h>

#include "cal.h"
#include "idx_builtin.h"
#include "yaesu.h"

#define FLOAT_TOLERANCE 0.0001f

static int expect_float(const char *name, float actual, float expected)
{
    if (fabsf(actual - expected) <= FLOAT_TOLERANCE)
    {
        return 0;
    }

    fprintf(stderr, "%s: expected %.3f, got %.3f\n", name, expected, actual);
    return 1;
}

int main(void)
{
    static const struct
    {
        int raw;
        float expected;
    } cases[] =
    {
        {0, 0.0f},
        {1, 0.1f},
        {53, 5.3f},
        {100, 10.0f},
        {107, 10.7f},
        {200, 20.0f},
        {255, 25.5f},
    };
    const cal_table_float_t *cal = &ft991_caps.id_meter_cal;
    const gran_t *gran = &ft991_caps.level_gran[LVL_ID_METER];
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        float actual = rig_raw2val_float(cases[i].raw, cal);

        if (expect_float("ID meter calibration", actual, cases[i].expected) != 0)
        {
            fprintf(stderr, "raw value: %d\n", cases[i].raw);
            return 1;
        }
    }

    if (expect_float("ID meter minimum", gran->min.f, 0.0f) != 0
            || expect_float("ID meter maximum", gran->max.f, 25.5f) != 0
            || expect_float("ID meter step", gran->step.f, 0.1f) != 0)
    {
        return 1;
    }

    return 0;
}
