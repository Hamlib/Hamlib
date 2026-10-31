/*
 * Hamlib asynchronous port lifecycle tests.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hamlib/config.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "iofunc.h"
#include "misc.h"
#include "rig_internal.h"

#define REQUIRE(condition) do { \
    if (!(condition)) \
    { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        fflush(stderr); \
        _Exit(EXIT_FAILURE); \
    } \
} while (0)

struct watchdog
{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    int done;
};

static void *watchdog_main(void *arg)
{
    struct watchdog *watchdog = arg;
    struct timespec deadline;

    REQUIRE(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_sec += 30;
    REQUIRE(pthread_mutex_lock(&watchdog->mutex) == 0);

    while (!watchdog->done)
    {
        int status = pthread_cond_timedwait(&watchdog->condition,
                                            &watchdog->mutex, &deadline);

        if (status == ETIMEDOUT)
        {
            fprintf(stderr, "async lifecycle test exceeded 30 seconds\n");
            fflush(stderr);
            _Exit(EXIT_FAILURE);
        }

        REQUIRE(status == 0);
    }

    REQUIRE(pthread_mutex_unlock(&watchdog->mutex) == 0);
    return NULL;
}

static void watchdog_start(struct watchdog *watchdog)
{
    watchdog->done = 0;
    REQUIRE(pthread_mutex_init(&watchdog->mutex, NULL) == 0);
    REQUIRE(pthread_cond_init(&watchdog->condition, NULL) == 0);
    REQUIRE(pthread_create(&watchdog->thread, NULL, watchdog_main,
                           watchdog) == 0);
}

static void watchdog_stop(struct watchdog *watchdog)
{
    REQUIRE(pthread_mutex_lock(&watchdog->mutex) == 0);
    watchdog->done = 1;
    REQUIRE(pthread_cond_signal(&watchdog->condition) == 0);
    REQUIRE(pthread_mutex_unlock(&watchdog->mutex) == 0);
    REQUIRE(pthread_join(watchdog->thread, NULL) == 0);
    REQUIRE(pthread_cond_destroy(&watchdog->condition) == 0);
    REQUIRE(pthread_mutex_destroy(&watchdog->mutex) == 0);
}

static void require_unprepared(const hamlib_port_t *port)
{
#ifdef _WIN32
    REQUIRE(port->sync_data_pipe == NULL);
    REQUIRE(port->sync_data_error_pipe == NULL);
#else
    REQUIRE(port->fd_sync_read == -1);
    REQUIRE(port->fd_sync_write == -1);
    REQUIRE(port->fd_sync_error_read == -1);
    REQUIRE(port->fd_sync_error_write == -1);
#endif
}

#ifndef _WIN32

/* Descriptor exhaustion must never affect the parent test process. */
static void pipe_failure_probe(int available)
{
    struct rlimit original_limit;
    struct rlimit limit;
    hamlib_port_t port;
    int fillers[64];
    int saved_flags[64];
    int replacements[2];
    int guard[2];
    int count = 0;
    int fd;
    int i;
    unsigned char sent = 0x5a;
    unsigned char received = 0;

    REQUIRE(getrlimit(RLIMIT_NOFILE, &original_limit) == 0);
    limit = original_limit;

    if (limit.rlim_cur > 64)
    {
        limit.rlim_cur = 64;
    }

    REQUIRE(limit.rlim_cur >= 16);
    REQUIRE(setrlimit(RLIMIT_NOFILE, &limit) == 0);
    REQUIRE(pipe(guard) == 0);

    while ((fd = open("/dev/null", O_RDWR)) >= 0)
    {
        REQUIRE(count < (int)(sizeof(fillers) / sizeof(fillers[0])));
        fillers[count++] = fd;
    }

    REQUIRE(errno == EMFILE);
    REQUIRE(count >= available);

    for (i = 0; i < available; ++i)
    {
        REQUIRE(close(fillers[--count]) == 0);
    }

    for (i = 0; i < (int)limit.rlim_cur; ++i)
    {
        saved_flags[i] = fcntl(i, F_GETFL);
    }

    memset(&port, 0, sizeof(port));
    port.type.rig = RIG_PORT_NONE;
    port.timeout = 200;
    port.asyncio = 1;
    REQUIRE(port_open(&port) == RIG_OK);
    REQUIRE(!port.asyncio);
    REQUIRE(!port_async_is_prepared(&port));
    require_unprepared(&port);
    REQUIRE(port_prepare_async(&port) == -RIG_EINTERNAL);
    REQUIRE(!port.asyncio);
    REQUIRE(!port_async_is_prepared(&port));
    require_unprepared(&port);
    require_unprepared(&port);

    for (i = 0; i < (int)limit.rlim_cur; ++i)
    {
        REQUIRE(fcntl(i, F_GETFL) == saved_flags[i]);
    }

    REQUIRE(write(guard[1], &sent, 1) == 1);
    REQUIRE(read(guard[0], &received, 1) == 1);
    REQUIRE(received == sent);

    /* A failed second pipe must return both descriptors of the first. */
    for (i = 0; i < available; ++i)
    {
        replacements[i] = open("/dev/null", O_RDWR);
        REQUIRE(replacements[i] >= 0);
    }

    REQUIRE(open("/dev/null", O_RDWR) == -1);
    REQUIRE(errno == EMFILE);

    for (i = 0; i < available; ++i)
    {
        REQUIRE(close(replacements[i]) == 0);
    }

    for (i = 0; i < count; ++i)
    {
        REQUIRE(close(fillers[i]) == 0);
    }

    REQUIRE(setrlimit(RLIMIT_NOFILE, &original_limit) == 0);
    REQUIRE(port_close(&port, port.type.rig) == RIG_OK);
    require_unprepared(&port);
    port.asyncio = 1;
    REQUIRE(port_open(&port) == RIG_OK);
    REQUIRE(port_prepare_async(&port) == RIG_OK);
    REQUIRE(write_block_sync(&port, &sent, 1) == 1);
    port.asyncio = 1;
    REQUIRE(read_block(&port, &received, 1) == 1);
    REQUIRE(received == sent);
    REQUIRE(port_close(&port, port.type.rig) == RIG_OK);
    require_unprepared(&port);
    REQUIRE(close(guard[0]) == 0);
    REQUIRE(close(guard[1]) == 0);
}

