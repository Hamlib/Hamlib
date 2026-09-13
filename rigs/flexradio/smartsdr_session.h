/*
 *  Hamlib SmartSDR control session
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

/* The TCP control session with a SmartSDR radio: commanding it, absorbing the */
/* status it pushes back, and keeping the connection alive.                    */

#ifndef SMARTSDR_SESSION_H
#define SMARTSDR_SESSION_H

#include <hamlib/rig.h>

/* ------------------------------------------------------------------ */
/* Commanding the radio                                                */
/* ------------------------------------------------------------------ */

/* Start the thread that owns the read side of the control connection. It must
 * be running before any command is sent, because it is what delivers the
 * reply. Returns -RIG_EINTERNAL if the thread could not be created. */
extern int smartsdr_control_start(RIG *rig);

/* Stop the control connection's reader and wait for it to finish. Callers
 * still waiting for a reply are released with a timeout. */
extern void smartsdr_control_stop(RIG *rig);

/* Send a TCP command and capture the R-line response, which the control
 * thread matches back to this caller by the sequence number the command
 * carried. resp_buf/resp_buf_len may be NULL/0 if the response text is not
 * needed. A NULL cmd_buf sends nothing. */
/* Send a setup command and log a refusal; see the definition. */
extern void smartsdr_command_or_warn(RIG *rig, char *cmd);

extern int smartsdr_transaction_resp(RIG *rig, char *cmd_buf,
                                     char *resp_buf, int resp_buf_len);

/* Send a command whose response text is of no interest. */
extern int smartsdr_transaction(RIG *rig, char *buf);

/* Identify as a client and subscribe to the objects the backend reads. The
 * radio grants no stream privileges and sends no status until this succeeds,
 * so a failure here ends the session rather than degrading it. */
extern int smartsdr_register_session(RIG *rig);

/* Re-subscribe and ping so S|slice / S|display pan lines repopulate pan &
 * waterfall. */
extern int smartsdr_sync_display_slice_status(RIG *rig);

/* Create panafall (or pan) and a slice bound to it; refresh slice status.
 * freq_mhz is the slice RF frequency in MHz. Use before IQ RX on radios that
 * require an existing slice and pan (e.g. WAN / strict display paths). */
extern int smartsdr_prepare_slice_panafall(RIG *rig, double freq_mhz);


/* ------------------------------------------------------------------ */
/* Session liveness                                                    */
/* ------------------------------------------------------------------ */

/* Record that the radio is gone, once, with the reason. Safe to call from any
 * thread and from any number of them. */
extern void smartsdr_session_lost(RIG *rig, unsigned reason);

/* Start the keepalive, the NAT re-registration backstop and, when the rig is
 * configured for it, the reconnect thread. Returns -RIG_EINTERNAL if the
 * keepalive could not be started, which leaves no session worth having. */
extern int smartsdr_session_start_threads(RIG *rig);

/* Stop the reconnect thread. Closing is deliberate, so this runs before
 * anything else is dismantled: the thread would otherwise race to rebuild it. */
extern void smartsdr_session_stop_reconnect(RIG *rig);

/* Stop the keepalive and the NAT re-registration backstop. */
extern void smartsdr_session_stop_threads(RIG *rig);


/* Apply one S-line of radio status to this rig's cached state. Only lines
 * belonging to the rig's own slice change it. Exposed for unit tests. */
extern int smartsdr_parse_status_line(RIG *rig, char *line);

#endif /* SMARTSDR_SESSION_H */
