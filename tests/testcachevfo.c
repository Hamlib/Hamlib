/*
 * Hamlib VFO cache semantics tests
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
#include "cache.h"

static int (*dummy_get_vfo)(RIG *, vfo_t *);
static int get_vfo_calls;
static int set_vfo_calls;

static int failing_get_vfo(RIG *rig, vfo_t *vfo)
{
    (void)rig;
    *vfo = RIG_VFO_B;
    return -RIG_EIO;
}

static int counting_get_vfo(RIG *rig, vfo_t *vfo)
{
    get_vfo_calls++;
    return dummy_get_vfo(rig, vfo);
}

static int counting_set_vfo(RIG *rig, vfo_t vfo)
{
    (void)rig;
    (void)vfo;
    set_vfo_calls++;
    return RIG_OK;
}

static int check_status(const char *operation, int status)
{
    if (status == RIG_OK)
    {
        return 0;
    }

    fprintf(stderr, "%s failed: %s\n", operation, rigerror(status));
    return 1;
}

int main(void)
{
    RIG *rig;
    int (*dummy_set_vfo)(RIG *, vfo_t);
    vfo_t vfo;
    int age_before;
    int age_after;
    int timeout_ms;
    int result = 1;

    rig_set_debug(RIG_DEBUG_ERR);

    if (check_status("rig_load_backend", rig_load_backend("dummy")))
    {
        return 1;
    }

    rig = rig_init(RIG_MODEL_DUMMY);

    if (rig == NULL || check_status("rig_open", rig_open(rig)))
    {
        fprintf(stderr, "failed to open Dummy rig\n");
        return 1;
    }

    dummy_get_vfo = rig->caps->get_vfo;
    dummy_set_vfo = rig->caps->set_vfo;

    if (check_status("disable VFO cache",
                     rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 0)))
    {
        goto cleanup;
    }

    rig->caps->get_vfo = failing_get_vfo;
    vfo = RIG_VFO_MEM;

    if (rig_get_vfo(rig, &vfo) != -RIG_EIO || vfo != RIG_VFO_MEM)
    {
        fprintf(stderr, "failed get_vfo changed its caller output\n");
        goto cleanup;
    }

    rig->caps->set_vfo = counting_set_vfo;
    set_vfo_calls = 0;

    if (check_status("set VFO after failed equality probe",
                     rig_set_vfo(rig, RIG_VFO_B)) || set_vfo_calls != 1)
    {
        fprintf(stderr, "failed equality probe suppressed set_vfo\n");
        goto cleanup;
    }

    rig->caps->get_vfo = dummy_get_vfo;
    rig->caps->set_vfo = dummy_set_vfo;

    if (check_status("restore Dummy VFO A",
                     dummy_set_vfo(rig, RIG_VFO_A))
            || check_status("observe VFO A", rig_get_vfo(rig, &vfo))
            || check_status("set VFO cache timeout",
                            rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL,
                                    1000)))
    {
        goto cleanup;
    }

    rig->caps->get_vfo = counting_get_vfo;
    get_vfo_calls = 0;

    if (check_status("toggle current VFO",
                     rig_vfo_op(rig, RIG_VFO_CURR, RIG_OP_TOGGLE))
            || check_status("query toggled VFO", rig_get_vfo(rig, &vfo)))
    {
        goto cleanup;
    }

    if (vfo != RIG_VFO_B || get_vfo_calls != 1)
    {
        fprintf(stderr, "successful TOGGLE did not invalidate VFO freshness\n");
        goto cleanup;
    }

    if (check_status("disable VFO cache again",
                     rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 0))
            || check_status("observe VFO B", rig_get_vfo(rig, &vfo)))
    {
        goto cleanup;
    }

    hl_usleep(25 * 1000);
    rig_get_cached_vfo(rig, &vfo, &age_before, &timeout_ms);

    if (check_status("exchange VFO contents",
                     rig_vfo_op(rig, RIG_VFO_CURR, RIG_OP_XCHG)))
    {
        goto cleanup;
    }

    rig_get_cached_vfo(rig, &vfo, &age_after, &timeout_ms);

    if (age_after < age_before)
    {
        fprintf(stderr, "XCHG incorrectly refreshed VFO freshness\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    rig->caps->get_vfo = dummy_get_vfo;
    rig->caps->set_vfo = dummy_set_vfo;
    rig_close(rig);
    rig_cleanup(rig);
    return result;
}
