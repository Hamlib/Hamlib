/*
 * Hamlib backend library for the Expert amplifier set.
 *
 *  Copyright (c) 2023 by Michael Black W9MDB
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
 *
 * This shared library provides an API for communicating
 * via serial interface to Expert amplifiers.
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

#ifndef _AMP_EXPERT_H
#define _AMP_EXPERT_H 1

#include "hamlib/amplifier.h"
#include "iofunc.h"

#define EXPERTBUFSZ 128

#define TOKEN_BACKEND(t) (t)

#define TOK_CFG_STATUS_CACHE_TIMEOUT TOKEN_BACKEND(1)
#define EXPERT_CACHE_TIMEOUT_DEFAULT 40

extern const struct amp_caps expert_13k_fa_amp_caps;
extern const struct amp_caps expert_15k_fa_amp_caps;
extern const struct amp_caps expert_2k_fa_amp_caps;

int expert_init(AMP *amp);
int expert_open(AMP *amp);
int expert_close(AMP *amp);
int expert_cleanup(AMP *amp);
int expert_reset(AMP *amp, amp_reset_t reset);
const char *expert_get_info(AMP *amp);
int expert_get_freq(AMP *amp, freq_t *freq);
int expert_get_status(AMP *amp, amp_status_t *status);

int expert_get_level(AMP *amp, setting_t level, value_t *val);
int expert_set_level(AMP *amp, setting_t level, value_t val);
int expert_get_func(AMP *amp, setting_t func, int *status);
int expert_get_powerstat(AMP *amp, powerstat_t *status);
int expert_set_powerstat(AMP *amp, powerstat_t status);

int expert_get_input(AMP *amp, ant_t *input);
int expert_set_input(AMP *amp, ant_t input);
int expert_get_ant(AMP *amp, ant_t *ant);
int expert_set_ant(AMP *amp, ant_t ant);
int expert_amp_op(AMP *amp, amp_op_t op);
int expert_set_parm(AMP *amp, setting_t parm, value_t val);

#endif  /* _AMP_EXPERT_H */