static void test_pipe_failure(int available)
{
    struct watchdog watchdog;
    pid_t child = fork();
    pid_t result;
    int status;

    REQUIRE(child >= 0);
    /* Both processes are still single-threaded at fork(). */
    watchdog_start(&watchdog);

    if (child == 0)
    {
        pipe_failure_probe(available);
        watchdog_stop(&watchdog);
        _Exit(EXIT_SUCCESS);
    }

    do
    {
        result = waitpid(child, &status, 0);
    }
    while (result < 0 && errno == EINTR);

    REQUIRE(result == child);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == EXIT_SUCCESS);
    watchdog_stop(&watchdog);
    printf("PASS: pipe failure with %d descriptors available\n", available);
}

#endif


#define TEST_ASYNC_MODEL RIG_MAKE_MODEL(RIG_DUMMY, 999)
#define TEST_MORSE_MODEL RIG_MAKE_MODEL(RIG_DUMMY, 998)
#define TEST_FRAME_BYTE 0x5a

static int backend_opened_in_direct_mode;
static int backend_closed_in_direct_mode;
static int frame_sent;

static int stub_rig_open(RIG *rig)
{
    hamlib_port_t *port = RIGPORT(rig);

    require_unprepared(port);
    backend_opened_in_direct_mode = !port->asyncio
                                    && !port_async_is_prepared(port);
    return backend_opened_in_direct_mode ? RIG_OK : -RIG_EINTERNAL;
}

