#include "hamlib/config.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

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

int main(void)
{
    test_parser();
    test_exact_limit_and_release();
    test_concurrent_limit();

    return failures ? 1 : 0;
}
