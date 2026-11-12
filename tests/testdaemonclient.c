#include "hamlib/config.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#ifndef __MINGW32__
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "daemon_client.h"

#define THREAD_COUNT 64
#define CONCURRENT_LIMIT 7

struct concurrent_test
{
    struct daemon_client_tracker tracker;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    unsigned ready;
    unsigned attempted;
    unsigned admitted;
    int start;
    int release;
};

static int failures;

static void check(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void check_parse(const char *value, int expected, unsigned expected_limit)
{
    unsigned limit = 0;
    int result = daemon_client_limit_parse(value, &limit);

    check((result == 0) == expected, value ? value : "NULL limit");

    if (expected && result == 0)
    {
        check(limit == expected_limit, "parsed limit differs from expected value");
    }
}

static void *concurrent_acquire(void *arg)
{
    struct concurrent_test *test = arg;
    int acquired;

    pthread_mutex_lock(&test->mutex);
    ++test->ready;
    pthread_cond_broadcast(&test->condition);

    while (!test->start)
    {
        pthread_cond_wait(&test->condition, &test->mutex);
    }

    pthread_mutex_unlock(&test->mutex);
    acquired = daemon_client_tracker_acquire(&test->tracker);
    pthread_mutex_lock(&test->mutex);
    ++test->attempted;

    if (acquired)
    {
        ++test->admitted;
    }

    pthread_cond_broadcast(&test->condition);

    while (acquired && !test->release)
    {
        pthread_cond_wait(&test->condition, &test->mutex);
    }

    pthread_mutex_unlock(&test->mutex);

    if (acquired)
    {
        daemon_client_tracker_release(&test->tracker);
    }

    return NULL;
}

static void test_parser(void)
{
    struct daemon_client_tracker tracker = DAEMON_CLIENT_TRACKER_INITIALIZER;
    char max_value[32];

    check(tracker.limit == 32, "default client limit is not 32");
    snprintf(max_value, sizeof(max_value), "%u", UINT_MAX);
    check_parse("1", 1, 1);
    check_parse("00032", 1, 32);
    check_parse(max_value, 1, UINT_MAX);
    check_parse(NULL, 0, 0);
    check_parse("", 0, 0);
    check_parse("0", 0, 0);
    check_parse("-1", 0, 0);
    check_parse("+1", 0, 0);
    check_parse(" 1", 0, 0);
    check_parse("1 ", 0, 0);
    check_parse("1x", 0, 0);
    check_parse("0x10", 0, 0);
    check_parse("999999999999999999999999999999999999", 0, 0);
}

static void test_exact_limit_and_release(void)
{
    struct daemon_client_tracker tracker;

    check(daemon_client_tracker_init(&tracker, 2) == 0,
          "tracker initialization failed");
    check(daemon_client_tracker_acquire(&tracker), "first client was rejected");
    check(daemon_client_tracker_acquire(&tracker), "second client was rejected");
    check(!daemon_client_tracker_acquire(&tracker), "limit was not enforced");
    daemon_client_tracker_release(&tracker);
    check(daemon_client_tracker_acquire(&tracker),
          "released slot was not available for readmission");
    daemon_client_tracker_release(&tracker);
    daemon_client_tracker_release(&tracker);
    daemon_client_tracker_destroy(&tracker);
}

static void test_concurrent_limit(void)
{
    struct concurrent_test test;
    pthread_t threads[THREAD_COUNT];
    unsigned created = 0;
    unsigned i;

    memset(&test, 0, sizeof(test));
    check(daemon_client_tracker_init(&test.tracker, CONCURRENT_LIMIT) == 0,
          "concurrent tracker initialization failed");
    check(pthread_mutex_init(&test.mutex, NULL) == 0,
          "test mutex initialization failed");
    check(pthread_cond_init(&test.condition, NULL) == 0,
          "test condition initialization failed");

    for (i = 0; i < THREAD_COUNT; ++i)
    {
        if (pthread_create(&threads[created], NULL, concurrent_acquire, &test) != 0)
        {
            check(0, "worker creation failed");
            break;
        }

        ++created;
    }

    if (created != THREAD_COUNT)
    {
        pthread_mutex_lock(&test.mutex);
        test.start = 1;
        test.release = 1;
        pthread_cond_broadcast(&test.condition);
        pthread_mutex_unlock(&test.mutex);

        for (i = 0; i < created; ++i)
        {
            pthread_join(threads[i], NULL);
        }

        pthread_cond_destroy(&test.condition);
        pthread_mutex_destroy(&test.mutex);
        daemon_client_tracker_destroy(&test.tracker);
        return;
    }

    pthread_mutex_lock(&test.mutex);

    while (test.ready < THREAD_COUNT)
    {
        pthread_cond_wait(&test.condition, &test.mutex);
    }

    test.start = 1;
    pthread_cond_broadcast(&test.condition);

    while (test.attempted < THREAD_COUNT)
    {
        pthread_cond_wait(&test.condition, &test.mutex);
    }

    check(test.admitted == CONCURRENT_LIMIT,
          "concurrent admission did not enforce the exact limit");
    test.release = 1;
    pthread_cond_broadcast(&test.condition);
    pthread_mutex_unlock(&test.mutex);

    for (i = 0; i < THREAD_COUNT; ++i)
    {
        check(pthread_join(threads[i], NULL) == 0, "worker join failed");
    }

    for (i = 0; i < CONCURRENT_LIMIT; ++i)
    {
        check(daemon_client_tracker_acquire(&test.tracker),
              "concurrent worker leaked an admission slot");
    }

    check(!daemon_client_tracker_acquire(&test.tracker),
          "tracker exceeded limit after concurrent release");

    for (i = 0; i < CONCURRENT_LIMIT; ++i)
    {
        daemon_client_tracker_release(&test.tracker);
    }

    pthread_cond_destroy(&test.condition);
    pthread_mutex_destroy(&test.mutex);
    daemon_client_tracker_destroy(&test.tracker);
}

#ifndef __MINGW32__
/*
 * Closing a stream must close only the stream's own descriptor, leaving the
 * accepted socket open for the handler to close once.
 */
static void test_fdopen_dup(void)
{
    int sockets[2];
    char byte = 0;
    FILE *stream;
    int free_fd;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        check(0, "socketpair");
        return;
    }

    stream = daemon_client_fdopen_dup(sockets[0], "wb");
    check(stream != NULL, "a stream on a duplicate of the socket");

    if (stream != NULL)
    {
        check(fileno(stream) != sockets[0], "the stream has a descriptor of its own");
        check(fputc('a', stream) == 'a' && fflush(stream) == 0, "writing to the stream");
        check(read(sockets[1], &byte, 1) == 1 && byte == 'a',
              "the socket receiving what the stream wrote");
        check(fclose(stream) == 0, "closing the stream");
        check(write(sockets[0], "b", 1) == 1,
              "the socket is still open after the stream is closed");
        check(read(sockets[1], &byte, 1) == 1 && byte == 'b',
              "the socket still works after the stream is closed");
    }

    check(daemon_client_fdopen_dup(-1, "rb") == NULL, "a bad descriptor");

    /* A stream that cannot be opened must not leave its duplicate open */
    free_fd = dup(sockets[0]);
    close(free_fd);
    stream = daemon_client_fdopen_dup(sockets[0], "");

    if (stream == NULL)
    {
        int next_fd = dup(sockets[0]);

        check(next_fd == free_fd, "a failed open closing its duplicate");
        close(next_fd);
    }
    else
    {
        fclose(stream);
    }

    close(sockets[0]);
    close(sockets[1]);
}
#endif

int main(void)
{
    test_parser();
    test_exact_limit_and_release();
    test_concurrent_limit();
#ifndef __MINGW32__
    test_fdopen_dup();
#endif

    return failures ? 1 : 0;
}
