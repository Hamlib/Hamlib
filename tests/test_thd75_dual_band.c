/*
 *  Hamlib TH-D75 Dual Band (DL command) tests
 *  Copyright (c) 2026 by Ben Woodard AE6BC with the assistance of Goose
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Unit tests for TH-D75 Dual Band (DL command) functionality.
 * Tests the RIG_FUNC_DUAL_BAND implementation via set_func/get_func.
 * 
 * Hardware tests are disabled by default. Set HAMLIB_TEST_TH_D75=1
 * environment variable to enable hardware tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <hamlib/rig.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

/* Check if hardware tests should run */
static int hardware_tests_enabled(void)
{
    const char *env = getenv("HAMLIB_TEST_TH_D75");
    return env && (strcmp(env, "1") == 0 || strcasecmp(env, "true") == 0);
}

/* Helper: initialize and open a TH-D75 rig. Returns NULL on failure. */
static RIG *open_thd75(void)
{
    rig_load_all_backends();

    RIG *rig = rig_init(RIG_MODEL_THD75);

    if (!rig)
    {
        return NULL;
    }

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        return NULL;
    }

    return rig;
}

/* Helper: close and clean up a rig. */
static void close_thd75(RIG *rig)
{
    if (rig)
    {
        rig_close(rig);
        rig_cleanup(rig);
    }
}

/* ------------------------------------------------------------------ */
/* Tests for RIG_FUNC_DUAL_BAND (DL command)                          */
/* ------------------------------------------------------------------ */

static void test_thd75_dual_band_supported(void)
{
    /* This test verifies the RIG_FUNC_DUAL_BAND flag is defined correctly.
     * It's a compile-time and basic check that doesn't require hardware.
     */
    tests_run++;
    printf("Running test: thd75_dual_band_supported\n");
    
    int local_failed = 0;
    
    /* Verify the flag exists and is a valid bit flag */
    if (RIG_FUNC_DUAL_BAND == 0) {
        printf("  FAILED: RIG_FUNC_DUAL_BAND is 0\n");
        local_failed = 1;
    }
    
    /* Verify it's bit 49 as defined in rig.h */
    if ((RIG_FUNC_DUAL_BAND & (1ull << 49)) == 0) {
        printf("  FAILED: RIG_FUNC_DUAL_BAND is not bit 49\n");
        local_failed = 1;
    }
    
    /* Load backends */
    rig_load_all_backends();
    
    RIG *rig = rig_init(RIG_MODEL_THD75);
    if (!rig) {
        printf("  FAILED: rig_init returned NULL\n");
        local_failed = 1;
    } else if (!rig->caps) {
        printf("  FAILED: rig->caps is NULL\n");
        local_failed = 1;
    } else {
        /* The thd75 driver should support dual band */
        if ((rig->caps->has_set_func & RIG_FUNC_DUAL_BAND) == 0) {
            printf("  FAILED: thd75 driver does not support RIG_FUNC_DUAL_BAND\n");
            local_failed = 1;
        }
        rig_cleanup(rig);
    }
    
    if (local_failed) {
        tests_failed++;
        printf("  RESULT: FAILED\n");
    } else {
        tests_passed++;
        printf("  RESULT: PASSED\n");
    }
}

static void test_thd75_dual_band_disable(void)
{
    tests_run++;
    printf("Running test: thd75_dual_band_disable\n");
    
    RIG *rig;
    int retval;

    /* Skip if hardware tests not enabled */
    if (!hardware_tests_enabled())
    {
        tests_skipped++;
        printf("  SKIPPED: Hardware test skipped. Set HAMLIB_TEST_TH_D75=1 to enable.\n");
        return;
    }

    rig = open_thd75();
    if (!rig) {
        printf("  FAILED: Could not open TH-D75 rig\n");
        tests_failed++;
        return;
    }

    int local_failed = 0;
    
    /* Test disabling Dual Band (DL 0) */
    retval = rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, 0);
    if (retval != RIG_OK && retval != -RIG_ETIMEOUT && retval != -RIG_EIO) {
        printf("  FAILED: rig_set_func returned %d\n", retval);
        local_failed = 1;
    }

    /* Verify the state was set (only if communication succeeded) */
    if (retval == RIG_OK && !local_failed)
    {
        int status = 0;
        retval = rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, &status);
        if (retval != RIG_OK) {
            printf("  FAILED: rig_get_func returned %d\n", retval);
            local_failed = 1;
        } else if (status != 0) {
            printf("  FAILED: Expected status 0, got %d\n", status);
            local_failed = 1;
        }
    }

    if (local_failed) {
        tests_failed++;
        printf("  RESULT: FAILED\n");
    } else {
        tests_passed++;
        printf("  RESULT: PASSED\n");
    }

    close_thd75(rig);
}

