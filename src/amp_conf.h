/*
 *  Hamlib Interface - configuration header
 *  Copyright (c) 2000,2001,2002 by Stephane Fillod and Frank Singleton
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

#ifndef _AMP_CONF_H
#define _AMP_CONF_H 1

#include <hamlib/amplifier.h>

int frontamp_set_conf(AMP *amp, token_t token, const char *val);
static int frontamp_get_conf2(AMP *amp, token_t token, char *val, int val_len);


#endif /* _AMP_CONF_H */
