/*
 *  Hamlib Kenwood backend - TS2000 description
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (c) 2023 by Mikael Nousiainen
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hamlib/rig.h>
#include "kenwood.h"
#include "token.h"
#include "misc.h"
#include "iofunc.h"
#include "cal.h"

#define TS2000_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS2000_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS2000_AM_TX_MODES RIG_MODE_AM

#define TS2000_LEVEL_GET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_MICGAIN|RIG_LEVEL_STRENGTH|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH| \
    RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_NR|RIG_LEVEL_PREAMP|RIG_LEVEL_COMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_VOXGAIN|RIG_LEVEL_BKIN_DLYMS| \
    RIG_LEVEL_METER|RIG_LEVEL_SWR|RIG_LEVEL_COMP_METER|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW)

#define TS2000_LEVEL_SET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH| \
    RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_NR|RIG_LEVEL_PREAMP|RIG_LEVEL_COMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_VOXGAIN|RIG_LEVEL_BKIN_DLYMS| \
    RIG_LEVEL_METER|RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW)

#define TS2000_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_BC2|RIG_FUNC_RIT|RIG_FUNC_XIT| \
    RIG_FUNC_TUNER|RIG_FUNC_MON|RIG_FUNC_FBKIN|RIG_FUNC_LOCK|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_ANF)

#define TS2000_MAINVFO (RIG_VFO_A|RIG_VFO_B)
#define TS2000_SUBVFO (RIG_VFO_C)

#define TS2000_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|RIG_OP_CPY|RIG_OP_TUNE)

#define TS2000_SCAN_OP (RIG_SCAN_VFO)
#define TS2000_ANTS (RIG_ANT_1|RIG_ANT_2)

#define TS2000_STR_CAL {9, {\
               {0x00, -54},\
               {0x03, -48},\
               {0x06, -36},\
               {0x09, -24},\
               {0x0C, -12},\
               {0x0F,   0},\
               {0x14,  20},\
               {0x19,  40},\
               {0x1E,  60}}\
               }

#define TS2000_SWR_CAL { 5, \
    { \
        {   0, 1.0f }, \
        {   4, 1.5f }, \
        {   8, 2.0f }, \
        {   12, 3.0f }, \
        {   20, 10.0f } \
    } }

#define TOK_FUNC_NOISE_REDUCTION_2 TOKEN_BACKEND(102)
#define TOK_LEVEL_DSP_RX_EQUALIZER TOKEN_BACKEND(104)
#define TOK_LEVEL_DSP_TX_EQUALIZER TOKEN_BACKEND(105)
#define TOK_LEVEL_DSP_TX_BANDWIDTH TOKEN_BACKEND(106)
#define TOK_LEVEL_BEEP_VOLUME TOKEN_BACKEND(107)
#define TOK_LEVEL_TX_SIDETONE_VOLUME TOKEN_BACKEND(108)

/*
 * 38 CTCSS sub-audible tones + 1750 tone
 */
tone_t ts2000_ctcss_list[] =
{
    670,  719,  744,  770,  797,  825,  854,  885,  915,  948,
    974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
    1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862,
    1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503, 17500,
    0,
};

/*
 * 103 available DCS codes
 */
tone_t ts2000_dcs_list[] =
{
    23,  25,  26,  31,   32,  36,  43,  47,       51,  53,
    54,  65,  71,  72,  73,   74, 114, 115, 116, 122, 125, 131,
    132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205,
    212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261,
    263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343,
    346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432,
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516,
    523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654,
    662, 664, 703, 712, 723, 731, 732, 734, 743, 754,
    0,
};

int ts2000_ext_tokens[] =
{
    TOK_FUNC_NOISE_REDUCTION_2, TOK_FUNC_FILTER_WIDTH_DATA,
    TOK_LEVEL_DSP_RX_EQUALIZER, TOK_LEVEL_DSP_TX_EQUALIZER, TOK_LEVEL_DSP_TX_BANDWIDTH,
    TOK_LEVEL_BEEP_VOLUME, TOK_LEVEL_TX_SIDETONE_VOLUME,
    TOK_BACKEND_NONE,
};

const struct confparams ts2000_ext_funcs[] =
{
    {
        TOK_FUNC_NOISE_REDUCTION_2, "NR2", "Noise reduction 2", "Noise reduction 2",
        NULL, RIG_CONF_CHECKBUTTON,
    },
    { RIG_CONF_END, NULL, }
};

