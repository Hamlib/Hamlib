/*
 *  Hamlib SmartSDR configuration tokens
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

/* The settings a SmartSDR rig accepts, and the handlers that read them into */
/* the private data. The token numbers themselves are in smartsdr_priv.h,    */
/* because the modules that act on a setting need them too.                  */

#ifndef SMARTSDR_CONF_H
#define SMARTSDR_CONF_H

#include <hamlib/rig.h>

/* Config parameter table; assign to rig_caps.cfgparams. */
extern const struct confparams smartsdr_config_params[];

/* set_conf/get_conf handlers; assign to rig_caps.set_conf/get_conf. */
extern int smartsdr_set_conf(RIG *rig, hamlib_token_t token, const char *val);
extern int smartsdr_get_conf2(RIG *rig, hamlib_token_t token, char *val,
                              int val_len);

#endif /* SMARTSDR_CONF_H */
