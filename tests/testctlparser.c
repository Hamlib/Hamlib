/*
 * Hamlib command parser boundary tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "hamlib/port.h"

#include "iofunc.h"
#include "daemon_client.h"
#include "rigctl_parse.h"
#include "rigctld_client.h"
#include "rigctld_stream.h"
#include "rig_tests.h"

extern powerstat_t rig_powerstat;
extern int is_rigctld;
static char captured_description[sizeof(((channel_t *)0)->channel_desc)];
static freq_t captured_freq;
static double captured_msec;

static int capture_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    (void)rig;
    (void)vfo;
    captured_freq = freq;
    return RIG_OK;
}

static int capture_split_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    return capture_freq(rig, vfo, freq);
}

static int capture_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq,
                                   rmode_t mode, pbwidth_t width)
{
    (void)mode;
    (void)width;
    return capture_freq(rig, vfo, freq);
}

static int capture_clock(RIG *rig, int year, int month, int day, int hour,
                         int minute, int second, double msec, int utc_offset)
{
    (void)rig;
    (void)year;
    (void)month;
    (void)day;
    (void)hour;
    (void)minute;
    (void)second;
    (void)utc_offset;
    captured_msec = msec;
    return RIG_OK;
}

static int capture_get_clock(RIG *rig, int *year, int *month, int *day,
                             int *hour, int *minute, int *second,
                             double *msec, int *utc_offset)
{
    (void)rig;
    *year = 2026;
    *month = 1;
    *day = 2;
    *hour = 3;
    *minute = 4;
    *second = 5;
    *msec = 500;
    *utc_offset = 0;
    return RIG_OK;
}

static int capture_mw_to_power(RIG *rig, float *power, unsigned int mwpower,
                               freq_t freq, rmode_t mode)
{
    (void)rig;
    (void)mwpower;
    (void)freq;
    (void)mode;
    *power = 0.5f;
    return RIG_OK;
}

static int checking_halt_authorization;
static int powerstat_calls;
static int powerstat_retcode = RIG_OK;
static powerstat_t reported_powerstat = RIG_POWER_ON;

static void fail_if_halt_exits(void)
{
    if (checking_halt_authorization)
    {
        abort();
    }
}

static int capture_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    (void)rig;
    (void)vfo;
    memcpy(captured_description, chan->channel_desc,
           sizeof(captured_description));
    return RIG_OK;
}

static int fail_rig_open(RIG *rig)
{
    (void)rig;
    return -RIG_EIO;
}

static int test_get_powerstat(RIG *rig, powerstat_t *powerstat)
{
    (void)rig;
    powerstat_calls++;

    if (powerstat_retcode == RIG_OK)
    {
        *powerstat = reported_powerstat;
    }

    return powerstat_retcode;
}

static void count_last_client(void *data)
{
    int *count = data;

    (*count)++;
}

#ifdef HAVE_SOCKETPAIR
static pthread_mutex_t parser_sync_mutex = PTHREAD_MUTEX_INITIALIZER;

struct parser_thread_context
{
    RIG *rig;
    FILE *input;
    FILE *output;
    sync_cb_t sync_cb;
    pthread_t thread;
    pthread_mutex_t state_mutex;
    int vfo_mode;
    int ext_resp;
    char resp_sep;
    int ret;
    int lock_entered;
    int finished;
};

static struct parser_thread_context *waiting_parser_context;

static void parser_sync(int lock)
{
    if (lock)
    {
        pthread_mutex_lock(&parser_sync_mutex);
    }
    else
    {
        pthread_mutex_unlock(&parser_sync_mutex);
    }
}

static void waiting_parser_sync(int lock)
{
    parser_sync(lock);

    if (lock)
    {
        pthread_mutex_lock(&waiting_parser_context->state_mutex);
        waiting_parser_context->lock_entered = 1;
        pthread_mutex_unlock(&waiting_parser_context->state_mutex);
    }
}

static void *run_parser_thread(void *data)
{
    struct parser_thread_context *context = data;
    char *argv[] = { "testctlparser" };

    context->ret = rigctl_parse(context->rig, context->input,
                                context->output, argv, 1, context->sync_cb,
                                1, 0, &context->vfo_mode, 0,
                                &context->ext_resp, &context->resp_sep, 0);
    pthread_mutex_lock(&context->state_mutex);
    context->finished = 1;
    pthread_mutex_unlock(&context->state_mutex);
    return NULL;
}

static int parser_thread_finished(struct parser_thread_context *context)
{
    int finished;

    pthread_mutex_lock(&context->state_mutex);
    finished = context->finished;
    pthread_mutex_unlock(&context->state_mutex);
    return finished;
}

static int parser_thread_entered_lock(struct parser_thread_context *context)
{
    int lock_entered;

    pthread_mutex_lock(&context->state_mutex);
    lock_entered = context->lock_entered;
    pthread_mutex_unlock(&context->state_mutex);
    return lock_entered;
}

static int wait_for_parser_thread(struct parser_thread_context *context,
                                  unsigned int timeout_ms)
{
    for (unsigned int i = 0; i < timeout_ms; i++)
    {
        if (parser_thread_finished(context))
        {
            return 1;
        }

        usleep(1000);
    }

    return parser_thread_finished(context);
}

static int init_parser_thread(struct parser_thread_context *context, RIG *rig,
                              FILE *input)
{
    memset(context, 0, sizeof(*context));
    context->rig = rig;
    context->input = input;
    context->output = tmpfile();
    context->sync_cb = parser_sync;
    context->resp_sep = '\n';

    if (context->output == NULL
            || pthread_mutex_init(&context->state_mutex, NULL) != 0)
    {
        if (context->output != NULL)
        {
            fclose(context->output);
        }

        return -1;
    }

    return 0;
}

static void destroy_parser_thread(struct parser_thread_context *context)
{
    fclose(context->input);
    fclose(context->output);
    pthread_mutex_destroy(&context->state_mutex);
}

struct pool_stop_context
{
    struct rigctld_client_pool *pool;
    pthread_mutex_t mutex;
    int finished;
};

static void *stop_client_pool(void *data)
{
    struct pool_stop_context *context = data;

    rigctld_client_pool_stop(context->pool);
    pthread_mutex_lock(&context->mutex);
    context->finished = 1;
    pthread_mutex_unlock(&context->mutex);
    return NULL;
}

static int wait_for_socket_shutdown(int socket_fd, unsigned int timeout_ms)
{
    fd_set read_set;
    struct timeval timeout =
    {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (int)(timeout_ms % 1000) * 1000
    };
    char byte;

    FD_ZERO(&read_set);
    FD_SET(socket_fd, &read_set);

    if (select(socket_fd + 1, &read_set, NULL, NULL, &timeout) != 1)
    {
        return 0;
    }

    return recv(socket_fd, &byte, 1, MSG_DONTWAIT) == 0;
}

struct parser_wait_case
{
    const char *name;
    const char *initial_input;
    const char *remaining_input;
    int expected_ret;
};

static int check_parser_wait_case(RIG *rig,
                                  const struct parser_wait_case *test_case)
{
    struct parser_thread_context waiting;
    struct parser_thread_context other;
    FILE *waiting_input = NULL;
    FILE *other_input = NULL;
    int sockets[2] = { -1, -1 };
    int waiting_started = 0;
    int other_started = 0;
    int result = 1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        return 1;
    }

    waiting_input = fdopen(sockets[0], "r");
    other_input = tmpfile();

    if (waiting_input == NULL || other_input == NULL)
    {
        goto done;
    }

    sockets[0] = -1;
    fputs("\\chk_vfo\n", other_input);
    rewind(other_input);

    if (init_parser_thread(&waiting, rig, waiting_input) != 0)
    {
        goto done;
    }

    waiting_input = NULL;
    waiting.sync_cb = waiting_parser_sync;
    waiting_parser_context = &waiting;

    if (init_parser_thread(&other, rig, other_input) != 0)
    {
        destroy_parser_thread(&waiting);
        goto done;
    }

    other_input = NULL;

    if (send(sockets[1], test_case->initial_input,
             strlen(test_case->initial_input), 0)
            != (ssize_t)strlen(test_case->initial_input)
            || pthread_create(&waiting.thread, NULL, run_parser_thread,
                              &waiting) != 0)
    {
        goto release;
    }

    waiting_started = 1;

    for (unsigned int i = 0; i < 500
            && !parser_thread_entered_lock(&waiting); i++)
    {
        usleep(1000);
    }

    if (!parser_thread_entered_lock(&waiting))
    {
        fprintf(stderr, "%s did not enter the parser lock\n", test_case->name);
        goto release;
    }

    if (parser_thread_finished(&waiting))
    {
        fprintf(stderr, "%s did not wait for its client\n", test_case->name);
        goto release;
    }

    if (pthread_create(&other.thread, NULL, run_parser_thread, &other) != 0)
    {
        goto release;
    }

    other_started = 1;

    if (!wait_for_parser_thread(&other, 500))
    {
        fprintf(stderr, "%s held the parser lock\n", test_case->name);
        goto release;
    }

    result = 0;

release:

    if (test_case->remaining_input != NULL)
    {
        send(sockets[1], test_case->remaining_input,
             strlen(test_case->remaining_input), 0);
    }

    shutdown(sockets[1], SHUT_WR);

    if (waiting_started)
    {
        pthread_join(waiting.thread, NULL);

        if (waiting.ret != test_case->expected_ret)
        {
            fprintf(stderr, "%s returned %d instead of %d\n",
                    test_case->name, waiting.ret, test_case->expected_ret);
            result = 1;
        }
    }

    if (other_started)
    {
        pthread_join(other.thread, NULL);

        if (other.ret != RIG_OK)
        {
            fprintf(stderr, "concurrent command returned %d during %s\n",
                    other.ret, test_case->name);
            result = 1;
        }
    }

    destroy_parser_thread(&waiting);
    destroy_parser_thread(&other);

done:

    if (waiting_input != NULL)
    {
        fclose(waiting_input);
    }
    else if (sockets[0] >= 0)
    {
        close(sockets[0]);
    }

    if (other_input != NULL)
    {
        fclose(other_input);
    }

    if (sockets[1] >= 0)
    {
        close(sockets[1]);
    }

    waiting_parser_context = NULL;

    return result;
}

static int check_client_wait_lock_scope(RIG *rig)
{
    static const struct parser_wait_case cases[] =
    {
        { "invalid set_vfo", "V INVALID ", "ignored\n", -RIG_EINVAL },
        { "set_channel", "H 0 ", "memory\n", RIG_OK },
        {
            "stream_open",
            "+\\stream_open AUDIO_RX PCM_S16 48000 ",
            "require_native=BOGUS\n",
            -RIG_EINVAL
        },
        { "pause", "\\pause 1\n", NULL, RIG_OK }
    };
    int saved_is_rigctld = is_rigctld;
    int result = 0;

    is_rigctld = 1;
    HAMLIB_STATE(rig)->powerstat = RIG_POWER_ON;
    rig_powerstat = RIG_POWER_ON;

    if (HAMLIB_STATE(rig)->comm_state == 0 && rig_open(rig) != RIG_OK)
    {
        result = 1;
        goto done;
    }

    if (rigctld_stream_registry_init(&g_stream_registry) != 0)
    {
        result = 1;
        goto done;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (check_parser_wait_case(rig, &cases[i]) != 0)
        {
            result = 1;
            break;
        }
    }

    rigctld_stream_registry_destroy(&g_stream_registry);

done:
    is_rigctld = saved_is_rigctld;
    return result;
}
#endif

static int parse_command(RIG *rig, FILE *input, char *argv[], int argc,
                         char send_cmd_term)
{
    FILE *output = tmpfile();
    int vfo_mode = 0;
    int ext_resp = 0;
    char resp_sep = '\n';
    int ret;

    if (output == NULL)
    {
        return -RIG_EINTERNAL;
    }

    optind = 1;
    ret = rigctl_parse(rig, input, output, argv, argc, NULL, 0, 0,
                       &vfo_mode, send_cmd_term, &ext_resp, &resp_sep, 0);
    fclose(output);
    return ret;
}

static int parse_command_output(RIG *rig, FILE *input, char *argv[], int argc,
                                char send_cmd_term, unsigned char *output,
                                size_t output_size, size_t *output_length)
{
    FILE *stream = tmpfile();
    int vfo_mode = 0;
    int ext_resp = 0;
    char resp_sep = '\n';
    int ret;

    if (stream == NULL)
    {
        return -RIG_EINTERNAL;
    }

    optind = 1;
    ret = rigctl_parse(rig, input, stream, argv, argc, NULL, 0, 0,
                       &vfo_mode, send_cmd_term, &ext_resp, &resp_sep, 0);
    rewind(stream);
    *output_length = fread(output, 1, output_size, stream);
    fclose(stream);
    return ret;
}

static int parser_lock_calls;
static int parser_unlock_calls;

static void count_parser_lock(int lock)
{
    if (lock)
    {
        parser_lock_calls++;
    }
    else
    {
        parser_unlock_calls++;
    }
}

static int parse_secure_network_command_output(RIG *rig, const char *command,
        struct handle_data *connection, char *response, size_t response_size)
{
    FILE *input = tmpfile();
    FILE *output = tmpfile();
    char *argv[] = { "testctlparser" };
    int ext_resp = 0;
    char resp_sep = '\n';
    int ret;

    if (input == NULL || output == NULL)
    {
        if (input != NULL) { fclose(input); }

        if (output != NULL) { fclose(output); }

        return -RIG_EINTERNAL;
    }

    fputs(command, input);
    rewind(input);
    pthread_setspecific(thread_data_key, connection);
    parser_lock_calls = 0;
    parser_unlock_calls = 0;
    ret = rigctl_parse(rig, input, output, argv, 1, count_parser_lock, 1, 0,
                       &connection->vfo_mode, 0, &ext_resp, &resp_sep, 1);

    if (response != NULL && response_size > 0)
    {
        size_t length;

        rewind(output);
        length = fread(response, 1, response_size - 1, output);
        response[length] = '\0';
    }

    pthread_setspecific(thread_data_key, NULL);
    fclose(input);
    fclose(output);
    return ret;
}

static int parse_secure_network_command(RIG *rig, const char *command,
                                        struct handle_data *connection)
{
    return parse_secure_network_command_output(rig, command, connection,
            NULL, 0);
}

static int check_invalid_vfo_line_recovery(RIG *rig)
{
    FILE *input = tmpfile();
    FILE *output = tmpfile();
    char *argv[] = { "testctlparser" };
    char response[64];
    int vfo_mode = 0;
    int ext_resp = 0;
    char resp_sep = '\n';
    int first_ret;
    int second_ret;
    size_t length;

    if (input == NULL || output == NULL)
    {
        if (input != NULL) { fclose(input); }

        if (output != NULL) { fclose(output); }

        return 1;
    }

    fputs("V INVALID ignored\n\\chk_vfo\n", input);
    rewind(input);
    first_ret = rigctl_parse(rig, input, output, argv, 1, NULL, 1, 0,
                             &vfo_mode, 0, &ext_resp, &resp_sep, 0);
    second_ret = rigctl_parse(rig, input, output, argv, 1, NULL, 1, 0,
                              &vfo_mode, 0, &ext_resp, &resp_sep, 0);
    rewind(output);
    length = fread(response, 1, sizeof(response) - 1, output);
    response[length] = '\0';
    fclose(input);
    fclose(output);

    if (first_ret != -RIG_EINVAL || second_ret != RIG_OK
            || strcmp(response, "RPRT -1\n0\n") != 0)
    {
        fprintf(stderr,
                "invalid VFO line recovery failed: first=%d second=%d '%s'\n",
                first_ret, second_ret, response);
        return 1;
    }

    return 0;
}

static int check_client_pool(void)
{
    struct daemon_client_tracker admission;
    struct daemon_client_tracker large_admission;
    struct rigctld_client_pool pool;
    struct rigctld_client_pool large_pool;
    int last_client_count = 0;
    int first_slot;
    int second_slot;
    int failed = 0;

    if (daemon_client_tracker_init(&large_admission, 129) != 0)
    {
        return 1;
    }

    if (rigctld_client_pool_init(&large_pool, &large_admission, 0) != RIG_OK)
    {
        daemon_client_tracker_destroy(&large_admission);
        return 1;
    }

    rigctld_client_pool_destroy(&large_pool);
    daemon_client_tracker_destroy(&large_admission);

    if (daemon_client_tracker_init(&admission, 2) != 0)
    {
        return 1;
    }

    if (rigctld_client_pool_init(&pool, &admission, 0) != RIG_OK)
    {
        daemon_client_tracker_destroy(&admission);
        return 1;
    }

    first_slot = rigctld_client_reserve(&pool, -1, 0);
    second_slot = rigctld_client_reserve(&pool, -1, 0);
    failed = !first_slot || !second_slot
             || rigctld_client_reserve(&pool, -1, 0)
             || rigctld_client_count(&pool) != 2
             || rigctld_client_release(&pool, first_slot) != 1
             || rigctld_client_release_last(&pool, second_slot,
                                            count_last_client,
                                            &last_client_count) != 0
             || rigctld_client_release_last(&pool, second_slot,
                                            count_last_client,
                                            &last_client_count) != 0
             || last_client_count != 1;

    rigctld_client_pool_stop(&pool);
    failed = failed || rigctld_client_reserve(&pool, -1, 0) != 0;
    rigctld_client_pool_destroy(&pool);
    daemon_client_tracker_destroy(&admission);

    if (failed)
    {
        fprintf(stderr, "client registry accounting failed\n");
    }

    return failed;
}

static int check_authentication_deadline(void)
{
#ifdef HAVE_SOCKETPAIR
    struct daemon_client_tracker admission;
    struct rigctld_client_pool pool;
    int protected_pair[2];
    int expiring_pair[2];
    int protected_slot;
    int expiring_slot;
    char byte = 'x';
    int failed = 0;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, protected_pair) != 0)
    {
        return 1;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, expiring_pair) != 0)
    {
        rigctld_socket_close(protected_pair[0]);
        rigctld_socket_close(protected_pair[1]);
        return 1;
    }

    if (daemon_client_tracker_init(&admission, 2) != 0)
    {
        rigctld_socket_close(protected_pair[0]);
        rigctld_socket_close(protected_pair[1]);
        rigctld_socket_close(expiring_pair[0]);
        rigctld_socket_close(expiring_pair[1]);
        return 1;
    }

    if (rigctld_client_pool_init(&pool, &admission, 500) != RIG_OK)
    {
        daemon_client_tracker_destroy(&admission);
        rigctld_socket_close(protected_pair[0]);
        rigctld_socket_close(protected_pair[1]);
        rigctld_socket_close(expiring_pair[0]);
        rigctld_socket_close(expiring_pair[1]);
        return 1;
    }

    protected_slot = rigctld_client_reserve(&pool, protected_pair[0], 1);
    expiring_slot = rigctld_client_reserve(&pool, expiring_pair[0], 1);
    rigctld_client_set_authenticated(&pool, protected_slot, 1);

    for (int i = 0; i < 3; i++)
    {
        usleep(30000);
        rigctld_client_set_authenticated(&pool, expiring_slot, 0);
        send(expiring_pair[1], &byte, 1, 0);
    }

    if (!wait_for_socket_shutdown(expiring_pair[1], 2000)
            || wait_for_socket_shutdown(protected_pair[1], 0))
    {
        failed = 1;
    }

    rigctld_client_set_authenticated(&pool, protected_slot, 0);

    if (!wait_for_socket_shutdown(protected_pair[1], 2000))
    {
        failed = 1;
    }

    rigctld_client_detach_socket(&pool, protected_slot);
    rigctld_client_detach_socket(&pool, expiring_slot);
    rigctld_socket_close(protected_pair[0]);
    rigctld_socket_close(protected_pair[1]);
    rigctld_socket_close(expiring_pair[0]);
    rigctld_socket_close(expiring_pair[1]);
    rigctld_client_release(&pool, protected_slot);
    rigctld_client_release(&pool, expiring_slot);
    rigctld_client_pool_destroy(&pool);
    daemon_client_tracker_destroy(&admission);

    if (failed)
    {
        fprintf(stderr, "absolute authentication deadline failed\n");
    }

    return failed;
#else
    return 0;
#endif
}

static int check_client_pool_shutdown(void)
{
#ifdef HAVE_SOCKETPAIR
    struct daemon_client_tracker admission;
    struct rigctld_client_pool pool;
    struct pool_stop_context context = { .pool = &pool };
    pthread_t stop_thread;
    int sockets[2];
    int client_slot = 0;
    int callback_count = 0;
    int stopped_early;
    int client_released = 0;
    int failed = 0;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        return 1;
    }

    if (daemon_client_tracker_init(&admission, 1) != 0)
    {
        rigctld_socket_close(sockets[0]);
        rigctld_socket_close(sockets[1]);
        return 1;
    }

    if (rigctld_client_pool_init(&pool, &admission, 0) != RIG_OK)
    {
        daemon_client_tracker_destroy(&admission);
        rigctld_socket_close(sockets[0]);
        rigctld_socket_close(sockets[1]);
        return 1;
    }

    if (pthread_mutex_init(&context.mutex, NULL) != 0)
    {
        rigctld_client_pool_destroy(&pool);
        daemon_client_tracker_destroy(&admission);
        rigctld_socket_close(sockets[0]);
        rigctld_socket_close(sockets[1]);
        return 1;
    }

    client_slot = rigctld_client_reserve(&pool, sockets[0], 0);

    if (!client_slot
            || pthread_create(&stop_thread, NULL, stop_client_pool,
                              &context) != 0)
    {
        failed = 1;
        goto done;
    }

    int stop_started = 0;

    for (int i = 0; i < 1000; i++)
    {
        pthread_mutex_lock(&pool.mutex);
        stop_started = pool.stopping;
        pthread_mutex_unlock(&pool.mutex);

        if (stop_started)
        {
            break;
        }

        usleep(1000);
    }

    pthread_mutex_lock(&context.mutex);
    stopped_early = context.finished;
    pthread_mutex_unlock(&context.mutex);

    rigctld_client_detach_socket(&pool, client_slot);
    rigctld_socket_close(sockets[0]);
    rigctld_socket_close(sockets[1]);
    rigctld_client_release_last(&pool, client_slot, count_last_client,
                                &callback_count);
    client_released = 1;
    pthread_join(stop_thread, NULL);

    if (!stop_started || stopped_early || !context.finished
            || callback_count != 0)
    {
        failed = 1;
    }

done:

    if (!client_released)
    {
        if (client_slot)
        {
            rigctld_client_detach_socket(&pool, client_slot);
        }

        rigctld_socket_close(sockets[0]);
        rigctld_socket_close(sockets[1]);

        if (client_slot)
        {
            rigctld_client_release(&pool, client_slot);
        }
    }

    rigctld_client_pool_destroy(&pool);
    daemon_client_tracker_destroy(&admission);
    pthread_mutex_destroy(&context.mutex);

    if (failed)
    {
        fprintf(stderr, "client pool shutdown synchronization failed\n");
    }

    return failed;
#else
    return 0;
#endif
}

static int check_sensitive_write(void)
{
    static const unsigned char secret[] = "unique-write-secret";
    FILE *sink = tmpfile();
    hamlib_port_t port = { 0 };
    int ret;

    if (sink == NULL)
    {
        return 1;
    }

    port.fd = fileno(sink);

    if (port.fd < 0)
    {
        fclose(sink);
        return 1;
    }

    rig_debug_clear();
    ret = write_block_sensitive(&port, secret, sizeof(secret) - 1);
    fclose(sink);

    if (ret != RIG_OK || strstr(debugmsgsave, (const char *)secret) != NULL)
    {
        fprintf(stderr, "sensitive write exposed its payload in debug history\n");
        return 1;
    }

    return 0;
}

static int check_startup_argument_redaction(void)
{
    static const char secret[] = "startup-secret-value";
    static const char expected[] =
        "test: rigctl -m 1 -A <redacted> -A<redacted> "
        "--password <redacted> --password=<redacted> password <redacted> "
        "\\password <redacted> +\\password <redacted> "
        "\x98 <redacted> ordinary";
    char separate_secret[] = "startup-secret-value";
    char inline_short[] = "-Astartup-secret-value";
    char inline_long[] = "--password=startup-secret-value";
    char command_secret[] = "startup-secret-value";
    char slash_secret[] = "startup-secret-value";
    char extended_secret[] = "startup-secret-value";
    char opcode_password[] = { (char)0x98, '\0' };
    char opcode_secret[] = "startup-secret-value";
    char *arguments[] =
    {
        "rigctl", "-m", "1", "-A", separate_secret, inline_short,
        "--password", separate_secret, inline_long, "password",
        command_secret, "\\password", slash_secret, "+\\password",
        extended_secret, opcode_password, opcode_secret, "ordinary"
    };
    char formatted[512];
    char small[12];
    char wiped[] = "erase-me";
    int ret;

    rigctl_wipe_password(separate_secret);
    rigctl_wipe_password(inline_short + 2);
    rigctl_wipe_password(inline_long + 11);
    ret = rigctl_format_startup_args(formatted, sizeof(formatted), "test:",
                                     sizeof(arguments) / sizeof(arguments[0]),
                                     arguments);

    if (ret != RIG_OK || strcmp(formatted, expected) != 0
            || strstr(formatted, secret) != NULL)
    {
        fprintf(stderr, "startup argument redaction failed: '%s'\n", formatted);
        return 1;
    }

    rig_debug_clear();
    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", formatted);

    if (strstr(debugmsgsave, secret) != NULL)
    {
        fprintf(stderr, "startup credential reached debug history\n");
        return 1;
    }

    memset(small, 0xa5, sizeof(small));
    ret = rigctl_format_startup_args(small, sizeof(small), "test:",
                                     sizeof(arguments) / sizeof(arguments[0]),
                                     arguments);

    if (ret != -RIG_ETRUNC || small[sizeof(small) - 1] != '\0')
    {
        fprintf(stderr, "bounded startup formatting failed\n");
        return 1;
    }

    rigctl_wipe_password(wiped);

    if (strcmp(wiped, "********") != 0)
    {
        fprintf(stderr, "password argument was not wiped\n");
        return 1;
    }

    return 0;
}

static int check_batch_password_redaction(RIG *rig)
{
    static const char secret[] = "batch-password-secret";
    FILE *input = tmpfile();
    FILE *output = tmpfile();
    char *arguments[] = { "testctlparser", "-" };
    char transcript[256];
    int vfo_mode = 0;
    int ext_resp = 0;
    char resp_sep = '\n';
    int ret;
    size_t length;

    if (input == NULL || output == NULL)
    {
        if (input != NULL) { fclose(input); }

        if (output != NULL) { fclose(output); }

        return 1;
    }

    fprintf(input, "password %s\n", secret);
    rewind(input);
    optind = 1;
    is_rigctld = 1;
    rig_debug_clear();
    ret = rigctl_parse(rig, input, output, arguments, 2, NULL, 0, 0,
                       &vfo_mode, 0, &ext_resp, &resp_sep, 1);
    is_rigctld = 0;
    rewind(output);
    length = fread(transcript, 1, sizeof(transcript) - 1, output);
    transcript[length] = '\0';

    /* Consume EOF so the parser leaves batch-input mode for later tests. */
    rigctl_parse(rig, input, output, arguments, 2, NULL, 0, 0,
                 &vfo_mode, 0, &ext_resp, &resp_sep, 1);
    fclose(input);
    fclose(output);

    if (ret != -RIG_ESECURITY
            || strstr(transcript, "password <redacted>") == NULL
            || strstr(transcript, secret) != NULL
            || strstr(debugmsgsave, secret) != NULL)
    {
        fprintf(stderr, "batch password redaction failed: '%s'\n", transcript);
        return 1;
    }

    return 0;
}

