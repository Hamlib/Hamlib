/*
 * Hamlib NET rigctl parser tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "hamlib/config.h"

#include <stdio.h>
#include <string.h>

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_SYS_SOCKET_H) && \
    defined(HAVE_UNISTD_H)
#define HAVE_NETRIGCTL_SOCKET_TEST 1
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"

#include "../rigs/dummy/dummy.h"
#include "../rigs/dummy/dummy_common.h"

struct parse_case
{
    const char *name;
    char *value;
    int expected_ret;
    int expected_count;
    const enum agc_level_e *expected_levels;
};

static void fill_levels(enum agc_level_e *levels, enum agc_level_e value)
{
    for (int i = 0; i < HAMLIB_MAX_AGC_LEVELS; i++)
    {
        levels[i] = value;
    }
}

static int expect_state(const char *name, const enum agc_level_e *levels,
                        const enum agc_level_e *expected, int count,
                        int expected_count)
{
    if (count != expected_count)
    {
        fprintf(stderr, "%s: expected count %d, got %d\n", name,
                expected_count, count);
        return 1;
    }

    for (int i = 0; i < HAMLIB_MAX_AGC_LEVELS; i++)
    {
        if (levels[i] != expected[i])
        {
            fprintf(stderr, "%s: level %d expected %d, got %d\n", name, i,
                    expected[i], levels[i]);
            return 1;
        }
    }

    return 0;
}

#ifdef HAVE_NETRIGCTL_SOCKET_TEST
struct wire_peer
{
    int fd;
    int status;
};

static int read_line(int fd, char *buffer, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        ssize_t count = read(fd, buffer + used, 1);

        if (count != 1)
        {
            return -1;
        }

        if (buffer[used++] == '\n')
        {
            buffer[used] = '\0';
            return 0;
        }
    }

    return -1;
}

static int write_all(int fd, const char *buffer, size_t length)
{
    size_t written = 0;

    while (written < length)
    {
        ssize_t count = write(fd, buffer + written, length - written);

        if (count <= 0)
        {
            return -1;
        }

        written += (size_t)count;
    }

    return 0;
}

static void *run_wire_peer(void *arg)
{
    struct wire_peer *peer = arg;
    char command[16];

    peer->status = -1;

    if (read_line(peer->fd, command, sizeof(command)) != 0
            || strcmp(command, "_\n") != 0
            || write_all(peer->fd, "RPRT 1\n", 7) != 0)
    {
        return NULL;
    }

    peer->status = 0;
    return NULL;
}

static void register_client_models(void)
{
    static int registered;

    if (!registered)
    {
        rig_register(&netrigctl_caps);
        rig_register(&quisk_caps);
        registered = 1;
    }
}

static int test_positive_rprt_model(rig_model_t model)
{
    int sockets[2];
    pthread_t thread;
    struct wire_peer peer = { .fd = -1, .status = -1 };
    RIG *rig;
    const char *info;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        perror("socketpair");
        return 1;
    }

    peer.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_wire_peer, &peer) != 0)
    {
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    rig = rig_init(model);

    if (rig == NULL)
    {
        close(sockets[0]);
        close(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    info = rig->caps->get_info(rig);
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);

    if (peer.status != 0 || info != NULL)
    {
        fprintf(stderr, "model %d accepted a positive RPRT status\n", model);
        return 1;
    }

    return 0;
}

static int test_positive_rprt(void)
{
    const rig_model_t models[] = { RIG_MODEL_NETRIGCTL, RIG_MODEL_QUISK };

    register_client_models();

    for (size_t i = 0; i < sizeof(models) / sizeof(models[0]); ++i)
    {
        if (test_positive_rprt_model(models[i]) != 0)
        {
            return 1;
        }
    }

    return 0;
}
#else
static int test_positive_rprt(void)
{
    fprintf(stderr, "testnetrigctl: socketpair unavailable; "
            "skipping positive RPRT cases\n");
    return 0;
}
#endif

static int expect_parm_command(const char *name, setting_t parm, value_t val,
                               const char *expected)
{
    char command[64];
    int ret = netrigctl_format_parm_command(command, sizeof(command), parm, val);

    if (ret < 0)
    {
        fprintf(stderr, "%s: command formatting failed with %d\n", name, ret);
        return 1;
    }

    if ((size_t)ret != strlen(expected) || strcmp(command, expected) != 0)
    {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", name, expected,
                command);
        return 1;
    }

    return 0;
}

static int expect_parm_command_error(const char *name, setting_t parm,
                                     value_t val, int expected_ret)
{
    char command[64] = "partial command";
    int ret = netrigctl_format_parm_command(command, sizeof(command), parm, val);

    if (ret != expected_ret)
    {
        fprintf(stderr, "%s: expected return %d, got %d\n", name,
                expected_ret, ret);
        return 1;
    }

    if (command[0] != '\0')
    {
        fprintf(stderr, "%s: formatter retained a partial command\n", name);
        return 1;
    }

    return 0;
}

static int test_parm_commands(void)
{
    char maximum_value[51];
    char oversized_value[52];
    char command[64];
    value_t val;
    int ret;

    val.cs = "0";

    if (expect_parm_command("KEYERTYPE command", RIG_PARM_KEYERTYPE, val,
                            "P KEYERTYPE 0\n") != 0)
    {
        return 1;
    }

    val.cs = "BAND160M";

    if (expect_parm_command("BANDSELECT command", RIG_PARM_BANDSELECT, val,
                            "P BANDSELECT BAND160M\n") != 0)
    {
        return 1;
    }

    val.i = 1;

    if (expect_parm_command("integer command", RIG_PARM_BEEP, val,
                            "P BEEP 1\n") != 0)
    {
        return 1;
    }

    val.cs = NULL;

    if (expect_parm_command_error("null string", RIG_PARM_KEYERTYPE, val,
                                  -RIG_EINVAL) != 0)
    {
        return 1;
    }

    val.cs = "";

    if (expect_parm_command_error("empty string", RIG_PARM_KEYERTYPE, val,
                                  -RIG_EINVAL) != 0)
    {
        return 1;
    }

    val.cs = "0\nP BEEP 1";

    if (expect_parm_command_error("embedded command", RIG_PARM_KEYERTYPE, val,
                                  -RIG_EINVAL) != 0)
    {
        return 1;
    }

    memset(maximum_value, 'A', sizeof(maximum_value) - 1);
    maximum_value[sizeof(maximum_value) - 1] = '\0';
    val.cs = maximum_value;
    ret = netrigctl_format_parm_command(command, sizeof(command),
                                        RIG_PARM_KEYERTYPE, val);

    if (ret != (int)sizeof(command) - 1 || command[ret - 1] != '\n')
    {
        fprintf(stderr, "maximum string: expected a complete 63-byte command\n");
        return 1;
    }

    memset(oversized_value, 'A', sizeof(oversized_value) - 1);
    oversized_value[sizeof(oversized_value) - 1] = '\0';
    val.cs = oversized_value;

    return expect_parm_command_error("oversized string", RIG_PARM_KEYERTYPE,
                                     val, -RIG_ETRUNC);
}

static int expect_string_response(const char *name, setting_t parm,
                                  const char *response, const char *expected)
{
    char string_value[64];
    value_t val;
    int ret = netrigctl_decode_parm_response(parm, &val, response,
              strlen(response), string_value, sizeof(string_value));

    if (ret != RIG_OK)
    {
        fprintf(stderr, "%s: response decoding failed with %d\n", name, ret);
        return 1;
    }

    if (val.cs != string_value || strcmp(val.cs, expected) != 0)
    {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", name, expected,
                val.cs);
        return 1;
    }

    return 0;
}

static int test_parm_responses(void)
{
    const char embedded_nul[] = { 'B', 'U', 'G', '\0', 'X', '\n' };
    const char embedded_newline[] = "BUG\nX\n";
    char string_value[64];
    char small_value[4];
    value_t val;
    int ret;

    if (expect_string_response("KEYERTYPE STRAIGHT", RIG_PARM_KEYERTYPE,
                               "STRAIGHT\n", "0") != 0
            || expect_string_response("KEYERTYPE BUG", RIG_PARM_KEYERTYPE,
                                      "BUG\r\n", "1") != 0
            || expect_string_response("KEYERTYPE PADDLE", RIG_PARM_KEYERTYPE,
                                      "PADDLE\n", "2") != 0
            || expect_string_response("KEYERTYPE UNKNOWN", RIG_PARM_KEYERTYPE,
                                      "UNKNOWN\n", "4") != 0
            || expect_string_response("numeric KEYERTYPE", RIG_PARM_KEYERTYPE,
                                      "0\n", "0") != 0
            || expect_string_response("BANDSELECT response", RIG_PARM_BANDSELECT,
                                      "BAND160M\n", "BAND160M") != 0)
    {
        return 1;
    }

    ret = netrigctl_decode_parm_response(RIG_PARM_BEEP, &val, "7\n", 2,
                                          NULL, 0);

    if (ret != RIG_OK || val.i != 7)
    {
        fprintf(stderr, "integer response: expected 7, got %d (ret=%d)\n",
                val.i, ret);
        return 1;
    }

    ret = netrigctl_decode_parm_response(RIG_PARM_BANDSELECT, &val,
                                          "BAND160M", 8, string_value,
                                          sizeof(string_value));

    if (ret != -RIG_EPROTO)
    {
        fprintf(stderr, "unterminated response: expected return %d, got %d\n",
                -RIG_EPROTO, ret);
        return 1;
    }

    ret = netrigctl_decode_parm_response(RIG_PARM_KEYERTYPE, &val,
                                          embedded_nul, sizeof(embedded_nul),
                                          string_value, sizeof(string_value));

    if (ret != -RIG_EPROTO)
    {
        fprintf(stderr, "embedded NUL: expected return %d, got %d\n",
                -RIG_EPROTO, ret);
        return 1;
    }

    ret = netrigctl_decode_parm_response(RIG_PARM_KEYERTYPE, &val,
                                          embedded_newline,
                                          sizeof(embedded_newline) - 1,
                                          string_value, sizeof(string_value));

    if (ret != -RIG_EPROTO)
    {
        fprintf(stderr, "embedded newline: expected return %d, got %d\n",
                -RIG_EPROTO, ret);
        return 1;
    }

    ret = netrigctl_decode_parm_response(RIG_PARM_BANDSELECT, &val,
                                          "BAND160M\n", 9, small_value,
                                          sizeof(small_value));

    if (ret != -RIG_ETRUNC)
    {
        fprintf(stderr, "small string buffer: expected return %d, got %d\n",
                -RIG_ETRUNC, ret);
        return 1;
    }

    return 0;
}

int main(void)
{
    char short_value[] = "2=FAST 3=SLOW";
    char maximum_value[] =
        "0=OFF 1=SUPERFAST 2=FAST 3=SLOW 4=USER 5=MEDIUM 6=AUTO 7=LONG";
    char ninth_value[] =
        "0=OFF 1=SUPERFAST 2=FAST 3=SLOW 4=USER 5=MEDIUM 6=AUTO 7=LONG 8=ON";
    char malformed_value[] = "0=OFF invalid 2=FAST";
    const enum agc_level_e none[HAMLIB_MAX_AGC_LEVELS] = {
        RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE,
        RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE
    };
    const enum agc_level_e user[HAMLIB_MAX_AGC_LEVELS] = {
        RIG_AGC_USER, RIG_AGC_USER, RIG_AGC_USER, RIG_AGC_USER,
        RIG_AGC_USER, RIG_AGC_USER, RIG_AGC_USER, RIG_AGC_USER
    };
    const enum agc_level_e short_levels[HAMLIB_MAX_AGC_LEVELS] = {
        RIG_AGC_FAST, RIG_AGC_SLOW, RIG_AGC_NONE, RIG_AGC_NONE,
        RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE, RIG_AGC_NONE
    };
    const enum agc_level_e maximum_levels[HAMLIB_MAX_AGC_LEVELS] = {
        RIG_AGC_OFF, RIG_AGC_SUPERFAST, RIG_AGC_FAST, RIG_AGC_SLOW,
        RIG_AGC_USER, RIG_AGC_MEDIUM, RIG_AGC_AUTO, RIG_AGC_LONG
    };
    struct parse_case cases[] = {
        { "short list", short_value, RIG_OK, 2, short_levels },
        { "maximum list", maximum_value, RIG_OK, HAMLIB_MAX_AGC_LEVELS,
          maximum_levels },
        { "ninth entry truncated", ninth_value, RIG_OK, HAMLIB_MAX_AGC_LEVELS,
          maximum_levels },
        { "malformed token", malformed_value, -RIG_EPROTO, 42, user }
    };
    enum agc_level_e levels[HAMLIB_MAX_AGC_LEVELS];
    int count = 42;

    fill_levels(levels, RIG_AGC_USER);
    dummy_reset_agc_levels(levels, &count);

    if (expect_state("reset", levels, none, count, 0) != 0)
    {
        return 1;
    }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        fill_levels(levels, RIG_AGC_USER);
        count = 42;
        int ret = dummy_parse_agc_levels(cases[i].value, levels, &count);

        if (ret != cases[i].expected_ret)
        {
            fprintf(stderr, "%s: expected return %d, got %d\n", cases[i].name,
                    cases[i].expected_ret, ret);
            return 1;
        }

        if (expect_state(cases[i].name, levels, cases[i].expected_levels,
                         count, cases[i].expected_count) != 0)
        {
            return 1;
        }
    }

    if (test_parm_commands() != 0 || test_parm_responses() != 0)
    {
        return 1;
    }

    return test_positive_rprt();
}
