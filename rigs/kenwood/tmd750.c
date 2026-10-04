/*
 *  Hamlib Kenwood TM-D750 backend
 *  Copyright (c) 2026 by Hamlib Team
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

/*
 * Commands used, with b the band (0 = A, 1 = B). A set command is answered
 * with its own text, "N" when the radio refuses the value and "?" when it
 * cannot parse it.
 *
 *  FO b / FO b,<record>   read / set a band's VFO record (layout below).
 *                         The radio rounds the frequency down to the step.
 *  ME ccc / ME ccc,<record> / ME ccc,
 *                         read / write / erase memory channel ccc.
 *                         Reading an empty channel answers "N".
 *  MD b / MD b,m          mode: 0 FM, 1 DV, 2 AM, 3 narrow FM, 4 DR.
 *  VM b / VM b,v          0 VFO mode, 1 memory mode.
 *  MR b / MR b,ccc        memory channel on a band. The read answers
 *                         "MR ccc" without the band; the set needs memory mode.
 *  BC                     "BC ctrl,ptt": the band the front panel operates
 *                         on and the band the microphone PTT keys.
 *  TX / RX                key / unkey; TX answers "TX b" with the keyed band.
 *  BY b                   busy (carrier detect).
 *  SM b                   S-meter, 0 to 9.
 *  SQ b / SQ b,n          squelch, 0 (open) to 31.
 *  PC b / PC b,n          power: 0 high, 1 medium, 2 low.
 *  AG b / AG b,nnn        volume, 000 to 200, always three digits.
 *  VX, VG, VD             VOX on/off, gain 0 to 9, delay 0 to 6.
 *  RA b / RA b,v          attenuator 0 off, 1 on. F has been seen while
 *                         off and is read as off.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "cache.h"
#include "kenwood.h"
#include "th.h"
#include "misc.h"

#define TMD750_MODES (RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_AM|RIG_MODE_DSTAR)
#define TMD750_MODES_TX (RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_DSTAR)
#define TMD750_MODES_ANALOG (RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_AM)

#define TMD750_FUNC_ALL (RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_VOX)

#define TMD750_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_SQL|RIG_LEVEL_AF|\
                          RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|\
                          RIG_LEVEL_RAWSTR|RIG_LEVEL_ATT)

/* The attenuator is on or off; Kenwood gives no figure, so "on" reads as this nominal value. */
#define TMD750_ATT_DB 10

#define TMD750_VFO (RIG_VFO_A|RIG_VFO_B)

#define TMD750_FREQUENCY_MAX UINT64_C(9999999999)
#define TMD750_URCALL_MAX 8
#define TMD750_BUFSIZE 96
#define TMD750_SQL_MAX 31
#define TMD750_AF_MAX 200
#define TMD750_RAWSTR_MAX 9

#define TMD750_CHANNEL_CAPS \
    .freq = 1, \
    .tx_freq = 1, \
    .split = 1, \
    .mode = 1, \
    .width = 1, \
    .tuning_step = 1, \
    .rptr_shift = 1, \
    .rptr_offs = 1, \
    .funcs = RIG_FUNC_REV, \
    .ctcss_tone = 1, \
    .ctcss_sql = 1, \
    .dcs_code = 1, \
    .dcs_sql = 1, \
    .flags = 1

/*
 * For reading MD. DV (1) and DR (4) both read as D-STAR. Setting D-STAR
 * always sends DV: MD cannot enter or leave DR, so set_mode never sends 4.
 */
static rmode_t tmd750_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_DSTAR,
    [2] = RIG_MODE_AM,
    [3] = RIG_MODE_FMN,
    [4] = RIG_MODE_DSTAR,
};

static pbwidth_t tmd750_width_table[5] =
{
    [0] = 14000,
    [1] = 6000,
    [2] = 9000,
    [3] = 7000,
    [4] = 6000,
};

static const rptr_shift_t tmd750_shift_table[3] =
{
    RIG_RPT_SHIFT_NONE,
    RIG_RPT_SHIFT_PLUS,
    RIG_RPT_SHIFT_MINUS,
};

/* SF/FO step codes 0-C; 8.33 kHz is for the airband only. */
static const shortfreq_t tmd750_steps[13] =
{
    1000, 2500, 5000, 6250, 8330, 10000, 12500, 15000, 20000, 25000, 30000,
    50000, 100000
};

static const int tmd750_voxdelay[7] = { 3, 5, 8, 10, 15, 20, 30 };

static tone_t tmd750_dcs_list[105] =
{
    23,  25,  26,  31,  32,  36,  43,  47,
    51,  53,  54,  65,  71,  72,  73,  74,
    114, 115, 116, 122, 125, 131, 132, 134,
    143, 145, 152, 155, 156, 162, 165, 172,
    174, 205, 212, 223, 225, 226, 243, 244,
    245, 246, 251, 252, 255, 261, 263, 265,
    266, 271, 274, 306, 311, 315, 325, 331,
    332, 343, 346, 351, 356, 364, 365, 371,
    411, 412, 413, 423, 431, 432, 445, 446,
    452, 454, 455, 462, 464, 465, 466, 503,
    506, 516, 523, 526, 532, 546, 565, 606,
    612, 624, 627, 631, 632, 654, 662, 664,
    703, 712, 723, 731, 732, 734, 743, 754,
    0
};

static struct kenwood_priv_caps tmd750_priv_caps =
{
    .cmdtrm = EOM_TH,
    .mode_table = tmd750_mode_table,
};

/* One FO or ME record, fields in the order the radio sends them. */
struct tmd750_record
{
    int is_memory;              /* 1 for an ME record, 0 for FO; not sent */
    unsigned int number;        /* FO: band, 1 digit; ME: channel, 3 digits */
    uint64_t frequency_hz;      /* Hz, 10 digits */
    uint64_t offset_hz;         /* Hz, 10 digits; ME split channel: TX frequency */
    char rx_step;               /* step code 0-C; SF reports this one */
    char tx_step;               /* step code 0-C; stored by the radio, kept as read */
    uint8_t mode;               /* MD code */
    uint8_t tone_enabled;
    uint8_t ctcss_enabled;
    uint8_t dcs_enabled;
    uint8_t cross_enabled;
    uint8_t reverse_enabled;
    uint8_t odd_split_enabled;  /* ME only, between reverse and shift */
    uint8_t shift;              /* 0 none, 1 plus, 2 minus */
    uint8_t tone_index;         /* 2 digits, kenwood42_ctcss_list */
    uint8_t ctcss_index;        /* 2 digits, kenwood42_ctcss_list */
    uint8_t dcs_index;          /* 3 digits, tmd750_dcs_list */
    uint8_t cross_selector;     /* cross tone type */
    char urcall[TMD750_URCALL_MAX + 1];
    uint8_t digital_squelch_type;
    uint8_t digital_squelch_code; /* 2 digits; last FO field */
    uint8_t lockout_enabled;    /* ME only, last field */
};

