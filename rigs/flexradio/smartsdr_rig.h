/*
 *  Hamlib SmartSDR classic rig controls
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* Frequency, mode, PTT, RIT, XIT, tuning step, the keyer and the radio's own */
/* description of itself, plus the mode-name conversion they are built on.    */

#ifndef SMARTSDR_RIG_H
#define SMARTSDR_RIG_H

#include <hamlib/rig.h>

extern int smartsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
extern int smartsdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
extern int smartsdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
extern int smartsdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                             pbwidth_t *width);
extern int smartsdr_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
extern int smartsdr_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
extern int smartsdr_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
extern int smartsdr_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
extern int smartsdr_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
extern int smartsdr_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);
extern int smartsdr_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
extern int smartsdr_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
extern int smartsdr_send_morse(RIG *rig, vfo_t vfo, const char *msg);
extern int smartsdr_stop_morse(RIG *rig, vfo_t vfo);

/* Ask the radio for its model, serial and firmware, and report what it said.
 * The query is made once per session; the reader is free. */
extern void smartsdr_query_info(RIG *rig);
extern const char *smartsdr_get_info(RIG *rig);

/* SmartSDR name for a Hamlib mode, or NULL if the radio has no equivalent. */
extern const char *smartsdr_mode_name(rmode_t mode);

/* Parse slice status mode= value (name or numeric index).
 * Returns 0 if *out_mode is valid, 1 if unknown (keep cached mode), -1 if empty. */
extern int smartsdr_parse_mode_status_value(const char *value,
        rmode_t *out_mode);

#endif /* SMARTSDR_RIG_H */
