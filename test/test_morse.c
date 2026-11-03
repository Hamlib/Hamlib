/*
 * Hamlib Morse queue tests, run without a radio.
 *
 * rig_send_morse() only queues the text; a thread takes it from the queue
 * and hands it to the backend.  The queue exists before the thread runs, so
 * text queued right after rig_open() reaches the backend too.  A text that
 * does not fit in the queue is refused whole, and the queue keeps working.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"

#include <pthread.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "rig_internal.h"
#include "fifo.h"

/* A private model number avoids loading or modifying a production backend. */
#define TEST_MORSE_MODEL RIG_MAKE_MODEL(RIG_DUMMY, 998)

static pthread_mutex_t sent_lock = PTHREAD_MUTEX_INITIALIZER;
static char sent[256];

static int stub_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    (void) rig;
    (void) vfo;
    *mode = RIG_MODE_CW;
    *width = 500;
    return RIG_OK;
}

/* Runs on the Morse thread: records what the backend was asked to send. */
static int stub_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    (void) rig;
    (void) vfo;
    pthread_mutex_lock(&sent_lock);
    strncat(sent, msg, sizeof(sent) - strlen(sent) - 1);
    pthread_mutex_unlock(&sent_lock);
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
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS,
};

static RIG *open_test_rig(void)
{
    static int registered;
    RIG *rig;

    if (!registered)
    {
        if (rig_register(&morse_caps) != RIG_OK)
        {
            return NULL;
        }

        registered = 1;
    }

    pthread_mutex_lock(&sent_lock);
    sent[0] = '\0';
    pthread_mutex_unlock(&sent_lock);

    rig = rig_init(TEST_MORSE_MODEL);

    if (rig == NULL)
    {
        return NULL;
    }

    /* Polling would add an unrelated background thread to this fixture. */
    STATE(rig)->poll_interval = 0;

    if (rig_open(rig) != RIG_OK)
    {
        rig_cleanup(rig);
        return NULL;
    }

    return rig;
}

static int sent_equals(const char *text)
{
    int equal;

    pthread_mutex_lock(&sent_lock);
    equal = strcmp(sent, text) == 0;
    pthread_mutex_unlock(&sent_lock);
    return equal;
}

/* The thread takes the queue every 100 ms; 3 s is far beyond that. */
static void wait_until_sent(const char *text)
{
    int i;

    for (i = 0; i < 300 && !sent_equals(text); i++)
    {
        hl_usleep(10 * 1000);
    }
}

static void *(*late_routine)(void *);

static void *late_trampoline(void *arg)
{
    hl_usleep(100 * 1000);
    return late_routine(arg);
}

/* Runs the Morse thread 100 ms late, as a busy system may schedule a new
 * thread only after rig_open() has returned. */
static int late_thread_start(pthread_t *thread, void *(*routine)(void *),
                             void *arg)
{
    late_routine = routine;
    return pthread_create(thread, NULL, late_trampoline, arg);
}

static void test_text_queued_before_the_thread_runs_is_sent(void)
{
    RIG *rig;

    /* skip_init: rig_open() starts no thread; the test starts it late. */
    skip_init = 1;
    rig = open_test_rig();
    skip_init = 0;

    TEST_ASSERT(rig != NULL);
    TEST_ASSERT(rig_morse_data_handler_start(rig, late_thread_start) == RIG_OK);
    /* rig_send_morse() would dereference a missing queue. */
    TEST_ASSERT_(STATE(rig)->fifo_morse != NULL,
                 "the Morse queue exists before its thread runs");
    TEST_ASSERT(rig_send_morse(rig, RIG_VFO_CURR, "E") == RIG_OK);
    wait_until_sent("E");
    TEST_CHECK(sent_equals("E"));
    TEST_MSG("sent: \"%s\"", sent);
    TEST_CHECK(rig_close(rig) == RIG_OK);
    rig_cleanup(rig);
}

static void test_text_longer_than_the_queue_is_refused(void)
{
    static char too_long[HAMLIB_FIFO_SIZE + 1];
    RIG *rig = open_test_rig();

    TEST_ASSERT(rig != NULL);
    memset(too_long, 'E', HAMLIB_FIFO_SIZE);
    TEST_CHECK(rig_send_morse(rig, RIG_VFO_CURR, too_long) == -RIG_EDOM);
    TEST_CHECK(rig_close(rig) == RIG_OK);
    TEST_CHECK(sent_equals(""));
    TEST_MSG("sent: \"%.20s\"", sent);
    rig_cleanup(rig);
}

struct after_refused_text
{
    RIG *rig;
    int send_rc;
    int close_rc;
    int done;
};

static void *send_and_close(void *arg)
{
    struct after_refused_text *run = arg;

    run->send_rc = rig_send_morse(run->rig, RIG_VFO_CURR, "T");
    wait_until_sent("ET");
    run->close_rc = rig_close(run->rig);
    __atomic_store_n(&run->done, 1, __ATOMIC_RELEASE);
    return NULL;
}

static void test_queue_works_after_a_refused_text(void)
{
    static char too_long[HAMLIB_FIFO_SIZE + 1];
    struct after_refused_text run = { 0 };
    pthread_t thread;
    RIG *rig = open_test_rig();
    int i;

    TEST_ASSERT(rig != NULL);
    /* A sent text moves the queue's read position off its first slot. */
    TEST_ASSERT(rig_send_morse(rig, RIG_VFO_CURR, "E") == RIG_OK);
    wait_until_sent("E");
    TEST_ASSERT(sent_equals("E"));
    memset(too_long, 'E', HAMLIB_FIFO_SIZE);
    TEST_CHECK(rig_send_morse(rig, RIG_VFO_CURR, too_long) == -RIG_EDOM);

    /* A queue left locked blocks both calls for good, hence the deadline. */
    run.rig = rig;
    TEST_ASSERT(pthread_create(&thread, NULL, send_and_close, &run) == 0);

    for (i = 0; i < 1000 && !__atomic_load_n(&run.done, __ATOMIC_ACQUIRE); i++)
    {
        hl_usleep(10 * 1000);
    }

    TEST_ASSERT_(__atomic_load_n(&run.done, __ATOMIC_ACQUIRE),
                 "sending and closing finish within 10 s");
    pthread_join(thread, NULL);
    TEST_CHECK(run.send_rc == RIG_OK);
    TEST_CHECK(sent_equals("ET"));
    TEST_MSG("sent: \"%.20s\"", sent);
    TEST_CHECK(run.close_rc == RIG_OK);
    rig_cleanup(rig);
}

TEST_LIST =
{
    { "text_queued_before_the_thread_runs_is_sent",
      test_text_queued_before_the_thread_runs_is_sent },
    { "text_longer_than_the_queue_is_refused",
      test_text_longer_than_the_queue_is_refused },
    { "queue_works_after_a_refused_text", test_queue_works_after_a_refused_text },
    { NULL, NULL }
};
