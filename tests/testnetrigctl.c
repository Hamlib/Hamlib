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

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
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

#ifdef HAVE_NETRIGCTL_SOCKET_TEST
enum wire_operation
{
    WIRE_SET_FREQ,
    WIRE_SET_SPLIT_FREQ,
    WIRE_SET_LEVEL,
    WIRE_SET_PARM,
    WIRE_MW2POWER,
    WIRE_POWER2MW
};

struct wire_case
{
    const char *name;
    enum wire_operation operation;
    const char *expected_command;
    const char *response;
};

struct wire_peer
{
    int fd;
    const char *expected_command;
    const char *response;
    int status;
};
#endif

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
static int read_line(int fd, char *buffer, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        ssize_t count = read(fd, buffer + used, 1);

        if (count != 1) { return -1; }

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

        if (count <= 0) { return -1; }

        written += (size_t)count;
    }

    return 0;
}

static void *run_wire_peer(void *arg)
{
    struct wire_peer *peer = arg;
    char command[128];
    int command_matches;

    peer->status = -1;

    if (read_line(peer->fd, command, sizeof(command)) != 0) { return NULL; }

    command_matches = strcmp(command, peer->expected_command) == 0;

    if (!command_matches)
    {
        fprintf(stderr, "expected command '%s', got '%s'", peer->expected_command,
                command);
    }

    if (write_all(peer->fd, peer->response, strlen(peer->response)) != 0)
    {
        return NULL;
    }

    peer->status = command_matches ? 0 : 1;
    return NULL;
}

static int invoke_wire_operation(RIG *rig, enum wire_operation operation)
{
    value_t value = { .f = 0.5f };
    unsigned int mwpower = 0;
    float power = 0.0f;

    switch (operation)
    {
    case WIRE_SET_FREQ:
        return rig->caps->set_freq(rig, RIG_VFO_A, 7177000.0);

    case WIRE_SET_SPLIT_FREQ:
        return rig->caps->set_split_freq(rig, RIG_VFO_A, 7177000.0);

    case WIRE_SET_LEVEL:
        return rig->caps->set_level(rig, RIG_VFO_A, RIG_LEVEL_AF, value);

    case WIRE_SET_PARM:
        return rig->caps->set_parm(rig, RIG_PARM_BACKLIGHT, value);

    case WIRE_MW2POWER:
        return rig->caps->mW2power(rig, &power, 100000, 1296000000.0,
                                   RIG_MODE_PKTUSB);

    case WIRE_POWER2MW:
        return rig->caps->power2mW(rig, &mwpower, 0.5f, 7177000.0,
                                   RIG_MODE_USB);
    }

    return -RIG_EINVAL;
}

static int run_wire_case(const struct wire_case *test)
{
    int sockets[2];
    pthread_t thread;
    struct wire_peer peer = { .fd = -1,
        .expected_command = test->expected_command,
        .response = test->response,
        .status = -1
    };
    RIG *rig;
    int retval;

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

    rig = rig_init(RIG_MODEL_NETRIGCTL);

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
    retval = invoke_wire_operation(rig, test->operation);

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);

    if (peer.status != 0 || retval != RIG_OK)
    {
        fprintf(stderr, "%s: expected status 0/%d, got %d/%d\n", test->name,
                RIG_OK, peer.status, retval);
        return 1;
    }

    if (strcmp(localeconv()->decimal_point, ",") != 0)
    {
        fprintf(stderr, "%s: numeric locale was not restored\n", test->name);
        return 1;
    }

    return 0;
}

static int test_locale_output(void)
{
    static const char *comma_locales[] = {
        "de_DE.UTF-8",
        "de_DE.utf8",
        "fr_FR.UTF-8",
        "fr_FR.utf8",
        "German_Germany.1252",
        "French_France.1252"
    };
    static const struct wire_case cases[] = {
        { "set frequency", WIRE_SET_FREQ, "F 7177000.000000\n", "RPRT 0\n" },
        { "set split frequency", WIRE_SET_SPLIT_FREQ,
          "I 7177000.000000\n", "RPRT 0\n" },
        { "set level", WIRE_SET_LEVEL, "L AF 0.500000\n", "RPRT 0\n" },
        { "set parameter", WIRE_SET_PARM, "P BACKLIGHT 0.500000\n",
          "RPRT 0\n" },
        { "mW to power", WIRE_MW2POWER,
          "\\mW2power 100000 1296000000 PKTUSB\n", "1\n" },
        { "power to mW", WIRE_POWER2MW,
          "\\power2mW 0.500 7177000 USB\n", "1000\n" }
    };
    const char *current_locale = setlocale(LC_NUMERIC, NULL);
    char *saved_locale = current_locale == NULL ? NULL : strdup(current_locale);
    int comma_locale_found = 0;
    int result = 0;

    if (current_locale != NULL && saved_locale == NULL)
    {
        return 1;
    }

    for (size_t i = 0; i < sizeof(comma_locales) / sizeof(comma_locales[0]); i++)
    {
        if (setlocale(LC_NUMERIC, comma_locales[i]) != NULL &&
                strcmp(localeconv()->decimal_point, ",") == 0)
        {
            comma_locale_found = 1;
            break;
        }
    }

    if (!comma_locale_found)
    {
        fprintf(stderr, "testnetrigctl: no comma-decimal locale available; "
                "skipping locale output cases\n");
        result = 77;
        goto done;
    }

    rig_register(&netrigctl_caps);

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_wire_case(&cases[i]) != 0)
        {
            result = 1;
            break;
        }
    }

done:
    if (saved_locale != NULL)
    {
        setlocale(LC_NUMERIC, saved_locale);
        free(saved_locale);
    }

    return result;
}
#else
static int test_locale_output(void)
{
    fprintf(stderr, "testnetrigctl: socketpair unavailable; "
            "skipping locale output cases\n");
    return 77;
}
#endif

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

    return test_locale_output();
}