static int check_socket_timeouts(void)
{
#ifdef HAVE_SOCKETPAIR
    struct timeval receive_timeout;
    struct timeval send_timeout;
    socklen_t receive_length = sizeof(receive_timeout);
    socklen_t send_length = sizeof(send_timeout);
    int sockets[2];
    int failed;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        return 1;
    }

    failed = rigctld_socket_set_timeout(sockets[0], 1) != RIG_OK
             || getsockopt(sockets[0], SOL_SOCKET, SO_RCVTIMEO,
                           &receive_timeout, &receive_length) != 0
             || getsockopt(sockets[0], SOL_SOCKET, SO_SNDTIMEO,
                           &send_timeout, &send_length) != 0
             || (receive_timeout.tv_sec == 0 && receive_timeout.tv_usec == 0)
             || (send_timeout.tv_sec == 0 && send_timeout.tv_usec == 0)
             || rigctld_socket_set_timeout(sockets[0], 0) != RIG_OK;
    rigctld_socket_close(sockets[0]);
    rigctld_socket_close(sockets[1]);

    if (failed)
    {
        fprintf(stderr, "socket timeout configuration failed\n");
        return 1;
    }

#endif

    return 0;
}

static int check_powerstat_refresh(RIG *rig)
{
    static const powerstat_t stale_states[] =
    {
        RIG_POWER_OFF, RIG_POWER_STANDBY
    };
    int (*saved_get_powerstat)(RIG *, powerstat_t *) =
        rig->caps->get_powerstat;
    int result = 1;
    int ret;

    ((struct rig_caps *)rig->caps)->get_powerstat = NULL;
    ret = rig_open(rig);

    if (ret != RIG_OK || HAMLIB_STATE(rig)->comm_state == 0)
    {
        fprintf(stderr, "unable to open rig for power refresh test: %d\n", ret);
        goto done;
    }

    for (size_t i = 0; i < sizeof(stale_states) / sizeof(stale_states[0]); i++)
    {
        HAMLIB_STATE(rig)->powerstat = stale_states[i];
        rig_powerstat = stale_states[i];
        ret = rigctl_refresh_powerstat(rig);

        if (ret != RIG_OK || HAMLIB_STATE(rig)->powerstat != RIG_POWER_ON
                || rig_powerstat != RIG_POWER_ON)
        {
            fprintf(stderr,
                    "unsupported power query kept stale state: state=%d ret=%d\n",
                    stale_states[i], ret);
            goto done;
        }
    }

    result = 0;

done:
    HAMLIB_STATE(rig)->powerstat = RIG_POWER_ON;
    rig_powerstat = RIG_POWER_ON;

    if (HAMLIB_STATE(rig)->comm_state != 0)
    {
        rig_close(rig);
    }

    ((struct rig_caps *)rig->caps)->get_powerstat = saved_get_powerstat;

    return result;
}