struct tmd750_cursor
{
    const char *next;
    size_t remaining;
};

static int tmd750_field(struct tmd750_cursor *cursor, int final,
                        const char **value, size_t *length)
{
    size_t n = 0;

    while (n < cursor->remaining && cursor->next[n] != ',')
    {
        n++;
    }

    if ((!final && n == cursor->remaining) || (final && n != cursor->remaining))
    {
        return -RIG_EPROTO;
    }

    *value = cursor->next;
    *length = n;
    cursor->next += n + (final ? 0 : 1);
    cursor->remaining -= n + (final ? 0 : 1);
    return RIG_OK;
}

static int tmd750_decimal(struct tmd750_cursor *cursor, int final,
                          size_t width, uint64_t maximum, uint64_t *out)
{
    const char *value;
    size_t length;
    uint64_t parsed = 0;

    if (tmd750_field(cursor, final, &value, &length) != RIG_OK
            || length != width)
    {
        return -RIG_EPROTO;
    }

    for (size_t i = 0; i < length; i++)
    {
        if (value[i] < '0' || value[i] > '9')
        {
            return -RIG_EPROTO;
        }

        parsed = parsed * 10 + (uint64_t)(value[i] - '0');
    }

    if (parsed > maximum)
    {
        return -RIG_EPROTO;
    }

    *out = parsed;
    return RIG_OK;
}

