/*
 * Hamlib FTX-1 response parser tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "ftx1.h"

static int expect_clar_state(const char *response, char vfo, int expected,
                             char expected_rx, char expected_tx)
{
    char rx = 'x';
    char tx = 'y';
    int ret = ftx1_parse_clar_state(response, vfo, &rx, &tx);

    if (ret != expected)
    {
        fprintf(stderr, "CF state '%s': expected %d, got %d\n",
                response, expected, ret);
        return 1;
    }

    if (ret == RIG_OK && (rx != expected_rx || tx != expected_tx))
    {
        fprintf(stderr, "CF state '%s': expected %c/%c, got %c/%c\n",
                response, expected_rx, expected_tx, rx, tx);
        return 1;
    }

    if (ret != RIG_OK && (rx != 'x' || tx != 'y'))
    {
        fprintf(stderr, "CF state '%s' changed output on failure\n", response);
        return 1;
    }

    return 0;
}

static int expect_clar_offset(const char *response, char vfo, int expected,
                              shortfreq_t expected_offset)
{
    shortfreq_t offset = 42;
    int ret = ftx1_parse_clar_offset(response, vfo, &offset);

    if (ret != expected)
    {
        fprintf(stderr, "CF offset '%s': expected %d, got %d\n",
                response, expected, ret);
        return 1;
    }

    if (ret == RIG_OK && offset != expected_offset)
    {
        fprintf(stderr, "CF offset '%s': expected %ld, got %ld\n",
                response, (long)expected_offset, (long)offset);
        return 1;
    }

    if (ret != RIG_OK && offset != 42)
    {
        fprintf(stderr, "CF offset '%s' changed output on failure\n", response);
        return 1;
    }

    return 0;
}

static int expect_ex(const char *response, int expected, int expected_value)
{
    int value = 42;
    int ret = ftx1_parse_ex_menu_response(response, 3, 1, 4, &value);

    if (ret != expected)
    {
        fprintf(stderr, "EX response '%s': expected %d, got %d\n",
                response, expected, ret);
        return 1;
    }

    if (ret == RIG_OK && value != expected_value)
    {
        fprintf(stderr, "EX response '%s': expected %d, got %d\n",
                response, expected_value, value);
        return 1;
    }

    if (ret != RIG_OK && value != 42)
    {
        fprintf(stderr, "EX response '%s' changed output on failure\n", response);
        return 1;
    }

    return 0;
}

static int expect_smeter(const char *response, int p1, int expected,
                         int expected_value)
{
    int value = 42;
    int ret = ftx1_parse_smeter_response(response, p1, &value);

    if (ret != expected)
    {
        fprintf(stderr, "SM response '%s': expected %d, got %d\n",
                response, expected, ret);
        return 1;
    }

    if (ret == RIG_OK && value != expected_value)
    {
        fprintf(stderr, "SM response '%s': expected %d, got %d\n",
                response, expected_value, value);
        return 1;
    }

    if (ret != RIG_OK && value != 42)
    {
        fprintf(stderr, "SM response '%s' changed output on failure\n", response);
        return 1;
    }

    return 0;
}

int main(void)
{
    static const char *invalid_states[] =
    {
        "CF10010000;", "CF00020000;", "CF0001000;", "CF000100000;"
    };
    static const char *invalid_offsets[] =
    {
        "CF00101234;", "CF001+12x4;", "CF001+12345;"
    };
    static const char *invalid_ex[] =
    {
        "EX0301051;", "EX030104;", "EX0301041junk;",
        "EX0301042147483648;"
    };

    if (expect_clar_state("CF00000000;", '0', RIG_OK, '0', '0') != 0
            || expect_clar_state("CF00010000;", '0', RIG_OK, '1', '0') != 0
            || expect_clar_state("CF10010000;", '1', RIG_OK, '1', '0') != 0
            || expect_clar_offset("CF001+1234;", '0', RIG_OK, 1234) != 0
            || expect_clar_offset("CF101-9999;", '1', RIG_OK, -9999) != 0)
    {
        return 1;
    }

    for (size_t i = 0; i < sizeof(invalid_states) / sizeof(invalid_states[0]); i++)
    {
        if (expect_clar_state(invalid_states[i], '0', -RIG_EPROTO, 0, 0) != 0)
        {
            return 1;
        }
    }

    for (size_t i = 0; i < sizeof(invalid_offsets) / sizeof(invalid_offsets[0]);
            i++)
    {
        if (expect_clar_offset(invalid_offsets[i], '0', -RIG_EPROTO, 0) != 0)
        {
            return 1;
        }
    }

    if (expect_ex("EX0301041;", RIG_OK, 1) != 0
            || expect_ex("EX030104-1;", RIG_OK, -1) != 0
            || expect_ex("EX030104-2147483648;", RIG_OK, INT_MIN) != 0)
    {
        return 1;
    }

    for (size_t i = 0; i < sizeof(invalid_ex) / sizeof(invalid_ex[0]); i++)
    {
        if (expect_ex(invalid_ex[i], -RIG_EPROTO, 0) != 0)
        {
            return 1;
        }
    }

    if (expect_smeter("SM0000;", 0, RIG_OK, 0) != 0
            || expect_smeter("SM1255;", 1, RIG_OK, 255) != 0
            || expect_smeter("RM0123;", 0, -RIG_EPROTO, 0) != 0
            || expect_smeter("SM1123;", 0, -RIG_EPROTO, 0) != 0
            || expect_smeter("SM2123;", 2, -RIG_EPROTO, 0) != 0
            || expect_smeter("SM0123", 0, -RIG_EPROTO, 0) != 0
            || expect_smeter("SM0123;x", 0, -RIG_EPROTO, 0) != 0
            || expect_smeter("SM0-01;", 0, -RIG_EPROTO, 0) != 0
            || expect_smeter("SM0+01;", 0, -RIG_EPROTO, 0) != 0
            || expect_smeter("SM0256;", 0, -RIG_EPROTO, 0) != 0
            || expect_smeter("SM0999;", 0, -RIG_EPROTO, 0) != 0)
    {
        return 1;
    }

    return 0;
}
