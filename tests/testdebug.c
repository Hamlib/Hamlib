/*
 * Test debug history behavior.
 *
 * Copyright (C) 2026 The Hamlib Group
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hamlib/rig.h>

#define THREAD_COUNT 8
#define PADDING_SIZE 1536
#define MESSAGE_SIZE 1664
#define HISTORY_LINE_COUNT 25
#define RETAINED_LINE_COUNT 20

struct start_gate
{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int open;
};

struct worker_context
{
    struct start_gate *gate;
    unsigned int thread;
};

static int ignore_debug_output(enum rig_debug_level_e debug_level,
                               rig_ptr_t arg, const char *fmt, va_list ap)
{
    (void)debug_level;
    (void)arg;
    (void)fmt;
    (void)ap;
    return RIG_OK;
}

static int build_message(char *message, size_t size, unsigned int thread)
{
    char padding[PADDING_SIZE + 1];
    int length;

    memset(padding, (int)('A' + thread), PADDING_SIZE);
    padding[PADDING_SIZE] = '\0';
    length = snprintf(message, size,
                      "concurrent history thread=%u padding=%s marker=%u\n",
                      thread, padding, thread);

    return length >= 0 && (size_t)length < size;
}

static void *worker(void *arg)
{
    struct worker_context *context = arg;
    char message[MESSAGE_SIZE];

    if (!build_message(message, sizeof(message), context->thread))
    {
        abort();
    }

    pthread_mutex_lock(&context->gate->mutex);
    while (!context->gate->open)
    {
        pthread_cond_wait(&context->gate->condition, &context->gate->mutex);
    }
    pthread_mutex_unlock(&context->gate->mutex);

    rig_debug(RIG_DEBUG_TRACE, "%s", message);
    return NULL;
}

static int test_concurrent_history(void)
{
    struct start_gate gate = {
        PTHREAD_MUTEX_INITIALIZER,
        PTHREAD_COND_INITIALIZER,
        0
    };
    struct worker_context contexts[THREAD_COUNT];
    pthread_t threads[THREAD_COUNT];
    size_t expected_length = 0;
    unsigned int thread;

    rig_debug_clear();
    rig_set_debug(RIG_DEBUG_NONE);

    for (thread = 0; thread < THREAD_COUNT; ++thread)
    {
        contexts[thread].gate = &gate;
        contexts[thread].thread = thread;
        if (pthread_create(&threads[thread], NULL, worker,
                           &contexts[thread]) != 0)
        {
            fprintf(stderr, "failed to create worker thread %u\n", thread);
            return 0;
        }
    }

    pthread_mutex_lock(&gate.mutex);
    gate.open = 1;
    pthread_cond_broadcast(&gate.condition);
    pthread_mutex_unlock(&gate.mutex);

    for (thread = 0; thread < THREAD_COUNT; ++thread)
    {
        if (pthread_join(threads[thread], NULL) != 0)
        {
            fprintf(stderr, "failed to join worker thread %u\n", thread);
            return 0;
        }
    }

    pthread_cond_destroy(&gate.condition);
    pthread_mutex_destroy(&gate.mutex);

    for (thread = 0; thread < THREAD_COUNT; ++thread)
    {
        char expected[MESSAGE_SIZE];

        if (!build_message(expected, sizeof(expected), thread))
        {
            return 0;
        }
        expected_length += strlen(expected);
        if (strstr(debugmsgsave, expected) == NULL)
        {
            fprintf(stderr,
                    "history does not contain an intact message "
                    "from thread %u\n",
                    thread);
            return 0;
        }
    }

    if (strlen(debugmsgsave) != expected_length)
    {
        fprintf(stderr, "history retained %zu bytes; expected %zu\n",
                strlen(debugmsgsave), expected_length);
        return 0;
    }

    return 1;
}

static int test_rolling_history(void)
{
    const char *history;
    unsigned int line;

    rig_debug_clear();
    rig_set_debug_callback(ignore_debug_output, NULL);
    rig_set_debug(RIG_DEBUG_TRACE);

    for (line = 0; line < HISTORY_LINE_COUNT; ++line)
    {
        rig_debug(RIG_DEBUG_TRACE, "history line=%u\n", line);
    }

    rig_set_debug_callback(NULL, NULL);
    rig_set_debug(RIG_DEBUG_NONE);

    history = debugmsgsave;
    for (line = HISTORY_LINE_COUNT - RETAINED_LINE_COUNT;
            line < HISTORY_LINE_COUNT; ++line)
    {
        char expected[32];
        int written = snprintf(expected, sizeof(expected),
                               "history line=%u\n", line);

        if (written < 0 || (size_t)written >= sizeof(expected)
                || strncmp(history, expected, (size_t)written) != 0)
        {
            fprintf(stderr, "history did not retain line %u in order\n", line);
            return 0;
        }
        history += written;
    }

    if (*history != '\0')
    {
        fprintf(stderr, "history retained more than %u lines\n",
                RETAINED_LINE_COUNT);
        return 0;
    }

    return 1;
}

int main(void)
{
    if (!test_concurrent_history() || !test_rolling_history())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
