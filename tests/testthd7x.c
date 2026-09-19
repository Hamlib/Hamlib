/*
 * Hamlib Kenwood TH-D74/TH-D75 record codec tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "../rigs/kenwood/thd7x.h"

static int failures;

#define EXPECT_TRUE(name, condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, name); \
            failures++; \
        } \
    } while (0)

#define EXPECT_INT(name, expected, actual) \
    do \
    { \
        int expected_value = (expected); \
        int actual_value = (actual); \
        if (expected_value != actual_value) \
        { \
            fprintf(stderr, "%s:%d: %s expected %d, got %d\n", \
                    __FILE__, __LINE__, name, expected_value, actual_value); \
            failures++; \
        } \
    } while (0)

static void expect_string(const char *name, const char *expected,
                          const char *actual)
{
    if (strcmp(expected, actual) != 0)
    {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", name,
                expected, actual);
        failures++;
    }
}

static void test_analog_fo(void)
{
    static const char command[] =
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00";
    struct thd7x_fo_record record;
    char output[THD7X_COMMAND_BUFSIZE];
    size_t output_len = 0;

    EXPECT_INT("parse analog FO", RIG_OK,
               thd7x_parse_fo(command, sizeof(command) - 1, &record));
    EXPECT_INT("FO band", 0, record.band);
    EXPECT_TRUE("FO frequency", record.frequency_hz == UINT64_C(146520000));
    EXPECT_TRUE("FO offset", record.offset_hz == UINT64_C(600000));
    EXPECT_INT("FO RX step", 0, record.rx_step);
    EXPECT_INT("FO mode", 0, record.mode);
    EXPECT_INT("FO tone", 1, record.tone_enabled);
    EXPECT_INT("FO shift", 2, record.shift);
    EXPECT_INT("FO tone index", 8, record.tone_index);
    expect_string("FO empty URCALL", "", record.urcall);
    EXPECT_INT("serialize analog FO", RIG_OK,
               thd7x_serialize_fo(&record, output, sizeof(output), &output_len));
    expect_string("analog FO round trip", command, output);
    EXPECT_TRUE("analog FO length", output_len == strlen(command));
}

static void test_dv_fo(void)
{
    static const char command[] =
        "FO 1,0440000000,0000000000,A,B,1,0,0,0,0,0,0,0,0,12,12,047,0,CQCQCQ,2,23";
    struct thd7x_fo_record record;
    struct thd7x_fo_record reparsed;
    char output[THD7X_COMMAND_BUFSIZE];

    EXPECT_INT("parse DV FO", RIG_OK,
               thd7x_parse_fo(command, sizeof(command) - 1, &record));
    EXPECT_INT("DV FO RX step A", 10, record.rx_step);
    EXPECT_INT("DV FO TX step B", 11, record.tx_step);
    EXPECT_INT("DV FO mode", 1, record.mode);
    expect_string("DV FO URCALL", "CQCQCQ", record.urcall);
    EXPECT_INT("DV FO squelch type", 2, record.digital_squelch_type);
    EXPECT_INT("serialize DV FO", RIG_OK,
               thd7x_serialize_fo(&record, output, sizeof(output), NULL));
    expect_string("DV FO canonical form", command, output);
    EXPECT_INT("reparse DV FO", RIG_OK,
               thd7x_parse_fo(output, strlen(output), &reparsed));
    EXPECT_INT("reparsed DV FO RX step", record.rx_step, reparsed.rx_step);
    expect_string("reparsed DV FO URCALL", record.urcall, reparsed.urcall);
}

static void test_analog_me(void)
{
    static const char command[] =
        "ME 042,0145370000,0000600000,0,0,0,0,1,1,0,0,0,0,0,2,12,12,000,0,CQCQCQ,0,00,1";
    struct thd7x_me_record record;
    char output[THD7X_COMMAND_BUFSIZE];

    EXPECT_INT("parse analog ME", RIG_OK,
               thd7x_parse_me(command, sizeof(command) - 1, &record));
    EXPECT_INT("ME channel", 42, record.channel);
    EXPECT_TRUE("ME frequency", record.frequency_hz == UINT64_C(145370000));
    EXPECT_TRUE("ME offset", record.offset_hz == UINT64_C(600000));
    EXPECT_INT("ME mode", 0, record.mode);
    EXPECT_INT("ME fine step", 1, record.fine_step);
    EXPECT_INT("ME tone index", 12, record.tone_index);
    EXPECT_INT("ME lockout", 1, record.lockout_enabled);
    EXPECT_INT("serialize analog ME", RIG_OK,
               thd7x_serialize_me(&record, output, sizeof(output), NULL));
    expect_string("analog ME round trip", command, output);
}

static void test_79_byte_dv_me(void)
{
    static const char command[] =
        "ME 010,0145370000,0000600000,0,0,1,0,1,1,0,0,0,0,0,2,12,12,000,0,CQCQCQ,0,00,0\r";
    static const char canonical[] =
        "ME 010,0145370000,0000600000,0,0,1,0,1,1,0,0,0,0,0,2,12,12,000,0,CQCQCQ,0,00,0";
    struct thd7x_me_record record;
    struct thd7x_me_record reparsed;
    char output[THD7X_COMMAND_BUFSIZE];
    size_t output_len = 0;

    EXPECT_TRUE("DV ME fixture is 79 bytes", sizeof(command) - 1 == 79);
    EXPECT_INT("parse 79-byte DV ME", RIG_OK,
               thd7x_parse_me(command, sizeof(command) - 1, &record));
    EXPECT_INT("DV ME mode", 1, record.mode);
    expect_string("DV ME URCALL", "CQCQCQ", record.urcall);
    EXPECT_INT("serialize DV ME", RIG_OK,
               thd7x_serialize_me(&record, output, sizeof(output), &output_len));
    expect_string("DV ME strips terminal CR", canonical, output);
    EXPECT_TRUE("DV ME canonical length", output_len == sizeof(canonical) - 1);
    EXPECT_INT("reparse DV ME", RIG_OK,
               thd7x_parse_me(output, output_len, &reparsed));
    EXPECT_INT("reparsed DV ME channel", record.channel, reparsed.channel);
    expect_string("reparsed DV ME URCALL", record.urcall, reparsed.urcall);
}

static void test_empty_and_max_urcall(void)
{
    static const char empty_command[] =
        "ME 999,9999999999,9999999999,B,A,9,1,3,1,1,1,1,1,1,3,41,41,103,3,,2,99,1";
    static const char max_command[] =
        "ME 999,9999999999,9999999999,B,A,9,1,3,1,1,1,1,1,1,3,41,41,103,3,ABCDEFGH,2,99,1";
    struct thd7x_me_record record;
    char output[THD7X_COMMAND_BUFSIZE];
    size_t output_len = 0;

    EXPECT_INT("parse empty URCALL", RIG_OK,
               thd7x_parse_me(empty_command, sizeof(empty_command) - 1,
                              &record));
    expect_string("preserve empty URCALL", "", record.urcall);
    EXPECT_INT("serialize empty URCALL", RIG_OK,
               thd7x_serialize_me(&record, output, sizeof(output), NULL));
    expect_string("empty URCALL round trip", empty_command, output);

    EXPECT_INT("parse max URCALL", RIG_OK,
               thd7x_parse_me(max_command, sizeof(max_command) - 1, &record));
    EXPECT_INT("serialize max command", RIG_OK,
               thd7x_serialize_me(&record, output, sizeof(output), &output_len));
    EXPECT_TRUE("maximum command length",
                output_len == THD7X_MAX_COMMAND_LENGTH);
    expect_string("max URCALL round trip", max_command, output);
}

static void test_malformed_fo(void)
{
    static const char *const malformed[] = {
        "FX 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00",
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,",
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00,0",
        "FO 0,,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00",
        "FO 0,X146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00",
        "FO 0,00146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00",
        "FO 0,0146520000,0000600000,C,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00",
        "FO 2,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00",
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,42,08,000,0,,0,00",
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,104,0,,0,00",
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,123456789,0,00",
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00\n",
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00\rX"
    };
    struct thd7x_fo_record record;

    for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); i++)
    {
        EXPECT_INT("reject malformed FO", -RIG_EPROTO,
                   thd7x_parse_fo(malformed[i], strlen(malformed[i]), &record));
    }

    EXPECT_INT("reject null FO input", -RIG_EINVAL,
               thd7x_parse_fo(NULL, 0, &record));
}

static void test_malformed_me(void)
{
    static const char *const malformed[] = {
        "ME 1000,0145370000,0000600000,0,0,0,0,1,1,0,0,0,0,0,2,12,12,000,0,,0,00,0",
        "ME 010,0145370000,0000600000,0,0,0,0,1,1,0,0,0,0,0,2,12,12,000,0,,0,00,0,1",
        "ME 010,0145370000,0000600000,0,0,A,0,1,1,0,0,0,0,0,2,12,12,000,0,,0,00,0",
        "ME 010,0145370000,0000600000,0,0,0,0,1,1,0,0,0,0,0,2,12,12,000,0,,0,00,"
    };
    struct thd7x_me_record record;

    for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); i++)
    {
        EXPECT_INT("reject malformed ME", -RIG_EPROTO,
                   thd7x_parse_me(malformed[i], strlen(malformed[i]), &record));
    }
}

static void test_length_delimited_input(void)
{
    static const char command[] =
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00";
    char unterminated[sizeof(command) - 1];
    struct thd7x_fo_record record;

    memcpy(unterminated, command, sizeof(unterminated));
    EXPECT_INT("parse unterminated buffer", RIG_OK,
               thd7x_parse_fo(unterminated, sizeof(unterminated), &record));

    for (size_t i = 0; i < sizeof(unterminated); i++)
    {
        EXPECT_INT("reject every truncated prefix", -RIG_EPROTO,
                   thd7x_parse_fo(unterminated, i, &record));
    }
}

static void test_serializer_errors(void)
{
    static const char command[] =
        "FO 0,0146520000,0000600000,0,0,0,0,0,1,0,0,0,0,2,08,08,000,0,,0,00";
    struct thd7x_fo_record record;
    char output[8];
    size_t required = 0;

    EXPECT_INT("parse serializer fixture", RIG_OK,
               thd7x_parse_fo(command, sizeof(command) - 1, &record));
    EXPECT_INT("report truncated output", -RIG_ETRUNC,
               thd7x_serialize_fo(&record, output, sizeof(output), &required));
    EXPECT_TRUE("report required size", required == strlen(command));
    EXPECT_TRUE("truncated output terminated", output[sizeof(output) - 1] == '\0');

    record.mode = 10;
    EXPECT_INT("reject invalid serialized value", -RIG_EINVAL,
               thd7x_serialize_fo(&record, output, sizeof(output), NULL));
    EXPECT_INT("reject null output", -RIG_EINVAL,
               thd7x_serialize_fo(&record, NULL, 0, NULL));
}

int main(void)
{
    test_analog_fo();
    test_dv_fo();
    test_analog_me();
    test_79_byte_dv_me();
    test_empty_and_max_urcall();
    test_malformed_fo();
    test_malformed_me();
    test_length_delimited_input();
    test_serializer_errors();

    if (failures != 0)
    {
        fprintf(stderr, "%d TH-D7x record codec test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
