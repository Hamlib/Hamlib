/*
 * Hamlib Kenwood TH-D74/TH-D75 record codec
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
#include "thd7x.h"

#define THD7X_FREQUENCY_MAX UINT64_C(9999999999)

struct thd7x_cursor
{
    const char *next;
    size_t remaining;
};

struct thd7x_field
{
    const char *value;
    size_t length;
};

static int thd7x_next_field(struct thd7x_cursor *cursor, int final,
                            struct thd7x_field *field)
{
    size_t length = 0;

    while (length < cursor->remaining && cursor->next[length] != ',')
    {
        length++;
    }

    if ((!final && length == cursor->remaining)
            || (final && length != cursor->remaining))
    {
        return -RIG_EPROTO;
    }

    field->value = cursor->next;
    field->length = length;
    cursor->next += length + (final ? 0 : 1);
    cursor->remaining -= length + (final ? 0 : 1);

    return RIG_OK;
}

static int thd7x_decimal_field(struct thd7x_cursor *cursor, int final,
                               size_t width, uint64_t maximum,
                               uint64_t *value)
{
    struct thd7x_field field;
    uint64_t parsed = 0;
    int retval;

    retval = thd7x_next_field(cursor, final, &field);

    if (retval != RIG_OK || field.length != width)
    {
        return -RIG_EPROTO;
    }

    for (size_t i = 0; i < field.length; i++)
    {
        uint64_t digit;

        if (field.value[i] < '0' || field.value[i] > '9')
        {
            return -RIG_EPROTO;
        }

        digit = (uint64_t)(field.value[i] - '0');

        if (digit > maximum || parsed > (maximum - digit) / 10)
        {
            return -RIG_EPROTO;
        }

        parsed = parsed * 10 + digit;
    }

    *value = parsed;
    return RIG_OK;
}

static int thd7x_step_field(struct thd7x_cursor *cursor, uint8_t *step)
{
    struct thd7x_field field;
    char code;
    int retval;

    retval = thd7x_next_field(cursor, 0, &field);

    if (retval != RIG_OK || field.length != 1)
    {
        return -RIG_EPROTO;
    }

    code = field.value[0];

    if (code >= '0' && code <= '9')
    {
        *step = (uint8_t)(code - '0');
        return RIG_OK;
    }

    if (code == 'A' || code == 'B')
    {
        *step = (uint8_t)(10 + code - 'A');
        return RIG_OK;
    }

    return -RIG_EPROTO;
}

static int thd7x_urcall_field(struct thd7x_cursor *cursor, char *urcall)
{
    struct thd7x_field field;
    int retval;

    retval = thd7x_next_field(cursor, 0, &field);

    if (retval != RIG_OK || field.length > THD7X_URCALL_MAX)
    {
        return -RIG_EPROTO;
    }

    for (size_t i = 0; i < field.length; i++)
    {
        if (field.value[i] == '\0' || field.value[i] == '\r'
                || field.value[i] == '\n')
        {
            return -RIG_EPROTO;
        }
    }

    memcpy(urcall, field.value, field.length);
    urcall[field.length] = '\0';
    return RIG_OK;
}

static int thd7x_start_cursor(const char *input, size_t input_len,
                              const char *prefix,
                              struct thd7x_cursor *cursor)
{
    const size_t prefix_len = 3;

    if (input_len > 0 && input[input_len - 1] == '\r')
    {
        input_len--;
    }

    if (input_len < prefix_len || memcmp(input, prefix, prefix_len) != 0)
    {
        return -RIG_EPROTO;
    }

    cursor->next = input + prefix_len;
    cursor->remaining = input_len - prefix_len;
    return RIG_OK;
}

static int thd7x_urcall_length(const char *urcall, size_t *length)
{
    for (size_t i = 0; i <= THD7X_URCALL_MAX; i++)
    {
        if (urcall[i] == '\0')
        {
            *length = i;
            return RIG_OK;
        }

        if (urcall[i] == ',' || urcall[i] == '\r' || urcall[i] == '\n')
        {
            return -RIG_EINVAL;
        }
    }

    return -RIG_EINVAL;
}

static char thd7x_step_code(uint8_t step)
{
    return step < 10 ? (char)('0' + step) : (char)('A' + step - 10);
}

static int thd7x_fo_valid(const struct thd7x_fo_record *record,
                          size_t *urcall_len)
{
    if (record->band > 1 || record->frequency_hz > THD7X_FREQUENCY_MAX
            || record->offset_hz > THD7X_FREQUENCY_MAX
            || record->rx_step > 11 || record->tx_step > 11
            || record->mode > 9 || record->fine_enabled > 1
            || record->fine_step > 3 || record->tone_enabled > 1
            || record->ctcss_enabled > 1 || record->dcs_enabled > 1
            || record->cross_enabled > 1 || record->reverse_enabled > 1
            || record->shift > 3 || record->tone_index > 41
            || record->ctcss_index > 41 || record->dcs_index > 103
            || record->cross_selector > 3
            || record->digital_squelch_type > 2
            || record->digital_squelch_code > 99)
    {
        return -RIG_EINVAL;
    }

    return thd7x_urcall_length(record->urcall, urcall_len);
}

static int thd7x_me_valid(const struct thd7x_me_record *record,
                          size_t *urcall_len)
{
    if (record->channel > 999 || record->frequency_hz > THD7X_FREQUENCY_MAX
            || record->offset_hz > THD7X_FREQUENCY_MAX
            || record->rx_step > 11 || record->tx_step > 11
            || record->mode > 9 || record->fine_enabled > 1
            || record->fine_step > 3 || record->tone_enabled > 1
            || record->ctcss_enabled > 1 || record->dcs_enabled > 1
            || record->cross_enabled > 1 || record->reverse_enabled > 1
            || record->odd_split_enabled > 1 || record->shift > 3
            || record->tone_index > 41 || record->ctcss_index > 41
            || record->dcs_index > 103 || record->cross_selector > 3
            || record->digital_squelch_type > 2
            || record->digital_squelch_code > 99
            || record->lockout_enabled > 1)
    {
        return -RIG_EINVAL;
    }

    return thd7x_urcall_length(record->urcall, urcall_len);
}

int thd7x_parse_fo(const char *input, size_t input_len,
                   struct thd7x_fo_record *record)
{
    struct thd7x_fo_record parsed = { 0 };
    struct thd7x_cursor cursor;
    uint64_t value;

    if (input == NULL || record == NULL)
    {
        return -RIG_EINVAL;
    }

    if (thd7x_start_cursor(input, input_len, "FO ", &cursor) != RIG_OK
            || thd7x_decimal_field(&cursor, 0, 1, 1, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.band = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 0, 10, THD7X_FREQUENCY_MAX,
                            &parsed.frequency_hz) != RIG_OK
            || thd7x_decimal_field(&cursor, 0, 10, THD7X_FREQUENCY_MAX,
                                    &parsed.offset_hz) != RIG_OK
            || thd7x_step_field(&cursor, &parsed.rx_step) != RIG_OK
            || thd7x_step_field(&cursor, &parsed.tx_step) != RIG_OK
            || thd7x_decimal_field(&cursor, 0, 1, 9, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.mode = (uint8_t)value;

#define THD7X_PARSE_FO_FIELD(member, maximum) \
    if (thd7x_decimal_field(&cursor, 0, 1, maximum, &value) != RIG_OK) \
    { \
        return -RIG_EPROTO; \
    } \
    parsed.member = (uint8_t)value

    THD7X_PARSE_FO_FIELD(fine_enabled, 1);
    THD7X_PARSE_FO_FIELD(fine_step, 3);
    THD7X_PARSE_FO_FIELD(tone_enabled, 1);
    THD7X_PARSE_FO_FIELD(ctcss_enabled, 1);
    THD7X_PARSE_FO_FIELD(dcs_enabled, 1);
    THD7X_PARSE_FO_FIELD(cross_enabled, 1);
    THD7X_PARSE_FO_FIELD(reverse_enabled, 1);
    THD7X_PARSE_FO_FIELD(shift, 3);

#undef THD7X_PARSE_FO_FIELD

    if (thd7x_decimal_field(&cursor, 0, 2, 41, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.tone_index = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 0, 2, 41, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.ctcss_index = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 0, 3, 103, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.dcs_index = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 0, 1, 3, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.cross_selector = (uint8_t)value;

    if (thd7x_urcall_field(&cursor, parsed.urcall) != RIG_OK
            || thd7x_decimal_field(&cursor, 0, 1, 2, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.digital_squelch_type = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 1, 2, 99, &value) != RIG_OK
            || cursor.remaining != 0)
    {
        return -RIG_EPROTO;
    }

    parsed.digital_squelch_code = (uint8_t)value;
    *record = parsed;
    return RIG_OK;
}

int thd7x_serialize_fo(const struct thd7x_fo_record *record, char *output,
                       size_t output_size, size_t *output_len)
{
    size_t urcall_len;
    int length;

    if (record == NULL || output == NULL)
    {
        return -RIG_EINVAL;
    }

    if (thd7x_fo_valid(record, &urcall_len) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    length = snprintf(output, output_size,
                      "FO %u,%010" PRIu64 ",%010" PRIu64
                      ",%c,%c,%u,%u,%u,%u,%u,%u,%u,%u,%u"
                      ",%02u,%02u,%03u,%u,%.*s,%u,%02u",
                      (unsigned int)record->band, record->frequency_hz,
                      record->offset_hz, thd7x_step_code(record->rx_step),
                      thd7x_step_code(record->tx_step),
                      (unsigned int)record->mode,
                      (unsigned int)record->fine_enabled,
                      (unsigned int)record->fine_step,
                      (unsigned int)record->tone_enabled,
                      (unsigned int)record->ctcss_enabled,
                      (unsigned int)record->dcs_enabled,
                      (unsigned int)record->cross_enabled,
                      (unsigned int)record->reverse_enabled,
                      (unsigned int)record->shift,
                      (unsigned int)record->tone_index,
                      (unsigned int)record->ctcss_index,
                      (unsigned int)record->dcs_index,
                      (unsigned int)record->cross_selector,
                      (int)urcall_len, record->urcall,
                      (unsigned int)record->digital_squelch_type,
                      (unsigned int)record->digital_squelch_code);

    if (length < 0)
    {
        return -RIG_EINTERNAL;
    }

    if (output_len != NULL)
    {
        *output_len = (size_t)length;
    }

    return (size_t)length < output_size ? RIG_OK : -RIG_ETRUNC;
}

int thd7x_parse_me(const char *input, size_t input_len,
                   struct thd7x_me_record *record)
{
    struct thd7x_me_record parsed = { 0 };
    struct thd7x_cursor cursor;
    uint64_t value;

    if (input == NULL || record == NULL)
    {
        return -RIG_EINVAL;
    }

    if (thd7x_start_cursor(input, input_len, "ME ", &cursor) != RIG_OK
            || thd7x_decimal_field(&cursor, 0, 3, 999, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.channel = (uint16_t)value;

    if (thd7x_decimal_field(&cursor, 0, 10, THD7X_FREQUENCY_MAX,
                            &parsed.frequency_hz) != RIG_OK
            || thd7x_decimal_field(&cursor, 0, 10, THD7X_FREQUENCY_MAX,
                                    &parsed.offset_hz) != RIG_OK
            || thd7x_step_field(&cursor, &parsed.rx_step) != RIG_OK
            || thd7x_step_field(&cursor, &parsed.tx_step) != RIG_OK
            || thd7x_decimal_field(&cursor, 0, 1, 9, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.mode = (uint8_t)value;

#define THD7X_PARSE_ME_FIELD(member, maximum) \
    if (thd7x_decimal_field(&cursor, 0, 1, maximum, &value) != RIG_OK) \
    { \
        return -RIG_EPROTO; \
    } \
    parsed.member = (uint8_t)value

    THD7X_PARSE_ME_FIELD(fine_enabled, 1);
    THD7X_PARSE_ME_FIELD(fine_step, 3);
    THD7X_PARSE_ME_FIELD(tone_enabled, 1);
    THD7X_PARSE_ME_FIELD(ctcss_enabled, 1);
    THD7X_PARSE_ME_FIELD(dcs_enabled, 1);
    THD7X_PARSE_ME_FIELD(cross_enabled, 1);
    THD7X_PARSE_ME_FIELD(reverse_enabled, 1);
    THD7X_PARSE_ME_FIELD(odd_split_enabled, 1);
    THD7X_PARSE_ME_FIELD(shift, 3);

#undef THD7X_PARSE_ME_FIELD

    if (thd7x_decimal_field(&cursor, 0, 2, 41, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.tone_index = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 0, 2, 41, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.ctcss_index = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 0, 3, 103, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.dcs_index = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 0, 1, 3, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.cross_selector = (uint8_t)value;

    if (thd7x_urcall_field(&cursor, parsed.urcall) != RIG_OK
            || thd7x_decimal_field(&cursor, 0, 1, 2, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.digital_squelch_type = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 0, 2, 99, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.digital_squelch_code = (uint8_t)value;

    if (thd7x_decimal_field(&cursor, 1, 1, 1, &value) != RIG_OK
            || cursor.remaining != 0)
    {
        return -RIG_EPROTO;
    }

    parsed.lockout_enabled = (uint8_t)value;
    *record = parsed;
    return RIG_OK;
}

int thd7x_serialize_me(const struct thd7x_me_record *record, char *output,
                       size_t output_size, size_t *output_len)
{
    size_t urcall_len;
    int length;

    if (record == NULL || output == NULL)
    {
        return -RIG_EINVAL;
    }

    if (thd7x_me_valid(record, &urcall_len) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    length = snprintf(output, output_size,
                      "ME %03u,%010" PRIu64 ",%010" PRIu64
                      ",%c,%c,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u"
                      ",%02u,%02u,%03u,%u,%.*s,%u,%02u,%u",
                      (unsigned int)record->channel, record->frequency_hz,
                      record->offset_hz, thd7x_step_code(record->rx_step),
                      thd7x_step_code(record->tx_step),
                      (unsigned int)record->mode,
                      (unsigned int)record->fine_enabled,
                      (unsigned int)record->fine_step,
                      (unsigned int)record->tone_enabled,
                      (unsigned int)record->ctcss_enabled,
                      (unsigned int)record->dcs_enabled,
                      (unsigned int)record->cross_enabled,
                      (unsigned int)record->reverse_enabled,
                      (unsigned int)record->odd_split_enabled,
                      (unsigned int)record->shift,
                      (unsigned int)record->tone_index,
                      (unsigned int)record->ctcss_index,
                      (unsigned int)record->dcs_index,
                      (unsigned int)record->cross_selector,
                      (int)urcall_len, record->urcall,
                      (unsigned int)record->digital_squelch_type,
                      (unsigned int)record->digital_squelch_code,
                      (unsigned int)record->lockout_enabled);

    if (length < 0)
    {
        return -RIG_EINTERNAL;
    }

    if (output_len != NULL)
    {
        *output_len = (size_t)length;
    }

    return (size_t)length < output_size ? RIG_OK : -RIG_ETRUNC;
}