const struct confparams ts2000_ext_levels[] =
{
    {
        TOK_LEVEL_DSP_RX_EQUALIZER, "DSP_RX_EQUALIZER", "DSP RX equalizer", "DSP RX equalizer type",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "OFF", "H BOOST", "F PASS", "B BOOST", "CONV-EN", "USER", NULL } } }
    },
    {
        TOK_LEVEL_DSP_TX_EQUALIZER, "DSP_TX_EQUALIZER", "DSP TX equalizer", "DSP TX equalizer type",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "OFF", "H BOOST", "F PASS", "B BOOST", "CONV-EN", "USER", NULL } } }
    },
    {
        TOK_LEVEL_DSP_TX_BANDWIDTH, "DSP_TX_BANDWIDTH", "DSP TX bandwidth", "DSP TX bandwidth for SSB and AM",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "2.0 kHz", "2.2 kHz", "2.4 kHz", "2.6 kHz", "2.8 kHz", "3.0 kHz", NULL } } }
    },
    {
        TOK_LEVEL_BEEP_VOLUME, "BEEP_VOLUME", "Beep volume", "Beep volume",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    {
        TOK_LEVEL_TX_SIDETONE_VOLUME, "TX_SIDETONE_VOLUME", "TX sidetone volume", "TX sidetone volume",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    { RIG_CONF_END, NULL, }
};

static struct kenwood_filter_width ts2000_filter_width[] =
{
    { RIG_MODE_CW | RIG_MODE_CWR, 50, 50 },
    { RIG_MODE_CW | RIG_MODE_CWR, 80, 80 },
    { RIG_MODE_CW | RIG_MODE_CWR, 100, 100 },
    { RIG_MODE_CW | RIG_MODE_CWR, 150, 150 },
    { RIG_MODE_CW | RIG_MODE_CWR, 200, 200 },
    { RIG_MODE_CW | RIG_MODE_CWR, 300, 300 },
    { RIG_MODE_CW | RIG_MODE_CWR, 400, 400 },
    { RIG_MODE_CW | RIG_MODE_CWR, 500, 500 },
    { RIG_MODE_CW | RIG_MODE_CWR, 600, 600 },
    { RIG_MODE_CW | RIG_MODE_CWR, 1000, 1000 },
    { RIG_MODE_CW | RIG_MODE_CWR, 2000, 2000 },
    { RIG_MODE_RTTY | RIG_MODE_RTTYR, 250, 250 },
    { RIG_MODE_RTTY | RIG_MODE_RTTYR, 500, 500 },
    { RIG_MODE_RTTY | RIG_MODE_RTTYR, 1000, 1000 },
    { RIG_MODE_RTTY | RIG_MODE_RTTYR, 1500, 1500 },
    { RIG_MODE_SSB, 0, 2400 },
    { RIG_MODE_SSB, 1, 500 }, // NAR1 optional filter
    { RIG_MODE_SSB, 2, 270 }, // NAR2 optional filter
    { RIG_MODE_FM, 0, 6000 },
    { RIG_MODE_FM, 1, 12000 },
    { RIG_MODE_AM, 0, 2400 },
    { RIG_MODE_AM, 1, 6000 }, // NAR1 optional filter (?)
    { RIG_MODE_NONE, -1, -1 },
};

static struct kenwood_slope_filter ts2000_slope_filter_high[] =
{
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 0, 1400 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 1, 1600 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 2, 1800 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 3, 2000 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 4, 2200 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 5, 2400 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 6, 2600 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 7, 2800 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 8, 3000 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 9, 3400 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 10, 4000 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 11, 5000 },
    { RIG_MODE_AM, 0, 0, 2500 },
    { RIG_MODE_AM, 0, 1, 3000 },
    { RIG_MODE_AM, 0, 2, 4000 },
    { RIG_MODE_AM, 0, 3, 5000 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 0, 170 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 1, 1930 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 2, 2160 },
    { RIG_MODE_NONE, 0, -1, -1 },
};

static struct kenwood_slope_filter ts2000_slope_filter_low[] =
{
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 0, 0 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 1, 50 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 2, 100 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 3, 200 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 4, 300 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 5, 400 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 6, 500 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 7, 600 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 8, 700 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 9, 800 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 10, 900 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 10, 1, 1000 },
    { RIG_MODE_AM, 0, 0, 0 },
    { RIG_MODE_AM, 0, 1, 100 },
    { RIG_MODE_AM, 0, 2, 200 },
    { RIG_MODE_AM, 0, 3, 500 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 0, 2500 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 1, 1000 },
    { RIG_MODE_NONE, 0, -1, -1 },
};

static struct kenwood_priv_caps  ts2000_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .filter_width = ts2000_filter_width,
    .slope_filter_high = ts2000_slope_filter_high,
    .slope_filter_low = ts2000_slope_filter_low,
};

/* memory capabilities */
#define TS2000_MEM_CAP {    \
    .freq = 1,      \
    .mode = 1,      \
    .tx_freq=1,     \
    .tx_mode=1,     \
    .split=1,       \
    .rptr_shift=1,      \
    .rptr_offs=1,       \
    .funcs=RIG_FUNC_REV|RIG_FUNC_TONE|RIG_FUNC_TSQL,\
    .tuning_step=1, \
    .ctcss_tone=1,      \
    .ctcss_sql=1,       \
    .dcs_code=1,        \
    .dcs_sql=1,     \
    .scan_group=1,  \
    .flags=1,       \
    .channel_desc=1     \
}


/*
 * Function definitions below
 */

int ts2000_init(RIG *rig)
{
    struct kenwood_priv_data *priv;
    int retval;

    ENTERFUNC;

    retval = kenwood_init(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv = (struct kenwood_priv_data *) rig->state.priv;

    priv->ag_format = 3;
    priv->micgain_min = 0;
    priv->micgain_max = 100;

    RETURNFUNC(RIG_OK);
}

static int ts2000_set_ex_menu(RIG *rig, int number, int value_len, int value)
{
    char buf[20];

    ENTERFUNC;

    SNPRINTF(buf, sizeof(buf), "EX%03d0000%0*d", number, value_len, value);

    RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));
}

static int ts2000_get_ex_menu(RIG *rig, int number, int value_len, int *value)
{
    int retval;
    char buf[20];
    char ackbuf[20];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    SNPRINTF(buf, sizeof(buf), "EX%03d0000", number);

    retval = kenwood_safe_transaction(rig, buf, ackbuf, sizeof(ackbuf),
                                      9 + value_len);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    sscanf(ackbuf + 9, "%d", value);

    RETURNFUNC2(RIG_OK);
}

