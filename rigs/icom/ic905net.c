/*
 *  Hamlib CI-V backend - IC-905 network model
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
 *
 *  Binds the generic Icom LAN backend to the IC-905 capabilities, which are
 *  inherited unchanged from the serial model based on code by
 *  Michael Black W9MDB.
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


#include "hamlib/config.h"

#include "hamlib/rig.h"
#include "icom.h"
#include "icom_network.h"

struct rig_caps ic905net_caps;
/* Backing store for ic905net_caps.stream_caps (built at init). */
static struct rig_stream_caps ic905net_stream_caps[HAMLIB_MAX_STREAM_CAPS];

static const struct icom_network_model ic905net_model =
{
    .base_caps = &ic905_caps,
    .rig_model = RIG_MODEL_IC905NET,
    .macro_name = "RIG_MODEL_IC905NET",
    .model_name = "IC-905 (Network)",
    .radio_name = "IC-905",
    .rx_only = 0,
    .iq_capable = 1,
    /* never exercised over the network */
    .status = RIG_STATUS_UNTESTED,
    .version = "20260819.0",
};

void ic905net_init_caps(void)
{
    icom_network_build_caps(&ic905net_caps, ic905net_stream_caps,
                            &ic905net_model);
}
