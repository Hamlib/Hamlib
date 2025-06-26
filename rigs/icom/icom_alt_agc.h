/*
 *  Hamlib CI-V backend - alternate AGC header
 *  Copyright (c) 2000-2025 by Stephane Fillod
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

#ifndef _ICOM_ALT_AGC_H
#define _ICOM_ALT_AGC_H 1

#define ICOM_AGC_FAST      0x00
#define ICOM_AGC_MID       0x01
#define ICOM_AGC_SLOW      0x02
#define ICOM_AGC_SUPERFAST 0x03 /* IC746 pro */

int icom_rig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int icom_rig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

#endif /* _ICOM_ALT_AGC_H */