static int ts2000_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[20];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_MON:
        SNPRINTF(buf, sizeof(buf), "ML00%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_LOCK:
        SNPRINTF(buf, sizeof(buf), "LK%c%c", (status == 0) ? '0' : '1',
                 (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    default:
        return kenwood_set_func(rig, vfo, func, status);
    }
}

static int ts2000_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char buf[20];
    int retval;

    ENTERFUNC;

    switch (func)
    {
    case RIG_FUNC_MON:
    {
        int raw_value;
        retval = kenwood_safe_transaction(rig, "ML", buf, sizeof(buf), 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(buf, "ML%d", &raw_value);

        *status = (raw_value > 0);
        break;
    }

    case RIG_FUNC_LOCK:
        retval = kenwood_safe_transaction(rig, "LK", buf, sizeof(buf), 4);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        *status = buf[2] != '0' || buf[3] != '0';
        break;

    default:
        return kenwood_get_func(rig, vfo, func, status);
    }

    RETURNFUNC(RIG_OK);
}

/*
 * WARNING: The commands differ slightly from the general versions in kenwood.c
 * e.g.: "SQ"=>"SQ0" , "AG"=>"AG0"
 */
static int ts2000_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int kenwood_val;
    char vfo_num = (vfo == RIG_VFO_C) ? '1' : '0';

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_RF:
        kenwood_val = val.f * 255;
        SNPRINTF(levelbuf, sizeof(levelbuf), "RG%03d", kenwood_val);
        break;

    case RIG_LEVEL_AF:
        return kenwood_set_level(rig, vfo, level, val);

    case RIG_LEVEL_SQL:
        kenwood_val = val.f * 255;
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ%c%03d", vfo_num, kenwood_val);
        break;

    case RIG_LEVEL_AGC:

        /* Possible values for TS-2000 are 0(=off)-020(=slow) */

        switch (val.i)
        {
        case RIG_AGC_OFF:
            kenwood_val = 0;
            break;

        case RIG_AGC_SUPERFAST:
            kenwood_val = 1;
            break;

        case RIG_AGC_FAST:
            kenwood_val = 5;
            break;

        case RIG_AGC_MEDIUM:
            kenwood_val = 10;
            break;

        case RIG_AGC_SLOW:
            kenwood_val = 20;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported agc value", __func__);
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "GT%03d", kenwood_val);
        break;

    case RIG_LEVEL_MONITOR_GAIN:
        kenwood_val = val.f * 9.0;
        SNPRINTF(levelbuf, sizeof(levelbuf), "ML%03d", kenwood_val);
        break;

    case RIG_LEVEL_NB:
        kenwood_val = val.f * 10.0;
        SNPRINTF(levelbuf, sizeof(levelbuf), "NL%03d", kenwood_val);
        break;

    case RIG_LEVEL_NR:
        kenwood_val = val.f * 9.0;
        SNPRINTF(levelbuf, sizeof(levelbuf), "RL%02d", kenwood_val);
        break;

    case RIG_LEVEL_PREAMP:
        if (val.i != 12 && val.i != 0)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "PA%c", (val.i == 12) ? '1' : '0');
        break;

    case RIG_LEVEL_ATT:
        if (val.i != 12 && val.i != 0)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "RA%02d", (val.i == 12) ? 1 : 0);
        break;

    case RIG_LEVEL_METER:
        switch (val.i)
        {
        case RIG_METER_SWR:
            kenwood_val = 1;
            break;

        case RIG_METER_COMP:
            kenwood_val = 2;
            break;

        case RIG_METER_ALC:
            kenwood_val = 3;
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "RM%d", kenwood_val);
        break;

    case RIG_LEVEL_CWPITCH:
        if (val.i > 1000 || val.i < 400)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        RETURNFUNC(ts2000_set_ex_menu(rig, 31, 2, (val.i - 400) / 50));

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    return kenwood_transaction(rig, levelbuf, NULL, 0);
}

// TS-2000 can only read one meter at a time and the user must select
// the meter using RIG_LEVEL_METER. This function returns the meter value if
// the selected meter matches the expected meter.
static int ts2000_read_meter(RIG *rig, int expected_meter, int *value)
{
    int retval;
    char cmdbuf[8];
    struct rig_state *rs = &rig->state;
    char ackbuf[32];
    int expected_len = 8;
    int read_meter;
    int read_value;

    ENTERFUNC;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "RM;");

    retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

    rig_debug(RIG_DEBUG_TRACE, "%s: write_block retval=%d\n", __func__, retval);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    // TS-2000 returns values for a single meter at the same time, for example: RM10000;

    retval = read_string(&rs->rigport, (unsigned char *) ackbuf, expected_len + 1,
                         NULL, 0, 0, 1);

    rig_debug(RIG_DEBUG_TRACE, "%s: read_string retval=%d\n", __func__, retval);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to read rig response\n", __func__);
        RETURNFUNC(retval);
    }

    if (retval != expected_len)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: expected %d bytes, got %d in '%s'\n", __func__,
                  expected_len, retval, ackbuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    retval = sscanf(ackbuf, "RM%1d%d;", &read_meter, &read_value);

    if (retval != 2)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: expected 2 values to parse for meters, got %d in '%s'\n", __func__, retval,
                  ackbuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    if (read_meter == expected_meter)
    {
        *value = read_value;
    }
    else
    {
        *value = 0;
    }

    RETURNFUNC(RIG_OK);
}