static int tmd750_small(struct tmd750_cursor *cursor, int final,
                        size_t width, unsigned int maximum, uint8_t *out)
{
    uint64_t value;

    if (tmd750_decimal(cursor, final, width, maximum, &value) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    *out = (uint8_t)value;
    return RIG_OK;
}

static int tmd750_step_index(char code)
{
    return code >= 'A' ? 10 + code - 'A' : code - '0';
}

static int tmd750_step(struct tmd750_cursor *cursor, char *out)
{
    const char *value;
    size_t length;

    if (tmd750_field(cursor, 0, &value, &length) != RIG_OK || length != 1
            || !((value[0] >= '0' && value[0] <= '9')
                 || (value[0] >= 'A' && value[0] <= 'C')))
    {
        return -RIG_EPROTO;
    }

    *out = value[0];
    return RIG_OK;
}

static int tmd750_parse(const char *reply, int is_memory,
                        struct tmd750_record *record)
{
    struct tmd750_record parsed;
    struct tmd750_cursor cursor;
    const char *value;
    size_t length = strlen(reply);
    uint64_t number;

    memset(&parsed, 0, sizeof(parsed));
    parsed.is_memory = is_memory;

    if (length > 0 && reply[length - 1] == '\r')
    {
        length--;
    }

    if (length < 3 || memcmp(reply, is_memory ? "ME " : "FO ", 3) != 0)
    {
        return -RIG_EPROTO;
    }

    cursor.next = reply + 3;
    cursor.remaining = length - 3;

    if (tmd750_decimal(&cursor, 0, is_memory ? 3 : 1, is_memory ? 999 : 1,
                       &number) != RIG_OK
            || tmd750_decimal(&cursor, 0, 10, TMD750_FREQUENCY_MAX,
                              &parsed.frequency_hz) != RIG_OK
            || tmd750_decimal(&cursor, 0, 10, TMD750_FREQUENCY_MAX,
                              &parsed.offset_hz) != RIG_OK
            || tmd750_step(&cursor, &parsed.rx_step) != RIG_OK
            || tmd750_step(&cursor, &parsed.tx_step) != RIG_OK
            || tmd750_small(&cursor, 0, 1, 9, &parsed.mode) != RIG_OK
            || tmd750_small(&cursor, 0, 1, 1, &parsed.tone_enabled) != RIG_OK
            || tmd750_small(&cursor, 0, 1, 1, &parsed.ctcss_enabled) != RIG_OK
            || tmd750_small(&cursor, 0, 1, 1, &parsed.dcs_enabled) != RIG_OK
            || tmd750_small(&cursor, 0, 1, 1, &parsed.cross_enabled) != RIG_OK
            || tmd750_small(&cursor, 0, 1, 1, &parsed.reverse_enabled) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    parsed.number = (unsigned int)number;

    if (is_memory
            && tmd750_small(&cursor, 0, 1, 1, &parsed.odd_split_enabled) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    if (tmd750_small(&cursor, 0, 1, 2, &parsed.shift) != RIG_OK
            || tmd750_small(&cursor, 0, 2, 41, &parsed.tone_index) != RIG_OK
            || tmd750_small(&cursor, 0, 2, 41, &parsed.ctcss_index) != RIG_OK
            || tmd750_small(&cursor, 0, 3, 103, &parsed.dcs_index) != RIG_OK
            || tmd750_small(&cursor, 0, 1, 3, &parsed.cross_selector) != RIG_OK
            || tmd750_field(&cursor, 0, &value, &length) != RIG_OK
            || length > TMD750_URCALL_MAX)
    {
        return -RIG_EPROTO;
    }

    memcpy(parsed.urcall, value, length);
    parsed.urcall[length] = '\0';

    if (tmd750_small(&cursor, 0, 1, 2, &parsed.digital_squelch_type) != RIG_OK
            || tmd750_small(&cursor, !is_memory, 2, 99,
                            &parsed.digital_squelch_code) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    if (is_memory
            && tmd750_small(&cursor, 1, 1, 1, &parsed.lockout_enabled) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    *record = parsed;
    return RIG_OK;
}

static int tmd750_serialize_fo(const struct tmd750_record *record,
                               char *output, size_t size)
{
    int length;

    if (record->is_memory || record->number > 1
            || record->frequency_hz > TMD750_FREQUENCY_MAX
            || record->offset_hz > TMD750_FREQUENCY_MAX
            || strchr(record->urcall, ',') != NULL)
    {
        return -RIG_EINVAL;
    }

    length = snprintf(output, size,
                      "FO %u,%010" PRIu64 ",%010" PRIu64
                      ",%c,%c,%u,%u,%u,%u,%u,%u,%u,%02u,%02u,%03u,%u,%s,%u,%02u",
                      record->number, record->frequency_hz, record->offset_hz,
                      record->rx_step, record->tx_step,
                      (unsigned int)record->mode,
                      (unsigned int)record->tone_enabled,
                      (unsigned int)record->ctcss_enabled,
                      (unsigned int)record->dcs_enabled,
                      (unsigned int)record->cross_enabled,
                      (unsigned int)record->reverse_enabled,
                      (unsigned int)record->shift,
                      (unsigned int)record->tone_index,
                      (unsigned int)record->ctcss_index,
                      (unsigned int)record->dcs_index,
                      (unsigned int)record->cross_selector,
                      record->urcall,
                      (unsigned int)record->digital_squelch_type,
                      (unsigned int)record->digital_squelch_code);

    return length > 0 && (size_t)length < size ? RIG_OK : -RIG_EINVAL;
}

static int tmd750_band(RIG *rig, vfo_t vfo, char *band)
{
    vfo = (vfo == RIG_VFO_CURR) ? rig_get_current_vfo_state(rig) : vfo;

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
        *band = '0';
        return RIG_OK;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        *band = '1';
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_ENTARGET;
    }
}

/* Replies of the form "XX b,d...": check the command and band, return the number. */
static int tmd750_band_value(const char *reply, const char *command, char band,
                             int maximum, int *value)
{
    int parsed = 0;
    size_t i;

    if (strlen(reply) < 6 || reply[0] != command[0] || reply[1] != command[1]
            || reply[2] != ' ' || reply[3] != band || reply[4] != ',')
    {
        return -RIG_EPROTO;
    }

    for (i = 5; reply[i] != '\0'; i++)
    {
        if (reply[i] < '0' || reply[i] > '9' || i > 7)
        {
            return -RIG_EPROTO;
        }

        parsed = parsed * 10 + reply[i] - '0';
    }

    if (parsed > maximum)
    {
        return -RIG_EPROTO;
    }

    *value = parsed;
    return RIG_OK;
}

static int tmd750_query_band(RIG *rig, vfo_t vfo, const char *command,
                             int maximum, int *value)
{
    char band, cmd[8], reply[TMD750_BUFSIZE];
    int retval;

    retval = tmd750_band(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "%s %c", command, band);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd750_band_value(reply, command, band, maximum, value);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
    }

    return retval;
}

/* Send "XX b,value" and require the radio to echo it. */
static int tmd750_set_band(RIG *rig, vfo_t vfo, const char *command,
                           const char *format, int value)
{
    char band, cmd[16], reply[TMD750_BUFSIZE];
    int retval, length;

    retval = tmd750_band(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    length = snprintf(cmd, sizeof(cmd), "%s %c,", command, band);
    snprintf(cmd + length, sizeof(cmd) - length, format, value);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (strcmp(reply, "N") == 0)
    {
        return -RIG_ERJCTED;
    }

    return strcmp(cmd, reply) == 0 ? RIG_OK : -RIG_EPROTO;
}

static int tmd750_query_global(RIG *rig, const char *command, int maximum,
                               int *value)
{
    char reply[TMD750_BUFSIZE];
    int retval;

    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (strlen(reply) != 4 || strncmp(reply, command, 2) != 0 || reply[2] != ' '
            || reply[3] < '0' || reply[3] > '0' + maximum)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    *value = reply[3] - '0';
    return RIG_OK;
}

static int tmd750_set_global(RIG *rig, const char *command, int value)
{
    char cmd[8], reply[TMD750_BUFSIZE];
    int retval;

    SNPRINTF(cmd, sizeof(cmd), "%s %d", command, value);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    return strcmp(cmd, reply) == 0 ? RIG_OK : -RIG_EPROTO;
}

/* BC ctrl,ptt */
static int tmd750_get_bands(RIG *rig, int *ctrl, int *ptt)
{
    char reply[TMD750_BUFSIZE];
    int retval;

    retval = kenwood_transaction(rig, "BC", reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (strlen(reply) != 6 || strncmp(reply, "BC ", 3) != 0
            || (reply[3] != '0' && reply[3] != '1') || reply[4] != ','
            || (reply[5] != '0' && reply[5] != '1'))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    *ctrl = reply[3] - '0';
    *ptt = reply[5] - '0';
    return RIG_OK;
}

static int tmd750_pull_fo(RIG *rig, vfo_t vfo, struct tmd750_record *record)
{
    char band, cmd[8], reply[TMD750_BUFSIZE];
    int retval;

    retval = tmd750_band(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "FO %c", band);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (tmd750_parse(reply, 0, record) != RIG_OK
            || record->number != (unsigned int)(band - '0'))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int tmd750_push_fo(RIG *rig, struct tmd750_record *record)
{
    struct tmd750_record acknowledged;
    char cmd[TMD750_BUFSIZE], reply[TMD750_BUFSIZE];
    int retval;

    retval = tmd750_serialize_fo(record, cmd, sizeof(cmd));

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * FO on a band in memory mode changes the VFO but leaves the memory tag on
     * the display, so put the band in VFO mode first (a no-op if it already is).
     */
    {
        char vm[8], vm_reply[TMD750_BUFSIZE];

        SNPRINTF(vm, sizeof(vm), "VM %u,0", record->number);
        retval = kenwood_transaction(rig, vm, vm_reply, sizeof(vm_reply));

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (strcmp(vm, vm_reply) != 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, vm_reply);
            return -RIG_EPROTO;
        }
    }

    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (tmd750_parse(reply, 0, &acknowledged) != RIG_OK
            || acknowledged.number != record->number)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    *record = acknowledged;
    return RIG_OK;
}

static int tmd750_set_vfo(RIG *rig, vfo_t vfo)
{
    char band;

    /* Only checks the VFO; the frontend keeps it as the current VFO. */
    return vfo == RIG_VFO_CURR ? RIG_OK : tmd750_band(rig, vfo, &band);
}

static int tmd750_get_vfo(RIG *rig, vfo_t *vfo)
{
    int ctrl, ptt, retval;

    *vfo = rig_get_current_vfo_state(rig);

    if (*vfo == RIG_VFO_A || *vfo == RIG_VFO_B)
    {
        return RIG_OK;
    }

    /* Nothing selected yet: start on the band the radio transmits on. */
    retval = tmd750_get_bands(rig, &ctrl, &ptt);

    if (retval == RIG_OK)
    {
        *vfo = ptt ? RIG_VFO_B : RIG_VFO_A;
    }

    return retval;
}

static int tmd750_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct tmd750_record record;
    int retval;

    if (freq < 0.0 || freq > 9999999999.0)
    {
        return -RIG_EINVAL;
    }

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* The radio may round down to the step; the frontend rereads the frequency. */
    record.frequency_hz = (uint64_t)llround(freq);
    return tmd750_push_fo(rig, &record);
}
static int tmd750_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct tmd750_record record;
    int retval;

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *freq = (freq_t)record.frequency_hz;
    return RIG_OK;
}

static int tmd750_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int current, kmode, retval;

    switch (mode)
    {
    case RIG_MODE_FM: kmode = 0; break;

    case RIG_MODE_DSTAR: kmode = 1; break;

    case RIG_MODE_AM: kmode = 2; break;

    case RIG_MODE_FMN: kmode = 3; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    /* MD cannot enter or leave DR (MD 4); that is done on the radio. */
    retval = tmd750_query_band(rig, vfo, "MD", 9, &current);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (current == 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: the band is in DR mode\n", __func__);
        return -RIG_ERJCTED;
    }

    return tmd750_set_band(rig, vfo, "MD", "%d", kmode);
}

static int tmd750_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int kmode, retval;

    retval = tmd750_query_band(rig, vfo, "MD", 4, &kmode);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (tmd750_mode_table[kmode] == RIG_MODE_NONE)
    {
        return -RIG_EPROTO;
    }

    *mode = tmd750_mode_table[kmode];
    *width = tmd750_width_table[kmode];
    return RIG_OK;
}

static int tmd750_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char band, reply[TMD750_BUFSIZE];
    int ctrl, ptt_band, retval;

    switch (ptt)
    {
    case RIG_PTT_OFF:
        return kenwood_transaction(rig, "RX", reply, sizeof(reply));

    case RIG_PTT_ON:
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = tmd750_band(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd750_get_bands(rig, &ctrl, &ptt_band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* TX keys the radio's PTT band, which may not be the one asked for. */
    if (ptt_band != band - '0')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: the radio's PTT band is %c, not %c\n",
                  __func__, 'A' + ptt_band, 'A' + band - '0');
        return -RIG_ENTARGET;
    }

    retval = kenwood_transaction(rig, "TX", reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* The reply names the keyed band: "TX b". */
    if (strlen(reply) == 4 && strncmp(reply, "TX ", 3) == 0 && reply[3] != band)
    {
        kenwood_transaction(rig, "RX", reply, sizeof(reply));
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int tmd750_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    int busy, retval;

    retval = tmd750_query_band(rig, vfo, "BY", 1, &busy);

    if (retval == RIG_OK)
    {
        *dcd = busy ? RIG_DCD_ON : RIG_DCD_OFF;
    }

    return retval;
}

static int tmd750_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
    struct tmd750_record record;
    int retval;

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (shift)
    {
    case RIG_RPT_SHIFT_NONE: record.shift = 0; break;

    case RIG_RPT_SHIFT_PLUS: record.shift = 1; break;

    case RIG_RPT_SHIFT_MINUS: record.shift = 2; break;

    default:
        return -RIG_EINVAL;
    }

    return tmd750_push_fo(rig, &record);
}

static int tmd750_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift)
{
    struct tmd750_record record;
    int retval;

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval == RIG_OK)
    {
        *shift = tmd750_shift_table[record.shift];
    }

    return retval;
}

static int tmd750_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    struct tmd750_record record;
    int retval;

    if (offs < 0)
    {
        return -RIG_EINVAL;
    }

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.offset_hz = (uint64_t)offs;
    return tmd750_push_fo(rig, &record);
}

