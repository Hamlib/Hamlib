/*
 * Hamlib frontend configuration tests
 * Copyright (c) 2026 by Hamlib Team
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "acutest.h"
#include "test_debug.h"
#include <string.h>

static void check_ptt_config(RIG *rig, const char *expected)
{
    char actual[32];
    hamlib_token_t token = rig_token_lookup(rig, "ptt_type");

    TEST_ASSERT(token != RIG_CONF_END);
    TEST_ASSERT(rig_get_conf2(rig, token, actual, sizeof(actual)) == RIG_OK);
    TEST_CHECK_(strcmp(actual, expected) == 0,
                "PTT configuration: expected %s, got %s", expected, actual);
}

static void test_ptt_overrides_are_instance_local(void)
{
    const char *values[] =
    {
        "None", "DTR", "RTS", "Parallel", "CM108",
        "GPIO", "GPION", "RIGMICDATA", "RIG"
    };
    RIG *configured = rig_init(RIG_MODEL_DUMMY);
    RIG *existing = rig_init(RIG_MODEL_DUMMY);

    TEST_ASSERT(configured != NULL && existing != NULL);
    hamlib_token_t token = rig_token_lookup(configured, "ptt_type");
    TEST_ASSERT(token != RIG_CONF_END);
    TEST_ASSERT(configured->caps->ptt_type == RIG_PTT_RIG);

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        TEST_CASE(values[i]);
        TEST_ASSERT(rig_set_conf(configured, token, values[i]) == RIG_OK);
        check_ptt_config(configured, values[i]);
        check_ptt_config(existing, "RIG");
        TEST_CHECK(configured->caps->ptt_type == RIG_PTT_RIG);
        TEST_CHECK(rig_get_caps_int(RIG_MODEL_DUMMY, RIG_CAPS_PTT_TYPE)
                   == RIG_PTT_RIG);

        RIG *subsequent = rig_init(RIG_MODEL_DUMMY);
        TEST_ASSERT(subsequent != NULL);
        check_ptt_config(subsequent, "RIG");
        TEST_CHECK(rig_cleanup(subsequent) == RIG_OK);
    }

    TEST_CHECK(rig_set_conf(configured, token, "invalid") == -RIG_EINVAL);
    check_ptt_config(configured, "RIG");
    TEST_CHECK(rig_cleanup(configured) == RIG_OK);
    TEST_CHECK(rig_cleanup(existing) == RIG_OK);
}

static void test_ptt_none_then_cat(void)
{
    RIG *configured = rig_init(RIG_MODEL_DUMMY);
    RIG *existing = rig_init(RIG_MODEL_DUMMY);
    ptt_t ptt;

    TEST_ASSERT(configured != NULL && existing != NULL);
    hamlib_token_t token = rig_token_lookup(configured, "ptt_type");
    TEST_ASSERT(token != RIG_CONF_END);
    TEST_ASSERT(rig_set_conf(configured, token, "None") == RIG_OK);
    TEST_ASSERT(rig_set_cache_timeout_ms(configured, HAMLIB_CACHE_ALL, 0)
                == RIG_OK);
    TEST_ASSERT(rig_open(configured) == RIG_OK);
    TEST_CHECK(rig_get_ptt(configured, RIG_VFO_CURR, &ptt) == -RIG_ENAVAIL);

    RIG *subsequent = rig_init(RIG_MODEL_DUMMY);
    TEST_ASSERT(subsequent != NULL);
    RIG *cat_rigs[] = { existing, subsequent };

    for (size_t i = 0; i < sizeof(cat_rigs) / sizeof(cat_rigs[0]); ++i)
    {
        TEST_ASSERT(rig_set_cache_timeout_ms(cat_rigs[i], HAMLIB_CACHE_ALL, 0)
                    == RIG_OK);
        TEST_ASSERT(rig_open(cat_rigs[i]) == RIG_OK);
        TEST_CHECK(rig_get_ptt(cat_rigs[i], RIG_VFO_CURR, &ptt) == RIG_OK);
        TEST_CHECK(rig_close(cat_rigs[i]) == RIG_OK);
        TEST_CHECK(rig_cleanup(cat_rigs[i]) == RIG_OK);
    }

    TEST_CHECK(rig_close(configured) == RIG_OK);
    TEST_ASSERT(rig_set_conf(configured, token, "RIG") == RIG_OK);
    TEST_ASSERT(rig_open(configured) == RIG_OK);
    TEST_CHECK(rig_get_ptt(configured, RIG_VFO_CURR, &ptt) == RIG_OK);
    TEST_CHECK(rig_close(configured) == RIG_OK);
    TEST_CHECK(rig_cleanup(configured) == RIG_OK);
}

TEST_LIST =
{
    { "ptt_overrides_are_instance_local", test_ptt_overrides_are_instance_local },
    { "ptt_none_then_cat", test_ptt_none_then_cat },
    { NULL, NULL }
};
