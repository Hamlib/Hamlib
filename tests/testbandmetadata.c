/*
 * Hamlib band metadata parser tests
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
#include "misc.h"
#include "idx_builtin.h"

int main(void)
{
    RIG rig;
    struct rig_caps caps;

    memset(&rig, 0, sizeof(rig));
    memset(&caps, 0, sizeof(caps));
    rig.caps = &caps;

    if (rig_get_band(&rig, 0, 0) != 0)
    {
        fprintf(stderr, "missing metadata returned a band\n");
        return 1;
    }

    if (rig_get_band_str(&rig, 0, 1) != NULL)
    {
        fprintf(stderr, "missing metadata returned a band name\n");
        return 1;
    }

    caps.parm_gran[PARM_BANDSELECT].step.s =
        "BAND2200M,BAND600M,BAND160M,BAND80M";

    if (rig_get_band(&rig, 0, 0) != (hamlib_band_t)RIG_BANDSELECT_2200M)
    {
        fprintf(stderr, "valid metadata did not return the first band\n");
        return 1;
    }

    const char *band = rig_get_band_str(&rig, 0, 1);

    if (band == NULL || strcmp(band, "BAND2200M") != 0)
    {
        fprintf(stderr, "valid metadata did not return the first band name\n");
        return 1;
    }

    return 0;
}
