/*
 *  Hamlib CI-V backend - main header
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
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

#ifndef _OPTOSCAN_H
#define _OPTOSCAN_H 1

#include <hamlib/rig.h>
#include <cal.h>
#include <tones.h>

#define TOK_TAPECNTL  TOKEN_BACKEND(1)
#define TOK_5KHZWIN   TOKEN_BACKEND(2)
#define TOK_SPEAKER   TOKEN_BACKEND(3)
#define TOK_AUDIO     TOKEN_BACKEND(4)
#define TOK_DTMFPEND  TOKEN_BACKEND(5)
#define TOK_DTMFOVRR  TOKEN_BACKEND(6)
#define TOK_CTCSSACT  TOKEN_BACKEND(7)
#define TOK_DCSACT    TOKEN_BACKEND(8)

#define OPTO_BUFF_SIZE   64

int optoscan_open(RIG *rig);
int optoscan_close(RIG *rig);
const char* optoscan_get_info(RIG *rig);
int optoscan_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
int optoscan_get_dcs_code(RIG * rig, vfo_t vfo, tone_t *code);
int optoscan_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length);
int optoscan_set_ext_parm(RIG *rig, token_t token, value_t val);
int optoscan_get_ext_parm(RIG *rig, token_t token, value_t *val);
int optoscan_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int optoscan_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int optoscan_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);

struct optostat {
  int remote_control;
  int DTMF_pending;
  int DTMF_overrun;
  int squelch_open;
  int CTCSS_active;
  int DCS_active;
  int tape_enabled;
  int speaker_enabled;
  int fivekhz_enabled;
  int audio_present;
};

#endif /* _OPTOSCAN_H */
