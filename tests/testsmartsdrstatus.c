/*
 * Hamlib SmartSDR status parser tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <string.h>

#include "flexradio.h"

static int expect_value(const char *token, const char *prefix,
                        size_t value_size,
                        int expected_ret, const char *expected_value)
{
    char value[32] = "unchanged";
    int ret = smartsdr_parse_status_value(token, prefix, value, value_size);

    if (ret != expected_ret)
    {
        fprintf(stderr, "token '%s': expected %d, got %d\n",
                token, expected_ret, ret);
        return 1;
    }

    if (ret == 1 && strcmp(value, expected_value) != 0)
    {
        fprintf(stderr, "token '%s': expected '%s', got '%s'\n",
                token, expected_value, value);
        return 1;
    }

    if (ret != 1 && strcmp(value, "unchanged") != 0)
    {
        fprintf(stderr, "token '%s' changed output on failure\n", token);
        return 1;
    }

    return 0;
}

int main(void)
{
    char oversized_mode[sizeof("mode=") + 256];

    memcpy(oversized_mode, "mode=", sizeof("mode=") - 1);
    memset(oversized_mode + sizeof("mode=") - 1, 'A', 256);
    oversized_mode[sizeof(oversized_mode) - 1] = '\0';

    if (expect_value("mode=USB", "mode=", 16, 1, "USB") != 0
            || expect_value("state=TRANSMITTING", "state=", 16, 1,
                            "TRANSMITTING") != 0
            || expect_value("state=123456789012345", "state=", 16, 1,
                            "123456789012345") != 0
            || expect_value("state=1234567890123456", "state=", 16,
                            -RIG_EPROTO, NULL) != 0
            || expect_value(oversized_mode, "mode=", 16, -RIG_EPROTO,
                            NULL) != 0
            || expect_value("mode=", "mode=", 16, -RIG_EPROTO, NULL) != 0
            || expect_value("other=USB", "mode=", 16, 0, NULL) != 0)
    {
        return 1;
    }

    return 0;
}
