/*
 * Hamlib frontend configuration buffer tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hamlib/amplifier.h>
#include <hamlib/amplist.h>
#include <hamlib/port.h>
#include <hamlib/rig.h>
#include <hamlib/rig_state.h>
#include <hamlib/riglist.h>
#include <hamlib/rotator.h>
#include <hamlib/rotlist.h>

struct guarded_buffer
{
    char value[4];
    unsigned char guard[4];
};

static void prepare_buffer(struct guarded_buffer *buffer)
{
    memset(buffer, 0xa5, sizeof(*buffer));
}

static int verify_buffer(const char *name, int result,
                         const struct guarded_buffer *buffer,
                         const char *expected)
{
    size_t i;

    if (result != RIG_OK)
    {
        fprintf(stderr, "%s returned %s\n", name, rigerror(result));
        return 1;
    }

    if (memcmp(buffer->value, expected, sizeof(buffer->value) - 1) != 0
            || buffer->value[sizeof(buffer->value) - 1] != '\0')
    {
        fprintf(stderr, "%s did not truncate and terminate its output\n", name);
        return 1;
    }

    for (i = 0; i < sizeof(buffer->guard); ++i)
    {
        if (buffer->guard[i] != 0xa5)
        {
            fprintf(stderr, "%s wrote beyond the output buffer\n", name);
            return 1;
        }
    }

    return 0;
}

static void make_long_value(char *value, size_t size)
{
    memset(value, 'x', size - 1);
    value[size - 1] = '\0';
}

static int test_rig_frontend(void)
{
    char long_value[HAMLIB_FILPATHLEN + 32];
    struct guarded_buffer output;
    struct rig_state *state;
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    int result = 1;

    if (rig == NULL)
    {
        fprintf(stderr, "unable to initialize dummy rig\n");
        return 1;
    }

    make_long_value(long_value, sizeof(long_value));
    state = HAMLIB_STATE(rig);

    if (rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"),
                     long_value) != RIG_OK
            || RIGPORT(rig)->pathname[HAMLIB_FILPATHLEN - 1] != '\0')
    {
        fprintf(stderr, "rig pathname setter did not terminate storage\n");
        goto done;
    }

    prepare_buffer(&output);

    if (verify_buffer("rig pathname getter",
                      rig_get_conf2(rig,
                                    rig_token_lookup(rig, "rig_pathname"),
                                    output.value, sizeof(output.value)),
                      &output, "xxx"))
    {
        goto done;
    }

    if (rig_set_conf(rig, rig_token_lookup(rig, "ptt_pathname"),
                     long_value) != RIG_OK
            || PTTPORT(rig)->pathname[HAMLIB_FILPATHLEN - 1] != '\0')
    {
        fprintf(stderr, "PTT pathname setter did not terminate storage\n");
        goto done;
    }

    prepare_buffer(&output);

    if (verify_buffer("PTT pathname getter",
                      rig_get_conf2(rig,
                                    rig_token_lookup(rig, "ptt_pathname"),
                                    output.value, sizeof(output.value)),
                      &output, "xxx"))
    {
        goto done;
    }

    if (rig_set_conf(rig, rig_token_lookup(rig, "dcd_pathname"),
                     long_value) != RIG_OK
            || DCDPORT(rig)->pathname[HAMLIB_FILPATHLEN - 1] != '\0')
    {
        fprintf(stderr, "DCD pathname setter did not terminate storage\n");
        goto done;
    }

    prepare_buffer(&output);

    if (verify_buffer("DCD pathname getter",
                      rig_get_conf2(rig,
                                    rig_token_lookup(rig, "dcd_pathname"),
                                    output.value, sizeof(output.value)),
                      &output, "xxx"))
    {
        goto done;
    }

    if (rig_set_conf(rig, rig_token_lookup(rig, "device_id"),
                     long_value) != RIG_OK
            || state->device_id[sizeof(state->device_id) - 1] != '\0')
    {
        fprintf(stderr, "device ID setter did not terminate storage\n");
        goto done;
    }

    if (rig_set_conf(rig, rig_token_lookup(rig, "ptt_type"),
                     "Parallel") != RIG_OK)
    {
        fprintf(stderr, "unable to set PTT type\n");
        goto done;
    }

    prepare_buffer(&output);

    if (verify_buffer("PTT type getter",
                      rig_get_conf2(rig, rig_token_lookup(rig, "ptt_type"),
                                    output.value, sizeof(output.value)),
                      &output, "Par"))
    {
        goto done;
    }

    result = 0;

done:
    rig_cleanup(rig);
    return result;
}

static int test_rotator_frontend(void)
{
    char long_value[HAMLIB_FILPATHLEN + 32];
    struct guarded_buffer output;
    struct rot_caps caps;
    ROT *rot = rot_init(ROT_MODEL_DUMMY);
    int result = 1;

    if (rot == NULL)
    {
        fprintf(stderr, "unable to initialize dummy rotator\n");
        return 1;
    }

    make_long_value(long_value, sizeof(long_value));
    caps = *rot->caps;
    caps.port_type = RIG_PORT_SERIAL;
    rot->caps = &caps;

    if (rot_set_conf(rot, rot_token_lookup(rot, "rot_pathname"),
                     long_value) != RIG_OK
            || ROTPORT(rot)->pathname[HAMLIB_FILPATHLEN - 1] != '\0')
    {
        fprintf(stderr, "rotator pathname setter did not terminate storage\n");
        goto done;
    }

    prepare_buffer(&output);

    if (verify_buffer("rotator pathname getter",
                      rot_get_conf2(rot,
                                    rot_token_lookup(rot, "rot_pathname"),
                                    output.value, sizeof(output.value)),
                      &output, "xxx"))
    {
        goto done;
    }

    ROTPORT(rot)->type.rig = RIG_PORT_SERIAL;

    if (rot_set_conf(rot, rot_token_lookup(rot, "serial_parity"),
                     "Space") != RIG_OK)
    {
        fprintf(stderr, "unable to set rotator parity\n");
        goto done;
    }

    prepare_buffer(&output);

    if (verify_buffer("rotator parity getter",
                      rot_get_conf2(rot,
                                    rot_token_lookup(rot, "serial_parity"),
                                    output.value, sizeof(output.value)),
                      &output, "Spa"))
    {
        goto done;
    }

    result = 0;

done:
    rot_cleanup(rot);
    return result;
}

static int test_amplifier_frontend(void)
{
    char long_value[HAMLIB_FILPATHLEN + 32];
    struct guarded_buffer output;
    struct amp_caps caps;
    AMP *amp = amp_init(AMP_MODEL_DUMMY);
    int result = 1;

    if (amp == NULL)
    {
        fprintf(stderr, "unable to initialize dummy amplifier\n");
        return 1;
    }

    make_long_value(long_value, sizeof(long_value));
    caps = *amp->caps;
    caps.port_type = RIG_PORT_SERIAL;
    amp->caps = &caps;

    if (amp_set_conf(amp, amp_token_lookup(amp, "amp_pathname"),
                     long_value) != RIG_OK
            || AMPPORT(amp)->pathname[HAMLIB_FILPATHLEN - 1] != '\0')
    {
        fprintf(stderr, "amplifier pathname setter did not terminate storage\n");
        goto done;
    }

    prepare_buffer(&output);

    if (verify_buffer("amplifier pathname getter",
                      amp_get_conf2(amp,
                                    amp_token_lookup(amp, "amp_pathname"),
                                    output.value, sizeof(output.value)),
                      &output, "xxx"))
    {
        goto done;
    }

    AMPPORT(amp)->type.rig = RIG_PORT_SERIAL;

    if (amp_set_conf(amp, amp_token_lookup(amp, "serial_parity"),
                     "Space") != RIG_OK)
    {
        fprintf(stderr, "unable to set amplifier parity\n");
        goto done;
    }

    prepare_buffer(&output);

    if (verify_buffer("amplifier parity getter",
                      amp_get_conf2(amp,
                                    amp_token_lookup(amp, "serial_parity"),
                                    output.value, sizeof(output.value)),
                      &output, "Spa"))
    {
        goto done;
    }

    result = 0;

done:
    amp_cleanup(amp);
    return result;
}

int main(void)
{
    rig_load_backend("dummy");
    rot_load_backend("dummy");
    amp_load_backend("dummy");

    if (test_rig_frontend() || test_rotator_frontend()
            || test_amplifier_frontend())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
