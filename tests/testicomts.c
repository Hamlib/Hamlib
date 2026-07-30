/*
 * Hamlib Icom tuning-step table tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>

#include "icom.h"

int main(void)
{
    static const struct ts_sc_list steps[] =
    {
        { 1000, 0x02 },
        { 2500, 0x03 },
        { 0, 0 }
    };
    unsigned char sc = 0xff;
    shortfreq_t ts = -1;

    if (icom_ts_to_sc(steps, 1000, &sc) != RIG_OK || sc != 0x02)
    {
        fprintf(stderr, "supported tuning step was not mapped\n");
        return 1;
    }

    if (icom_ts_to_sc(steps, 0, &sc) != -RIG_EINVAL ||
            icom_ts_to_sc(steps, 5000, &sc) != -RIG_EINVAL ||
            icom_ts_to_sc(NULL, 1000, &sc) != -RIG_EINVAL)
    {
        fprintf(stderr, "unsupported set tuning step was accepted\n");
        return 1;
    }

    if (icom_sc_to_ts(steps, 0x03, &ts) != RIG_OK || ts != 2500)
    {
        fprintf(stderr, "supported tuning-step code was not mapped\n");
        return 1;
    }

    if (icom_sc_to_ts(steps, 0, &ts) != -RIG_EPROTO ||
            icom_sc_to_ts(steps, 0x7f, &ts) != -RIG_EPROTO ||
            icom_sc_to_ts(NULL, 0x02, &ts) != -RIG_EPROTO)
    {
        fprintf(stderr, "unsupported get tuning-step code was accepted\n");
        return 1;
    }

    return 0;
}
