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
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"

#include "rigctl_parse.h"

int lock_mode;
powerstat_t rig_powerstat = RIG_POWER_ON;

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
    static const char *comma_locales[] = {
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

    {
        char *set_freq_dot[] = { "testctlparser", "F", "14.25" };
        char *set_freq_comma[] = { "testctlparser", "F", "14,25" };
        char *set_split_comma[] = { "testctlparser", "I", "7,125" };
        char *set_split_mode_comma[] = {
            "testctlparser", "K", "7,125", "USB", "2400"
        };
        char *set_level_comma[] = {
            "testctlparser", "L", "AF", "0,5"
        };
        char *set_parm_comma[] = {
            "testctlparser", "P", "BACKLIGHT", "0,5"
        };
        char *power_comma[] = {
            "testctlparser", "2", "0,5", "14,25", "USB"
        };
        char *mw_power_comma[] = {
            "testctlparser", "4", "100", "14,25", "USB"
        };
        char *invalid_trailing[] = { "testctlparser", "F", "14.25junk" };
        char *invalid_mixed[] = { "testctlparser", "F", "14,25.5" };
        char *invalid_nonfinite[] = { "testctlparser", "F", "nan" };
        char *clock_dot[] = {
            "testctlparser", "set_clock", "2026-01-02T03:04:05.500+00:00"
        };
        char *clock_comma[] = {
            "testctlparser", "set_clock", "2026-01-02T03:04:05,500+00:00"
        };
        char *clock_missing_separator[] = {
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
