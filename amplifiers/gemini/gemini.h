/*
 * Hamlib backend library for the Gemini amplifier set.
 *
 * gemini.h - (C) Michael Black W9MDB 2022
 *
 * This shared library provides an API for communicating
 * via serial interface to Gemini amplifiers.
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

#ifndef _AMP_GEMINI_H
#define _AMP_GEMINI_H 1

#include <hamlib/amplifier.h>
#include <iofunc.h>
#include <serial.h>

// Is this big enough?
#define GEMINIBUFSZ 1024

extern const struct amp_caps gemini_amp_caps;

/*
 * Private data structure
 */
struct gemini_priv_data
{
    long band; // Hz
    char antenna;
    int power_current; // Watts
    int power_peak; // Watts
    double vswr;
    int current; // Amps
    int temperature; // Centigrade
    char state[5];
    int ptt;
    char trip[256];
};


int gemini_init(AMP *amp);
int gemini_close(AMP *amp);
int gemini_reset(AMP *amp, amp_reset_t reset);
int gemini_flush_buffer(AMP *amp);
int gemini_transaction(AMP *amp, const char *cmd, char *response,
                    int response_len);
const char *gemini_get_info(AMP *amp);
int gemini_get_freq(AMP *amp, freq_t *freq);
int gemini_set_freq(AMP *amp, freq_t freq);

int gemini_get_level(AMP *amp, setting_t level, value_t *val);
int gemini_set_level(AMP *amp, setting_t level, value_t val);
int gemini_get_powerstat(AMP *amp, powerstat_t *status);
int gemini_set_powerstat(AMP *amp, powerstat_t status);

#endif  /* _AMP_GEMINI_H */