static int tmd750_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs)
{
    struct tmd750_record record;
    int retval;

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval == RIG_OK)
    {
        *offs = (shortfreq_t)record.offset_hz;
    }

    return retval;
}

static int tmd750_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    struct tmd750_record record;
    int index, retval;

    for (index = 0; index < 13 && tmd750_steps[index] != ts; index++)
    {
    }

    if (index == 13)
    {
        return -RIG_EINVAL;
    }

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.rx_step = index < 10 ? (char)('0' + index) : (char)('A' + index - 10);
    record.tx_step = record.rx_step;
    return tmd750_push_fo(rig, &record);
}

static int tmd750_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    struct tmd750_record record;
    int retval;

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval == RIG_OK)
    {
        *ts = tmd750_steps[tmd750_step_index(record.rx_step)];
    }

    return retval;
}

static int tmd750_tone_index(tone_t tone, const tone_t *list, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (list[i] == tone)
        {
            return i;
        }
    }

    return -1;
}

static int tmd750_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct tmd750_record record;
    int index = 0, retval;

    if (tone != 0 && (index = tmd750_tone_index(tone, kenwood42_ctcss_list, 42)) < 0)
    {
        return -RIG_EINVAL;
    }

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.tone_enabled = tone != 0;

    if (tone != 0)
    {
        record.tone_index = (uint8_t)index;
    }

    return tmd750_push_fo(rig, &record);
}

static int tmd750_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct tmd750_record record;
    int retval;

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval == RIG_OK)
    {
        *tone = record.tone_enabled ? kenwood42_ctcss_list[record.tone_index] : 0;
    }

    return retval;
}

static int tmd750_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct tmd750_record record;
    int index = 0, retval;

    if (tone != 0 && (index = tmd750_tone_index(tone, kenwood42_ctcss_list, 42)) < 0)
    {
        return -RIG_EINVAL;
    }

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.ctcss_enabled = tone != 0;

    if (tone != 0)
    {
        record.ctcss_index = (uint8_t)index;
    }

    return tmd750_push_fo(rig, &record);
}

static int tmd750_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct tmd750_record record;
    int retval;

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval == RIG_OK)
    {
        *tone = record.ctcss_enabled ? kenwood42_ctcss_list[record.ctcss_index] : 0;
    }

    return retval;
}

static int tmd750_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    struct tmd750_record record;
    int index = 0, retval;

    if (code != 0 && (index = tmd750_tone_index(code, tmd750_dcs_list, 104)) < 0)
    {
        return -RIG_EINVAL;
    }

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.dcs_enabled = code != 0;

    if (code != 0)
    {
        record.dcs_index = (uint8_t)index;
    }

    return tmd750_push_fo(rig, &record);
}

static int tmd750_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct tmd750_record record;
    int retval;

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval == RIG_OK)
    {
        *code = record.dcs_enabled ? tmd750_dcs_list[record.dcs_index] : 0;
    }

    return retval;
}

static int tmd750_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct tmd750_record record;
    int retval;

    if (status != 0 && status != 1)
    {
        return -RIG_EINVAL;
    }

    if (func == RIG_FUNC_VOX)
    {
        return tmd750_set_global(rig, "VX", status);
    }

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (func)
    {
    case RIG_FUNC_TONE: record.tone_enabled = (uint8_t)status; break;

    case RIG_FUNC_TSQL: record.ctcss_enabled = (uint8_t)status; break;

    default:
        return -RIG_EINVAL;
    }

    return tmd750_push_fo(rig, &record);
}

