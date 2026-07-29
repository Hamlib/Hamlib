/*
 * Hamlib Kenwood TH-D74/TH-D75 record codec
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 */

#ifndef HAMLIB_KENWOOD_THD7X_H
#define HAMLIB_KENWOOD_THD7X_H 1

#include <stddef.h>
#include <stdint.h>

#define THD7X_URCALL_MAX 8
#define THD7X_MAX_COMMAND_LENGTH 80
#define THD7X_COMMAND_BUFSIZE (THD7X_MAX_COMMAND_LENGTH + 1)

struct thd7x_fo_record
{
    uint8_t band;
    uint64_t frequency_hz;
    uint64_t offset_hz;
    uint8_t rx_step;
    uint8_t tx_step;
    uint8_t mode;
    uint8_t fine_enabled;
    uint8_t fine_step;
    uint8_t tone_enabled;
    uint8_t ctcss_enabled;
    uint8_t dcs_enabled;
    uint8_t cross_enabled;
    uint8_t reverse_enabled;
    uint8_t shift;
    uint8_t tone_index;
    uint8_t ctcss_index;
    uint8_t dcs_index;
    uint8_t cross_selector;
    char urcall[THD7X_URCALL_MAX + 1];
    uint8_t digital_squelch_type;
    uint8_t digital_squelch_code;
};

struct thd7x_me_record
{
    uint16_t channel;
    uint64_t frequency_hz;
    uint64_t offset_hz;
    uint8_t rx_step;
    uint8_t tx_step;
    uint8_t mode;
    uint8_t fine_enabled;
    uint8_t fine_step;
    uint8_t tone_enabled;
    uint8_t ctcss_enabled;
    uint8_t dcs_enabled;
    uint8_t cross_enabled;
    uint8_t reverse_enabled;
    uint8_t odd_split_enabled;
    uint8_t shift;
    uint8_t tone_index;
    uint8_t ctcss_index;
    uint8_t dcs_index;
    uint8_t cross_selector;
    char urcall[THD7X_URCALL_MAX + 1];
    uint8_t digital_squelch_type;
    uint8_t digital_squelch_code;
    uint8_t lockout_enabled;
};

int thd7x_parse_fo(const char *input, size_t input_len,
                   struct thd7x_fo_record *record);
int thd7x_serialize_fo(const struct thd7x_fo_record *record, char *output,
                       size_t output_size, size_t *output_len);
int thd7x_parse_me(const char *input, size_t input_len,
                   struct thd7x_me_record *record);
int thd7x_serialize_me(const struct thd7x_me_record *record, char *output,
                       size_t output_size, size_t *output_len);

#endif
