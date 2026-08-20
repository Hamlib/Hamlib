/*
 * Hamlib command parser boundary tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
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
#include "rigctl_parse.h"
#include "rigctld_client.h"
#include "rig_tests.h"

int lock_mode;
powerstat_t rig_powerstat = RIG_POWER_ON;
extern int is_rigctld;

static char captured_description[sizeof(((channel_t *)0)->channel_desc)];
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

static int parse_secure_network_command_output(RIG *rig, const char *command,
        struct handle_data *connection, char *response, size_t response_size)
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

        return -RIG_EINTERNAL;
    }

    fputs(command, input);
    rewind(input);
    pthread_setspecific(thread_data_key, connection);
    ret = rigctl_parse(rig, input, output, argv, 1, NULL, 1, 0,
                       &vfo_mode, 0, &ext_resp, &resp_sep, 1);

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

static int check_client_pool(void)
{
    struct rigctld_client_pool pool;
    int last_client_count = 0;
    int first_slot;
    int second_slot;
    int failed = 0;

    if (rigctld_client_pool_init(&pool, 2, 0) != RIG_OK)
    {
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

    if (failed)
    {
        fprintf(stderr, "client pool limit or accounting failed\n");
    }

    return failed;
}

static int check_authentication_deadline(void)
{
#ifdef HAVE_SOCKETPAIR
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

    if (rigctld_client_pool_init(&pool, 2, 500) != RIG_OK)
    {
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

    if (rigctld_client_pool_init(&pool, 1, 0) != RIG_OK)
    {
        rigctld_socket_close(sockets[0]);
        rigctld_socket_close(sockets[1]);
        return 1;
    }

    if (pthread_mutex_init(&context.mutex, NULL) != 0)
    {
        rigctld_client_pool_destroy(&pool);
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

static int check_password_authorization(RIG *rig)
{
    static const char *protected_commands[] =
    {
        "\\halt\n",
        "\\set_vfo VFOA\n",
        "\\hamlib_version\n"
    };
    static const char *protected_names[] =
    {
        "halt",
        "set_vfo",
        "hamlib_version"
    };
    static const char *preauth_commands[] =
    {
        "\\chk_vfo\n",
        "\\dump_state\n"
    };
    static const char *preauth_names[] =
    {
        "chk_vfo",
        "dump_state"
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
    }

    if (HAMLIB_STATE(rig)->comm_state != 0)
    {
        fprintf(stderr, "unauthenticated commands opened the rig\n");
        goto done;
    }

    connection.auth_failures = 0;

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

    ret = parse_secure_network_command(rig, "\\hamlib_version\n",
                                       &other_connection);

    if (ret != -RIG_ESECURITY || other_connection.is_passwordOK)
    {
        fprintf(stderr, "password authentication leaked between connections\n");
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

    for (unsigned int i = 0; i < RIGCTLD_MAX_AUTH_FAILURES; i++)
    {
        ret = parse_secure_network_command(rig, "\\hamlib_version\n",
                                           &failure_connection);

        if (ret != -RIG_ESECURITY)
        {
            fprintf(stderr, "authentication failure limit setup failed\n");
            goto done;
        }
    }

    if (failure_connection.auth_failures != RIGCTLD_MAX_AUTH_FAILURES)
    {
        fprintf(stderr, "authentication failures were not counted\n");
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
    checking_halt_authorization = 0;
    is_rigctld = 0;
    return result;
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
    rig->caps = &caps;
    state = HAMLIB_STATE(rig);
    memcpy(state->chan_list, caps.chan_list, sizeof(state->chan_list));
    rigctl_parse_init();
    atexit(fail_if_halt_exits);

    if (check_client_pool() != 0 || check_authentication_deadline() != 0
            || check_client_pool_shutdown() != 0
            || check_sensitive_write() != 0
            || check_startup_argument_redaction() != 0
            || check_socket_timeouts() != 0
            || check_password_authorization(rig) != 0
       )
    {
        rig_cleanup(rig);
        return 1;
    }

    if (check_description(rig, maximum, maximum) != 0
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
            || check_bad_input_stream(rig) != 0)
    {
        rig_cleanup(rig);
        return 1;
    }

    rig_cleanup(rig);
    return 0;
}
