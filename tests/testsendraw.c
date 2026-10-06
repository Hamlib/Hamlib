/*
 * Hamlib rig_send_raw capacity tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/riglist.h"

int main(void)
{
    unsigned char request[255];
    unsigned char reply[256];
    RIG *rig;
    int count;

    for (size_t i = 0; i < sizeof(request); ++i)
    {
        request[i] = (unsigned char)i;
    }

    rig = rig_init(RIG_MODEL_DUMMY);

    if (rig == NULL)
    {
        fprintf(stderr, "failed to initialize Dummy rig\n");
        return 1;
    }

    count = rig_send_raw(rig, request, sizeof(request), reply, sizeof(reply),
                         NULL);
    rig_cleanup(rig);

    if (count != sizeof(request) || memcmp(request, reply, count) != 0)
    {
        fprintf(stderr, "expected a %zu-byte echoed reply, got %d\n",
                sizeof(request), count);
        return 1;
    }

    return 0;
}
