/*
 * Hamlib rig registry tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>

#include "hamlib/rig.h"

/* Distinct IDs sharing a bucket in the registry's modulo-65535 hash. */
static struct rig_caps caps[] =
{
    { .rig_model = 0x70000000U },
    { .rig_model = 0x70000000U + 65535U },
    { .rig_model = 0x70000000U + 2U * 65535U }
};

struct enumeration
{
    unsigned int seen[3];
    unsigned int unexpected;
};

static int count_model(rig_model_t model, rig_ptr_t data)
{
    struct enumeration *entries = data;

    for (unsigned int i = 0; i < 3; i++)
    {
        if (model == caps[i].rig_model)
        {
            entries->seen[i]++;
            return 1;
        }
    }

    entries->unexpected++;
    return 1;
}

static int count_caps(const struct rig_caps *entry, rig_ptr_t data)
{
    struct enumeration *entries = data;

    if (rig_get_caps(entry->rig_model) != entry)
    {
        entries->unexpected++;
    }

    return count_model(entry->rig_model, data);
}

static int check_registry(unsigned int present)
{
    struct enumeration by_caps = { { 0 }, 0 };
    struct enumeration by_model = { { 0 }, 0 };

    if (rig_list_foreach(count_caps, &by_caps) != RIG_OK
            || rig_list_foreach_model(count_model, &by_model) != RIG_OK
            || by_caps.unexpected || by_model.unexpected)
    {
        fprintf(stderr, "registry enumeration failed or found extra entries\n");
        return 1;
    }

    for (unsigned int i = 0; i < 3; i++)
    {
        unsigned int expected = (present >> i) & 1U;
        struct rig_caps *entry = expected ? &caps[i] : NULL;

        if (rig_get_caps(caps[i].rig_model) != entry
                || by_caps.seen[i] != expected
                || by_model.seen[i] != expected)
        {
            fprintf(stderr, "incorrect lookup or enumeration for model %u\n",
                    caps[i].rig_model);
            return 1;
        }
    }

    return 0;
}

static int check_result(const char *operation, int actual, int expected)
{
    if (actual != expected)
    {
        fprintf(stderr, "%s returned %d, expected %d\n",
                operation, actual, expected);
        return 1;
    }

    return 0;
}

struct backend_snapshot
{
    const struct rig_caps *entries[MAX_MODELS_PER_BACKEND];
    unsigned int count;
    int failed;
};

static int snapshot_dummy(const struct rig_caps *entry, rig_ptr_t data)
{
    struct backend_snapshot *snapshot = data;
    unsigned int index = entry->rig_model % MAX_MODELS_PER_BACKEND;

    if (RIG_BACKEND_NUM(entry->rig_model) != RIG_DUMMY
            || snapshot->entries[index] != NULL)
    {
        fprintf(stderr, "unexpected or duplicate loaded model %u\n",
                entry->rig_model);
        snapshot->failed = 1;
        return 0;
    }

    snapshot->entries[index] = entry;
    snapshot->count++;
    return 1;
}

static int check_dummy_snapshot(const struct backend_snapshot *expected)
{
    struct backend_snapshot actual = { 0 };

    if (rig_list_foreach(snapshot_dummy, &actual) != RIG_OK
            || actual.failed || actual.count != expected->count)
    {
        fprintf(stderr, "loaded Dummy registry entry count changed\n");
        return 1;
    }

    for (unsigned int i = 0; i < MAX_MODELS_PER_BACKEND; i++)
    {
        if (actual.entries[i] != expected->entries[i]
                || rig_get_caps(RIG_MAKE_MODEL(RIG_DUMMY, i))
                != expected->entries[i])
        {
            fprintf(stderr, "loaded Dummy model %u changed capabilities\n", i);
            return 1;
        }
    }

    return 0;
}