static int stub_rig_close(RIG *rig)
{
    hamlib_port_t *port = RIGPORT(rig);

    require_unprepared(port);
    backend_closed_in_direct_mode = !port->asyncio
                                    && !port_async_is_prepared(port);
    return RIG_OK;
}

static char thread_error[512];

static int stub_read_frame_direct(RIG *rig, size_t buffer_length,
                                  const unsigned char *buffer)
{
    (void)rig;

    if (!frame_sent && buffer_length > 0)
    {
        /* The callback ABI const-qualifies its output buffer. */
        ((unsigned char *)buffer)[0] = TEST_FRAME_BYTE;
        frame_sent = 1;
        return 1;
    }

    hl_usleep(1000);
    return -RIG_ETIMEOUT;
}

static int stub_is_async_frame(RIG *rig, size_t frame_length,
                               const unsigned char *frame)
{
    (void)rig;
    (void)frame_length;
    (void)frame;
    return 0;
}

static int stub_process_async_frame(RIG *rig, size_t frame_length,
                                    const unsigned char *frame)
{
    (void)rig;
    (void)frame_length;
    (void)frame;
    return RIG_OK;
}

static int stub_send_morse(RIG *rig, vfo_t vfo, const char *message)
{
    (void)rig;
    (void)vfo;
    (void)message;
    return RIG_OK;
}

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

static struct rig_caps morse_caps;

static RIG *new_test_rig(rig_model_t model, int async_enabled)
{
    RIG *rig = rig_init(model);

    REQUIRE(rig != NULL);
    STATE(rig)->async_data_enabled = async_enabled;
    STATE(rig)->poll_interval = 0;
    return rig;
}

static int capture_thread_error(enum rig_debug_level_e level, rig_ptr_t arg,
                                const char *format, va_list args)
{
    (void)level;
    (void)arg;

    if (strstr(format, "pthread_create error") != NULL)
    {
        vsnprintf(thread_error, sizeof(thread_error), format, args);
    }

    return RIG_OK;
}

static int fail_thread_start(pthread_t *thread, void *(*routine)(void *),
                             void *arg)
{
    (void)thread;
    (void)routine;
    (void)arg;
    errno = 0;
    return EAGAIN;
}

static void start_error_capture(void)
{
    thread_error[0] = '\0';
    rig_set_debug(RIG_DEBUG_ERR);
    rig_set_debug_callback(capture_thread_error, NULL);
}

static void check_thread_error(void)
{
    rig_set_debug_callback(NULL, NULL);
    rig_set_debug(RIG_DEBUG_NONE);
    REQUIRE(strstr(thread_error, strerror(EAGAIN)) != NULL);
}

static void test_failed_reader_start_rolls_back_state(void)
{
    RIG *rig = new_test_rig(TEST_ASYNC_MODEL, 1);
    int attempt;

    RIGPORT(rig)->asyncio = 1;
    REQUIRE(port_open(RIGPORT(rig)) == RIG_OK);

    for (attempt = 0; attempt < 2; ++attempt)
    {
        start_error_capture();
        REQUIRE(rig_async_data_handler_start(rig, fail_thread_start)
                == -RIG_EINTERNAL);
        check_thread_error();
        REQUIRE(!RIGPORT(rig)->asyncio);
        REQUIRE(!port_async_is_prepared(RIGPORT(rig)));
        require_unprepared(RIGPORT(rig));
        REQUIRE(!STATE(rig)->async_data_handler_thread_run);
        REQUIRE(STATE(rig)->async_data_handler_priv_data == NULL);
    }

    REQUIRE(port_close(RIGPORT(rig), RIGPORT(rig)->type.rig) == RIG_OK);
    frame_sent = 0;
    REQUIRE(rig_open(rig) == RIG_OK);
    REQUIRE(STATE(rig)->async_data_handler_thread_run);
    REQUIRE(STATE(rig)->async_data_handler_priv_data != NULL);
    REQUIRE(rig_close(rig) == RIG_OK);
    REQUIRE(!STATE(rig)->async_data_handler_thread_run);
    REQUIRE(STATE(rig)->async_data_handler_priv_data == NULL);
    REQUIRE(rig_cleanup(rig) == RIG_OK);
}

