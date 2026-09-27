/*
 * Hamlib NET rigctl parser tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>

#include "../rigs/dummy/dummy_common.h"

struct parse_case
{
    const char *name;
    char *value;
    int expected_ret;
    int expected_count;
    const enum agc_level_e *expected_levels;
};

static void fill_levels(enum agc_level_e *levels, enum agc_level_e value)
{
    for (int i = 0; i < HAMLIB_MAX_AGC_LEVELS; i++)
    {
        levels[i] = value;
    }
}

static int expect_state(const char *name, const enum agc_level_e *levels,
                        const enum agc_level_e *expected, int count,
                        int expected_count)
{
    if (count != expected_count)
    {
        fprintf(stderr, "%s: expected count %d, got %d\n", name,
                expected_count, count);
        return 1;
    }

    for (int i = 0; i < HAMLIB_MAX_AGC_LEVELS; i++)
    {
        if (levels[i] != expected[i])
        {
            fprintf(stderr, "%s: level %d expected %d, got %d\n", name, i,
                    expected[i], levels[i]);
            return 1;
        }
    }

    return 0;
}

int main(void)
{
    char short_value[] = "2=FAST 3=SLOW";
    char maximum_value[] =
        "0=OFF 1=SUPERFAST 2=FAST 3=SLOW 4=USER 5=MEDIUM 6=AUTO 7=LONG";
    char ninth_value[] =
        "0=OFF 1=SUPERFAST 2=FAST 3=SLOW 4=USER 5=MEDIUM 6=AUTO 7=LONG 8=ON";
    char malformed_value[] = "0=OFF invalid 2=FAST";
    const enum agc_level_e none[HAMLIB_MAX_AGC_LEVELS] = {
        RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE,
        RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE
    };
    const enum agc_level_e user[HAMLIB_MAX_AGC_LEVELS] = {
        RIG_AGC_USER, RIG_AGC_USER, RIG_AGC_USER, RIG_AGC_USER,
        RIG_AGC_USER, RIG_AGC_USER, RIG_AGC_USER, RIG_AGC_USER
    };
    const enum agc_level_e short_levels[HAMLIB_MAX_AGC_LEVELS] = {
        RIG_AGC_FAST, RIG_AGC_SLOW, RIG_AGC_NONE, RIG_AGC_NONE,
        RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE
    };
    const enum agc_level_e maximum_levels[HAMLIB_MAX_AGC_LEVELS] = {
        RIG_AGC_OFF, RIG_AGC_SUPERFAST, RIG_AGC_FAST, RIG_AGC_SLOW,
        RIG_AGC_USER, RIG_AGC_MEDIUM, RIG_AGC_AUTO, RIG_AGC_LONG
    };
    struct parse_case cases[] = {
        { "short list", short_value, RIG_OK, 2, short_levels },
        { "maximum list", maximum_value, RIG_OK, HAMLIB_MAX_AGC_LEVELS,
          maximum_levels },
        { "ninth entry truncated", ninth_value, RIG_OK, HAMLIB_MAX_AGC_LEVELS,
          maximum_levels },
        { "malformed token", malformed_value, -RIG_EPROTO, 42, user }
    };
    enum agc_level_e levels[HAMLIB_MAX_AGC_LEVELS];
    int count = 42;

    fill_levels(levels, RIG_AGC_USER);
    dummy_reset_agc_levels(levels, &count);

    if (expect_state("reset", levels, none, count, 0) != 0)
    {
        return 1;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        fill_levels(levels, RIG_AGC_USER);
        count = 42;
        int ret = dummy_parse_agc_levels(cases[i].value, levels, &count);

        if (ret != cases[i].expected_ret)
        {
            fprintf(stderr, "%s: expected return %d, got %d\n", cases[i].name,
                    cases[i].expected_ret, ret);
            return 1;
        }

        if (expect_state(cases[i].name, levels, cases[i].expected_levels,
                         count, cases[i].expected_count) != 0)
        {
            return 1;
        }
    }

    return 0;
}
