/*
 * Hamlib asynchronous port lifecycle tests, run without a radio or network
 * peer.
 *
 * Async operation has three distinct states: async_data_enabled records the
 * requested configuration, the pipes carry the reader's responses, and
 * port.asyncio routes frontend reads through those pipes only while the
 * reader owns the physical port.  Starting the reader or the Morse handler
 * either leaves the thread running or leaves nothing behind.
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

#include "hamlib/port.h"
#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "iofunc.h"
#include "misc.h"
#include "rig_internal.h"

/* A private model number avoids loading or modifying a production backend. */
#define TEST_ASYNC_MODEL RIG_MAKE_MODEL(RIG_DUMMY, 999)
#define TEST_FRAME_BYTE 0x5a

static int backend_opened_in_direct_mode;
static int backend_closed_in_direct_mode;
static int frame_sent;

/* The backend's open and close hooks run while no reader owns the port. */
static int stub_rig_open(RIG *rig)
{
    hamlib_port_t *port = RIGPORT(rig);

    backend_opened_in_direct_mode = !port->asyncio
                                    && !port_async_is_prepared(port);
    return backend_opened_in_direct_mode ? RIG_OK : -RIG_EINTERNAL;
}

static int stub_rig_close(RIG *rig)
{
    hamlib_port_t *port = RIGPORT(rig);

    backend_closed_in_direct_mode = !port->asyncio
                                    && !port_async_is_prepared(port);
    return RIG_OK;
}

static int stub_read_frame_direct(RIG *rig, size_t buffer_length,
                                  const unsigned char *buffer)
{
    if (!frame_sent && buffer_length > 0)
    {
        /* The callback ABI const-qualifies what is actually its output buffer. */
        ((unsigned char *) buffer)[0] = TEST_FRAME_BYTE;
        frame_sent = 1;
        return 1;
    }

    /* Keep the reader alive until the normal stop path asks it to exit. */
    while (STATE(rig)->async_data_handler_thread_run)
    {
        hl_usleep(1000);
    }

    return -RIG_ETIMEOUT;
}

static int stub_is_async_frame(RIG *rig, size_t frame_length,
                               const unsigned char *frame)
{
    (void) rig;
    (void) frame_length;
    (void) frame;
    /* Treat the emitted byte as a command response that belongs in the pipe. */
    return 0;
}

static int stub_process_async_frame(RIG *rig, size_t frame_length,
                                    const unsigned char *frame)
{
    (void) rig;
    (void) frame_length;
    (void) frame;
    /* Required by async-capable caps; unreachable because every frame is sync. */
    return RIG_OK;
}

/* A private backend keeps rig_open()/rig_close() real while replacing only I/O. */
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
    .rig_open = stub_rig_open,
    .rig_close = stub_rig_close,
    .async_data_supported = 1,
    .read_frame_direct = stub_read_frame_direct,
    .is_async_frame = stub_is_async_frame,
    .process_async_frame = stub_process_async_frame,
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

/* Prepared pipes must work before, and remain distinct from, active routing. */
static void test_sync_pipe_is_separate_from_active_routing(void)
{
    hamlib_port_t port;
    unsigned char sent = TEST_FRAME_BYTE;
    unsigned char received = 0;
    unsigned char error = (unsigned char) -RIG_EIO;

    memset(&port, 0, sizeof(port));
    port.fd = -1;
    port.type.rig = RIG_PORT_NONE;
    port.timeout = 200;
    port.asyncio = 1;

    /* Opening a port always establishes direct mode; startup prepares later. */
    TEST_ASSERT(port_open(&port) == RIG_OK);
    TEST_CHECK(!port.asyncio);
    TEST_CHECK(!port_async_is_prepared(&port));
    TEST_ASSERT(port_prepare_async(&port) == RIG_OK);
    TEST_CHECK(!port.asyncio);
    TEST_CHECK(port_async_is_prepared(&port));

    /* Replies go into the pipe whatever the flag, never back to the rig. */
    TEST_CHECK(write_block_sync(&port, &sent, 1) == 1);
    port.asyncio = 1;
    TEST_CHECK(read_block(&port, &received, 1) == 1);
    TEST_CHECK(received == sent);

    /* Error codes take the same way. */
    port.asyncio = 0;
    TEST_CHECK(write_block_sync_error(&port, &error, 1) == 1);

    port_cleanup_async(&port);
    TEST_CHECK(!port.asyncio);
    TEST_CHECK(!port_async_is_prepared(&port));
    TEST_CHECK(port_close(&port, port.type.rig) == RIG_OK);
}

