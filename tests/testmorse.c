/*
 * Hamlib Morse queue tests, run without a radio.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hamlib/config.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "fifo.h"
#include "misc.h"
#include "rig_internal.h"

#define TEST_MORSE_MODEL RIG_MAKE_MODEL(RIG_DUMMY, 998)

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

struct test_deadline
{
    const char *name;
    int done;
};

static void *watchdog(void *arg)
{
    struct test_deadline *deadline = arg;

    for (int i = 0; i < 150; ++i)
    {
        if (__atomic_load_n(&deadline->done, __ATOMIC_ACQUIRE))
        {
            return NULL;
        }

        hl_usleep(100 * 1000);
    }

    fprintf(stderr, "FAIL: %s exceeded its 15 s deadline\n", deadline->name);
    exit(EXIT_FAILURE);
}

static void run_test(const char *name, void (*test)(void))
{
    struct test_deadline deadline = { name, 0 };
    pthread_t thread;

    printf("%s\n", name);
    fflush(stdout);
    CHECK(pthread_create(&thread, NULL, watchdog, &deadline) == 0);
    test();
    __atomic_store_n(&deadline.done, 1, __ATOMIC_RELEASE);
    CHECK(pthread_join(thread, NULL) == 0);
}

static pthread_mutex_t sent_lock = PTHREAD_MUTEX_INITIALIZER;
static char sent[2 * HAMLIB_FIFO_SIZE + 1];
static int sent_invalid;

static int stub_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    (void)rig;
    (void)vfo;
    *mode = RIG_MODE_CW;
    *width = 500;
    return RIG_OK;
}

static int stub_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    size_t length = strlen(msg);
    size_t used;

    (void)rig;
    (void)vfo;
    CHECK(pthread_mutex_lock(&sent_lock) == 0);
    used = strlen(sent);
    CHECK(length < sizeof(sent) - used);
    memcpy(sent + used, msg, length + 1);
    sent_invalid |= memchr(msg, 0xff, length) != NULL;
    CHECK(pthread_mutex_unlock(&sent_lock) == 0);
    return RIG_OK;
}

static int stub_stop_morse(RIG *rig, vfo_t vfo)
{
    (void)rig;
    (void)vfo;
    return RIG_OK;
}

static struct rig_caps morse_caps =
{
    .rig_model = TEST_MORSE_MODEL,
    .model_name = "Morse queue test",
    .mfg_name = "Hamlib",
    .version = "1",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .port_type = RIG_PORT_NONE,
    .timeout = 200,
    .retry = 0,
    .get_mode = stub_get_mode,
    .send_morse = stub_send_morse,
    .stop_morse = stub_stop_morse,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS,
};

static RIG *open_test_rig(void)
{
    RIG *rig;

    CHECK(pthread_mutex_lock(&sent_lock) == 0);
    sent[0] = '\0';
    sent_invalid = 0;
    CHECK(pthread_mutex_unlock(&sent_lock) == 0);
    rig = rig_init(TEST_MORSE_MODEL);
    CHECK(rig != NULL);
    STATE(rig)->poll_interval = 0;
    CHECK(rig_open(rig) == RIG_OK);
    return rig;
}

static int sent_equals(const char *text)
{
    int equal;

    CHECK(pthread_mutex_lock(&sent_lock) == 0);
    equal = !sent_invalid && strcmp(sent, text) == 0;
    CHECK(pthread_mutex_unlock(&sent_lock) == 0);
    return equal;
}

static void wait_until_sent(const char *text)
{
    for (int i = 0; i < 300 && !sent_equals(text); ++i)
    {
        hl_usleep(10 * 1000);
    }

    CHECK(sent_equals(text));
}

static struct
{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    void *(*routine)(void *);
    void *arg;
    int entered;
    int released;
} worker_gate =
{
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_COND_INITIALIZER,
    NULL,
    NULL,
    0,
    0
};

static void *held_trampoline(void *arg)
{
    void *(*routine)(void *);
    void *routine_arg;

    (void)arg;
    CHECK(pthread_mutex_lock(&worker_gate.mutex) == 0);
    worker_gate.entered = 1;
    CHECK(pthread_cond_broadcast(&worker_gate.condition) == 0);

    while (!worker_gate.released)
    {
        CHECK(pthread_cond_wait(&worker_gate.condition,
                                &worker_gate.mutex) == 0);
    }

    routine = worker_gate.routine;
    routine_arg = worker_gate.arg;
    CHECK(pthread_mutex_unlock(&worker_gate.mutex) == 0);
    return routine(routine_arg);
}

static int held_thread_start(pthread_t *thread, void *(*routine)(void *),
                             void *arg)
{
    CHECK(pthread_mutex_lock(&worker_gate.mutex) == 0);
    worker_gate.routine = routine;
    worker_gate.arg = arg;
    worker_gate.entered = 0;
    worker_gate.released = 0;
    CHECK(pthread_mutex_unlock(&worker_gate.mutex) == 0);
    return pthread_create(thread, NULL, held_trampoline, NULL);
}

static RIG *open_held_rig(void)
{
    RIG *rig;

    skip_init = 1;
    rig = open_test_rig();
    skip_init = 0;
    CHECK(rig_morse_data_handler_start(rig, held_thread_start) == RIG_OK);
    CHECK(pthread_mutex_lock(&worker_gate.mutex) == 0);

    while (!worker_gate.entered)
    {
        CHECK(pthread_cond_wait(&worker_gate.condition,
                                &worker_gate.mutex) == 0);
    }

    CHECK(pthread_mutex_unlock(&worker_gate.mutex) == 0);
    CHECK(STATE(rig)->fifo_morse != NULL);
    return rig;
}

static void release_worker(void)
{
    CHECK(pthread_mutex_lock(&worker_gate.mutex) == 0);
    worker_gate.released = 1;
    CHECK(pthread_cond_broadcast(&worker_gate.condition) == 0);
    CHECK(pthread_mutex_unlock(&worker_gate.mutex) == 0);
}

static void test_queue_ready_before_worker(void)
{
    RIG *rig = open_held_rig();

    CHECK(rig_send_morse(rig, RIG_VFO_CURR, "E") == RIG_OK);
    CHECK(sent_equals(""));
    release_worker();
    wait_until_sent("E");
    CHECK(rig_close(rig) == RIG_OK);
    CHECK(rig_cleanup(rig) == RIG_OK);
}

static void check_fifo_text(FIFO_RIG *fifo, const char *text)
{
    for (size_t i = 0; text[i] != '\0'; ++i)
    {
        CHECK(hl_peek(fifo) == (unsigned char)text[i]);
        CHECK(hl_pop(fifo) == (unsigned char)text[i]);
    }

    CHECK(hl_peek(fifo) == -1);
    CHECK(hl_pop(fifo) == -1);
}

static void test_exact_capacity(void)
{
    FIFO_RIG fifo = { 0 };
    char full[HAMLIB_FIFO_SIZE];

    memset(full, 'E', sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';
    initFIFO(&fifo);
    CHECK(hl_push(&fifo, full) == RIG_OK);
    CHECK(hl_push(&fifo, "T") == -RIG_EDOM);
    check_fifo_text(&fifo, full);
    CHECK(hl_push(&fifo, "T") == RIG_OK);
    check_fifo_text(&fifo, "T");
    CHECK(pthread_mutex_destroy(&fifo.mutex) == 0);
}

static void test_filtered_capacity(void)
{
    FIFO_RIG fifo = { 0 };
    char full[HAMLIB_FIFO_SIZE];

    memset(full, 'E', sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';
    initFIFO(&fifo);
    CHECK(hl_push(&fifo, "\rE\n\x80\xffT\r\n") == RIG_OK);
    check_fifo_text(&fifo, "ET");
    CHECK(hl_push(&fifo, full) == RIG_OK);
    CHECK(hl_push(&fifo, "\r\n\x80\xff") == RIG_OK);
    CHECK(hl_push(&fifo, "\r\nT\x80") == -RIG_EDOM);
    check_fifo_text(&fifo, full);
    CHECK(pthread_mutex_destroy(&fifo.mutex) == 0);
}

static void test_wraparound_rejection_preserves_text(void)
{
    FIFO_RIG fifo = { 0 };
    char padding[HAMLIB_FIFO_SIZE - 3];
    char too_long[HAMLIB_FIFO_SIZE];

    memset(padding, 'E', sizeof(padding) - 1);
    padding[sizeof(padding) - 1] = '\0';
    memset(too_long, 'T', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    initFIFO(&fifo);
    CHECK(hl_push(&fifo, padding) == RIG_OK);
    check_fifo_text(&fifo, padding);
    CHECK(hl_push(&fifo, "ABCDEF") == RIG_OK);
    CHECK(fifo.tail < fifo.head);
    CHECK(hl_push(&fifo, too_long) == -RIG_EDOM);
    CHECK(hl_push(&fifo, "G") == RIG_OK);
    check_fifo_text(&fifo, "ABCDEFG");
    CHECK(pthread_mutex_destroy(&fifo.mutex) == 0);
}

static void test_oversized_fresh_queue(void)
{
    char too_long[HAMLIB_FIFO_SIZE + 1];
    RIG *rig = open_held_rig();

    memset(too_long, 'E', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    CHECK(rig_send_morse(rig, RIG_VFO_CURR, too_long) == -RIG_EDOM);
    CHECK(hl_peek(STATE(rig)->fifo_morse) == -1);
    release_worker();
    CHECK(rig_close(rig) == RIG_OK);
    CHECK(sent_equals(""));
    CHECK(rig_cleanup(rig) == RIG_OK);
}

static void test_queue_recovers_after_overflow(void)
{
    char too_long[HAMLIB_FIFO_SIZE + 1];
    RIG *rig = open_test_rig();

    CHECK(rig_send_morse(rig, RIG_VFO_CURR, "E") == RIG_OK);
    wait_until_sent("E");
    memset(too_long, 'E', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    CHECK(rig_send_morse(rig, RIG_VFO_CURR, too_long) == -RIG_EDOM);
    CHECK(rig_send_morse(rig, RIG_VFO_CURR, "T") == RIG_OK);
    wait_until_sent("ET");
    CHECK(rig_close(rig) == RIG_OK);
    CHECK(sent_equals("ET"));
    CHECK(rig_cleanup(rig) == RIG_OK);
}

static void test_reset_keeps_empty_sentinels(void)
{
    FIFO_RIG fifo = { 0 };

    initFIFO(&fifo);
    CHECK(hl_push(&fifo, "ET") == RIG_OK);
    resetFIFO(&fifo);
    CHECK(fifo.flush == 1);
    CHECK(hl_peek(&fifo) == -1);
    CHECK(hl_pop(&fifo) == -1);
    CHECK(hl_pop(&fifo) == -1);
    CHECK(fifo.head == fifo.tail);
    CHECK(hl_push(&fifo, "A") == RIG_OK);
    check_fifo_text(&fifo, "A");
    CHECK(pthread_mutex_destroy(&fifo.mutex) == 0);
}

static void test_reset_before_dequeue_sends_nothing(void)
{
    RIG *rig = open_test_rig();
    FIFO_RIG *fifo = STATE(rig)->fifo_morse;

    CHECK(pthread_mutex_lock(&fifo->mutex) == 0);
    fifo->data[fifo->tail] = 'E';
    fifo->tail = (fifo->tail + 1) % HAMLIB_FIFO_SIZE;
    /* Give the worker a chance to wait on the queue; this does not force
     * every possible interleaving between its peek and pop. */
    hl_usleep(300 * 1000);
    fifo->head = fifo->tail;
    CHECK(pthread_mutex_unlock(&fifo->mutex) == 0);
    CHECK(rig_close(rig) == RIG_OK);
    CHECK(sent_equals(""));
    CHECK(rig_cleanup(rig) == RIG_OK);
}