static int check_password_authorization(RIG *rig)
{
    static const char *protected_commands[] =
    {
        "\\halt\n",
        "\\set_vfo VFOA\n",
        "\\hamlib_version\n",
        "\\set_channel 0\n"
    };
    static const char *protected_names[] =
    {
        "halt",
        "set_vfo",
        "hamlib_version",
        "set_channel"
    };
    static const char *preauth_commands[] =
    {
        "\\chk_vfo\n",
        "\\dump_state\n",
        "+\\stream_caps\n"
    };
    static const char *preauth_names[] =
    {
        "chk_vfo",
        "dump_state",
        "stream_caps"
    };
    struct handle_data connection = { .rig = rig };
    struct handle_data other_connection = { .rig = rig };
    struct handle_data failure_connection = { .rig = rig };
    char password[] = "test-password";
    char maximum_password[65];
    char overlong_password[66];
    char secret[HAMLIB_SECRET_LENGTH + 1];
    char repeated_secret[HAMLIB_SECRET_LENGTH + 1];
    char wrong_secret[HAMLIB_SECRET_LENGTH + 1];
    char command[64];
    char expected_response[64];
    char response[256];
    int (*saved_rig_open)(RIG *) = rig->caps->rig_open;
    int (*saved_get_powerstat)(RIG *, powerstat_t *) =
        rig->caps->get_powerstat;
    int saved_vfo_opt = HAMLIB_STATE(rig)->vfo_opt;
    int result = 1;
    int ret;

    is_rigctld = 1;

    for (size_t i = 0;
            i < sizeof(protected_commands) / sizeof(protected_commands[0]); i++)
    {
        checking_halt_authorization = i == 0;
        ret = parse_secure_network_command(rig, protected_commands[i],
                                           &connection);
        checking_halt_authorization = 0;

        if (ret != -RIG_ESECURITY)
        {
            fprintf(stderr, "unauthenticated %s: expected %d, got %d\n",
                    protected_names[i], -RIG_ESECURITY, ret);
            goto done;
        }

        if (parser_lock_calls != 0 || parser_unlock_calls != 0)
        {
            fprintf(stderr, "rejected %s used the rig lock: %d/%d\n",
                    protected_names[i], parser_lock_calls, parser_unlock_calls);
            goto done;
        }
    }

    for (unsigned int i = 0; i <= RIGCTLD_MAX_AUTH_FAILURES; i++)
    {
        ret = parse_secure_network_command(rig, "\\hamlib_version\n",
                                           &connection);

        if (ret != -RIG_ESECURITY)
        {
            fprintf(stderr, "rejected command setup failed\n");
            goto done;
        }
    }

    if (HAMLIB_STATE(rig)->comm_state != 0 || connection.auth_failures != 0)
    {
        fprintf(stderr,
                "unauthenticated commands opened the rig or counted as failures\n");
        goto done;
    }

    for (size_t i = 0;
            i < sizeof(preauth_commands) / sizeof(preauth_commands[0]); i++)
    {
        ret = parse_secure_network_command(rig, preauth_commands[i], &connection);

        if (ret != RIG_OK)
        {
            fprintf(stderr, "unauthenticated %s: expected success, got %d\n",
                    preauth_names[i], ret);
            goto done;
        }

        if (parser_lock_calls != 1 || parser_unlock_calls != 1)
        {
            fprintf(stderr, "pre-authentication %s lost rig locking: %d/%d\n",
                    preauth_names[i], parser_lock_calls, parser_unlock_calls);
            goto done;
        }
    }

    if (HAMLIB_STATE(rig)->comm_state != 0)
    {
        fprintf(stderr, "pre-authentication commands opened the rig\n");
        goto done;
    }

    ret = parse_secure_network_command_output(rig,
        "+\\set_vfo VFOA\n", &connection, response, sizeof(response));

    if (ret != -RIG_ESECURITY
            || strcmp(response, "set_vfo: VFOA\nRPRT -19\n") != 0)
    {
        fprintf(stderr, "protected extended response was incompatible: '%s'\n",
                response);
        goto done;
    }

    memset(maximum_password, 'm', sizeof(maximum_password) - 1);
    maximum_password[sizeof(maximum_password) - 1] = '\0';
    memset(overlong_password, 'x', sizeof(overlong_password) - 1);
    overlong_password[sizeof(overlong_password) - 1] = '\0';

    if (rigctld_password_configure("", secret) != -RIG_EINVAL
            || rigctld_password_configure(overlong_password, secret)
            != -RIG_EINVAL || rigctld_password_is_enabled())
    {
        fprintf(stderr, "invalid password was configured\n");
        goto done;
    }

    if (rigctld_password_configure("x", secret) != RIG_OK
            || rigctld_password_configure(maximum_password, secret) != RIG_OK)
    {
        fprintf(stderr, "valid password boundary was rejected\n");
        goto done;
    }

    ret = rigctld_password_configure(password, secret);
    rig_password_generate_secret(password, repeated_secret);

    if (ret != RIG_OK || !rigctld_password_is_enabled()
            || strcmp(secret, repeated_secret) != 0)
    {
        fprintf(stderr, "unable to configure a stable password secret\n");
        goto done;
    }

    snprintf(command, sizeof(command), "\\password %s\n", secret);
    ret = parse_secure_network_command(rig, command, &connection);

    if (ret != RIG_OK || !connection.is_passwordOK
            || connection.auth_failures != 0
            || HAMLIB_STATE(rig)->comm_state != 0)
    {
        fprintf(stderr,
                "authentication failed after repeated rejected commands\n");
        goto done;
    }

    memset(wrong_secret, '0', HAMLIB_SECRET_LENGTH);
    wrong_secret[HAMLIB_SECRET_LENGTH] = '\0';

    if (strcmp(wrong_secret, secret) == 0)
    {
        wrong_secret[0] = '1';
    }

    snprintf(command, sizeof(command), "\\password %.31s\n", secret);
    ret = parse_secure_network_command(rig, command, &connection);

    if (ret != -RIG_ESECURITY || connection.is_passwordOK)
    {
        fprintf(stderr, "short password secret was accepted\n");
        goto done;
    }

    snprintf(command, sizeof(command), "\\password %s\n", wrong_secret);
    ret = parse_secure_network_command(rig, command, &connection);

    if (ret != -RIG_ESECURITY || connection.is_passwordOK)
    {
        fprintf(stderr, "wrong password secret was accepted\n");
        goto done;
    }

    snprintf(command, sizeof(command), "\\password %sx\n", secret);
    ret = parse_secure_network_command(rig, command, &connection);

    if (ret != -RIG_ESECURITY || connection.is_passwordOK)
    {
        fprintf(stderr, "password secret with trailing data was accepted\n");
        goto done;
    }

    rig_debug_clear();
    snprintf(command, sizeof(command), "+\\password %s\n", secret);
    ret = parse_secure_network_command_output(rig, command, &connection,
        response, sizeof(response));

    if (ret != RIG_OK || !connection.is_passwordOK
            || connection.auth_failures != 0
            || strcmp(response, "password:\nRPRT 0\n") != 0
            || strstr(response, secret) != NULL
            || strstr(debugmsgsave, secret) != NULL)
    {
        fprintf(stderr,
                "password authentication response or redaction failed: "
                "ret=%d response='%s'\n", ret, response);
        goto done;
    }

    connection.vfo_mode = 1;
    ret = parse_secure_network_command(rig, "\\chk_vfo\n", &connection);

    if (ret != RIG_OK || HAMLIB_STATE(rig)->vfo_opt != 1)
    {
        fprintf(stderr, "authenticated VFO mode was not applied\n");
        goto done;
    }

    if (parser_lock_calls != 1 || parser_unlock_calls != 1)
    {
        fprintf(stderr, "authenticated command lost rig locking: %d/%d\n",
                parser_lock_calls, parser_unlock_calls);
        goto done;
    }

    ret = parse_secure_network_command_output(rig, "\\chk_vfo\n",
        &other_connection, response, sizeof(response));

    if (ret != RIG_OK || strcmp(response, "0\n") != 0
            || HAMLIB_STATE(rig)->vfo_opt != 1)
    {
        fprintf(stderr,
                "pre-authentication VFO check changed shared mode: '%s'\n",
                response);
        goto done;
    }

    ret = parse_secure_network_command(rig, "\\hamlib_version\n",
                                       &other_connection);

    if (ret != -RIG_ESECURITY || other_connection.is_passwordOK
            || HAMLIB_STATE(rig)->vfo_opt != 1)
    {
        fprintf(stderr,
                "unauthenticated connection changed authenticated state\n");
        goto done;
    }

    snprintf(command, sizeof(command), "\\password %s\n", wrong_secret);
    ret = parse_secure_network_command(rig, command, &connection);

    if (ret != -RIG_ESECURITY || connection.is_passwordOK
            || connection.auth_failures != 1)
    {
        fprintf(stderr, "wrong password did not revoke authentication\n");
        goto done;
    }

    ret = parse_secure_network_command(rig, "\\hamlib_version\n", &connection);

    if (ret != -RIG_ESECURITY)
    {
        fprintf(stderr, "protected command ran after authentication revocation\n");
        goto done;
    }

    snprintf(command, sizeof(command), "\\password %s\n", wrong_secret);

    for (unsigned int i = 0; i < RIGCTLD_MAX_AUTH_FAILURES; i++)
    {
        ret = parse_secure_network_command(rig, command, &failure_connection);

        if (ret != -RIG_ESECURITY)
        {
            fprintf(stderr, "wrong-password failure limit setup failed\n");
            goto done;
        }
    }

    if (failure_connection.auth_failures != RIGCTLD_MAX_AUTH_FAILURES)
    {
        fprintf(stderr, "wrong password attempts were not counted\n");
        goto done;
    }

    snprintf(command, sizeof(command), "\\password %s\n", secret);
    HAMLIB_STATE(rig)->powerstat = RIG_POWER_OFF;
    rig_powerstat = RIG_POWER_OFF;
    ret = parse_secure_network_command(rig, command, &connection);
    HAMLIB_STATE(rig)->powerstat = RIG_POWER_ON;
    rig_powerstat = RIG_POWER_ON;

    if (ret != RIG_OK || !connection.is_passwordOK
            || HAMLIB_STATE(rig)->comm_state != 0)
    {
        fprintf(stderr, "password authentication required an open powered rig\n");
        goto done;
    }

    ((struct rig_caps *)rig->caps)->rig_open = fail_rig_open;
    ret = parse_secure_network_command_output(rig, "\\hamlib_version\n",
        &connection, response, sizeof(response));
    ((struct rig_caps *)rig->caps)->rig_open = saved_rig_open;
    snprintf(expected_response, sizeof(expected_response), "RPRT %d\n",
             -RIG_EIO);

    if (ret != -RIG_EIO || strcmp(response, expected_response) != 0)
    {
        fprintf(stderr, "lazy-open failure did not return an error: '%s'\n",
                response);
        goto done;
    }

    {
        FILE *input = tmpfile();
        char *local_command[] = { "testctlparser", "hamlib_version" };

        if (input == NULL)
        {
            goto done;
        }

        is_rigctld = 0;
        ((struct rig_caps *)rig->caps)->rig_open = fail_rig_open;
        ret = parse_command(rig, input, local_command, 2, 0);
        ((struct rig_caps *)rig->caps)->rig_open = saved_rig_open;
        is_rigctld = 1;
        fclose(input);

        if (ret != RIG_OK)
        {
            fprintf(stderr,
                    "local command did not preserve ignored open error: %d\n",
                    ret);
            goto done;
        }
    }

    ((struct rig_caps *)rig->caps)->rig_open = saved_rig_open;
    ((struct rig_caps *)rig->caps)->get_powerstat = test_get_powerstat;
    powerstat_calls = 0;
    powerstat_retcode = RIG_OK;
    reported_powerstat = RIG_POWER_OFF;
    int powerstat_calls_before = powerstat_calls;
    HAMLIB_STATE(rig)->powerstat = RIG_POWER_ON;
    rig_powerstat = RIG_POWER_ON;
    ret = parse_secure_network_command(rig, "\\hamlib_version\n", &connection);

    if (ret != -RIG_EPOWER || powerstat_calls <= powerstat_calls_before
            || HAMLIB_STATE(rig)->powerstat != RIG_POWER_OFF
            || rig_powerstat != RIG_POWER_OFF)
    {
        fprintf(stderr,
                "lazy open did not refresh power before dispatch: ret=%d calls=%d\n",
                ret, powerstat_calls);
        goto done;
    }

    rig_close(rig);
    reported_powerstat = RIG_POWER_STANDBY;
    powerstat_calls_before = powerstat_calls;
    HAMLIB_STATE(rig)->powerstat = RIG_POWER_ON;
    rig_powerstat = RIG_POWER_ON;
    ret = parse_secure_network_command(rig, "\\hamlib_version\n", &connection);

    if (ret != -RIG_EPOWER || powerstat_calls <= powerstat_calls_before
            || HAMLIB_STATE(rig)->powerstat != RIG_POWER_STANDBY
            || rig_powerstat != RIG_POWER_STANDBY)
    {
        fprintf(stderr,
                "standby power refresh did not block dispatch: ret=%d calls=%d\n",
                ret, powerstat_calls);
        goto done;
    }

    rig_close(rig);
    reported_powerstat = RIG_POWER_ON;
    powerstat_calls_before = powerstat_calls;
    ret = parse_secure_network_command(rig, "\\hamlib_version\n", &connection);

    if (ret != RIG_OK || powerstat_calls <= powerstat_calls_before
            || HAMLIB_STATE(rig)->powerstat != RIG_POWER_ON
            || rig_powerstat != RIG_POWER_ON)
    {
        fprintf(stderr,
                "successful power refresh did not permit dispatch: ret=%d calls=%d\n",
                ret, powerstat_calls);
        goto done;
    }

    rig_close(rig);
    powerstat_retcode = -RIG_EIO;
    reported_powerstat = RIG_POWER_OFF;
    powerstat_calls_before = powerstat_calls;
    HAMLIB_STATE(rig)->powerstat = RIG_POWER_ON;
    rig_powerstat = RIG_POWER_ON;
    ret = parse_secure_network_command(rig, "\\hamlib_version\n", &connection);

    if (ret != RIG_OK || powerstat_calls <= powerstat_calls_before
            || HAMLIB_STATE(rig)->powerstat != RIG_POWER_ON
            || rig_powerstat != RIG_POWER_ON)
    {
        fprintf(stderr,
                "failed power refresh changed cached state: ret=%d calls=%d\n",
                ret, powerstat_calls);
        goto done;
    }

    powerstat_retcode = RIG_OK;
    ((struct rig_caps *)rig->caps)->get_powerstat = saved_get_powerstat;

    ret = parse_secure_network_command(rig, "\\hamlib_version\n", &connection);

    if (ret != RIG_OK)
    {
        fprintf(stderr, "authenticated hamlib_version: expected success, got %d\n",
                ret);
        goto done;
    }

    if (HAMLIB_STATE(rig)->comm_state == 0)
    {
        fprintf(stderr, "authenticated protected command did not open the rig\n");
        goto done;
    }

    result = 0;

done:
    ((struct rig_caps *)rig->caps)->rig_open = saved_rig_open;
    ((struct rig_caps *)rig->caps)->get_powerstat = saved_get_powerstat;
    powerstat_retcode = RIG_OK;
    HAMLIB_STATE(rig)->vfo_opt = saved_vfo_opt;
    checking_halt_authorization = 0;
    is_rigctld = 0;
    return result;
}

