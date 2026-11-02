/*
 *  Hamlib SmartSDR VITA-49 streaming
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

/* Everything a SmartSDR radio sends over UDP: the shared socket, the        */
/* dispatcher that fans packets out by stream ID, the per-stream threads     */
/* behind stream_open/stream_close, and the meter and panadapter readings    */
/* that arrive on the same socket.                                           */

#ifndef SMARTSDR_STREAM_H
#define SMARTSDR_STREAM_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdint.h>

#include <hamlib/rig.h>
#include "stream.h"
#include "smartsdr_priv.h"
#include "smartsdr_vita.h"

/* Panadapter created for spectrum when the slice has none: one bin per unit
 * of width, over a span wide enough to be useful on any band. */
#define SMARTSDR_SPECTRUM_BINS      512
#define SMARTSDR_SPECTRUM_SPAN_HZ  200000.0

/* First-read waits: one frame interval's grace for a timed meter, and much
 * longer for an fps=0 meter the radio only sends when it changes. */
#define SMARTSDR_METER_WAIT_MS       400
#define SMARTSDR_METER_SLOW_WAIT_MS 3000

/* How long the TX thread waits for the application to supply a whole frame
 * before sending silence instead. Only a real stall should get this far. */
#define SMARTSDR_TX_FRAME_WAIT_MS  100


/* Send the 1-byte UDP datagram that makes the radio (and any NAT on the path)
 * learn our source address for this socket. */
extern void smartsdr_send_udp_nudge(RIG *rig);
extern int smartsdr_meters_start(RIG *rig);
extern void smartsdr_meters_stop(RIG *rig);
extern int smartsdr_meter_read(RIG *rig, int slot, double *val);
extern int smartsdr_spectrum_start(RIG *rig);
extern void smartsdr_spectrum_stop(RIG *rig);
extern void smartsdr_apply_tx_audio_source(RIG *rig);

/* ------------------------------------------------------------------ */
/* Backend stream callbacks                                            */
/* ------------------------------------------------------------------ */

extern int smartsdr_stream_open(RIG *rig, struct rig_stream *stream);
extern int smartsdr_stream_close(RIG *rig, struct rig_stream *stream);
extern int smartsdr_stream_hardware_time(RIG *rig, struct rig_stream *stream,
                                         struct rig_stream_time_anchor *now);

#endif /* SMARTSDR_STREAM_H */