static void test_failed_morse_start_rolls_back_state(void)
{
    RIG *rig = new_test_rig(TEST_MORSE_MODEL, 0);
    int attempt;

    for (attempt = 0; attempt < 2; ++attempt)
    {
        start_error_capture();
        REQUIRE(rig_morse_data_handler_start(rig, fail_thread_start)
                == -RIG_EINTERNAL);
        check_thread_error();
        REQUIRE(!STATE(rig)->morse_data_handler_thread_run);
        REQUIRE(STATE(rig)->morse_data_handler_priv_data == NULL);
    }

    REQUIRE(rig_open(rig) == RIG_OK);
    REQUIRE(STATE(rig)->morse_data_handler_thread_run);
    REQUIRE(STATE(rig)->morse_data_handler_priv_data != NULL);
    REQUIRE(rig_close(rig) == RIG_OK);
    REQUIRE(!STATE(rig)->morse_data_handler_thread_run);
    REQUIRE(STATE(rig)->morse_data_handler_priv_data == NULL);
    REQUIRE(rig_cleanup(rig) == RIG_OK);
}


static void test_sync_pipe_is_separate_from_active_routing(void)
{
    hamlib_port_t port;
    unsigned char sent = TEST_FRAME_BYTE;
    unsigned char received = 0;
    unsigned char error = (unsigned char) - RIG_EIO;

    memset(&port, 0, sizeof(port));
    port.type.rig = RIG_PORT_NONE;
    port.timeout = 200;
    port.asyncio = 1;

    REQUIRE(port_open(&port) == RIG_OK);
    REQUIRE(!port.asyncio);
    REQUIRE(!port_async_is_prepared(&port));
    require_unprepared(&port);
    REQUIRE(port_prepare_async(&port) == RIG_OK);
    REQUIRE(!port.asyncio);
    REQUIRE(port_async_is_prepared(&port));

    REQUIRE(write_block_sync(&port, &sent, 1) == 1);
    port.asyncio = 1;
    REQUIRE(read_block(&port, &received, 1) == 1);
    REQUIRE(received == sent);
    port.asyncio = 0;
    REQUIRE(write_block_sync_error(&port, &error, 1) == 1);

    port_cleanup_async(&port);
    REQUIRE(!port.asyncio);
    REQUIRE(!port_async_is_prepared(&port));
    require_unprepared(&port);
    port_cleanup_async(&port);
    REQUIRE(port_close(&port, port.type.rig) == RIG_OK);
}

static void test_reader_owns_port_only_while_running(void)
{
    RIG *rig = new_test_rig(TEST_ASYNC_MODEL, 1);
    unsigned char received = 0;

    frame_sent = 0;
    backend_opened_in_direct_mode = 0;
    REQUIRE(rig_open(rig) == RIG_OK);
    REQUIRE(backend_opened_in_direct_mode);
    REQUIRE(STATE(rig)->async_data_enabled);
    REQUIRE(RIGPORT(rig)->asyncio);
    REQUIRE(port_async_is_prepared(RIGPORT(rig)));
    REQUIRE(STATE(rig)->async_data_handler_thread_run);
    REQUIRE(STATE(rig)->async_data_handler_priv_data != NULL);
    REQUIRE(read_block(RIGPORT(rig), &received, 1) == 1);
    REQUIRE(received == TEST_FRAME_BYTE);

    backend_closed_in_direct_mode = 0;
    REQUIRE(rig_close(rig) == RIG_OK);
    REQUIRE(backend_closed_in_direct_mode);
    REQUIRE(STATE(rig)->async_data_enabled);
    REQUIRE(!RIGPORT(rig)->asyncio);
    REQUIRE(!port_async_is_prepared(RIGPORT(rig)));
    require_unprepared(RIGPORT(rig));
    REQUIRE(!STATE(rig)->async_data_handler_thread_run);
    REQUIRE(STATE(rig)->async_data_handler_priv_data == NULL);
    REQUIRE(rig_cleanup(rig) == RIG_OK);
}

