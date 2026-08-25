/*
 *  Hamlib streaming subsystem
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

/* Capture-time anchors for the Hamlib streaming subsystem. */
/* Records sample_index <-> UTC anchors and interpolates read-time stamps. */

#ifndef HAMLIB_STREAM_ANCHOR_H
#define HAMLIB_STREAM_ANCHOR_H

#include <hamlib/rig.h>

struct rig_stream;
struct rig_stream_read_info;


/* --- Time anchors --- */

/* Backend-facing: record a capture-time anchor (sample_index <-> UTC).
 * Drop-oldest ring of RIG_STREAM_ANCHOR_DEPTH entries.
 * Returns RIG_OK or -RIG_EINVAL. */
int rig_stream_push_time_anchor(struct rig_stream *stream,
                                const struct rig_stream_time_anchor *anchor);

/* Fill the time fields of info (time_valid/seconds/picoseconds/source/
 * flags/accuracy) for the sample position info->sample_index: interpolates
 * from the newest anchor at or before it and applies the staleness
 * watchdog. Sets RIG_STREAM_TIME_FLAG_DISCONTINUITY when info->dropped_samples
 * or an UNSIZED drop precedes. Call after stream_consume_account(). */
void stream_fill_read_time(struct rig_stream *stream,
                           struct rig_stream_read_info *info);


#endif /* HAMLIB_STREAM_ANCHOR_H */
