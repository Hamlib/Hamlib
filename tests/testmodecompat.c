/*
 * Hamlib generic and rig-specific mode compatibility tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>

#include "hamlib/rig.h"
#include "hamlib/riglist.h"

static int expect_mode(RIG *rig, rmode_t initial, rmode_t requested,
                       rmode_t expected)
{
    rmode_t actual;
    pbwidth_t width;
    int retval;

    retval = rig_set_mode(rig, RIG_VFO_A, initial, 2400);

    if (retval != RIG_OK)
    {
        fprintf(stderr, "cannot set initial mode %s: %s\n",
                rig_strrmode(initial), rigerror(retval));
        return 1;
    }

    retval = rig_set_mode(rig, RIG_VFO_A, requested,
                          RIG_PASSBAND_NOCHANGE);

    if (retval != RIG_OK)
    {
        fprintf(stderr, "cannot request mode %s: %s\n",
                rig_strrmode(requested), rigerror(retval));
        return 1;
    }

    retval = rig_get_mode(rig, RIG_VFO_A, &actual, &width);

    if (retval != RIG_OK)
    {
        fprintf(stderr, "cannot read resulting mode: %s\n", rigerror(retval));
        return 1;
    }

    if (actual != expected)
    {
        fprintf(stderr, "requesting %s from %s produced %s, expected %s\n",
                rig_strrmode(requested), rig_strrmode(initial),
                rig_strrmode(actual), rig_strrmode(expected));
        return 1;
    }

    return 0;
}

int main(void)
{
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    int failed = 0;

    if (rig == NULL)
    {
        fprintf(stderr, "cannot initialize dummy rig\n");
        return 1;
    }

    if (rig_open(rig) != RIG_OK)
    {
        fprintf(stderr, "cannot open dummy rig\n");
        rig_cleanup(rig);
        return 1;
    }

    failed |= expect_mode(rig, RIG_MODE_USBD2, RIG_MODE_PKTUSB,
                          RIG_MODE_USBD2);
    failed |= expect_mode(rig, RIG_MODE_LSBD3, RIG_MODE_PKTLSB,
                          RIG_MODE_LSBD3);
    failed |= expect_mode(rig, RIG_MODE_USBD2, RIG_MODE_USBD1,
                          RIG_MODE_USBD1);
    failed |= expect_mode(rig, RIG_MODE_USB, RIG_MODE_PKTUSB,
                          RIG_MODE_PKTUSB);

    rig_close(rig);
    rig_cleanup(rig);
    return failed;
}
