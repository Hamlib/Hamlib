/*
 * Hamlib backend library for the Elecraft amplifier set.
 *
 * elecraft.h - (C) Michael Black W9MDB 2019
 *
 * This shared library provides an API for communicating
 * via serial interface to Elecraft amplifiers.
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

#ifndef _AMP_ELECRAFT_H
#define _AMP_ELECRAFT_H 1

#include <hamlib/amplifier.h>
#include <iofunc.h>
#include <serial.h>

// Is this big enough?
#define KPABUFSZ 100

extern const struct amp_caps kpa1500_rot_caps;

/*
 * Private data structure
 */
struct kpa_priv_data
{
    char tmpbuf[256];  // for unknown error msg
};


int kpa_init(AMP *amp);
int kpa_close(AMP *amp);
int kpa_reset(AMP *amp, amp_reset_t reset);
int kpa_flush_buffer(AMP *amp);
int kpa_transaction(AMP *amp, const char *cmd, char *response,
                    int response_len);
const char *kpa_get_info(AMP *amp);
int kpa_get_freq(AMP *amp, freq_t *freq);
int kpa_set_freq(AMP *amp, freq_t freq);

int kpa_get_level(AMP *amp, setting_t level, value_t *val);
int kpa_get_powerstat(AMP *amp, powerstat_t *status);
int kpa_set_powerstat(AMP *amp, powerstat_t status);

#endif  /* _AMP_ELECRAFT_H */

