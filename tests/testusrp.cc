/*
 * Hamlib USRP backend state tests
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

extern "C"
{
    extern struct rig_caps usrp_caps;
}

static int check_conf(RIG *rig, hamlib_token_t token, const char *expected)
{
    char value[64] = {};
    int retval = rig_get_conf2(rig, token, value, sizeof(value));

    if (retval != RIG_OK || strcmp(value, expected) != 0)
    {
        fprintf(stderr, "expected USRP if_mix_freq %s, got %s (%d)\n",
                expected, value, retval);
        return 1;
    }

    return 0;
}

int main(void)
{
    RIG *rig;
    hamlib_token_t token;
    freq_t freq = 0;

    rig_register(&usrp_caps);
    rig = rig_init(RIG_MODEL_USRP);

    if (rig == NULL)
    {
        fprintf(stderr, "failed to initialize USRP rig\n");
        return 1;
    }

    token = rig_token_lookup(rig, "if_mix_freq");

    if (token == RIG_CONF_END)
    {
        fprintf(stderr, "USRP if_mix_freq token was not registered\n");
        rig_cleanup(rig);
        return 1;
    }

    if (check_conf(rig, token, "45000000") != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    if (rig_set_conf(rig, token, "12345678") != RIG_OK
            || check_conf(rig, token, "12345678") != 0)
    {
        fprintf(stderr, "USRP if_mix_freq did not retain an override\n");
        rig_cleanup(rig);
        return 1;
    }

    if (rig->caps->rig_close(rig) != RIG_OK
            || rig->caps->set_freq(rig, RIG_VFO_A, 145000000) != -RIG_EIO
            || rig->caps->get_freq(rig, RIG_VFO_A, &freq) != -RIG_EIO)
    {
        fprintf(stderr, "USRP callbacks were not safe without a receiver\n");
        rig_cleanup(rig);
        return 1;
    }

    rig_cleanup(rig);
    return 0;
}
