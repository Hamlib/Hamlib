/*
 * Hamlib mode cache semantics tests
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

static int get_mode_cache(RIG *rig, vfo_t vfo, rmode_t *mode,
                          pbwidth_t *width, int *mode_ms, int *width_ms)
{
    freq_t freq;
    int freq_ms;

    return rig_get_cache(rig, vfo, &freq, &freq_ms, mode, mode_ms, width,
                         width_ms);
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
    rmode_t mode;
    pbwidth_t width;
    pbwidth_t normal_width;
    int mode_ms;
    int width_ms;
    int width_ms_before;
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

    if (check_status("publish initial mode",
                     rig_set_cache_mode(rig, RIG_VFO_A, RIG_MODE_USB, 2100)))
    {
        goto cleanup;
    }

    hl_usleep(25 * 1000);

    if (check_status("read initial mode cache",
                     get_mode_cache(rig, RIG_VFO_A, &mode, &width,
                                    &mode_ms, &width_ms)))
    {
        goto cleanup;
    }

    width_ms_before = width_ms;

    if (check_status("publish mode only",
                     rig_set_cache_mode_only(rig, RIG_VFO_A, RIG_MODE_LSB))
            || check_status("read mode-only cache",
                            get_mode_cache(rig, RIG_VFO_A, &mode, &width,
                                           &mode_ms, &width_ms)))
    {
        goto cleanup;
    }

    if (mode != RIG_MODE_LSB || width != 2100
            || width_ms < width_ms_before || mode_ms > 10)
    {
        fprintf(stderr,
                "mode-only publication changed width freshness: %s/%d, ages %d/%d\n",
                rig_strrmode(mode), (int)width, mode_ms, width_ms);
        goto cleanup;
    }

    hl_usleep(20 * 1000);
    width_ms_before = width_ms;

    if (check_status("publish NOCHANGE mode",
                     rig_set_cache_mode(rig, RIG_VFO_A, RIG_MODE_CW,
                                        RIG_PASSBAND_NOCHANGE))
            || check_status("read NOCHANGE cache",
                            get_mode_cache(rig, RIG_VFO_A, &mode, &width,
                                           &mode_ms, &width_ms)))
    {
        goto cleanup;
    }

    if (mode != RIG_MODE_CW || width != 2100
            || width_ms < width_ms_before || mode_ms > 10)
    {
        fprintf(stderr, "NOCHANGE did not preserve width value and age\n");
        goto cleanup;
    }

    if (check_status("publish positive width",
                     rig_set_cache_mode(rig, RIG_VFO_SUB_A,
                                        RIG_MODE_PKTUSB, 1700))
            || check_status("read Sub A cache",
                            get_mode_cache(rig, RIG_VFO_SUB_A, &mode, &width,
                                           &mode_ms, &width_ms)))
    {
        goto cleanup;
    }

    if (mode != RIG_MODE_PKTUSB || width != 1700
            || mode_ms > 10 || width_ms > 10)
    {
        fprintf(stderr, "positive Sub A publication was not fresh\n");
        goto cleanup;
    }

    normal_width = rig_passband_normal(rig, RIG_MODE_USB);

    if (normal_width <= 0
            || check_status("publish normal width",
                            rig_set_cache_mode(rig, RIG_VFO_B, RIG_MODE_USB,
                                    RIG_PASSBAND_NORMAL))
            || check_status("read normal-width cache",
                            get_mode_cache(rig, RIG_VFO_B, &mode, &width,
                                           &mode_ms, &width_ms)))
    {
        goto cleanup;
    }

    if (width != normal_width || width_ms > 10)
    {
        fprintf(stderr, "normal width was not resolved and published\n");
        goto cleanup;
    }

    if (check_status("seed unresolved normal width",
                     rig_set_cache_mode(rig, RIG_VFO_B, RIG_MODE_USB, 900))
            || check_status("publish unresolved normal width",
                            rig_set_cache_mode(rig, RIG_VFO_B, RIG_MODE_NONE,
                                    RIG_PASSBAND_NORMAL))
            || check_status("read unresolved normal width",
                            get_mode_cache(rig, RIG_VFO_B, &mode, &width,
                                           &mode_ms, &width_ms)))
    {
        goto cleanup;
    }

    if (width != 900 || width_ms < 5000)
    {
        fprintf(stderr, "unresolved normal width remained fresh\n");
        goto cleanup;
    }

    if (check_status("set Dummy VFO B mode",
                     rig->caps->set_mode(rig, RIG_VFO_B, RIG_MODE_USB, 2500))
            || check_status("seed stale VFO B width",
                            rig_set_cache_mode(rig, RIG_VFO_B,
                                    RIG_MODE_USB, 900))
            || check_status("invalidate stale VFO B mode",
                            rig_invalidate_cache_mode(rig, RIG_VFO_B))
            || check_status("publish VFO B mode only",
                            rig_set_cache_mode_only(rig, RIG_VFO_B,
                                    RIG_MODE_USB))
            || check_status("set long cache timeout",
                            rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL,
                                    5000))
            || check_status("query VFO B mode",
                            rig_get_mode(rig, RIG_VFO_B, &mode, &width)))
    {
        goto cleanup;
    }

    if (mode != RIG_MODE_USB || width != 2500)
    {
        fprintf(stderr, "mode-only publication returned stale width %d\n",
                (int)width);
        goto cleanup;
    }

    if (check_status("invalidate VFO A mode",
                     rig_invalidate_cache_mode(rig, RIG_VFO_A))
            || check_status("read invalidated cache",
                            get_mode_cache(rig, RIG_VFO_A, &mode, &width,
                                           &mode_ms, &width_ms)))
    {
        goto cleanup;
    }

    if (mode_ms < 5000 || width_ms < 5000)
    {
        fprintf(stderr, "per-VFO mode invalidation left fresh cache ages\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    rig_close(rig);
    rig_cleanup(rig);
    return result;
}