static void test_disabled_async_never_prepares_routing(void)
{
    RIG *rig = new_test_rig(TEST_ASYNC_MODEL, 0);

    backend_opened_in_direct_mode = 0;
    REQUIRE(rig_open(rig) == RIG_OK);
    REQUIRE(backend_opened_in_direct_mode);
    REQUIRE(!RIGPORT(rig)->asyncio);
    REQUIRE(!port_async_is_prepared(RIGPORT(rig)));
    require_unprepared(RIGPORT(rig));
    REQUIRE(!STATE(rig)->async_data_handler_thread_run);
    REQUIRE(STATE(rig)->async_data_handler_priv_data == NULL);

    backend_closed_in_direct_mode = 0;
    REQUIRE(rig_close(rig) == RIG_OK);
    REQUIRE(backend_closed_in_direct_mode);
    REQUIRE(rig_cleanup(rig) == RIG_OK);
}

static void test_skip_init_never_prepares_routing(void)
{
    RIG *rig = new_test_rig(TEST_ASYNC_MODEL, 1);
    int status;

    backend_opened_in_direct_mode = 0;
    skip_init = 1;
    status = rig_open(rig);
    skip_init = 0;

    REQUIRE(status == RIG_OK);
    REQUIRE(backend_opened_in_direct_mode);
    REQUIRE(STATE(rig)->async_data_enabled);
    REQUIRE(!RIGPORT(rig)->asyncio);
    REQUIRE(!port_async_is_prepared(RIGPORT(rig)));
    require_unprepared(RIGPORT(rig));
    REQUIRE(!STATE(rig)->async_data_handler_thread_run);
    REQUIRE(STATE(rig)->async_data_handler_priv_data == NULL);

    backend_closed_in_direct_mode = 0;
    REQUIRE(rig_close(rig) == RIG_OK);
    REQUIRE(backend_closed_in_direct_mode);
    REQUIRE(rig_cleanup(rig) == RIG_OK);
}

static void run_test(const char *name, void (*test)(void))
{
    struct watchdog watchdog;

    watchdog_start(&watchdog);
    test();
    watchdog_stop(&watchdog);
    printf("PASS: %s\n", name);
}

int main(void)
{
    rig_set_debug(RIG_DEBUG_NONE);
#ifndef _WIN32
    test_pipe_failure(0);
    test_pipe_failure(2);
#else
    puts("SKIP: POSIX descriptor-limit pipe failure probes");
#endif

    skip_init = 0;
    REQUIRE(rig_register(&async_caps) == RIG_OK);
    morse_caps = async_caps;
    morse_caps.rig_model = TEST_MORSE_MODEL;
    morse_caps.model_name = "Morse startup test";
    morse_caps.async_data_supported = 0;
    morse_caps.send_morse = stub_send_morse;
    REQUIRE(rig_register(&morse_caps) == RIG_OK);

    run_test("prepared pipes are independent of routing",
             test_sync_pipe_is_separate_from_active_routing);
    run_test("reader owns routing only while running",
             test_reader_owns_port_only_while_running);
    run_test("disabled async never prepares routing",
             test_disabled_async_never_prepares_routing);
    run_test("skip_init never prepares routing",
             test_skip_init_never_prepares_routing);
    run_test("reader startup rollback and retry",
             test_failed_reader_start_rolls_back_state);
    run_test("Morse startup rollback and retry",
             test_failed_morse_start_rolls_back_state);

    REQUIRE(rig_unregister(TEST_ASYNC_MODEL) == RIG_OK);
    REQUIRE(rig_unregister(TEST_MORSE_MODEL) == RIG_OK);
    return EXIT_SUCCESS;
}
