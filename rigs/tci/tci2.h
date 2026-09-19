/*
 *  Hamlib TCI 2.0 backend — shared header
 *  Copyright (c) 2026 by Jeff Francis N0GQ <gjfrancis@protonmail.com>
 *
 *  Exposes the generic TCI 2.0 protocol layer (tci2.c) to per-radio
 *  caps files (e.g. sunsdr2-pro.c) that live in the same backend.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 */

#ifndef _TCI2_H
#define _TCI2_H 1

#include "hamlib/rig.h"
#include "token.h"

/* -------------------------------------------------------------------------
 * Capability bit-fields shared by every TCI-2.0 rig_caps
 * ---------------------------------------------------------------------- */

#define TCI2_MODES (RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_CW  | RIG_MODE_CWR  | \
                    RIG_MODE_AM  | RIG_MODE_SAM  | RIG_MODE_SAL | RIG_MODE_SAH  | \
                    RIG_MODE_DSB | RIG_MODE_FM   | RIG_MODE_FMN | RIG_MODE_WFM  | \
                    RIG_MODE_RTTY | RIG_MODE_RTTYR | \
                    RIG_MODE_PKTUSB | RIG_MODE_PKTLSB | RIG_MODE_C4FM)

#define TCI2_VFOS  (RIG_VFO_A | RIG_VFO_B)

#define TCI2_LEVELS_GET (RIG_LEVEL_AF | RIG_LEVEL_RFPOWER | RIG_LEVEL_SQL | \
                         RIG_LEVEL_STRENGTH | RIG_LEVEL_AGC | RIG_LEVEL_NB | \
                         RIG_LEVEL_KEYSPD | RIG_LEVEL_SWR | RIG_LEVEL_RFPOWER_METER)
#define TCI2_LEVELS_SET (RIG_LEVEL_AF | RIG_LEVEL_RFPOWER | RIG_LEVEL_SQL | \
                         RIG_LEVEL_AGC | RIG_LEVEL_NB | RIG_LEVEL_KEYSPD)

#define TCI2_FUNCS (RIG_FUNC_NB | RIG_FUNC_NR | RIG_FUNC_ANF | RIG_FUNC_LOCK | \
                    RIG_FUNC_MUTE | RIG_FUNC_TUNER)

/* -------------------------------------------------------------------------
 * Config tokens
 * ---------------------------------------------------------------------- */

#define TOK_TCI2_TRX         TOKEN_BACKEND(1)
#define TOK_TCI2_TXSRC       TOKEN_BACKEND(2)
#define TOK_TCI2_DIGL_OFFSET TOKEN_BACKEND(3)
#define TOK_TCI2_DIGU_OFFSET TOKEN_BACKEND(4)

extern const struct confparams tci2_cfg_params[];

/* -------------------------------------------------------------------------
 * Backend entry points (defined in tci2.c)
 * ---------------------------------------------------------------------- */

int  tci2_init(RIG *rig);
int  tci2_open(RIG *rig);
int  tci2_close(RIG *rig);
int  tci2_cleanup(RIG *rig);

int  tci2_set_conf(RIG *rig, hamlib_token_t token, const char *val);
int  tci2_get_conf(RIG *rig, hamlib_token_t token, char *val);

int  tci2_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int  tci2_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

int  tci2_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int  tci2_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

int  tci2_set_vfo(RIG *rig, vfo_t vfo);
int  tci2_get_vfo(RIG *rig, vfo_t *vfo);

int  tci2_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int  tci2_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);

int  tci2_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
int  tci2_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
int  tci2_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int  tci2_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);

int  tci2_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int  tci2_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
int  tci2_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
int  tci2_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);

int  tci2_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int  tci2_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int  tci2_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int  tci2_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);

int  tci2_send_morse(RIG *rig, vfo_t vfo, const char *msg);
int  tci2_stop_morse(RIG *rig, vfo_t vfo);
int  tci2_wait_morse(RIG *rig, vfo_t vfo);

int  tci2_power2mW(RIG *rig, unsigned int *mwpower,
                   float power, freq_t freq, rmode_t mode);
int  tci2_mW2power(RIG *rig, float *power,
                   unsigned int mwpower, freq_t freq, rmode_t mode);

const char *tci2_get_info(RIG *rig);

/* -------------------------------------------------------------------------
 * Per-radio rig_caps published by sunsdr2-pro.c / etc.
 * ---------------------------------------------------------------------- */

extern struct rig_caps sunsdr2_pro_caps;

#endif /* _TCI2_H */
