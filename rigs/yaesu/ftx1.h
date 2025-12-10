/*
 * Hamlib Yaesu backend - FTX-1 shared definitions
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _FTX1_H
#define _FTX1_H

#include <hamlib/rig.h>

/*
 * FTX-1 rig ID
 * Note: Firmware returns ID0763, not ID0840 as documented in CAT manual
 */
#define NC_RIGID_FTX1 0x763

/*
 * FTX-1 head type constants (detected via PC command response P1 value)
 * Field head: 0.5-10W portable configuration
 * SPA-1: 5-100W amplifier with internal tuner (Optima configuration)
 */
#define FTX1_HEAD_UNKNOWN   0
#define FTX1_HEAD_FIELD     1
#define FTX1_HEAD_SPA1      2

/*
 * FTX1_VFO_TO_P1 - Convert Hamlib VFO to FTX-1 P1 parameter
 *
 * FTX-1 CAT commands use P1=0 for MAIN VFO and P1=1 for SUB VFO.
 * This macro maps Hamlib VFO types to the appropriate P1 value.
 *
 * Returns: 0 for MAIN/VFO-A/CURR, 1 for SUB/VFO-B
 */
#define FTX1_VFO_TO_P1(vfo) \
    ((vfo == RIG_VFO_CURR || vfo == RIG_VFO_MAIN || vfo == RIG_VFO_A) ? 0 : 1)

/*
 * FTX-1 CTCSS mode values for CT command
 */
#define FTX1_CTCSS_MODE_OFF  0   /* CT0: CTCSS/DCS off */
#define FTX1_CTCSS_MODE_ENC  1   /* CT1: CTCSS encode only (TX tone) */
#define FTX1_CTCSS_MODE_TSQ  2   /* CT2: Tone squelch (TX+RX tone) */
#define FTX1_CTCSS_MODE_DCS  3   /* CT3: DCS mode */

/*
 * SPA-1 detection functions (defined in ftx1.c)
 */
extern int ftx1_has_spa1(void);
extern int ftx1_get_head_type(void);

#endif /* _FTX1_H */