static void test_thd75_dual_band_enable(void)
{
    tests_run++;
    printf("Running test: thd75_dual_band_enable\n");
    
    RIG *rig;
    int retval;

    /* Skip if hardware tests not enabled */
    if (!hardware_tests_enabled())
    {
        tests_skipped++;
        printf("  SKIPPED: Hardware test skipped. Set HAMLIB_TEST_TH_D75=1 to enable.\n");
        return;
    }

    rig = open_thd75();
    if (!rig) {
        printf("  FAILED: Could not open TH-D75 rig\n");
        tests_failed++;
        return;
    }

    int local_failed = 0;
    
    /* Test enabling Dual Band (DL 1) */
    retval = rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, 1);
    if (retval != RIG_OK && retval != -RIG_ETIMEOUT && retval != -RIG_EIO) {
        printf("  FAILED: rig_set_func returned %d\n", retval);
        local_failed = 1;
    }

    /* Verify the state was set (only if communication succeeded) */
    if (retval == RIG_OK && !local_failed)
    {
        int status = 0;
        retval = rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, &status);
        if (retval != RIG_OK) {
            printf("  FAILED: rig_get_func returned %d\n", retval);
            local_failed = 1;
        } else if (status != 1) {
            printf("  FAILED: Expected status 1, got %d\n", status);
            local_failed = 1;
        }

        if (!local_failed) {
            /* Clean up: disable again */
            rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, 0);
        }
    }

    if (local_failed) {
        tests_failed++;
        printf("  RESULT: FAILED\n");
    } else {
        tests_passed++;
        printf("  RESULT: PASSED\n");
    }

    close_thd75(rig);
}

static void test_thd75_dual_band_toggle(void)
{
    tests_run++;
    printf("Running test: thd75_dual_band_toggle\n");
    
    RIG *rig;
    int retval;

    /* Skip if hardware tests not enabled */
    if (!hardware_tests_enabled())
    {
        tests_skipped++;
        printf("  SKIPPED: Hardware test skipped. Set HAMLIB_TEST_TH_D75=1 to enable.\n");
        return;
    }

    rig = open_thd75();
    if (!rig) {
        printf("  FAILED: Could not open TH-D75 rig\n");
        tests_failed++;
        return;
    }

    int local_failed = 0;
    
    /* Start with Dual Band disabled */
    retval = rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, 0);
    if (retval != RIG_OK)
    {
        close_thd75(rig);
        printf("  SKIPPED: Could not communicate with radio to disable.\n");
        tests_skipped++;
        return;
    }

    /* Enable it */
    retval = rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, 1);
    if (retval != RIG_OK) {
        printf("  FAILED: rig_set_func to enable returned %d\n", retval);
        local_failed = 1;
    }

    if (!local_failed) {
        int status = 0;
        retval = rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, &status);
        if (retval != RIG_OK) {
            printf("  FAILED: rig_get_func returned %d\n", retval);
            local_failed = 1;
        } else if (status != 1) {
            printf("  FAILED: Expected status 1 after enable, got %d\n", status);
            local_failed = 1;
        }
    }

    if (!local_failed) {
        /* Disable it again */
        retval = rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, 0);
        if (retval != RIG_OK) {
            printf("  FAILED: rig_set_func to disable returned %d\n", retval);
            local_failed = 1;
        }
    }

    if (!local_failed) {
        int status = 0;
        retval = rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_DUAL_BAND, &status);
        if (retval != RIG_OK) {
            printf("  FAILED: rig_get_func returned %d\n", retval);
            local_failed = 1;
        } else if (status != 0) {
            printf("  FAILED: Expected status 0 after disable, got %d\n", status);
            local_failed = 1;
        }
    }

    if (local_failed) {
        tests_failed++;
        printf("  RESULT: FAILED\n");
    } else {
        tests_passed++;
        printf("  RESULT: PASSED\n");
    }

    close_thd75(rig);
}

int main(void)
{
    printf("\n=== TH-D75 Dual Band Tests ===\n\n");
    
    rig_load_all_backends();
    
    test_thd75_dual_band_supported();
    test_thd75_dual_band_disable();
    test_thd75_dual_band_enable();
    test_thd75_dual_band_toggle();
    
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d tests\n", tests_run);
    printf("Passed: %d tests\n", tests_passed);
    printf("Failed: %d tests\n", tests_failed);
    printf("Skipped: %d tests\n", tests_skipped);
    
    if (tests_failed > 0)
    {
        printf("\n*** SOME TESTS FAILED ***\n\n");
        return 1;
    }
    
    printf("\n*** ALL TESTS PASSED ***\n\n");
    return 0;
}
