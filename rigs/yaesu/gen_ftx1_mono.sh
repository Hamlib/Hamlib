#!/bin/bash
# Generate ftx1_mono.c from multi-file sources
# Run from rigs/yaesu directory

OUTPUT="ftx1_mono.c"

cat > "$OUTPUT" << 'HEADER'
/*
 * Hamlib Yaesu backend - FTX-1 (Monolithic version)
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
 * Monolithic FTX-1 backend combining all source files.
 * This file is auto-generated from the multi-file implementation.
 * To regenerate: ./gen_ftx1_mono.sh
 *
 * FIRMWARE NOTES (v1.08+):
 * - RT (RIT on/off) and XT (XIT on/off) commands return '?' - not implemented
 * - CF (Clarifier) sets offset value only, does not enable/disable clarifier
 * - ZI (Zero In) only works in CW mode (MD03 or MD07)
 * - BS (Band Select) is set-only - no read/query capability
 * - MR/MT/MZ (Memory) use 5-digit format, not 4-digit as documented
 * - MC (Memory Channel) uses 6-digit format
 * - CH command only accepts CH0/CH1 (not CH;)
 * - VM mode codes: 00=VFO, 11=Memory (not 01 as documented)
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "sprintflst.h"
#include "yaesu.h"
#include "newcat.h"

/* ========== Shared constants from ftx1.h ========== */
#define NC_RIGID_FTX1               0x840   /* Default rig model ID */
#define FTX1_RADIO_ID               "0840"  /* Radio ID (same for all configs) */

/*
 * FTX-1 head type constants
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
 * Returns: 0 for MAIN/VFO-A/CURR, 1 for SUB/VFO-B
 */
#define FTX1_VFO_TO_P1(vfo) \
    ((vfo == RIG_VFO_CURR || vfo == RIG_VFO_MAIN || vfo == RIG_VFO_A) ? 0 : 1)

/* FTX-1 CTCSS mode values for CT command */
#define FTX1_CTCSS_MODE_OFF  0   /* CT0: CTCSS/DCS off */
#define FTX1_CTCSS_MODE_ENC  1   /* CT1: CTCSS encode only (TX tone) */
#define FTX1_CTCSS_MODE_TSQ  2   /* CT2: Tone squelch (TX+RX tone) */
#define FTX1_CTCSS_MODE_DCS  3   /* CT3: DCS mode */

/* ========== Shared private data ========== */
static struct {
    int head_type;
    int spa1_detected;
    int detection_done;
} ftx1_priv = { FTX1_HEAD_UNKNOWN, 0, 0 };

static const struct newcat_priv_caps ftx1_priv_caps = { .roofing_filter_count = 0 };

/* SPA-1 detection functions */
static int ftx1_has_spa1(void) {
    return ftx1_priv.spa1_detected || ftx1_priv.head_type == FTX1_HEAD_SPA1;
}

static int ftx1_get_head_type(void) {
    return ftx1_priv.head_type;
}

HEADER

# Function to extract code from a module file (skip includes and header comments)
extract_module() {
    local file="$1"
    local name="$2"
    echo ""
    echo "/* ========== FROM $name ========== */"
    # Skip copyright comment block, skip #include lines, keep everything else
    sed -n '/^\/\*$/,/^ \*\//d; /^#include/d; /^$/!p' "$file" | \
        sed '1{/^$/d}'  # Remove leading blank lines
}

# Add each module
for module in ftx1/ftx1_vfo.c ftx1/ftx1_freq.c ftx1/ftx1_preamp.c ftx1/ftx1_audio.c \
              ftx1/ftx1_filter.c ftx1/ftx1_noise.c ftx1/ftx1_tx.c ftx1/ftx1_cw.c \
              ftx1/ftx1_ctcss.c ftx1/ftx1_mem.c ftx1/ftx1_scan.c ftx1/ftx1_info.c \
              ftx1/ftx1_ext.c ftx1/ftx1_func.c ftx1/ftx1_clarifier.c; do
    basename=$(basename "$module")
    echo "Processing $basename..."
    echo "" >> "$OUTPUT"
    echo "/* ========== FROM $basename ========== */" >> "$OUTPUT"
    # Skip copyright comment, skip includes, keep the rest
    awk '
        /^\/\*$/ && !in_comment { in_comment=1; next }
        /^ \*\/$/ && in_comment { in_comment=0; next }
        in_comment { next }
        /^#include/ { next }
        { print }
    ' "$module" >> "$OUTPUT"
done

# Add main ftx1.c content (detection functions and rig_caps)
echo "" >> "$OUTPUT"
echo "/* ========== FROM ftx1.c ========== */" >> "$OUTPUT"
# Extract from ftx1_detect_spa1 to end of file
awk '
    /^\/\*$/ && !in_comment && !started { in_comment=1; next }
    /^ \*\/$/ && in_comment && !started { in_comment=0; next }
    in_comment && !started { next }
    /^#include/ && !started { next }
    /^static struct$/ { started=1 }
    started { print }
' ftx1.c >> "$OUTPUT"

echo "Generated $OUTPUT"
wc -l "$OUTPUT"
