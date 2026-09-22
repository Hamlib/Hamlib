/*
 *  Hamlib Icom network backend
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

/* Generic Icom LAN (network-protocol) backend shared across models: open/close, */
/* config, audio/IQ streaming, and a caps builder driven by a per-model descriptor. */

#ifndef _ICOM_NETWORK_H
#define _ICOM_NETWORK_H 1

#include "hamlib/rig.h"

struct rig_caps;

struct icom_network_model
{
    const struct rig_caps *base_caps;  /* cloned for the network model */
    rig_model_t  rig_model;
    const char  *macro_name;
    const char  *model_name;           /* Hamlib display name */
    const char  *radio_name;           /* wire connection_info radio name */
    int          rx_only;              /* no TX audio stream cap / no TX */
    int          iq_capable;           /* advertise the IQ_RX stream cap */
    /* Declared per model rather than inherited from base_caps: the serial
     * model's status says nothing about whether the network transport has been
     * exercised on that radio. */
    int          status;               /* enum rig_status_e */
    const char  *version;              /* YYYYMMDD.N for the network backend */
};

/* Clone base_caps and wire the generic network open/close/stream/conf
 * callbacks + stream_caps, and register the descriptor for open() lookup. */
void icom_network_build_caps(struct rig_caps *out,
                             struct rig_stream_caps *stream_caps,
                             const struct icom_network_model *m);

#endif /* _ICOM_NETWORK_H */
