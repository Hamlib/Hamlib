/*
 * Hamlib asynchronous port lifecycle tests.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hamlib/config.h"

#include <errno.h>
#include <pthread.h>
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

#ifndef _WIN32

static void require_unprepared(const hamlib_port_t *port)
{
    REQUIRE(port->fd_sync_read == -1);
    REQUIRE(port->fd_sync_write == -1);
    REQUIRE(port->fd_sync_error_read == -1);
    REQUIRE(port->fd_sync_error_write == -1);
}

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
    REQUIRE(port_open(&port) == -RIG_EINTERNAL);
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
    REQUIRE(write_block_sync(&port, &sent, 1) == 1);
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

int main(void)
{
    rig_set_debug(RIG_DEBUG_NONE);
#ifndef _WIN32
    test_pipe_failure(0);
    test_pipe_failure(2);
#else
    struct watchdog watchdog;
    watchdog_start(&watchdog);
    puts("SKIP: POSIX descriptor-limit pipe failure probes");
    watchdog_stop(&watchdog);
#endif
    return EXIT_SUCCESS;
}