struct stop_request
{
    RIG *rig;
    int entered;
    int done;
    int result;
};

static void *stop_morse(void *arg)
{
    struct stop_request *request = arg;

    __atomic_store_n(&request->entered, 1, __ATOMIC_RELEASE);
    request->result = rig_stop_morse(request->rig, RIG_VFO_CURR);
    __atomic_store_n(&request->done, 1, __ATOMIC_RELEASE);
    return NULL;
}

static void test_stop_waits_for_queue_mutex(void)
{
    RIG *rig = open_held_rig();
    FIFO_RIG *fifo = STATE(rig)->fifo_morse;
    struct stop_request request = { rig, 0, 0, -RIG_EINTERNAL };
    pthread_t thread;

    CHECK(rig_send_morse(rig, RIG_VFO_CURR, "E") == RIG_OK);
    CHECK(pthread_mutex_lock(&fifo->mutex) == 0);
    CHECK(pthread_create(&thread, NULL, stop_morse, &request) == 0);

    for (int i = 0;
            i < 300 && !__atomic_load_n(&request.entered, __ATOMIC_ACQUIRE);
            ++i)
    {
        hl_usleep(10 * 1000);
    }

    CHECK(__atomic_load_n(&request.entered, __ATOMIC_ACQUIRE));
    hl_usleep(300 * 1000);
    CHECK(!__atomic_load_n(&request.done, __ATOMIC_ACQUIRE));
    CHECK(fifo->head != fifo->tail);
    CHECK(pthread_mutex_unlock(&fifo->mutex) == 0);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(request.result == RIG_OK);
    CHECK(hl_peek(fifo) == -1);
    CHECK(hl_pop(fifo) == -1);
    release_worker();
    CHECK(rig_close(rig) == RIG_OK);
    CHECK(sent_equals(""));
    CHECK(rig_cleanup(rig) == RIG_OK);
}

int main(void)
{
    rig_set_debug(RIG_DEBUG_NONE);
    CHECK(rig_register(&morse_caps) == RIG_OK);
    run_test("queue ready before worker", test_queue_ready_before_worker);
    run_test("exact usable capacity", test_exact_capacity);
    run_test("filtered capacity", test_filtered_capacity);
    run_test("wraparound rejection preserves text",
             test_wraparound_rejection_preserves_text);
    run_test("oversized fresh queue", test_oversized_fresh_queue);
    run_test("queue recovers after overflow", test_queue_recovers_after_overflow);
    run_test("reset keeps empty sentinels", test_reset_keeps_empty_sentinels);
    run_test("reset before dequeue sends nothing",
             test_reset_before_dequeue_sends_nothing);
    run_test("stop waits for queue mutex", test_stop_waits_for_queue_mutex);
    CHECK(rig_unregister(TEST_MORSE_MODEL) == RIG_OK);
    puts("Morse queue tests passed");
    return EXIT_SUCCESS;
}