static int parse_interactive_text(RIG *rig, const char *text, int parse_count,
                                  char *response, size_t response_size,
                                  char *remaining, size_t remaining_size)
{
    FILE *input = tmpfile();
    FILE *output = tmpfile();
    char *argv[] = { "testctlparser" };
    int vfo_mode = 0;
    int ext_resp = 0;
    char resp_sep = '\n';
    int ret = RIGCTL_PARSE_ERROR;

    if (response != NULL && response_size > 0) { response[0] = '\0'; }

    if (remaining != NULL && remaining_size > 0) { remaining[0] = '\0'; }

    if (input == NULL || output == NULL)
    {
        if (input != NULL) { fclose(input); }

        if (output != NULL) { fclose(output); }

        return ret;
    }

    fputs(text, input);
    rewind(input);

    for (int i = 0; i < parse_count; i++)
    {
        ret = rigctl_parse(rig, input, output, argv, 1, NULL, 1, 0,
                           &vfo_mode, 0, &ext_resp, &resp_sep, 0);

        if (ret != RIG_OK)
        {
            break;
        }
    }

    if (response != NULL && response_size > 0)
    {
        fflush(output);
        rewind(output);
        response[fread(response, 1, response_size - 1, output)] = '\0';
    }

    if (remaining != NULL && remaining_size > 0)
    {
        remaining[fread(remaining, 1, remaining_size - 1, input)] = '\0';
    }

    fclose(input);
    fclose(output);
    return ret;
}

