/*
 *  Hamlib Icom network backend - configuration tokens
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

/* Shared configuration tokens for the Icom network (RS-BA1/LAN) backends. */
/* Credentials, radio selection, audio codecs, rates and latencies. */

#ifndef _ICOM_NETWORK_CONF_H
#define _ICOM_NETWORK_CONF_H 1

#include "hamlib/rig.h"

/* Config parameter table; assign to rig_caps.cfgparams. */
extern const struct confparams icom_network_config_params[];

/* set_conf/get_conf handlers; assign to rig_caps.set_conf/get_conf. They store
 * into the icom_priv_data net_* fields and fall through to icom_set_conf/
 * icom_get_conf for any other token. */
int icom_network_set_conf(RIG *rig, hamlib_token_t token, const char *val);
int icom_network_get_conf(RIG *rig, hamlib_token_t token, char *val);

#endif /* _ICOM_NETWORK_CONF_H */