static int ts2000_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char cmdbuf[8];
    char ackbuf[50];
    size_t ack_len, ack_len_expected;
    int levelint;
    int retval;
    char vfo_num = (vfo == RIG_VFO_C) ? '1' : '0';

    ENTERFUNC;

    switch (level)
    {
    case RIG_LEVEL_AF:
        return kenwood_get_level(rig, vfo, level, val);

    case RIG_LEVEL_RF:
        retval = kenwood_transaction(rig, "RG", ackbuf, sizeof(ackbuf));

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (5 != ack_len)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[2], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        val->f = levelint / (float) 255;
        return RIG_OK;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "SQ%c", vfo_num);
        retval = kenwood_transaction(rig, cmdbuf, ackbuf, sizeof(ackbuf));
        ack_len_expected = 6;

        if (retval != RIG_OK)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (ack_len != ack_len_expected)
        {
            return -RIG_EPROTO;
        }

        if (sscanf(&ackbuf[ack_len_expected - 3], "%d", &levelint) != 1)
        {
            return -RIG_EPROTO;
        }

        val->f = (float) levelint / 255.f;
        return RIG_OK;

    case RIG_LEVEL_AGC:
        retval = kenwood_transaction(rig, "GT", ackbuf, sizeof(ackbuf));
        ack_len_expected = 5;

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (ack_len != ack_len_expected)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[ack_len_expected - 3], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        if (levelint == 0)
        {
            val->i = RIG_AGC_OFF;
        }
        else if (levelint <= 1)
        {
            val->i = RIG_AGC_SUPERFAST;
        }
        else if (levelint <= 5)
        {
            val->i = RIG_AGC_FAST;
        }
        else if (levelint <= 10)
        {
            val->i = RIG_AGC_MEDIUM;
        }
        else
        {
            val->i = RIG_AGC_SLOW;
        }

        return RIG_OK;

    case RIG_LEVEL_STRENGTH:
        if (rig->state.cache.ptt != RIG_PTT_OFF)
        {
            val->i = -9 * 6;
            break;
        }

        return kenwood_get_level(rig, vfo, level, val);

    case RIG_LEVEL_MONITOR_GAIN:
    {
        int raw_value;
        retval = kenwood_safe_transaction(rig, "ML", ackbuf, sizeof(ackbuf), 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(ackbuf, "ML%d", &raw_value);

        val->f = (float) raw_value / 9.0f;
        break;
    }

    case RIG_LEVEL_NB:
    {
        int raw_value;
        retval = kenwood_safe_transaction(rig, "NL", ackbuf, sizeof(ackbuf), 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(ackbuf, "NL%d", &raw_value);

        val->f = (float) raw_value / 10.0f;
        break;
    }

    case RIG_LEVEL_NR:
    {
        int raw_value;
        retval = kenwood_safe_transaction(rig, "RL", ackbuf, sizeof(ackbuf), 4);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(ackbuf, "RL%d", &raw_value);

        val->f = (float) raw_value / 9.0f;
        break;
    }

    case RIG_LEVEL_PREAMP:
        retval = kenwood_safe_transaction(rig, "PA", ackbuf, sizeof(ackbuf), 4);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->i = ackbuf[2] == '1' ? 12 : 0;
        break;

    case RIG_LEVEL_ATT:
        retval = kenwood_safe_transaction(rig, "RA", ackbuf, sizeof(ackbuf), 6);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->i = ackbuf[3] == '1' ? 12 : 0;
        break;

    case RIG_LEVEL_METER:
    {
        int raw_value;

        // TODO: Read all meters at the same time: RM10000;RM20000;RM30000;

        retval = kenwood_safe_transaction(rig, "RM", ackbuf, sizeof(ackbuf), 7);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(ackbuf, "RM%1d", &raw_value);

        switch (raw_value)
        {
        case 1:
            val->i = RIG_METER_SWR;
            break;

        case 2:
            val->i = RIG_METER_COMP;
            break;

        case 3:
            val->i = RIG_METER_ALC;
            break;

        default:
            val->i = RIG_METER_NONE;
        }

        break;
    }

    case RIG_LEVEL_SWR:
    case RIG_LEVEL_COMP_METER:
    case RIG_LEVEL_ALC:
    {
        int meter;
        int swr;
        int comp;
        int *value;
        int alc;

        switch (level)
        {
        case RIG_LEVEL_SWR:
            meter = 1;
            value = &swr;
            break;

        case RIG_LEVEL_COMP_METER:
            meter = 2;
            value = &comp;
            break;

        case RIG_LEVEL_ALC:
            meter = 3;
            value = &alc;
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts2000_read_meter(rig, meter, value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        switch (level)
        {
        case RIG_LEVEL_SWR:
            if (rig->caps->swr_cal.size)
            {
                val->f = rig_raw2val_float(swr, &rig->caps->swr_cal);
            }
            else
            {
                val->f = (float) swr / 2.0f;
            }

            break;

        case RIG_LEVEL_COMP_METER:
            val->f = (float) comp; // Maximum value is 20dB
            break;

        case RIG_LEVEL_ALC:
            // Maximum value is 20, so have the max at 5 just to be on the range where other rigs report ALC
            val->f = (float) alc / 4.0f;
            break;

        default:
            return -RIG_ENAVAIL;
        }

        break;
    }

    case RIG_LEVEL_RFPOWER_METER:
    {
        int raw_value;
        char read_vfo_num;

        if (rig->state.cache.ptt == RIG_PTT_OFF)
        {
            val->f = 0;
            break;
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "SM%c", vfo_num);

        retval = kenwood_safe_transaction(rig, cmdbuf, ackbuf, sizeof(ackbuf), 7);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(ackbuf, "SM%c%d", &read_vfo_num, &raw_value);

        if (vfo_num == '1')
        {
            val->f = (float) raw_value / 15.0f;
        }
        else
        {
            val->f = (float) raw_value / 30.0f;
        }

        break;
    }

    case RIG_LEVEL_CWPITCH:
    {
        int raw_value;
        retval = ts2000_get_ex_menu(rig, 31, 2, &raw_value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->i = 400 + raw_value * 50;
        break;
    }

    default:
        return kenwood_get_level(rig, vfo, level, val);
    }

    RETURNFUNC(RIG_OK);
}

static int ts2000_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    char buf[20];
    int retval;
    int rit_enabled;
    int xit_enabled;

    ENTERFUNC;

    if (rit < -19999 || rit > 19999)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    // RC clear command cannot be executed if RIT/XIT is not enabled

    retval = kenwood_get_func(rig, vfo, RIG_FUNC_RIT, &rit_enabled);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (!rit_enabled)
    {
        retval = kenwood_get_func(rig, vfo, RIG_FUNC_XIT, &xit_enabled);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    if (!rit_enabled && !xit_enabled)
    {
        retval = kenwood_set_func(rig, vfo, RIG_FUNC_RIT, 1);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    retval = kenwood_transaction(rig, "RC", NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (!rit_enabled && !xit_enabled)
    {
        retval = kenwood_set_func(rig, vfo, RIG_FUNC_RIT, 0);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    if (rit == 0)
    {
        RETURNFUNC(RIG_OK);
    }

    SNPRINTF(buf, sizeof(buf), "R%c%05d", (rit > 0) ? 'U' : 'D', abs((int) rit));
    retval = kenwood_transaction(rig, buf, NULL, 0);

    RETURNFUNC(retval);
}

static int ts2000_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    int retval;
    char buf[7];
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!rit)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = kenwood_get_if(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    memcpy(buf, &priv->info[18], 5);

    buf[6] = '\0';
    *rit = atoi(buf);

    RETURNFUNC(RIG_OK);
}

static int ts2000_set_ext_func(RIG *rig, vfo_t vfo, token_t token, int status)
{
    char cmdbuf[20];
    int retval;

    ENTERFUNC;

    switch (token)
    {
    case TOK_FUNC_NOISE_REDUCTION_2:
        if (status < 0 || status > 1)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "NR%d", status ? 2 : 0);
        retval = kenwood_transaction(rig, cmdbuf, NULL, 0);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static int ts2000_get_ext_func(RIG *rig, vfo_t vfo, token_t token, int *status)
{
    int retval;

    ENTERFUNC;

    switch (token)
    {
    case TOK_FUNC_NOISE_REDUCTION_2:
    {
        int value;
        char ackbuf[20];

        retval = kenwood_safe_transaction(rig, "NR", ackbuf, sizeof(ackbuf), 3);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(ackbuf, "NR%d", &value);

        *status = (value == 2) ? 1 : 0;
        break;
    }

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static int ts2000_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    int retval;

    ENTERFUNC;

    switch (token)
    {
    case TOK_LEVEL_DSP_RX_EQUALIZER:
        if (val.i < 0 || val.i > 5)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts2000_set_ex_menu(rig, 20, 1, val.i);
        break;

    case TOK_LEVEL_DSP_TX_EQUALIZER:
        if (val.i < 0 || val.i > 5)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts2000_set_ex_menu(rig, 21, 1, val.i);
        break;

    case TOK_LEVEL_DSP_TX_BANDWIDTH:
        if (val.i < 0 || val.i > 5)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts2000_set_ex_menu(rig, 22, 1, val.i);
        break;

    case TOK_LEVEL_BEEP_VOLUME:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts2000_set_ex_menu(rig, 12, 1, (int) val.f);
        break;

    case TOK_LEVEL_TX_SIDETONE_VOLUME:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts2000_set_ex_menu(rig, 13, 1, (int) val.f);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static int ts2000_get_ext_level(RIG *rig, vfo_t vfo, token_t token,
                                value_t *val)
{
    int retval;
    int value;

    ENTERFUNC;

    switch (token)
    {
    case TOK_LEVEL_DSP_RX_EQUALIZER:
        retval = ts2000_get_ex_menu(rig, 20, 1, &value);
        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_EQUALIZER:
        retval = ts2000_get_ex_menu(rig, 21, 1, &value);
        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_BANDWIDTH:
        retval = ts2000_get_ex_menu(rig, 22, 1, &value);
        val->i = value;
        break;

    case TOK_LEVEL_BEEP_VOLUME:
        retval = ts2000_get_ex_menu(rig, 12, 1, &value);
        val->f = value;
        break;

    case TOK_LEVEL_TX_SIDETONE_VOLUME:
        retval = ts2000_get_ex_menu(rig, 13, 1, &value);
        val->f = value;
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

/*
 * ts2000_get_channel
 * Read command format:
   M|R|P1|P2|P3|P3|;|
 * P1: 0 - RX frequency, 1 - TX frequency
     Memory channel 290 ~ 299: P1=0 (start frequency), P1=1 (end frequency)
   P2 - bank number
        allowed values: <space>, 0, 1 or 2
   P3 - channel number 00-99

   Returned value:
    M | R |P1 |P2 |P3 |P3 |P4 |P4 |P4 |P4 |
   P4 |P4 |P4 |P4 |P4 |P4 |P4 |P5 |P6 |P7 |
   P8 |P8 |P9 |P9 |P10|P10|P10|P11|P12|P13|
   P13|P13|P13|P13|P13|P13|P13|P13|P14|P14|
   P15|P16|P16|P16|P16|P16|P16|P16|P16| ; |
   P1 - P3 described above
   P4: Frequency in Hz (11-digit).
   P5: Mode. 1: LSB, 2: USB, 3: CW, 4: FM, 5: AM, 6: FSK, 7: CR-R, 8: Reserved, 9: FSK-R
   P6: Lockout status. 0: Lockout OFF, 1: Lockout ON.
   P7: 0: OFF, 1: TONE, 2: CTCSS, 3: DCS.
   P8: Tone Number. Allowed values 01 (67Hz) - 38 (250.3Hz)
   P9: CTCSS tone number. Allowed values 01 (67Hz) - 38 (250.3Hz)
  P10: DCS code. Allowed values  000 (023 DCS code) to 103 (754 DCS code).
  P11: REVERSE status.
  P12: SHIFT status. 0: Simplex, 1: +, 2: –, 3: = (All E-types)
  P13: Offset frequency in Hz (9-digit).
      Allowed values 000000000 - 059950000 in steps of 50000. Unused digits must be 0.
  P14: Step size. Allowed values:
      for SSB, CW, FSK mode: 00 - 03
      00: 1 kHz, 01: 2.5 kHz, 02: 5 kHz, 03: 10 kHz
      for AM, FM mode: 00 - 09
      00: 5 kHz, 01: 6.25 kHz, 02: 10 kHz, 03: 12.5 kHz, 04: 15 kHz,
      05: 20 kHz, 06: 25 kHz, 07: 30 kHz,  08: 50 kHz, 09: 100 kHz
  P15: Memory Group number (0 ~ 9).
  P16: Memory name. A maximum of 8 characters.

 */

int ts2000_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    int err;
    int tmp;
    size_t length;
    char buf[52];
    char cmd[8];
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!chan || chan->vfo != RIG_VFO_MEM)
    {
        return -RIG_EINVAL;
    }

    /* put channel num in the command string */
    SNPRINTF(cmd, sizeof(cmd), "MR0%03d;", chan->channel_num);

    err = kenwood_transaction(rig, cmd, buf, sizeof(buf));

    if (err != RIG_OK)
    {
        return err;
    }

    length = strlen(buf);
    memset(chan, 0x00, sizeof(channel_t));

    chan->vfo = RIG_VFO_MEM;

    /* parse from right to left */

    /* XXX based on the available documentation, there is no command
     * to read out the filters of a given memory channel. The rig, however,
     * stores this information.
     */
    /* First check if a name is assigned.
       Name is returned at positions 41-48 (counting from 0) */
    if (length > 41)
    {
//    rig_debug(RIG_DEBUG_VERBOSE, "Copying channel description: %s\n", &buf[ 41 ] );
        strcpy(chan->channel_desc, &buf[ 41 ]);
    }

    /* Memory group no */
    chan->scan_group = buf[ 40 ] - '0';
    /* Fields 38-39 contain tuning step as a number 00 - 09.
       Tuning step depends on this number and the mode,
       just save it for now */
    buf[ 40 ] = '\0';
    tmp = atoi(&buf[ 38]);
    /* Offset frequency */
    buf[ 38 ] = '\0';
    chan->rptr_offs = atoi(&buf[ 29 ]);

    /* Shift type
       WARNING: '=' shift type not programmed */
    if (buf[ 28 ] == '1')
    {
        chan->rptr_shift = RIG_RPT_SHIFT_PLUS;
    }
    else
    {
        if (buf[ 28 ] == '2')
        {
            chan->rptr_shift = RIG_RPT_SHIFT_MINUS;
        }
        else
        {
            chan->rptr_shift =  RIG_RPT_SHIFT_NONE;
        }
    }

    /* Reverse status */
    if (buf[27] == '1')
    {
        chan->funcs |= RIG_FUNC_REV;
    }

    /* Check for tone, CTCSS and DCS */
    /* DCS code first */
    if (buf[ 19 ] == '3')
    {
        if (rig->caps->dcs_list)
        {
            buf[ 27 ] = '\0';
            chan->dcs_code = rig->caps->dcs_list[ atoi(&buf[ 24 ]) ];
            chan->dcs_sql = chan->dcs_code;
            chan->ctcss_tone = 0;
            chan->ctcss_sql = 0;
        }
    }
    else
    {
        chan->dcs_code = 0;
        chan->dcs_sql = 0;
        /* CTCSS code
           Caution, CTCSS codes, unlike DCS codes, are numbered from 1! */
        buf[ 24 ] = '\0';

        if (buf[ 19 ] == '2')
        {
            chan->funcs |= RIG_FUNC_TSQL;

            if (rig->caps->ctcss_list)
            {
                chan->ctcss_sql = rig->caps->ctcss_list[ atoi(&buf[22]) - 1 ];
                chan->ctcss_tone = 0;
            }
        }
        else
        {
            chan->ctcss_sql = 0;

            /* CTCSS tone */
            if (buf[ 19 ] == '1')
            {
                chan->funcs |= RIG_FUNC_TONE;
                buf[ 22 ] = '\0';

                if (rig->caps->ctcss_list)
                {
                    chan->ctcss_tone = rig->caps->ctcss_list[ atoi(&buf[20]) - 1 ];
                }
            }
            else
            {
                chan->ctcss_tone = 0;
            }
        }
    }


    /* memory lockout */
    if (buf[18] == '1')
    {
        chan->flags |= RIG_CHFLAG_SKIP;
    }

    /* mode */
    chan->mode = kenwood2rmode(buf[17] - '0', caps->mode_table);

    /* Now we have the mode, let's finish the tuning step */
    if ((chan->mode == RIG_MODE_AM) || (chan->mode == RIG_MODE_FM))
    {
        switch (tmp)
        {
        case 0: chan->tuning_step = kHz(5); break;

        case 1: chan->tuning_step = kHz(6.25); break;

        case 2: chan->tuning_step = kHz(10); break;

        case 3: chan->tuning_step = kHz(12.5); break;

        case 4: chan->tuning_step = kHz(15); break;

        case 5: chan->tuning_step = kHz(20); break;

        case 6: chan->tuning_step = kHz(25); break;

        case 7: chan->tuning_step = kHz(30); break;

        case 8: chan->tuning_step = kHz(50); break;

        case 9: chan->tuning_step = kHz(100); break;

        default: chan->tuning_step = 0;
        }
    }
    else
    {
        switch (tmp)
        {
        case 0: chan->tuning_step = kHz(1); break;

        case 1: chan->tuning_step = kHz(2.5); break;

        case 2: chan->tuning_step = kHz(5); break;

        case 3: chan->tuning_step = kHz(10); break;

        default: chan->tuning_step = 0;
        }
    }

    /* Frequency */
    buf[17] = '\0';
    chan->freq = atoi(&buf[6]);

    if (chan->freq == RIG_FREQ_NONE)
    {
        return -RIG_ENAVAIL;
    }

    buf[6] = '\0';
    chan->channel_num = atoi(&buf[3]);


    /* Check split freq */
    cmd[2] = '1';
    err = kenwood_transaction(rig, cmd, buf, sizeof(buf));

    if (err != RIG_OK)
    {
        return err;
    }

    chan->tx_mode = kenwood2rmode(buf[17] - '0', caps->mode_table);

    buf[17] = '\0';
    chan->tx_freq = atoi(&buf[6]);

    if (chan->freq == chan->tx_freq)
    {
        chan->tx_freq = RIG_FREQ_NONE;
        chan->tx_mode = RIG_MODE_NONE;
        chan->split = RIG_SPLIT_OFF;
    }
    else
    {
        chan->split = RIG_SPLIT_ON;
    }

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

int ts2000_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char sqltype = '0';
    char buf[128];
    char mode, tx_mode = 0;
    char shift = '0';
    short dcscode = 0;
    short code = 0;
    int tstep = 0;
    int err;
    int tone = 0;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!chan)
    {
        return -RIG_EINVAL;
    }

    mode = rmode2kenwood(chan->mode, caps->mode_table);

    if (mode < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(chan->mode));
        return -RIG_EINVAL;
    }

    if (chan->split == RIG_SPLIT_ON)
    {
        tx_mode = rmode2kenwood(chan->tx_mode, caps->mode_table);

        if (tx_mode < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                      __func__, rig_strrmode(chan->tx_mode));
            return -RIG_EINVAL;
        }
    }

    /* find tone */

    if (chan->ctcss_tone)
    {
        for (; rig->caps->ctcss_list[tone] != 0; tone++)
        {
            if (chan->ctcss_tone == rig->caps->ctcss_list[tone])
            {
                break;
            }
        }

        if (chan->ctcss_tone != rig->caps->ctcss_list[tone]) { tone = -1; }
        else { sqltype = '1'; }
    }
    else
    {
        tone = -1; /* -1 because we will add 1 when outputting; this is necessary as CTCSS codes are numbered from 1 */
    }

    /* find CTCSS code */

    if (chan->ctcss_sql)
    {
        for (; rig->caps->ctcss_list[code] != 0; code++)
        {
            if (chan->ctcss_sql == rig->caps->ctcss_list[code])
            {
                break;
            }
        }

        if (chan->ctcss_sql != rig->caps->ctcss_list[code]) { code = -1; }
        else { sqltype = '2'; }
    }
    else
    {
        code = -1;
    }

    /* find DCS code */

    if (chan->dcs_code)
    {
        for (; rig->caps->dcs_list[dcscode] != 0; dcscode++)
        {
            if (chan->dcs_code == rig->caps->dcs_list[dcscode])
            {
                break;
            }
        }

        if (chan->dcs_code != rig->caps->dcs_list[dcscode]) { dcscode = 0; }
        else { sqltype = '3'; }
    }
    else
    {
        dcscode = 0;
    }

    if (chan->rptr_shift == RIG_RPT_SHIFT_PLUS)
    {
        shift = '1';
    }

    if (chan->rptr_shift ==  RIG_RPT_SHIFT_MINUS)
    {
        shift = '2';
    }


    if ((chan->mode == RIG_MODE_AM) || (chan->mode == RIG_MODE_FM))
    {
        switch (chan->tuning_step)
        {
        case s_kHz(6.25):   tstep = 1; break;

        case s_kHz(10):     tstep = 2; break;

        case s_kHz(12.5):   tstep = 3; break;

        case s_kHz(15):     tstep = 4; break;

        case s_kHz(20):     tstep = 5; break;

        case s_kHz(25):     tstep = 6; break;

        case s_kHz(30):     tstep = 7; break;

        case s_kHz(50):     tstep = 8; break;

        case s_kHz(100):    tstep = 9; break;

        default:            tstep = 0;
        }
    }
    else
    {
        switch (chan->tuning_step)
        {
        case s_kHz(2.5):    tstep = 1; break;

        case s_kHz(5):      tstep = 2; break;

        case s_kHz(10):     tstep = 3; break;

        default:            tstep = 0;
        }
    }

    /* P-number       2-3    4 5 6 7   8   9  101112  13 141516  */
    SNPRINTF(buf, sizeof(buf), "MW0%03d%011u%c%c%c%02d%02d%03d%c%c%09d0%c%c%s;",
             chan->channel_num,
             (unsigned) chan->freq,      /*  4 - frequency */
             '0' + mode,                 /*  5 - mode */
             (chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',    /*  6 - lockout status */
             sqltype,                    /*  7 - squelch and tone type */
             tone + 1,                   /*  8 - tone code */
             code + 1,                   /*  9 - CTCSS code */
             dcscode,                    /* 10 - DCS code */
             (chan->funcs & RIG_FUNC_REV) ? '1' : '0', /* 11 - Reverse status */
             shift,                      /* 12 - shift type */
             (int) chan->rptr_offs,              /* 13 - offset frequency */
             tstep + '0',                        /* 14 - Step size */
             chan->scan_group + '0',         /* 15 - Memory group no */
             chan->channel_desc              /* 16 - description */
            );
    rig_debug(RIG_DEBUG_VERBOSE, "The command will be: %s\n", buf);

    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    if (chan->split == RIG_SPLIT_ON)
    {
        SNPRINTF(buf, sizeof(buf), "MW1%03d%011u%c%c%c%02d%02d%03d%c%c%09d0%c%c%s;\n",
                 chan->channel_num,
                 (unsigned) chan->tx_freq,       /*  4 - frequency */
                 '0' + tx_mode,                  /*  5 - mode */
                 (chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',    /*  6 - lockout status */
                 sqltype,                    /*  7 - squelch and tone type */
                 tone + 1,                   /*  8 - tone code */
                 code + 1,                   /*  9 - CTCSS code */
                 dcscode + 1,                /* 10 - DCS code */
                 (chan->funcs & RIG_FUNC_REV) ? '1' : '0', /* 11 - Reverse status */
                 shift,                      /* 12 - shift type */
                 (int) chan->rptr_offs,              /* 13 - offset frequency */
                 tstep + '0',                        /* 14 - Step size */
                 chan->scan_group + '0',         /* Memory group no */
                 chan->channel_desc              /* 16 - description */
                );
        rig_debug(RIG_DEBUG_VERBOSE, "Split, the command will be: %s\n", buf);

        err = kenwood_transaction(rig, buf, NULL, 0);
    }

    return err;
}

/*
 * TS-2000 rig capabilities
 */
const struct rig_caps ts2000_caps =
{
    RIG_MODEL(RIG_MODEL_TS2000),
    .model_name = "TS-2000",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".2",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  0, /* ms */
    .timeout =  200,
    .retry =  3,

    .has_get_func =  TS2000_FUNC_ALL,
    .has_set_func =  TS2000_FUNC_ALL,
    .has_get_level =  TS2000_LEVEL_GET,
    .has_set_level =  TS2000_LEVEL_SET,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_kenwood.h"
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 30 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = {.min = {.i = 10}, .max = {.i = 60}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 400}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_BKIN_DLYMS] = {.min = {.i = 0}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_SLOPE_LOW] = {.min = {.i = 0}, .max = {.i = 1000}, .step = {.i = 10}},
        [LVL_SLOPE_HIGH] = {.min = {.i = 0}, .max = {.i = 5000}, .step = {.i = 10}},
    },
    .parm_gran =  {},
    .vfo_ops =  TS2000_VFO_OPS,
    .scan_ops =  TS2000_SCAN_OP,
    .ctcss_list =  ts2000_ctcss_list,
    .dcs_list =  ts2000_dcs_list,
    .preamp =   { 12, RIG_DBLST_END, },
    .attenuator =   { 12, RIG_DBLST_END, },
    .max_rit =  kHz(20),
    .max_xit =  kHz(20),
    .max_ifshift =  kHz(1),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .agc_level_count = 5,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST, RIG_AGC_SUPERFAST },
    .bank_qty =   0,
    .chan_desc_sz =  7,

    .chan_list =  {
        { 0, 299, RIG_MTYPE_MEM, TS2000_MEM_CAP  },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(300), MHz(60), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO, TS2000_ANTS},
        {MHz(144), MHz(146), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO},
        {MHz(430), MHz(440), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO},
        {MHz(144), MHz(146), TS2000_ALL_MODES, -1, -1, TS2000_SUBVFO},
        {MHz(430), MHz(440), TS2000_ALL_MODES, -1, -1, TS2000_SUBVFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {kHz(1830), kHz(1850), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(1830), kHz(1850), TS2000_AM_TX_MODES, 2000, 25000, TS2000_MAINVFO, TS2000_ANTS},
        {kHz(3500), kHz(3800), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(3500), kHz(3800), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(7), kHz(7100), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(7), kHz(7100), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(10.1), MHz(10.15), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(10.1), MHz(10.15), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(14), kHz(14350), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(14), kHz(14350), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(18068), kHz(18168), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(18068), kHz(18168), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(21), kHz(21450), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(21), kHz(21450), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(24890), kHz(24990), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(24890), kHz(24990), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(28), kHz(29700), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(28), kHz(29700), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(50), MHz(50.2), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(50), MHz(50.2), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(144), MHz(146), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO},
        {MHz(144), MHz(146), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO},
        {MHz(430), MHz(440), TS2000_OTHER_TX_MODES, W(5), W(50), TS2000_MAINVFO},
        {MHz(430), MHz(440), TS2000_AM_TX_MODES, W(5), W(12.5), TS2000_MAINVFO},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(300), MHz(60), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO, TS2000_ANTS},
        {MHz(142), MHz(152), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO},
        {MHz(420), MHz(450), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO},
        {MHz(118), MHz(174), TS2000_ALL_MODES, -1, -1, TS2000_SUBVFO},
        {MHz(220), MHz(512), TS2000_ALL_MODES, -1, -1, TS2000_SUBVFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(1800), MHz(2), TS2000_AM_TX_MODES, 2000, 25000, TS2000_MAINVFO, TS2000_ANTS},
        {kHz(3500), MHz(4), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(3500), MHz(4), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(7), kHz(7300), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(7), kHz(7300), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(10.1), MHz(10.15), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(10.1), MHz(10.15), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(14), kHz(14350), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(14), kHz(14350), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(18068), kHz(18168), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(18068), kHz(18168), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(21), kHz(21450), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(21), kHz(21450), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(24890), kHz(24990), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(24890), kHz(24990), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(28), kHz(29700), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(28), kHz(29700), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(50), MHz(54), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(50), MHz(54), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(144), MHz(148), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO},
        {MHz(144), MHz(148), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO},
        {MHz(430), MHz(450), TS2000_OTHER_TX_MODES, W(5), W(50), TS2000_MAINVFO},
        {MHz(430), MHz(450), TS2000_AM_TX_MODES, W(5), W(12.5), TS2000_MAINVFO},
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, 1},
        {TS2000_ALL_MODES, 10},
        {TS2000_ALL_MODES, 100},
        {TS2000_ALL_MODES, kHz(1)},
        {TS2000_ALL_MODES, kHz(2.5)},
        {TS2000_ALL_MODES, kHz(5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(6.25)},
        {TS2000_ALL_MODES, kHz(10)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(15)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(20)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(25)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(30)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(50)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(100)},
        {TS2000_ALL_MODES, MHz(1)},
        {TS2000_ALL_MODES, 0}, /* any tuning step */
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(200)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(50)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(1000)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(80)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(100)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(150)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(400)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(600)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(2000)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(250)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(1000)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(1500)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_FM, kHz(6)},
        RIG_FLT_END,
    },

    .str_cal = TS2000_STR_CAL,
    .swr_cal = TS2000_SWR_CAL,

    .ext_tokens = ts2000_ext_tokens,
    .extfuncs = ts2000_ext_funcs,
    .extlevels = ts2000_ext_levels,

    .priv = (void *)& ts2000_priv_caps,

    .rig_init = ts2000_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  ts2000_set_rit,
    .get_rit =  ts2000_get_rit,
    .set_xit =  ts2000_set_rit,
    .get_xit =  ts2000_get_rit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .set_ctcss_tone =  kenwood_set_ctcss_tone_tn,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .set_ctcss_sql =  kenwood_set_ctcss_sql,
    .get_ctcss_sql =  kenwood_get_ctcss_sql,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  ts2000_set_func,
    .get_func =  ts2000_get_func,
    .set_level =  ts2000_set_level,
    .get_level =  ts2000_get_level,
    .set_ext_func =  ts2000_set_ext_func,
    .get_ext_func =  ts2000_get_ext_func,
    .set_ext_level =  ts2000_set_ext_level,
    .get_ext_level =  ts2000_get_ext_level,
    .set_ant =  kenwood_set_ant,
    .get_ant =  kenwood_get_ant,
    .send_morse =  kenwood_send_morse,
    .wait_morse = rig_wait_morse,
    .send_voice_mem = kenwood_send_voice_mem,
    .stop_voice_mem = kenwood_stop_voice_mem,
    .vfo_op =  kenwood_vfo_op,
    .scan =  kenwood_scan,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .get_channel = ts2000_get_channel,
    .set_channel = ts2000_set_channel,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .set_powerstat =  kenwood_set_powerstat,
    .get_powerstat =  kenwood_get_powerstat,
    .get_info =  kenwood_get_info,
    .reset =  kenwood_reset,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