static int check_long_command_framing(RIG *rig)
{
    char response[64];
    char remaining[64];
    int ret;

    ret = parse_interactive_text(rig, "\\chk_vfo\n\\c", 2, response,
                                 sizeof(response), NULL, 0);

    if (ret != RIG_OK || strcmp(response, "0\n") != 0)
    {
        fprintf(stderr, "truncated long command replayed prior command: '%s'\n",
                response);
        return 1;
    }

    captured_freq = -1;
    ret = parse_interactive_text(rig, "\\f\n\\set_freq 123\n", 2, NULL, 0,
                                 NULL, 0);

    if (ret != RIG_OK || captured_freq != 123)
    {
        fprintf(stderr, "single-character long command consumed next line\n");
        return 1;
    }

    ret = parse_interactive_text(rig, "\\chk_vfo", 1, response,
                                 sizeof(response), NULL, 0);

    if (ret != RIG_OK || strcmp(response, "0\n") != 0)
    {
        fprintf(stderr, "complete long command at EOF failed: '%s'\n", response);
        return 1;
    }

    ret = parse_interactive_text(rig, "\\\n\\password test-secret\n", 1,
                                 NULL, 0, remaining, sizeof(remaining));

    if (ret != RIG_OK
            || strcmp(remaining, "\\password test-secret\n") != 0)
    {
        fprintf(stderr, "empty long command consumed next line: '%s'\n",
                remaining);
        return 1;
    }

    captured_freq = -1;
    ret = parse_interactive_text(rig, "\\ set_freq 1\n\\set_freq 456\n", 2,
                                 NULL, 0, NULL, 0);

    if (ret != RIG_OK || captured_freq != 456)
    {
        fprintf(stderr, "empty long command name ended the session\n");
        return 1;
    }

    captured_freq = -1;
    ret = parse_interactive_text(rig,
                                 "\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa set_freq 1\n"
                                 "\\set_freq 789\n", 2, NULL, 0, NULL, 0);

    if (ret != RIG_OK || captured_freq != 789)
    {
        fprintf(stderr, "overlong long command name ended the session\n");
        return 1;
    }

    ret = parse_interactive_text(rig,
                                 "\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
                                 "\\password test-secret\n", 1, NULL, 0,
                                 remaining, sizeof(remaining));

    if (ret != RIG_OK
            || strcmp(remaining, "\\password test-secret\n") != 0)
    {
        fprintf(stderr, "overlong long command consumed next line: '%s'\n",
                remaining);
        return 1;
    }

    return 0;
}

