/*
 * Hamlib Mini backend - main header
 * Copyright (c) 2026 by Hamlib Team & DG3BP
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ATS_H
#define _ATS_H 1

#include "hamlib/rig.h"

extern struct rig_caps ats_mini_caps;

#define MINI_BANDNAME_LEN 8

struct ats_mini_mon_data {
    freq_t freq;
    char bandname[MINI_BANDNAME_LEN];
    rmode_t mode;
    freq_t step;
    freq_t bandwidth;
    int attenuation;
    int volume;
    int RSSI;
    int SNR;
    int seq_no;
    double freq_khz_raw;
    double bfo_hz;
};

#define MINI_UART_CMD_DLY 10
#define MINI_UART_TO_CNT 200
#define MINI_UART_BUFF_SIZE 128
#define MINI_UART_MAX_TOKEN 20

#define MINI_VOLUME_MIN 0
#define MINI_VOLUME_MAX 63

typedef enum MINI_BAND_ID {
    MINI_BAND_VHF = 0,
    MINI_BAND_ALL,
    MINI_BAND_11M,
    MINI_BAND_13M,
    MINI_BAND_15ML,
    MINI_BAND_16M,
    MINI_BAND_19M,
    MINI_BAND_22M,
    MINI_BAND_25M,
    MINI_BAND_31M,
    MINI_BAND_41M,
    MINI_BAND_49M,
    MINI_BAND_60M,
    MINI_BAND_75M,
    MINI_BAND_90M,
    MINI_BAND_MW3,
    MINI_BAND_MW2,
    MINI_BAND_MW1,
    MINI_BAND_160M,
    MINI_BAND_80M,
    MINI_BAND_40M,
    MINI_BAND_30M,
    MINI_BAND_20M,
    MINI_BAND_17M,
    MINI_BAND_15MH,
    MINI_BAND_12M,
    MINI_BAND_10M,
    MINI_BAND_CB,
} mini_band_id_t;

static const char * const mini_band_ids[] = {
    [MINI_BAND_VHF] = "VHF",
    [MINI_BAND_ALL] = "ALL",
    [MINI_BAND_11M] = "11M",
    [MINI_BAND_13M] = "13M",
    [MINI_BAND_15ML] = "15M",
    [MINI_BAND_16M] = "16M",
    [MINI_BAND_19M] = "19M",
    [MINI_BAND_22M] = "22M",
    [MINI_BAND_25M] = "25M",
    [MINI_BAND_31M] = "31M",
    [MINI_BAND_41M] = "41M",
    [MINI_BAND_49M] = "49M",
    [MINI_BAND_60M] = "60M",
    [MINI_BAND_75M] = "75M",
    [MINI_BAND_90M] = "90M",
    [MINI_BAND_MW3] = "MW3",
    [MINI_BAND_MW2] = "MW2",
    [MINI_BAND_MW1] = "MW1",
    [MINI_BAND_160M] = "160M",
    [MINI_BAND_80M] = "80M",
    [MINI_BAND_40M] = "40M",
    [MINI_BAND_30M] = "30M",
    [MINI_BAND_20M] = "20M",
    [MINI_BAND_17M] = "17M",
    [MINI_BAND_15MH] = "15M",
    [MINI_BAND_12M] = "12M",
    [MINI_BAND_10M] = "10M",
    [MINI_BAND_CB] = "CB",
};

#endif /* _ATS_H */