static int tmd750_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct tmd750_record record;
    int retval;

    if (func == RIG_FUNC_VOX)
    {
        return tmd750_query_global(rig, "VX", 1, status);
    }

    retval = tmd750_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (func)
    {
    case RIG_FUNC_TONE: *status = record.tone_enabled; break;

    case RIG_FUNC_TSQL: *status = record.ctcss_enabled; break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int tmd750_nearest_vox_delay(int tenths)
{
    int nearest = 0;

    for (int i = 1; i < 7; i++)
    {
        if (abs(tenths - tmd750_voxdelay[i]) < abs(tenths - tmd750_voxdelay[nearest]))
        {
            nearest = i;
        }
    }

    return nearest;
}

static int tmd750_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    if (level != RIG_LEVEL_VOXDELAY && level != RIG_LEVEL_ATT
            && (!isfinite(val.f) || val.f < 0.0f || val.f > 1.0f))
    {
        return -RIG_EINVAL;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        /* PC: 0 high, 1 medium, 2 low */
        return tmd750_set_band(rig, vfo, "PC", "%d",
                               val.f <= 0.1f ? 2 : (val.f <= 0.2f ? 1 : 0));

    case RIG_LEVEL_SQL:
        return tmd750_set_band(rig, vfo, "SQ", "%d",
                               (int)lroundf(val.f * TMD750_SQL_MAX));

    case RIG_LEVEL_AF:
        return tmd750_set_band(rig, vfo, "AG", "%03d",
                               (int)lroundf(val.f * TMD750_AF_MAX));

    case RIG_LEVEL_VOXGAIN:
        return tmd750_set_global(rig, "VG", (int)lroundf(val.f * 9.0f));

    case RIG_LEVEL_VOXDELAY:
        if (val.i < tmd750_voxdelay[0] || val.i > tmd750_voxdelay[6])
        {
            return -RIG_EINVAL;
        }

        return tmd750_set_global(rig, "VD", tmd750_nearest_vox_delay(val.i));

    case RIG_LEVEL_ATT:
        /* Any nonzero level switches it on; the radio has no level to set. */
        return tmd750_set_band(rig, vfo, "RA", "%d", val.i != 0);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }
}

static int tmd750_get_att(RIG *rig, vfo_t vfo, int *db)
{
    char band, cmd[8], reply[TMD750_BUFSIZE];
    int retval;

    retval = tmd750_band(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "RA %c", band);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (strlen(reply) != 6 || strncmp(reply, cmd, 4) != 0 || reply[4] != ','
            || (reply[5] != '0' && reply[5] != '1' && reply[5] != 'F'))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    *db = reply[5] == '1' ? TMD750_ATT_DB : 0;
    return RIG_OK;
}

static int tmd750_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    static const float power[3] = { 1.0f, 0.2f, 0.1f };
    int value, retval;

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        retval = tmd750_query_band(rig, vfo, "PC", 2, &value);

        if (retval == RIG_OK) { val->f = power[value]; }

        return retval;

    case RIG_LEVEL_SQL:
        retval = tmd750_query_band(rig, vfo, "SQ", TMD750_SQL_MAX, &value);

        if (retval == RIG_OK) { val->f = (float)value / TMD750_SQL_MAX; }

        return retval;

    case RIG_LEVEL_AF:
        retval = tmd750_query_band(rig, vfo, "AG", TMD750_AF_MAX, &value);

        if (retval == RIG_OK) { val->f = (float)value / TMD750_AF_MAX; }

        return retval;

    case RIG_LEVEL_RAWSTR:
        retval = tmd750_query_band(rig, vfo, "SM", TMD750_RAWSTR_MAX, &value);

        if (retval == RIG_OK) { val->i = value; }

        return retval;

    case RIG_LEVEL_VOXGAIN:
        retval = tmd750_query_global(rig, "VG", 9, &value);

        if (retval == RIG_OK) { val->f = value / 9.0f; }

        return retval;

    case RIG_LEVEL_VOXDELAY:
        retval = tmd750_query_global(rig, "VD", 6, &value);

        if (retval == RIG_OK) { val->i = tmd750_voxdelay[value]; }

        return retval;

    case RIG_LEVEL_ATT:
        return tmd750_get_att(rig, vfo, &val->i);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }
}

static int tmd750_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int memory, retval;

    if (ch < 0 || ch > 999)
    {
        return -RIG_EINVAL;
    }

    /* MR answers N unless the band is in memory mode. */
    retval = tmd750_query_band(rig, vfo, "VM", 1, &memory);

    if (retval == RIG_OK && !memory)
    {
        retval = tmd750_set_band(rig, vfo, "VM", "%d", 1);
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    return tmd750_set_band(rig, vfo, "MR", "%03d", ch);
}

static int tmd750_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char band, cmd[8], reply[TMD750_BUFSIZE];
    int retval;

    retval = tmd750_band(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "MR %c", band);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* "MR ccc", without the band */
    if (strlen(reply) != 6 || strncmp(reply, "MR ", 3) != 0
            || reply[3] < '0' || reply[3] > '9' || reply[4] < '0' || reply[4] > '9'
            || reply[5] < '0' || reply[5] > '9')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    *ch = atoi(reply + 3);
    return RIG_OK;
}

static int tmd750_set_vfo_op_mode(RIG *rig, vfo_t vfo, int memory)
{
    return tmd750_set_band(rig, vfo, "VM", "%d", memory);
}

static int tmd750_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    switch (op)
    {
    case RIG_OP_TO_VFO:
        return tmd750_set_vfo_op_mode(rig, vfo, 0);

    default:
        return -RIG_EINVAL;
    }
}