/* The complete open/close path transfers port ownership to and from the reader. */
static void test_reader_owns_port_only_while_running(void)
{
    RIG *rig;
    unsigned char received = 0;

    backend_opened_in_direct_mode = 0;
    frame_sent = 0;
    rig = new_test_rig(1);
    TEST_ASSERT(rig != NULL);

    TEST_ASSERT(rig_open(rig) == RIG_OK);
    TEST_CHECK(backend_opened_in_direct_mode);
    TEST_CHECK(RIGPORT(rig)->asyncio);
    TEST_CHECK(port_async_is_prepared(RIGPORT(rig)));
    TEST_CHECK(STATE(rig)->async_data_handler_thread_run);
    TEST_CHECK(STATE(rig)->async_data_handler_priv_data != NULL);
    TEST_CHECK(read_block(RIGPORT(rig), &received, 1) == 1);
    TEST_CHECK(received == TEST_FRAME_BYTE);

    backend_closed_in_direct_mode = 0;
    TEST_CHECK(rig_close(rig) == RIG_OK);
    TEST_CHECK(backend_closed_in_direct_mode);
    TEST_CHECK(!RIGPORT(rig)->asyncio);
    TEST_CHECK(!port_async_is_prepared(RIGPORT(rig)));
    TEST_CHECK(!STATE(rig)->async_data_handler_thread_run);
    TEST_CHECK(STATE(rig)->async_data_handler_priv_data == NULL);
    TEST_CHECK(rig_cleanup(rig) == RIG_OK);
}

/* Configuration off means neither pipes nor a reader should be created. */
static void test_disabled_async_never_prepares_routing(void)
{
    RIG *rig;

    backend_opened_in_direct_mode = 0;
    rig = new_test_rig(0);
    TEST_ASSERT(rig != NULL);

    TEST_ASSERT(rig_open(rig) == RIG_OK);
    TEST_CHECK(backend_opened_in_direct_mode);
    TEST_CHECK(!RIGPORT(rig)->asyncio);
    TEST_CHECK(!port_async_is_prepared(RIGPORT(rig)));
    TEST_CHECK(!STATE(rig)->async_data_handler_thread_run);
    TEST_CHECK(STATE(rig)->async_data_handler_priv_data == NULL);

    TEST_CHECK(rig_close(rig) == RIG_OK);
    TEST_CHECK(rig_cleanup(rig) == RIG_OK);
}

/* skip_init deliberately bypasses background startup even when configured. */
static void test_skip_init_never_prepares_routing(void)
{
    RIG *rig;
    int status;

    backend_opened_in_direct_mode = 0;
    rig = new_test_rig(1);
    TEST_ASSERT(rig != NULL);

    skip_init = 1;
    status = rig_open(rig);
    skip_init = 0;

    TEST_ASSERT(status == RIG_OK);
    TEST_CHECK(backend_opened_in_direct_mode);
    TEST_CHECK(!RIGPORT(rig)->asyncio);
    TEST_CHECK(!port_async_is_prepared(RIGPORT(rig)));
    TEST_CHECK(!STATE(rig)->async_data_handler_thread_run);
    TEST_CHECK(STATE(rig)->async_data_handler_priv_data == NULL);

    TEST_CHECK(rig_close(rig) == RIG_OK);
    TEST_CHECK(rig_cleanup(rig) == RIG_OK);
}

/* Reader creation failure must undo pipe preparation and all published state. */
static void test_failed_reader_start_rolls_back_state(void)
{
    RIG *rig = new_test_rig(1);

    TEST_ASSERT(rig != NULL);
    TEST_ASSERT(port_open(RIGPORT(rig)) == RIG_OK);
    TEST_CHECK(rig_async_data_handler_start(rig, fail_thread_start)
               == -RIG_EINTERNAL);
    TEST_CHECK(!RIGPORT(rig)->asyncio);
    TEST_CHECK(!port_async_is_prepared(RIGPORT(rig)));
    TEST_CHECK(!STATE(rig)->async_data_handler_thread_run);
    TEST_CHECK(STATE(rig)->async_data_handler_priv_data == NULL);

    TEST_CHECK(port_close(RIGPORT(rig), RIGPORT(rig)->type.rig) == RIG_OK);
    TEST_CHECK(rig_cleanup(rig) == RIG_OK);
}

/* Morse creation has no pipes, but owns the same run-flag/private-data invariant. */
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
    { "sync_pipe_is_separate_from_active_routing",
      test_sync_pipe_is_separate_from_active_routing },
    { "reader_owns_port_only_while_running",
      test_reader_owns_port_only_while_running },
    { "disabled_async_never_prepares_routing",
      test_disabled_async_never_prepares_routing },
    { "skip_init_never_prepares_routing",
      test_skip_init_never_prepares_routing },
    { "failed_reader_start_rolls_back_state",
      test_failed_reader_start_rolls_back_state },
    { "failed_morse_thread_start_rolls_back_state",
      test_failed_morse_thread_start_rolls_back_state },
    { NULL, NULL }
};