static int check_send_command(RIG *rig, const char *command,
                              const unsigned char *expected,
                              size_t expected_length)
{
    unsigned char actual[64];
    FILE *input = tmpfile();
    char *argv[] = { "testctlparser", "w", (char *)command };
    size_t actual_length;
    int ret;

    if (input == NULL)
    {
        return 1;
    }

    ret = parse_command_output(rig, input, argv, 3, -1, actual,
                               sizeof(actual), &actual_length);
    fclose(input);

    if (ret != RIG_OK)
    {
        fprintf(stderr, "send command '%s': expected success, got %d\n",
                command, ret);
        return 1;
    }

    if (actual_length != expected_length
            || memcmp(actual, expected, expected_length) != 0)
    {
        fprintf(stderr, "send command '%s': unexpected decoded reply\n",
                command);
        return 1;
    }

    return 0;
}

static int check_invalid_send_command(RIG *rig, const char *command)
{
    FILE *input = tmpfile();
    char *argv[] = { "testctlparser", "w", (char *)command };
    int ret;

    if (input == NULL)
    {
        return 1;
    }

    ret = parse_command(rig, input, argv, 3, -1);
    fclose(input);

    if (ret != -RIG_EINVAL)
    {
        fprintf(stderr, "send command '%s': expected %d, got %d\n",
                command, -RIG_EINVAL, ret);
        return 1;
    }

    return 0;
}

static int check_receive_length(RIG *rig, const char *length, int expected)
{
    FILE *input = tmpfile();
    char *argv[] = { "testctlparser", "W", "x01", (char *)length };
    int ret;

    if (input == NULL)
    {
        return 1;
    }

    ret = parse_command(rig, input, argv, 4, -1);
    fclose(input);

    if (ret != expected)
    {
        fprintf(stderr, "receive length '%s': expected %d, got %d\n",
                length, expected, ret);
        return 1;
    }

    return 0;
}