static int test_repeated_loading(void)
{
    struct backend_snapshot initial = { 0 };
    unsigned int unused;
    int failed;

    fprintf(stderr, "repeat Dummy loading and initialize an unknown model\n");
    failed = check_result("load Dummy backend",
                          rig_load_backend("dummy"), RIG_OK);
    failed |= check_result("snapshot Dummy backend",
                           rig_list_foreach(snapshot_dummy, &initial), RIG_OK);

    if (initial.failed || rig_get_caps(RIG_MODEL_DUMMY) == NULL)
    {
        fprintf(stderr, "initial Dummy backend registration failed\n");
        failed = 1;
        goto cleanup;
    }

    for (unused = 1; unused < MAX_MODELS_PER_BACKEND; unused++)
    {
        if (initial.entries[unused] == NULL)
        {
            break;
        }
    }

    if (unused == MAX_MODELS_PER_BACKEND)
    {
        fprintf(stderr, "no unknown model ID available in Dummy family\n");
        failed = 1;
        goto cleanup;
    }

    failed |= check_result("reload Dummy backend",
                           rig_load_backend("dummy"), RIG_OK);
    failed |= check_dummy_snapshot(&initial);
    failed |= check_result("check registered Dummy model",
                           rig_check_backend(RIG_MODEL_DUMMY), RIG_OK);
    failed |= check_dummy_snapshot(&initial);

    RIG *rig = rig_init(RIG_MAKE_MODEL(RIG_DUMMY, unused));

    if (rig != NULL)
    {
        fprintf(stderr, "initializing an unknown Dummy model succeeded\n");
        rig_cleanup(rig);
        failed = 1;
    }

    failed |= check_dummy_snapshot(&initial);

cleanup:

    for (unsigned int i = 0; i < MAX_MODELS_PER_BACKEND; i++)
    {
        if (initial.entries[i] != NULL)
        {
            failed |= check_result("cleanup Dummy registration",
                                   rig_unregister(initial.entries[i]->rig_model),
                                   RIG_OK);
        }
    }

    failed |= check_registry(0);
    return failed;
}

int main(void)
{
    struct rig_caps duplicate = { .rig_model = 0x70000000U };
    rig_model_t absent = 0x70000000U + 3U * 65535U;
    unsigned int present = 0;
    int failed = 0;

    rig_set_debug(RIG_DEBUG_ERR);
    fprintf(stderr, "register distinct colliding model IDs\n");

    for (unsigned int i = 0; i < 3; i++)
    {
        failed |= check_result("register colliding model",
                               rig_register(&caps[i]), RIG_OK);
        present |= 1U << i;
        failed |= check_registry(present);
    }

    failed |= check_result("register same capabilities pointer",
                           rig_register(&caps[0]), -RIG_EINVAL);
    failed |= check_result("register different pointer with same ID",
                           rig_register(&duplicate), -RIG_EINVAL);
    failed |= check_result("register NULL", rig_register(NULL), -RIG_EINVAL);
    failed |= check_result("unregister absent colliding ID",
                           rig_unregister(absent), -RIG_EINVAL);
    failed |= check_registry(present);

    /* Prepending creates the chain caps[2] -> caps[1] -> caps[0]. */
    const unsigned int removal_order[] = { 2, 0, 1 };

    for (unsigned int i = 0; i < 3; i++)
    {
        unsigned int index = removal_order[i];
        failed |= check_result("unregister colliding model",
                               rig_unregister(caps[index].rig_model), RIG_OK);
        present &= ~(1U << index);
        failed |= check_registry(present);
    }

    for (unsigned int i = 0; i < 3; i++)
    {
        failed |= check_result("re-register colliding model",
                               rig_register(&caps[i]), RIG_OK);
        present |= 1U << i;
    }

    failed |= check_result("unregister interior node",
                           rig_unregister(caps[1].rig_model), RIG_OK);
    present &= ~(1U << 1);
    failed |= check_registry(present);

    for (unsigned int i = 0; i < 3; i++)
    {
        if (present & (1U << i))
        {
            failed |= check_result("cleanup registration",
                                   rig_unregister(caps[i].rig_model), RIG_OK);
        }
    }

    failed |= check_registry(0);
    failed |= check_result("unregister already removed ID",
                           rig_unregister(caps[0].rig_model), -RIG_EINVAL);

    failed |= check_result("re-register ID with different capabilities",
                           rig_register(&duplicate), RIG_OK);

    if (rig_get_caps(duplicate.rig_model) != &duplicate)
    {
        fprintf(stderr, "re-registration did not retain the new pointer\n");
        failed = 1;
    }

    failed |= check_result("cleanup replacement registration",
                           rig_unregister(duplicate.rig_model), RIG_OK);
    failed |= check_registry(0);
    failed |= test_repeated_loading();
    return failed != 0;
}
