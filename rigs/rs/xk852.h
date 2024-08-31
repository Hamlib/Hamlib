/*
 *  Hamlib R&S backend for XK852 - main header
 *  Reused from rs.c
 *  Based on xk2000.h
 *  Copyright (c) 2024 by Marc Fontaine DM1MF
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _XK852_H
#define _XK852_H 1

#undef BACKEND_VER
#define BACKEND_VER "20240921"

#include <hamlib/rig.h>

typedef enum {
    XK852_CMD_ADDRESS = 'A',
    XK852_CMD_BFO = 'B',
    XK852_CMD_FREQUENCY = 'F',
    XK852_CMD_LINE = 'G',
    XK852_CMD_FSK_STOP = 'H',
    XK852_CMD_MODE = 'I',
    XK852_CMD_EXTERN = 'J',
    XK852_CMD_CHANNEL = 'K',
    XK852_CMD_NOISE_BLANK = 'N',
    XK852_CMD_STATUS = 'O',
    XK852_CMD_OP_MODE = 'S',
    XK852_CMD_SELFTEST = 'T',
    XK852_CMD_VOICE = 'V',
    XK852_CMD_PTT = 'X',
    XK852_CMD_TUNE = 'Y',
    XK852_CMD_FSK_POLARITY = 'Z'
} xk852_cmd;

typedef enum {
  XK852_LINE_OFF = 1,
  XK852_LINE_ON = 1
} xk852_line;

typedef enum {
    XK852_FSK_STOP_OFF = 0,
    XK852_FSK_STOP_ON = 1
} xk852_fsk_stop;

typedef enum {
    XK852_MODE_AME = 1,
    XK852_MODE_USB = 2,
    XK852_MODE_LSB = 3,
    XK852_MODE_CW = 5,
    XK852_MODE_ISB = 6,
    XK852_MODE_FSK_LP = 7,
    XK852_MODE_FSK_NP = 8,
    XK852_MODE_FSK_HP = 9
} xk852_mode;

typedef enum {
    XK852_OP_MODE_OFF = 0,
    XK852_OP_MODE_RX = 1,
    XK852_OP_MODE_TX_LOW = 2,
    XK852_OP_MODE_TX_MID = 3,
    XK852_OP_MODE_TX_FULL = 4,
} xk852_op_mode;

// TODO: check Noiseblank command does not work !
typedef enum {
     XK852_NOISE_BLANK_OFF = 0,
     XK852_NOISE_BLANK_ON = 1,
} xk852_noise_blank;

typedef struct {
  int freq;
  xk852_mode mode;
  xk852_noise_blank noise_blank;
  xk852_op_mode op_mode;
} xk852_state;

int xk852_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int xk852_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int xk852_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int xk852_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int xk852_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int xk852_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int xk852_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int xk852_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int xk852_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int xk852_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int xk852_reset(RIG *rig, reset_t reset);
//const char * xk852_get_info(RIG *rig);

extern struct rig_caps xk852_caps;

#endif /* XK852_H */
