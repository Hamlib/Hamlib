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
 *
 * ACKNOWLEDGMENTS:
 *   Jeremy Miller (KO4SSD) - RIT/XIT using RC/TC commands, EX0306 tuning steps
 *   See: https://github.com/Hamlib/Hamlib/pull/1826
 */

#ifndef _FTX1_H
#define _FTX1_H

#include <hamlib/rig.h>

/*
 * FTX-1 rig ID constants
 *
 * The Radio ID command (ID;) always returns 0840 for ALL FTX-1 configurations.
 * Head type detection uses a two-stage process:
 *
 * Stage 1: PC Command Format
 *   PC1xxx = Field Head (battery or 12V)
 *   PC2xxx = Optima/SPA-1 (5-100W)
 *
 * Stage 2: Power Probe (Field Head Only)
 *   Attempt to set 8W power:
 *   - If radio accepts 8W → 12V power (0.5-10W range)
 *   - If radio rejects 8W (stays ≤6W) → Battery power (0.5-6W range)
 *   The probe saves and restores original power setting.
 */
#define NC_RIGID_FTX1               0x840   /* Default rig model ID */
#define FTX1_RADIO_ID               "0840"  /* Radio ID (same for all configs) */

/*
 * FTX-1 head type constants
 *
 * Power ranges by head type:
 *   FIELD_BATTERY: 0.5W - 6W (0.5W steps)
 *   FIELD_12V:     0.5W - 10W (0.5W steps)
 *   SPA1:          5W - 100W (1W steps) - "Optima" when attached to Field
 */
#define FTX1_HEAD_UNKNOWN       0
#define FTX1_HEAD_FIELD_BATTERY 1   /* Field Head on battery (0.5-6W) */
#define FTX1_HEAD_FIELD_12V     2   /* Field Head on 12V (0.5-10W) */
#define FTX1_HEAD_SPA1          3   /* Optima/SPA-1 amplifier (5-100W) */

/* Legacy alias for backwards compatibility */
#define FTX1_HEAD_FIELD FTX1_HEAD_FIELD_12V

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
