/*
 * Hamlib asynchronous port lifecycle tests, run without a radio or network
 * peer.
 *
 * Starting the asynchronous reader or the Morse handler either leaves the
 * thread running or leaves nothing behind.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"

#include <errno.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "rig_internal.h"

/* A private model number avoids loading or modifying a production backend. */
#define TEST_ASYNC_MODEL RIG_MAKE_MODEL(RIG_DUMMY, 999)

/* A private backend keeps rig_init()/rig_cleanup() real. */
static struct rig_caps async_caps =
{
    .rig_model = TEST_ASYNC_MODEL,
    .model_name = "Async lifecycle test",
    .mfg_name = "Hamlib",
    .version = "1",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .port_type = RIG_PORT_NONE,
    .timeout = 200,
    .retry = 0,
    .async_data_supported = 1,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS,
};

static RIG *new_test_rig(int async_enabled)
{
    static int registered;
    RIG *rig;

    if (!registered)
    {
        if (rig_register(&async_caps) != RIG_OK)
        {
            return NULL;
        }

        registered = 1;
    }

    rig = rig_init(TEST_ASYNC_MODEL);

    if (rig != NULL)
    {
        STATE(rig)->async_data_enabled = async_enabled;
        /* Polling would add an unrelated background thread to this fixture. */
        STATE(rig)->poll_interval = 0;
    }

    return rig;
}

/* Deterministically exercise pthread_create() rollback without interposition. */
static int fail_thread_start(pthread_t *thread, void *(*routine)(void *),
                             void *arg)
{
    (void) thread;
    (void) routine;
    (void) arg;
    return EAGAIN;
}

/* Reader creation failure must leave no run flag or private data behind. */
static void test_failed_reader_start_rolls_back_state(void)
{
    RIG *rig = new_test_rig(1);

    TEST_ASSERT(rig != NULL);
    TEST_CHECK(rig_async_data_handler_start(rig, fail_thread_start)
               == -RIG_EINTERNAL);
    TEST_CHECK(!STATE(rig)->async_data_handler_thread_run);
    TEST_CHECK(STATE(rig)->async_data_handler_priv_data == NULL);

    TEST_CHECK(rig_cleanup(rig) == RIG_OK);
}

/* Morse creation owns the same run-flag/private-data invariant. */
static void test_failed_morse_thread_start_rolls_back_state(void)
{
    RIG *rig = new_test_rig(0);

    TEST_ASSERT(rig != NULL);
    TEST_CHECK(rig_morse_data_handler_start(rig, fail_thread_start)
               == -RIG_EINTERNAL);
    TEST_CHECK(!STATE(rig)->morse_data_handler_thread_run);
    TEST_CHECK(STATE(rig)->morse_data_handler_priv_data == NULL);

    TEST_CHECK(rig_cleanup(rig) == RIG_OK);
}

TEST_LIST =
{
    { "failed_reader_start_rolls_back_state",
      test_failed_reader_start_rolls_back_state },
    { "failed_morse_thread_start_rolls_back_state",
      test_failed_morse_thread_start_rolls_back_state },
    { NULL, NULL }
};
