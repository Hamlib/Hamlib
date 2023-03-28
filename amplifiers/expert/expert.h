/*
 * Hamlib backend library for the Expert amplifier set.
 *
 * expert.h - (C) Michael Black W9MDB 2023
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

#include <hamlib/amplifier.h>
#include <iofunc.h>
#include <serial.h>

// Is this big enough?
#define KPABUFSZ 100

extern const struct amp_caps expert_amp_caps;

/*
 * Private data structure
 */
struct expert_priv_data
{
    char tmpbuf[256];  // for unknown error msg
};


int expert_init(AMP *amp);
int expert_close(AMP *amp);
int expert_reset(AMP *amp, amp_reset_t reset);
int expert_flush_buffer(AMP *amp);
int expert_transaction(AMP *amp, const unsigned  char *cmd, int cmd_len, unsigned char *response,
                    int response_len);
const char *expert_get_info(AMP *amp);
int expert_get_freq(AMP *amp, freq_t *freq);
int expert_set_freq(AMP *amp, freq_t freq);

int expert_get_level(AMP *amp, setting_t level, value_t *val);
int expert_get_powerstat(AMP *amp, powerstat_t *status);
int expert_set_powerstat(AMP *amp, powerstat_t status);

#endif  /* _AMP_EXPERT_H */