static int check_bad_input_stream(RIG *rig)
{
    FILE *input = tmpfile();
    FILE *output = tmpfile();
    char *argv[] = { "testctlparser" };
    int vfo_mode = 0;
    int ext_resp = 0;
    char resp_sep = '\n';
    int ret;

    if (input == NULL || output == NULL)
    {
        if (input != NULL) { fclose(input); }

        if (output != NULL) { fclose(output); }

        return 1;
    }

    close(fileno(input));
    ret = rigctl_parse(rig, input, output, argv, 1, NULL, 1, 0,
                       &vfo_mode, 0, &ext_resp, &resp_sep, 0);
    fclose(input);
    fclose(output);

    if (ret != RIGCTL_PARSE_ERROR)
    {
        fprintf(stderr, "bad input stream: expected %d, got %d\n",
                RIGCTL_PARSE_ERROR, ret);
        return 1;
    }

    return 0;
}

static int check_description(RIG *rig, const char *input,
                             const char *expected)
{
    FILE *fields = tmpfile();
    char *argv[] = { "testctlparser", "H", "0" };
    int ret;

    if (fields == NULL)
    {
        return 1;
    }

    fputs(input, fields);
    rewind(fields);
    memset(captured_description, 0xa5, sizeof(captured_description));
    ret = parse_command(rig, fields, argv, 3, 0);
    fclose(fields);

    if (ret != RIG_OK)
    {
        fprintf(stderr, "description '%s': expected success, got %d\n",
                input, ret);
        return 1;
    }

    if (strcmp(captured_description, expected) != 0)
    {
        fprintf(stderr, "description '%s': expected '%s', got '%s'\n",
                input, expected, captured_description);
        return 1;
    }

    if (captured_description[sizeof(captured_description) - 1] != '\0')
    {
        fprintf(stderr, "description '%s' was not terminated\n", input);
        return 1;
    }

    return 0;
}

static int check_decimal_command(RIG *rig, char *argv[], int argc,
                                 int expected, const char *description)
{
    FILE *input = tmpfile();
    int ret;

    if (input == NULL)
    {
        return 1;
    }

    ret = parse_command(rig, input, argv, argc, -1);
    fclose(input);

    if (ret != expected)
    {
        fprintf(stderr, "%s: expected %d, got %d\n", description, expected, ret);
        return 1;
    }

    return 0;
}

static int check_locale_decimal_output(RIG *rig)
{
    static const char *comma_locales[] =
    {
        "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8",
        "German_Germany.1252", "French_France.1252"
    };
    static const unsigned char power_expected[] = "0.500000\n";
    static const unsigned char clock_expected[] = "2026-01-02T03:04:05.500+00:00\n";
    const char *current_locale = setlocale(LC_NUMERIC, NULL);
    char *saved_locale = current_locale == NULL ? NULL : strdup(current_locale);
    FILE *input = tmpfile();
    char *argv[] = { "testctlparser", "4", "100", "14.25", "USB" };
    char *clock_argv[] = { "testctlparser", "get_clock" };
    unsigned char output[32];
    size_t output_length;
    int result = 0;

    if (input == NULL || (current_locale != NULL && saved_locale == NULL))
    {
        if (input != NULL) { fclose(input); }

        free(saved_locale);
        return 1;
    }

    for (size_t i = 0; i < sizeof(comma_locales) / sizeof(comma_locales[0]); ++i)
    {
        if (setlocale(LC_NUMERIC, comma_locales[i]) != NULL
                && strcmp(localeconv()->decimal_point, ",") == 0)
        {
            break;
        }

        if (i + 1 == sizeof(comma_locales) / sizeof(comma_locales[0]))
        {
            goto done;
        }
    }

    if (parse_command_output(rig, input, argv, 5, -1, output, sizeof(output),
                             &output_length) != RIG_OK
            || output_length != sizeof(power_expected) - 1
            || memcmp(output, power_expected, sizeof(power_expected) - 1) != 0)
    {
        fprintf(stderr, "comma locale decimal output was not canonical\n");
        result = 1;
    }

    if (result == 0 && (parse_command_output(rig, input, clock_argv, 2, -1,
            output, sizeof(output),
            &output_length) != RIG_OK
                        || output_length != sizeof(clock_expected) - 1
                        || memcmp(output, clock_expected,
                                  sizeof(clock_expected) - 1) != 0))
    {
        fprintf(stderr, "comma locale clock output was not canonical\n");
        result = 1;
    }

done:
    fclose(input);

    if (saved_locale != NULL)
    {
        setlocale(LC_NUMERIC, saved_locale);
        free(saved_locale);
    }

    return result;
}

static int contains_between(const char *start, const char *end,
                            const char *needle)
{
    const char *match = strstr(start, needle);

    return match != NULL && match < end;
}

static int check_state_granularity_output(RIG *rig)
{
    struct rig_state *state = HAMLIB_STATE(rig);
    const int level_index = rig_setting2idx(RIG_LEVEL_ATT);
    const int parm_index = rig_setting2idx(RIG_PARM_ANN);
    const int band_index = rig_setting2idx(RIG_PARM_BANDSELECT);
    const int keyer_index = rig_setting2idx(RIG_PARM_KEYERTYPE);
    unsigned char output[32768];
    char level_expected[64];
    char level_wrong[64];
    char parm_expected[64];
    char parm_wrong[64];
    char band_expected[64];
    char keyer_entry[32];
    char *level_start;
    char *parm_start;
    char *state_end;
    FILE *input = tmpfile();
    char *chk_vfo_argv[] = { "testctlparser", "chk_vfo" };
    char *dump_state_argv[] = { "testctlparser", "dump_state" };
    size_t output_length;
    int ret;

    if (input == NULL)
    {
        return 1;
    }

    state->level_gran[level_index].min.i = 101;
    state->level_gran[level_index].max.i = 102;
    state->level_gran[level_index].step.i = 103;
    state->parm_gran[level_index].min.i = 201;
    state->parm_gran[level_index].max.i = 202;
    state->parm_gran[level_index].step.i = 203;
    state->parm_gran[parm_index].min.i = 301;
    state->parm_gran[parm_index].max.i = 302;
    state->parm_gran[parm_index].step.i = 303;
    state->level_gran[parm_index].min.i = 401;
    state->level_gran[parm_index].max.i = 402;
    state->level_gran[parm_index].step.i = 403;
    state->parm_gran[band_index].step.cs = "BAND160M,BAND80M";
    state->parm_gran[keyer_index].step.cs = NULL;

    ret = parse_command_output(rig, input, chk_vfo_argv, 2, -1, output,
                               sizeof(output) - 1, &output_length);

    if (ret == RIG_OK)
    {
        ret = parse_command_output(rig, input, dump_state_argv, 2, -1, output,
                                   sizeof(output) - 1, &output_length);
    }

    fclose(input);

    if (ret != RIG_OK || output_length >= sizeof(output) - 1)
    {
        fprintf(stderr, "unable to capture complete state output\n");
        return 1;
    }

    output[output_length] = '\0';
    level_start = strstr((char *)output, "level_gran=");
    parm_start = strstr((char *)output, "\nparm_gran=");
    state_end = parm_start == NULL ? NULL : strstr(parm_start, "\nrig_model=");

    if (level_start == NULL || parm_start == NULL || state_end == NULL)
    {
        fprintf(stderr, "state output is missing granularity sections\n");
        return 1;
    }

    SNPRINTF(level_expected, sizeof(level_expected), "%d=101,102,103;",
             level_index);
    SNPRINTF(level_wrong, sizeof(level_wrong), "%d=201,202,203;", level_index);
    SNPRINTF(parm_expected, sizeof(parm_expected), "%d=301,302,303;",
             parm_index);
    SNPRINTF(parm_wrong, sizeof(parm_wrong), "%d=401,402,403;", parm_index);
    SNPRINTF(band_expected, sizeof(band_expected), "%d=BAND160M,BAND80M;",
             band_index);
    SNPRINTF(keyer_entry, sizeof(keyer_entry), "%d=", keyer_index);

    if (!contains_between(level_start, parm_start, level_expected)
            || contains_between(level_start, parm_start, level_wrong)
            || !contains_between(parm_start, state_end, parm_expected)
            || contains_between(parm_start, state_end, parm_wrong)
            || !contains_between(parm_start, state_end, band_expected)
            || contains_between(parm_start, state_end, keyer_entry))
    {
        fprintf(stderr, "state output contains incorrect granularity data\n");
        return 1;
    }

    return 0;
}

static int state_output_has(RIG *rig, const char *needle)
{
    unsigned char output[32768];
    FILE *input = tmpfile();
    char *chk_vfo_argv[] = { "testctlparser", "chk_vfo" };
    char *dump_state_argv[] = { "testctlparser", "dump_state" };
    size_t output_length;
    int ret;

    if (input == NULL)
    {
        return -1;
    }

    ret = parse_command_output(rig, input, chk_vfo_argv, 2, -1, output,
                               sizeof(output) - 1, &output_length);

    if (ret == RIG_OK)
    {
        ret = parse_command_output(rig, input, dump_state_argv, 2, -1, output,
                                   sizeof(output) - 1, &output_length);
    }

    fclose(input);

    if (ret != RIG_OK || output_length >= sizeof(output) - 1)
    {
        fprintf(stderr, "unable to capture complete state output\n");
        return -1;
    }

    output[output_length] = '\0';
    return strstr((char *)output, needle) != NULL;
}