static int tmd750_pull_me(RIG *rig, int channel, struct tmd750_record *record)
{
    char cmd[16], reply[TMD750_BUFSIZE];
    int retval;

    SNPRINTF(cmd, sizeof(cmd), "ME %03d", channel);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* An empty channel is refused with "N". */
    if (strcmp(reply, "N") == 0)
    {
        return -RIG_ENAVAIL;
    }

    if (tmd750_parse(reply, 1, record) != RIG_OK
            || record->number != (unsigned int)channel
            || tmd750_mode_table[record->mode] == RIG_MODE_NONE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/* Cross tone types, as on the TH-D75: 0 DCS/Off, 1 Tone/DCS, 2 DCS/CTCSS, 3 Tone/CTCSS. */
static void tmd750_record_tones(const struct tmd750_record *record,
                                channel_t *chan)
{
    tone_t tone = kenwood42_ctcss_list[record->tone_index];
    tone_t ctcss = kenwood42_ctcss_list[record->ctcss_index];
    tone_t dcs = tmd750_dcs_list[record->dcs_index];

    chan->ctcss_tone = 0;
    chan->ctcss_sql = 0;
    chan->dcs_code = 0;
    chan->dcs_sql = 0;

    if (record->cross_enabled)
    {
        switch (record->cross_selector)
        {
        case 0: chan->dcs_code = dcs; break;

        case 1: chan->ctcss_tone = tone; chan->dcs_sql = dcs; break;

        case 2: chan->dcs_code = dcs; chan->ctcss_sql = ctcss; break;

        default: chan->ctcss_tone = tone; chan->ctcss_sql = ctcss; break;
        }
    }
    else if (record->dcs_enabled)
    {
        chan->dcs_code = dcs;
        chan->dcs_sql = dcs;
    }
    else if (record->ctcss_enabled)
    {
        chan->ctcss_tone = ctcss;
        chan->ctcss_sql = ctcss;
    }
    else if (record->tone_enabled)
    {
        chan->ctcss_tone = tone;
    }
}

static int tmd750_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                              int read_only)
{
    struct tmd750_record record;
    int retval;

    if (chan->vfo != RIG_VFO_MEM || chan->channel_num < 0
            || chan->channel_num > 999)
    {
        return -RIG_EINVAL;
    }

    retval = tmd750_pull_me(rig, chan->channel_num, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    chan->freq = (freq_t)record.frequency_hz;
    chan->mode = tmd750_mode_table[record.mode];
    chan->width = tmd750_width_table[record.mode];
    chan->tuning_step = tmd750_steps[tmd750_step_index(record.rx_step)];
    chan->rptr_shift = tmd750_shift_table[record.shift];
    chan->rptr_offs = (shortfreq_t)record.offset_hz;
    chan->split = RIG_SPLIT_OFF;
    chan->tx_freq = RIG_FREQ_NONE;

    if (record.odd_split_enabled)
    {
        chan->split = RIG_SPLIT_ON;
        chan->tx_freq = (freq_t)record.offset_hz;
        chan->tx_mode = chan->mode;
        chan->tx_width = chan->width;
        chan->rptr_shift = RIG_RPT_SHIFT_NONE;
        chan->rptr_offs = 0;
    }

    chan->funcs = record.reverse_enabled ? RIG_FUNC_REV : 0;
    tmd750_record_tones(&record, chan);
    chan->flags = record.lockout_enabled ? RIG_CHFLAG_SKIP : 0;
    return RIG_OK;
}

static int tmd750_serialize_me(const struct tmd750_record *record,
                               char *output, size_t size)
{
    int length;

    if (!record->is_memory || record->number > 999
            || record->frequency_hz > TMD750_FREQUENCY_MAX
            || record->offset_hz > TMD750_FREQUENCY_MAX
            || strchr(record->urcall, ',') != NULL)
    {
        return -RIG_EINVAL;
    }

    length = snprintf(output, size,
                      "ME %03u,%010" PRIu64 ",%010" PRIu64
                      ",%c,%c,%u,%u,%u,%u,%u,%u,%u,%u,%02u,%02u,%03u,%u,%s,%u,%02u,%u",
                      record->number, record->frequency_hz, record->offset_hz,
                      record->rx_step, record->tx_step,
                      (unsigned int)record->mode,
                      (unsigned int)record->tone_enabled,
                      (unsigned int)record->ctcss_enabled,
                      (unsigned int)record->dcs_enabled,
                      (unsigned int)record->cross_enabled,
                      (unsigned int)record->reverse_enabled,
                      (unsigned int)record->odd_split_enabled,
                      (unsigned int)record->shift,
                      (unsigned int)record->tone_index,
                      (unsigned int)record->ctcss_index,
                      (unsigned int)record->dcs_index,
                      (unsigned int)record->cross_selector,
                      record->urcall,
                      (unsigned int)record->digital_squelch_type,
                      (unsigned int)record->digital_squelch_code,
                      (unsigned int)record->lockout_enabled);

    return length > 0 && (size_t)length < size ? RIG_OK : -RIG_EINVAL;
}

/* Turn the channel's tones into the record's flags, indexes and cross type. */
static int tmd750_channel_tones(const channel_t *chan,
                                struct tmd750_record *record)
{
    int tone = -1, ctcss = -1, dcs_code = -1, dcs_sql = -1;
    /* Tone/CTCSS cross with one frequency reads back as tone squelch; keep the cross if it was stored so. */
    int was_cross = record->cross_enabled && record->cross_selector == 3;

    if ((chan->ctcss_tone && (tone = tmd750_tone_index(chan->ctcss_tone, kenwood42_ctcss_list, 42)) < 0)
            || (chan->ctcss_sql && (ctcss = tmd750_tone_index(chan->ctcss_sql, kenwood42_ctcss_list, 42)) < 0)
            || (chan->dcs_code && (dcs_code = tmd750_tone_index(chan->dcs_code, tmd750_dcs_list, 104)) < 0)
            || (chan->dcs_sql && (dcs_sql = tmd750_tone_index(chan->dcs_sql, tmd750_dcs_list, 104)) < 0))
    {
        return -RIG_EINVAL;
    }

    record->tone_enabled = 0;
    record->ctcss_enabled = 0;
    record->dcs_enabled = 0;
    record->cross_enabled = 0;

    if (tone >= 0) { record->tone_index = (uint8_t)tone; }

    if (ctcss >= 0) { record->ctcss_index = (uint8_t)ctcss; }

    if (tone >= 0 && ctcss >= 0 && (ctcss != tone || was_cross)
            && dcs_code < 0 && dcs_sql < 0)
    {
        record->cross_enabled = 1;
        record->cross_selector = 3;
    }
    else if (tone >= 0 && dcs_sql >= 0 && ctcss < 0 && dcs_code < 0)
    {
        record->cross_enabled = 1;
        record->cross_selector = 1;
        record->dcs_index = (uint8_t)dcs_sql;
    }
    else if (dcs_code >= 0 && ctcss >= 0 && tone < 0 && dcs_sql < 0)
    {
        record->cross_enabled = 1;
        record->cross_selector = 2;
        record->dcs_index = (uint8_t)dcs_code;
    }
    else if (dcs_code >= 0 && dcs_sql < 0 && tone < 0 && ctcss < 0)
    {
        record->cross_enabled = 1;
        record->cross_selector = 0;
        record->dcs_index = (uint8_t)dcs_code;
    }
    else if (dcs_sql >= 0 && (dcs_code < 0 || dcs_code == dcs_sql)
             && tone < 0 && ctcss < 0)
    {
        record->dcs_enabled = 1;
        record->dcs_index = (uint8_t)dcs_sql;
    }
    else if (ctcss >= 0 && (tone < 0 || tone == ctcss)
             && dcs_code < 0 && dcs_sql < 0)
    {
        record->ctcss_enabled = 1;
    }
    else if (tone >= 0 && ctcss < 0 && dcs_code < 0 && dcs_sql < 0)
    {
        record->tone_enabled = 1;
    }
    else if (tone >= 0 || ctcss >= 0 || dcs_code >= 0 || dcs_sql >= 0)
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static char tmd750_step_code(shortfreq_t step)
{
    for (int i = 0; i < 13; i++)
    {
        if (tmd750_steps[i] == step)
        {
            return i < 10 ? (char)('0' + i) : (char)('A' + i - 10);
        }
    }

    return '\0';
}

/* The step a new channel gets on each band, as the radio's defaults. */
static char tmd750_default_step(uint64_t frequency_hz)
{
    if (frequency_hz >= 216000000 && frequency_hz < 260000000)
    {
        return tmd750_step_code(20000);
    }

    return tmd750_step_code(frequency_hz >= 300000000 ? 25000 : 5000);
}

static int tmd750_erase_me(RIG *rig, int channel)
{
    struct tmd750_record record;
    char cmd[16], reply[TMD750_BUFSIZE];
    int retval;

    SNPRINTF(cmd, sizeof(cmd), "ME %03d,", channel);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (strcmp(reply, "N") == 0 || strcmp(reply, "?") == 0)
    {
        return -RIG_ERJCTED;
    }

    retval = tmd750_pull_me(rig, channel, &record);
    return retval == -RIG_ENAVAIL ? RIG_OK : (retval == RIG_OK ? -RIG_EPROTO : retval);
}

static int tmd750_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct tmd750_record record, stored;
    char cmd[TMD750_BUFSIZE], reply[TMD750_BUFSIZE];
    int is_new = 0, mode, retval;

    if (chan == NULL || chan->vfo != RIG_VFO_MEM
            || chan->channel_num < 0 || chan->channel_num > 999)
    {
        return -RIG_EINVAL;
    }

    if (chan->freq == RIG_FREQ_NONE)
    {
        return tmd750_erase_me(rig, chan->channel_num);
    }

    if (!isfinite(chan->freq) || chan->freq <= 0.0 || chan->freq > 9999999999.0
            || chan->rptr_offs < 0 || chan->channel_desc[0] != '\0'
            || (chan->funcs & ~RIG_FUNC_REV) != 0
            || (chan->flags & ~RIG_CHFLAG_SKIP) != 0)
    {
        return -RIG_EINVAL;
    }

    switch (chan->mode)
    {
    case RIG_MODE_FM: mode = 0; break;

    case RIG_MODE_DSTAR: mode = 1; break;

    case RIG_MODE_AM: mode = 2; break;

    case RIG_MODE_FMN: mode = 3; break;

    default: return -RIG_EINVAL;
    }

    if (chan->width != 0 && chan->width != tmd750_width_table[mode])
    {
        return -RIG_EINVAL;
    }

    /* Start from what the channel holds, so fields Hamlib does not model are kept. */
    retval = tmd750_pull_me(rig, chan->channel_num, &record);

    if (retval == -RIG_ENAVAIL)
    {
        memset(&record, 0, sizeof(record));
        record.is_memory = 1;
        record.number = (unsigned int)chan->channel_num;
        record.tone_index = 8;
        record.ctcss_index = 8;
        strcpy(record.urcall, "CQCQCQ");
        is_new = 1;
    }
    else if (retval != RIG_OK)
    {
        return retval;
    }

    record.frequency_hz = (uint64_t)llround(chan->freq);
    record.mode = (uint8_t)mode;
    record.reverse_enabled = (chan->funcs & RIG_FUNC_REV) != 0;
    record.lockout_enabled = (chan->flags & RIG_CHFLAG_SKIP) != 0;

    if (chan->tuning_step != 0)
    {
        if ((record.rx_step = tmd750_step_code(chan->tuning_step)) == '\0')
        {
            return -RIG_EINVAL;
        }
    }
    else if (is_new)
    {
        record.rx_step = tmd750_default_step(record.frequency_hz);
    }

    if (is_new)
    {
        record.tx_step = record.rx_step;
    }

    if (chan->split == RIG_SPLIT_ON)
    {
        /* Split channels pair bands only within one band or 144 with 430 MHz. */
        if (!isfinite(chan->tx_freq) || chan->tx_freq <= 0.0
                || chan->tx_freq > 9999999999.0
                || (chan->tx_mode != RIG_MODE_NONE && chan->tx_mode != chan->mode)
                || (chan->funcs & RIG_FUNC_REV) != 0)
        {
            return -RIG_EINVAL;
        }

        record.odd_split_enabled = 1;
        record.offset_hz = (uint64_t)llround(chan->tx_freq);
        record.shift = 0;
    }
    else
    {
        record.odd_split_enabled = 0;
        record.offset_hz = (uint64_t)chan->rptr_offs;

        switch (chan->rptr_shift)
        {
        case RIG_RPT_SHIFT_NONE: record.shift = 0; break;

        case RIG_RPT_SHIFT_PLUS: record.shift = 1; break;

        case RIG_RPT_SHIFT_MINUS: record.shift = 2; break;

        default: return -RIG_EINVAL;
        }
    }

    retval = tmd750_channel_tones(chan, &record);

    if (retval == RIG_OK)
    {
        retval = tmd750_serialize_me(&record, cmd, sizeof(cmd));
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (strcmp(reply, "N") == 0 || strcmp(reply, "?") == 0)
    {
        return -RIG_ERJCTED;
    }

    /* Read the channel back rather than trusting the reply's format. */
    retval = tmd750_pull_me(rig, chan->channel_num, &stored);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (stored.frequency_hz != record.frequency_hz
            || stored.offset_hz != record.offset_hz
            || stored.odd_split_enabled != record.odd_split_enabled
            || stored.mode != record.mode)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %03d did not store as sent\n",
                  __func__, chan->channel_num);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

struct rig_caps tmd750_caps =
{
    RIG_MODEL(RIG_MODEL_TMD750),
    .model_name = "TM-D750",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".0",
    .copyright = "LGPL",
    .status = RIG_STATUS_ALPHA,
    .rig_type = RIG_TYPE_MOBILE | RIG_FLAG_APRS | RIG_FLAG_TNC,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 57600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 3,

    .has_get_func = TMD750_FUNC_ALL,
    .has_set_func = TMD750_FUNC_ALL,
    .has_get_level = TMD750_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(TMD750_LEVEL_ALL),
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .level_gran =
    {
        [LVL_SQL] = { .min = { .f = 0.0f }, .max = { .f = 1.0f }, .step = { .f = 1.0f / TMD750_SQL_MAX } },
        [LVL_RFPOWER] = { .min = { .f = 0.1f }, .max = { .f = 1.0f }, .step = { .f = 0.0f } },
        [LVL_AF] = { .min = { .f = 0.0f }, .max = { .f = 1.0f }, .step = { .f = 1.0f / TMD750_AF_MAX } },
        [LVL_VOXGAIN] = { .min = { .f = 0.0f }, .max = { .f = 1.0f }, .step = { .f = 1.0f / 9.0f } },
        [LVL_VOXDELAY] = { .min = { .i = 3 }, .max = { .i = 30 }, .step = { .i = 1 } },
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = TMD750_RAWSTR_MAX }, .step = { .i = 1 } },
    },
    .ctcss_list = kenwood42_ctcss_list,
    .dcs_list = tmd750_dcs_list,
    .preamp = { RIG_DBLST_END, },
    .attenuator = { TMD750_ATT_DB, RIG_DBLST_END, },
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
    .vfo_ops = RIG_OP_TO_VFO,
    .targetable_vfo = RIG_TARGETABLE_ALL,
    .transceive = RIG_TRN_OFF,
    .bank_qty = 0,
    .chan_desc_sz = 0,
    .chan_list =
    {
        { 0, 999, RIG_MTYPE_MEM, {TMD750_CHANNEL_CAPS}},
        RIG_CHAN_END,
    },
    /* TM-D750E. Band B also receives 174-410 and 470-524 MHz. DV/DR only in the ham bands. */
    .rx_range_list1 =
    {
        {MHz(108), MHz(137), RIG_MODE_AM, -1, -1, TMD750_VFO},
        {MHz(137), MHz(144), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(144), MHz(146), TMD750_MODES, -1, -1, TMD750_VFO},
        {MHz(146), MHz(174), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(410), MHz(430), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(430), MHz(440), TMD750_MODES, -1, -1, TMD750_VFO},
        {MHz(440), MHz(470), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(174), MHz(410), TMD750_MODES_ANALOG, -1, -1, RIG_VFO_B},
        {MHz(470), MHz(524), TMD750_MODES_ANALOG, -1, -1, RIG_VFO_B},
        RIG_FRNG_END,
    },
    .tx_range_list1 =
    {
        {MHz(144), MHz(146), TMD750_MODES_TX, W(5), W(50), TMD750_VFO},
        {MHz(430), MHz(440), TMD750_MODES_TX, W(5), W(50), TMD750_VFO},
        RIG_FRNG_END,
    },
    /* TM-D750A. Band B also receives 174-216, 260-410 and 470-524 MHz. */
    .rx_range_list2 =
    {
        {MHz(108), MHz(137), RIG_MODE_AM, -1, -1, TMD750_VFO},
        {MHz(137), MHz(144), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(144), MHz(148), TMD750_MODES, -1, -1, TMD750_VFO},
        {MHz(148), MHz(174), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(216), MHz(222), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(222), MHz(225), TMD750_MODES, -1, -1, TMD750_VFO},
        {MHz(225), MHz(260), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(410), MHz(430), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(430), MHz(450), TMD750_MODES, -1, -1, TMD750_VFO},
        {MHz(450), MHz(470), TMD750_MODES_ANALOG, -1, -1, TMD750_VFO},
        {MHz(174), MHz(216), TMD750_MODES_ANALOG, -1, -1, RIG_VFO_B},
        {MHz(260), MHz(410), TMD750_MODES_ANALOG, -1, -1, RIG_VFO_B},
        {MHz(470), MHz(524), TMD750_MODES_ANALOG, -1, -1, RIG_VFO_B},
        RIG_FRNG_END,
    },
    .tx_range_list2 =
    {
        {MHz(144), MHz(148), TMD750_MODES_TX, W(5), W(50), TMD750_VFO},
        {MHz(222), MHz(225), TMD750_MODES_TX, W(5), W(20), TMD750_VFO},
        {MHz(430), MHz(450), TMD750_MODES_TX, W(5), W(50), TMD750_VFO},
        RIG_FRNG_END,
    },
    .tuning_steps =
    {
        {TMD750_MODES, kHz(1)},
        {TMD750_MODES, kHz(2.5)},
        {TMD750_MODES, kHz(5)},
        {TMD750_MODES, kHz(6.25)},
        {RIG_MODE_AM, Hz(8330)},
        {TMD750_MODES, kHz(10)},
        {TMD750_MODES, kHz(12.5)},
        {TMD750_MODES, kHz(15)},
        {TMD750_MODES, kHz(20)},
        {TMD750_MODES, kHz(25)},
        {TMD750_MODES, kHz(30)},
        {TMD750_MODES, kHz(50)},
        {TMD750_MODES, kHz(100)},
        RIG_TS_END,
    },
    .filters =
    {
        {RIG_MODE_FM, kHz(14)},
        {RIG_MODE_FMN, kHz(7)},
        {RIG_MODE_DSTAR, kHz(6)},
        {RIG_MODE_AM, kHz(9)},
        RIG_FLT_END,
    },
    .priv = (void *)&tmd750_priv_caps,

    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .set_freq = tmd750_set_freq,
    .get_freq = tmd750_get_freq,
    .set_mode = tmd750_set_mode,
    .get_mode = tmd750_get_mode,
    .set_vfo = tmd750_set_vfo,
    .get_vfo = tmd750_get_vfo,
    .set_ptt = tmd750_set_ptt,
    .get_dcd = tmd750_get_dcd,
    .set_rptr_shift = tmd750_set_rptr_shift,
    .get_rptr_shift = tmd750_get_rptr_shift,
    .set_rptr_offs = tmd750_set_rptr_offs,
    .get_rptr_offs = tmd750_get_rptr_offs,
    .set_ts = tmd750_set_ts,
    .get_ts = tmd750_get_ts,
    .set_ctcss_tone = tmd750_set_ctcss_tone,
    .get_ctcss_tone = tmd750_get_ctcss_tone,
    .set_dcs_code = tmd750_set_dcs_code,
    .get_dcs_code = tmd750_get_dcs_code,
    .set_ctcss_sql = tmd750_set_ctcss_sql,
    .get_ctcss_sql = tmd750_get_ctcss_sql,
    .set_level = tmd750_set_level,
    .get_level = tmd750_get_level,
    .set_func = tmd750_set_func,
    .get_func = tmd750_get_func,
    .set_mem = tmd750_set_mem,
    .get_mem = tmd750_get_mem,
    .vfo_op = tmd750_vfo_op,
    .get_channel = tmd750_get_channel,
    .set_channel = tmd750_set_channel,
    .get_info = th_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
