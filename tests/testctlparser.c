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

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"

#include "rigctl_parse.h"

int lock_mode;
powerstat_t rig_powerstat = RIG_POWER_ON;
extern char rigctld_password[65];
extern int is_rigctld;

static char captured_description[sizeof(((channel_t *)0)->channel_desc)];
static int checking_halt_authorization;

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

static int parse_secure_network_command(RIG *rig, const char *command,
                                        struct handle_data *connection)
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
    pthread_setspecific(thread_data_key, NULL);
    fclose(input);
    fclose(output);
    return ret;
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
    char password[] = "test-password";
    char *secret;
    char command[64];
    int ret;

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
            return 1;
        }
    }

    if (HAMLIB_STATE(rig)->comm_state != 0)
    {
        fprintf(stderr, "unauthenticated commands opened the rig\n");
        return 1;
    }

    for (size_t i = 0;
            i < sizeof(preauth_commands) / sizeof(preauth_commands[0]); i++)
    {
        ret = parse_secure_network_command(rig, preauth_commands[i], &connection);

        if (ret != RIG_OK)
        {
            fprintf(stderr, "unauthenticated %s: expected success, got %d\n",
                    preauth_names[i], ret);
            return 1;
        }
    }

    strcpy(rigctld_password, password);
    secret = rig_make_md5(password);

    if (secret == NULL)
    {
        fprintf(stderr, "unable to create password secret\n");
        return 1;
    }

    snprintf(command, sizeof(command), "\\password %s\n", secret);
    is_rigctld = 1;
    ret = parse_secure_network_command(rig, command, &connection);
    is_rigctld = 0;
    free(secret);

    if (ret != RIG_OK || !connection.is_passwordOK)
    {
        fprintf(stderr, "password authentication failed: ret=%d authenticated=%d\n",
                ret, connection.is_passwordOK);
        return 1;
    }

    ret = parse_secure_network_command(rig, "\\hamlib_version\n", &connection);

    if (ret != RIG_OK)
    {
        fprintf(stderr, "authenticated hamlib_version: expected success, got %d\n",
                ret);
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

    if (check_password_authorization(rig) != 0)
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