/* A network client asks for the VFO only if dump_state says the rig answers */
static int check_state_get_vfo_output(RIG *rig)
{
    struct rig_state *state = HAMLIB_STATE(rig);
    int refused, answering;

    state->no_get_vfo = 1;
    refused = state_output_has(rig, "\nhas_get_vfo=0\n");
    state->no_get_vfo = 0;
    answering = state_output_has(rig, "\nhas_get_vfo=1\n");

    if (refused != 1)
    {
        fprintf(stderr, "dump_state advertises get_vfo for a rig that"
                " refused it\n");
        return 1;
    }

    if (answering != 1)
    {
        fprintf(stderr, "dump_state hides get_vfo from a rig that answers"
                " it\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    static const char maximum[] = "12345678901234567890123456789";
    static const char thirty[] = "123456789012345678901234567890";
    static const char thirty_one[] = "1234567890123456789012345678901";
    static const unsigned char binary_reply[] = "\\0x00\\0xFF 2\n";
    static const unsigned char control_reply[] = { 0x01, 0x02, '\n', '\n' };
    struct rig_caps caps;
    struct rig_state *state;
    RIG *rig;
    FILE *empty_input;
    char *empty_command[] = { "testctlparser", "w", "" };
    int ret;

    rig_load_backend("dummy");
    rig = rig_init(RIG_MODEL_DUMMY);

    if (rig == NULL)
    {
        fprintf(stderr, "unable to initialize dummy rig\n");
        return 1;
    }

    caps = *rig->caps;
    memset(caps.chan_list, 0, sizeof(caps.chan_list));
    caps.chan_list[0].startc = 0;
    caps.chan_list[0].endc = 0;
    caps.chan_list[0].type = RIG_MTYPE_MEM;
    caps.chan_list[0].mem_caps.channel_desc = 1;
    caps.set_channel = capture_channel;
    caps.set_freq = capture_freq;
    caps.set_split_freq = capture_split_freq;
    caps.set_split_freq_mode = capture_split_freq_mode;
    caps.set_clock = capture_clock;
    caps.get_clock = capture_get_clock;
    caps.mW2power = capture_mw_to_power;
    rig->caps = &caps;
    state = HAMLIB_STATE(rig);
    memcpy(state->chan_list, caps.chan_list, sizeof(state->chan_list));
    rigctl_parse_init();
    atexit(fail_if_halt_exits);

    if (check_client_pool() != 0 || check_authentication_deadline() != 0
            || check_client_pool_shutdown() != 0
            || check_sensitive_write() != 0
            || check_startup_argument_redaction() != 0
            || check_batch_password_redaction(rig) != 0
            || check_socket_timeouts() != 0
            || check_powerstat_refresh(rig) != 0
            || check_password_authorization(rig) != 0
            || check_invalid_vfo_line_recovery(rig) != 0
#ifdef HAVE_SOCKETPAIR
            || check_client_wait_lock_scope(rig) != 0
#endif
       )
    {
        rig_cleanup(rig);
        return 1;
    }

    if (check_long_command_framing(rig) != 0
            || check_description(rig, maximum, maximum) != 0
            || check_description(rig, thirty, maximum) != 0
            || check_description(rig, thirty_one, maximum) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    empty_input = tmpfile();

    if (empty_input == NULL)
    {
        rig_cleanup(rig);
        return 1;
    }

    ret = parse_command(rig, empty_input, empty_command, 3, -1);
    fclose(empty_input);

    if (ret != -RIG_EINVAL)
    {
        fprintf(stderr, "empty send command: expected %d, got %d\n",
                -RIG_EINVAL, ret);
        rig_cleanup(rig);
        return 1;
    }

    if (check_send_command(rig, "x00 xff", binary_reply,
                           sizeof(binary_reply) - 1) != 0
            || check_send_command(rig, "\\0x00\\0xff", binary_reply,
                                  sizeof(binary_reply) - 1) != 0
            || check_send_command(rig, "x00ff", binary_reply,
                                  sizeof(binary_reply) - 1) != 0
            || check_send_command(rig, "x00xff", binary_reply,
                                  sizeof(binary_reply) - 1) != 0
            || check_send_command(rig, "x01 x02", control_reply,
                                  sizeof(control_reply)) != 0
            || check_invalid_send_command(rig, "x0") != 0
            || check_invalid_send_command(rig, "x000") != 0
            || check_invalid_send_command(rig, "x00 nope") != 0
            || check_invalid_send_command(rig, "\\0x00\\0xgg") != 0
            || check_receive_length(rig, "511", RIG_OK) != 0
            || check_receive_length(rig, "\\0x03", RIG_OK) != 0
            || check_receive_length(rig, "512", -RIG_EINVAL) != 0
            || check_receive_length(rig, "700", -RIG_EINVAL) != 0
            || check_receive_length(rig, "-1", -RIG_EINVAL) != 0
            || check_receive_length(rig, "12junk", -RIG_EINVAL) != 0
            || check_receive_length(rig, "999999999999999999999999",
                                    -RIG_EINVAL) != 0
            || check_bad_input_stream(rig) != 0
            || check_state_granularity_output(rig) != 0
            || check_state_get_vfo_output(rig) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    {
        char *set_freq_dot[] = { "testctlparser", "F", "14.25" };
        char *set_freq_comma[] = { "testctlparser", "F", "14,25" };
        char *set_split_comma[] = { "testctlparser", "I", "7,125" };
        char *set_split_mode_comma[] =
        {
            "testctlparser", "K", "7,125", "USB", "2400"
        };
        char *set_level_comma[] =
        {
            "testctlparser", "L", "AF", "0,5"
        };
        char *set_parm_comma[] =
        {
            "testctlparser", "P", "BACKLIGHT", "0,5"
        };
        char *power_comma[] =
        {
            "testctlparser", "2", "0,5", "14,25", "USB"
        };
        char *mw_power_comma[] =
        {
            "testctlparser", "4", "100", "14,25", "USB"
        };
        char *invalid_trailing[] = { "testctlparser", "F", "14.25junk" };
        char *invalid_mixed[] = { "testctlparser", "F", "14,25.5" };
        char *invalid_nonfinite[] = { "testctlparser", "F", "nan" };
        char *clock_dot[] =
        {
            "testctlparser", "set_clock", "2026-01-02T03:04:05.500+00:00"
        };
        char *clock_comma[] =
        {
            "testctlparser", "set_clock", "2026-01-02T03:04:05,500+00:00"
        };
        char *clock_missing_separator[] =
        {
            "testctlparser", "set_clock", "2026-01-02T03:04:050+00:00"
        };

        if (check_decimal_command(rig, set_freq_dot, 3, RIG_OK,
                                  "dot frequency") != 0
                || captured_freq != 14.25
                || check_decimal_command(rig, set_freq_comma, 3, RIG_OK,
                                         "comma frequency") != 0
                || captured_freq != 14.25
                || check_decimal_command(rig, set_split_comma, 3, RIG_OK,
                                         "comma split frequency") != 0
                || captured_freq != 7.125
                || check_decimal_command(rig, set_split_mode_comma, 5, RIG_OK,
                                         "comma split frequency mode") != 0
                || captured_freq != 7.125
                || check_decimal_command(rig, set_level_comma, 4, RIG_OK,
                                         "comma level") != 0
                || check_decimal_command(rig, set_parm_comma, 4, RIG_OK,
                                         "comma parameter") != 0
                || check_decimal_command(rig, power_comma, 5, RIG_OK,
                                         "comma power and frequency") != 0
                || check_decimal_command(rig, mw_power_comma, 5, RIG_OK,
                                         "comma conversion frequency") != 0
                || check_decimal_command(rig, invalid_trailing, 3, -RIG_EINVAL,
                                         "trailing decimal input") != 0
                || check_decimal_command(rig, invalid_mixed, 3, -RIG_EINVAL,
                                         "mixed decimal input") != 0
                || check_decimal_command(rig, invalid_nonfinite, 3, -RIG_EINVAL,
                                         "nonfinite decimal input") != 0
                || check_decimal_command(rig, clock_dot, 3, RIG_OK,
                                         "dot clock timestamp") != 0
                || captured_msec != 0.5
                || check_decimal_command(rig, clock_comma, 3, -RIG_EINVAL,
                                         "comma clock timestamp") != 0
                || check_decimal_command(rig, clock_missing_separator, 3,
                                         -RIG_EINVAL,
                                         "clock timestamp without decimal separator") != 0
                || check_locale_decimal_output(rig) != 0)
        {
            rig_cleanup(rig);
            return 1;
        }
    }

    rig_cleanup(rig);
    return 0;
}
