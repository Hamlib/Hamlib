/*
 *  Hamlib Kenwood backend - TS-590(S/SG) description
 *  Copyright (c) 2010 by Stephane Fillod
 *  Copyright (c) 2023 by Mikael Nousiainen OH3BHX
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
#include <string.h>
#include <stdlib.h>

#include "hamlib/rig.h"
#include "kenwood.h"
#include "misc.h"
#include "cal.h"
#include "iofunc.h"

#define TS590_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTFM|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_PKTAM)
#define TS590_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS590_AM_TX_MODES (RIG_MODE_AM|RIG_MODE_PKTAM)
#define TS590_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define TS590_LEVEL_GET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_MICGAIN|RIG_LEVEL_STRENGTH|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH| \
    RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_NR|RIG_LEVEL_PREAMP|RIG_LEVEL_COMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_VOXGAIN|RIG_LEVEL_BKIN_DLYMS| \
    RIG_LEVEL_SWR|RIG_LEVEL_COMP_METER|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW)

#define TS590_LEVEL_SET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH| \
    RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_NR|RIG_LEVEL_PREAMP|RIG_LEVEL_COMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_VOXGAIN|RIG_LEVEL_BKIN_DLYMS| \
    RIG_LEVEL_METER|RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW)

#define TS590_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_BC2|RIG_FUNC_RIT|RIG_FUNC_XIT| \
    RIG_FUNC_TUNER|RIG_FUNC_MON|RIG_FUNC_FBKIN|RIG_FUNC_LOCK)

#define TS590_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_CPY|RIG_OP_TUNE)

#define TS590_SCAN_OPS (RIG_SCAN_VFO)

#define TS590_ANTS (RIG_ANT_1|RIG_ANT_2)

#define TS590_CHANNEL_CAPS { \
        .freq=1,\
        .mode=1,\
        .tx_freq=1,\
        .tx_mode=1,\
        .split=1,\
        .funcs=RIG_FUNC_TONE, \
        .flags=RIG_CHFLAG_SKIP \
        }

#define TS590_STR_CAL {9, {\
                       { 0, -60},\
                       { 3, -48},\
                       { 6, -36},\
                       { 9, -24},\
                       {12, -12},\
                       {15,   0},\
                       {20,  20},\
                       {25,  40},\
                       {30,  60}}\
                       }

#define TS590_SWR_CAL { 5, \
    { \
        {   0, 1.0f }, \
        {   6, 1.5f }, \
        {   12, 2.0f }, \
        {   18, 3.0f }, \
        {   30, 10.0f } \
    } }

#define TOK_FUNC_NOISE_REDUCTION_2 TOKEN_BACKEND(102)
#define TOK_LEVEL_DSP_RX_EQUALIZER TOKEN_BACKEND(104)
#define TOK_LEVEL_DSP_TX_EQUALIZER TOKEN_BACKEND(105)
#define TOK_LEVEL_BEEP_VOLUME TOKEN_BACKEND(107)
#define TOK_LEVEL_TX_SIDETONE_VOLUME TOKEN_BACKEND(108)
#define TOK_LEVEL_ACC2_AUDIO_INPUT_LEVEL TOKEN_BACKEND(109)
#define TOK_LEVEL_ACC2_AUDIO_OUTPUT_LEVEL TOKEN_BACKEND(110)
#define TOK_LEVEL_USB_AUDIO_INPUT_LEVEL TOKEN_BACKEND(113)
#define TOK_LEVEL_USB_AUDIO_OUTPUT_LEVEL TOKEN_BACKEND(114)
#define TOK_LEVEL_DSP_TX_SSB_AM_LOW_CUT_FILTER TOKEN_BACKEND(115)
#define TOK_LEVEL_DSP_TX_SSB_AM_HIGH_CUT_FILTER TOKEN_BACKEND(116)
#define TOK_LEVEL_DSP_TX_SSB_DATA_LOW_CUT_FILTER TOKEN_BACKEND(117)
#define TOK_LEVEL_DSP_TX_SSB_DATA_HIGH_CUT_FILTER TOKEN_BACKEND(118)

int ts590_ext_tokens[] =
{
    TOK_FUNC_NOISE_REDUCTION_2,
    TOK_LEVEL_DSP_RX_EQUALIZER, TOK_LEVEL_DSP_TX_EQUALIZER,
    TOK_LEVEL_BEEP_VOLUME, TOK_LEVEL_TX_SIDETONE_VOLUME,
    TOK_LEVEL_ACC2_AUDIO_INPUT_LEVEL, TOK_LEVEL_ACC2_AUDIO_OUTPUT_LEVEL,
    TOK_LEVEL_USB_AUDIO_INPUT_LEVEL, TOK_LEVEL_USB_AUDIO_OUTPUT_LEVEL,
    TOK_LEVEL_DSP_TX_SSB_AM_LOW_CUT_FILTER, TOK_LEVEL_DSP_TX_SSB_AM_HIGH_CUT_FILTER,
    TOK_LEVEL_DSP_TX_SSB_DATA_LOW_CUT_FILTER, TOK_LEVEL_DSP_TX_SSB_DATA_HIGH_CUT_FILTER,

    TOK_BACKEND_NONE,
};

const struct confparams ts590_ext_funcs[] =
{
    {
        TOK_FUNC_NOISE_REDUCTION_2, "NR2", "Noise reduction 2", "Noise reduction 2",
        NULL, RIG_CONF_CHECKBUTTON,
    },
    { RIG_CONF_END, NULL, }
};

const struct confparams ts590_ext_levels[] =
{
    {
        TOK_LEVEL_DSP_RX_EQUALIZER, "DSP_RX_EQUALIZER", "DSP RX equalizer", "DSP RX equalizer type",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "OFF", "Hb1", "Hb2", "FP", "bb1", "bb2", "c", "U", NULL } } }
    },
    {
        TOK_LEVEL_DSP_TX_EQUALIZER, "DSP_TX_EQUALIZER", "DSP TX equalizer", "DSP TX equalizer type",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "OFF", "Hb1", "Hb2", "FP", "bb1", "bb2", "flat", "U", NULL } } }
    },
    {
        TOK_LEVEL_BEEP_VOLUME, "BEEP_VOLUME", "Beep volume", "Beep volume",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 20, .step = 1 } }
    },
    {
        TOK_LEVEL_TX_SIDETONE_VOLUME, "TX_SIDETONE_VOLUME", "TX sidetone volume", "TX sidetone volume",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 20, .step = 1 } }
    },
    {
        TOK_LEVEL_ACC2_AUDIO_INPUT_LEVEL, "ACC2_AUDIO_INPUT_LEVEL", "ACC2 audio input level", "ACC2 audio input level",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    {
        TOK_LEVEL_ACC2_AUDIO_OUTPUT_LEVEL, "ACC2_AUDIO_OUTPUT_LEVEL", "ACC2 audio output level", "ACC2 audio output level",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    {
        TOK_LEVEL_USB_AUDIO_INPUT_LEVEL, "USB_AUDIO_INPUT_LEVEL", "USB audio input level", "USB audio input level",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    {
        TOK_LEVEL_USB_AUDIO_OUTPUT_LEVEL, "USB_AUDIO_OUTPUT_LEVEL", "USB audio output level", "USB audio output level",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    {
        TOK_LEVEL_DSP_TX_SSB_AM_LOW_CUT_FILTER, "DSP_TX_SSB_AM_LOW_CUT_FILTER", "DSP TX SSB/AM low-cut", "DSP TX low-cut filter for SSB and AM",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "10 Hz", "100 Hz", "200 Hz", "300 Hz", "400 Hz", "500 Hz", NULL } } }
    },
    {
        TOK_LEVEL_DSP_TX_SSB_AM_HIGH_CUT_FILTER, "DSP_TX_SSB_AM_HIGH_CUT_FILTER", "DSP TX SSB/AM high-cut", "DSP TX high-cut filter for SSB and AM",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "2500 Hz", "2600 Hz", "2700 Hz", "2800 Hz", "2900 Hz", "3000 Hz", NULL } } }
    },
    {
        TOK_LEVEL_DSP_TX_SSB_DATA_LOW_CUT_FILTER, "DSP_TX_SSB_DATA_LOW_CUT_FILTER", "DSP TX SSB data low-cut", "DSP TX low-cut filter for SSB data",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "10 Hz", "100 Hz", "200 Hz", "300 Hz", "400 Hz", "500 Hz", NULL } } }
    },
    {
        TOK_LEVEL_DSP_TX_SSB_DATA_HIGH_CUT_FILTER, "DSP_TX_SSB_DATA_HIGH_CUT_FILTER", "DSP TX SSB data high-cut", "DSP TX high-cut filter for SSB data",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "2500 Hz", "2600 Hz", "2700 Hz", "2800 Hz", "2900 Hz", "3000 Hz", NULL } } }
    },
    { RIG_CONF_END, NULL, }
};


/*
 * ts590_get_info
 * This is not documented in the manual as of 3/11/15 but confirmed from Kenwood
 * "TY" produces "TYK 00" for example
 */
const char *ts590_get_info(RIG *rig)
{
    char firmbuf[10];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = kenwood_safe_transaction(rig, "TY", firmbuf, 10, 6);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    switch (firmbuf[2])
    {
    case 'K': return "Firmware: USA version";

    case 'E': return "Firmware: European version";

    default: return "Firmware: unknown";
    }
}

// keep track of SF command ability
static int sf_fails;

static int ts590_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char kmode = rmode2kenwood(mode, caps->mode_table);
    char cmd[32], c;
    int retval = -RIG_EINTERNAL;

    if (kmode < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(mode));
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (kmode <= 9)
    {
        c = '0' + kmode;
    }
    else
    {
        c = 'A' + kmode - 10;
    }

    if (!sf_fails)
    {
        SNPRINTF(cmd, sizeof(cmd), "SF%d%011.0f%c", vfo == RIG_VFO_A ? 0 : 1,
                 vfo == RIG_VFO_A ? rig->state.cache.freqMainA : rig->state.cache.freqMainB,
                 c);
        retval = kenwood_transaction(rig, cmd, NULL, 0);
    }

    if (retval != RIG_OK || sf_fails)
    {
        return kenwood_set_mode(rig, vfo, mode, width);
    }

    return retval;
}

static int ts590_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char cmd[32], ackbuf[32];
    int retval;

    if (!sf_fails)
    {
        SNPRINTF(cmd, sizeof(cmd), "SF%d", vfo == RIG_VFO_A ? 0 : 1);
        retval = kenwood_safe_transaction(rig, cmd, ackbuf, sizeof(ackbuf), 15);
    }

    // if this fails fall back to old method
    if (retval != RIG_OK || sf_fails)
    {
        sf_fails = 1;
        return kenwood_get_mode(rig, vfo, mode, width);
    }

    *mode = ackbuf[14];

    if (*mode >= 'A') { *mode = *mode - 'A' + 10; }
    else { *mode -= '0'; }

    *mode = kenwood2rmode(*mode, caps->mode_table);

    // now let's get our widths
    SNPRINTF(cmd, sizeof(cmd), "SH");
    retval = kenwood_safe_transaction(rig, cmd, ackbuf, sizeof(ackbuf), 15);
    int hwidth;
    sscanf(cmd, "SH%d", &hwidth);
    int lwidth;
    int shift = 0;
    SNPRINTF(cmd, sizeof(cmd), "SL");
    sscanf(cmd, "SH%d", &lwidth);
    retval = kenwood_safe_transaction(rig, cmd, ackbuf, sizeof(ackbuf), 15);

    if (*mode == RIG_MODE_PKTUSB || *mode == RIG_MODE_PKTLSB
            || *mode == RIG_MODE_FM || *mode == RIG_MODE_PKTFM || *mode == RIG_MODE_USB
            || *mode == RIG_MODE_LSB)
    {
        const int ssb_htable[] = { 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400, 2600, 2800, 3000, 3400, 4000, 5000 };
        const int ssb_ltable[] = { 0, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 };
        *width = ssb_htable[hwidth];
        // we dont' do anything with shift yet which will be just the hwidth value
        shift = ssb_ltable[lwidth];
    }
    else if (*mode == RIG_MODE_AM || *mode == RIG_MODE_PKTAM)
    {
        const int am_htable[] = { 2500, 3000, 4000, 5000 };
        const int am_ltable[] = { 0, 100, 200, 300 };
        *width = am_htable[hwidth] - am_ltable[lwidth];
    }

#if 0 // is this different?  Manual is confusing
    else if (*mode == RIG_MODE_SSB || *mode == RIG_MODE_LSB)
    {
        const int ssb_htable[] = { 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400, 2600, 2800, 3000, 3400, 4000, 5000 };
        //const int ssb_ltable[] = { 0, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 };
    }

#endif
    rig_debug(RIG_DEBUG_VERBOSE, "%s: width=%ld, shift=%d, lwidth=%d, hwidth=%d\n",
              __func__, *width, shift, lwidth, hwidth);

    return RIG_OK;
}


static int ts590_set_ex_menu(RIG *rig, int number, int value_len, int value)
{
    char buf[20];

    ENTERFUNC;

    SNPRINTF(buf, sizeof(buf), "EX%03d0000%0*d", number, value_len, value);

    RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));
}

static int ts590_get_ex_menu(RIG *rig, int number, int value_len, int *value)
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

static int ts590_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[20];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_MON:
        SNPRINTF(buf, sizeof(buf), "ML00%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_LOCK:
        SNPRINTF(buf, sizeof(buf), "LK%c0", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_TUNER:
        SNPRINTF(buf, sizeof(buf), "AC%c%c0", (status == 0) ? '0' : '1',
                 (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    default:
        return kenwood_set_func(rig, vfo, func, status);
    }
}

static int ts590_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
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

        *status = buf[2] != '0';
        break;

    case RIG_FUNC_TUNER:
        retval = kenwood_safe_transaction(rig, "AC", buf, sizeof(buf), 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        *status = buf[3] != '0' ? 1 : 0;
        RETURNFUNC(RIG_OK);

    default:
        return kenwood_get_func(rig, vfo, func, status);
    }

    RETURNFUNC(RIG_OK);
}

static int ts590_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    char levelbuf[16];
    int kenwood_val;
    int result;

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
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ0%03d", kenwood_val);
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

        SNPRINTF(levelbuf, sizeof(levelbuf), "GT%02d", kenwood_val);
        break;

    case RIG_LEVEL_MONITOR_GAIN:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            kenwood_val = val.f * 9.0f;
        }
        else
        {
            kenwood_val = val.f * 20.0f;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "ML%03d", kenwood_val);
        break;

    case RIG_LEVEL_NB:
        priv->question_mark_response_means_rejected = 1;
        kenwood_val = val.f * 10.0;
        SNPRINTF(levelbuf, sizeof(levelbuf), "NL%03d", kenwood_val);
        break;

    case RIG_LEVEL_NR:
        priv->question_mark_response_means_rejected = 1;
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
        if (val.i > 1000 || val.i < 300)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        RETURNFUNC(ts590_set_ex_menu(rig, 40, 2, (val.i - 300) / 50));

    default:
        RETURNFUNC(kenwood_set_level(rig, vfo, level, val));
    }

    result = kenwood_transaction(rig, levelbuf, NULL, 0);
    priv->question_mark_response_means_rejected = 0;

    RETURNFUNC(result);
}

static int ts590_read_meters(RIG *rig, int *swr, int *comp, int *alc)
{
    int retval;
    char *cmd = "RM;";
    struct rig_state *rs = &rig->state;
    char ackbuf[32];
    int expected_len = 24;

    ENTERFUNC;

    retval = write_block(&rs->rigport, (unsigned char *) cmd, strlen(cmd));

    rig_debug(RIG_DEBUG_TRACE, "%s: write_block retval=%d\n", __func__, retval);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    // TS-590 returns values for all meters at the same time, for example: RM10000;RM20000;RM30000;

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

    retval = sscanf(ackbuf, "RM1%d;RM2%d;RM3%d;", swr, comp, alc);

    if (retval != 3)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: expected 3 meter values to parse, got %d in '%s'\n", __func__, retval,
                  ackbuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    RETURNFUNC(RIG_OK);
}

static int ts590_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    char ackbuf[50];
    size_t ack_len, ack_len_expected;
    int levelint;
    int retval;

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
        retval = kenwood_transaction(rig, "SQ0", ackbuf, sizeof(ackbuf));
        ack_len_expected = 6;

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

        val->f = (float) levelint / 255.;
        return RIG_OK;

    case RIG_LEVEL_AGC:
        priv->question_mark_response_means_rejected = 1;
        retval = kenwood_transaction(rig, "GT", ackbuf, sizeof(ackbuf));
        priv->question_mark_response_means_rejected = 0;
        ack_len_expected = 4;

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (ack_len != ack_len_expected)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[ack_len_expected - 2], "%d", &levelint))
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

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            val->f = (float) raw_value / 9.0f;
        }
        else
        {
            val->f = (float) raw_value / 20.0f;
        }

        break;
    }

    case RIG_LEVEL_NB:
    {
        int raw_value;
        priv->question_mark_response_means_rejected = 1;
        retval = kenwood_safe_transaction(rig, "NL", ackbuf, sizeof(ackbuf), 5);
        priv->question_mark_response_means_rejected = 0;

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
        priv->question_mark_response_means_rejected = 1;
        retval = kenwood_safe_transaction(rig, "RL", ackbuf, sizeof(ackbuf), 4);
        priv->question_mark_response_means_rejected = 0;

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

    case RIG_LEVEL_SWR:
    case RIG_LEVEL_COMP_METER:
    case RIG_LEVEL_ALC:
    {
        int swr;
        int comp;
        int alc;

        retval = ts590_read_meters(rig, &swr, &comp, &alc);

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
            val->f = (float) comp; // Maximum value is 30
            break;

        case RIG_LEVEL_ALC:
            // Maximum value is 30, so have the max at 5 just to be on the range where other rigs report ALC
            val->f = (float) alc / 6.0f;
            break;

        default:
            return -RIG_ENAVAIL;
        }

        break;
    }

    case RIG_LEVEL_RFPOWER_METER:
    {
        int raw_value;

        if (rig->state.cache.ptt == RIG_PTT_OFF)
        {
            val->f = 0;
            break;
        }

        retval = kenwood_safe_transaction(rig, "SM0", ackbuf, sizeof(ackbuf), 7);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(ackbuf, "SM0%d", &raw_value);

        val->f = (float) raw_value / 30.0f;
        break;
    }

    case RIG_LEVEL_CWPITCH:
    {
        int raw_value;
        retval = ts590_get_ex_menu(rig, 40, 2, &raw_value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->i = 300 + raw_value * 50;
        break;
    }

    default:
        return kenwood_get_level(rig, vfo, level, val);
    }

    RETURNFUNC(RIG_OK);
}

static int ts590_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    char buf[20];
    int retval;
    int rit_enabled;
    int xit_enabled;

    ENTERFUNC;

    if (rit < -9999 || rit > 9999)
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

static int ts590_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
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

static int ts590_set_ext_func(RIG *rig, vfo_t vfo, token_t token, int status)
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

static int ts590_get_ext_func(RIG *rig, vfo_t vfo, token_t token, int *status)
{
    char ackbuf[20];
    int retval;

    ENTERFUNC;

    switch (token)
    {
    case TOK_FUNC_NOISE_REDUCTION_2:
    {
        int value;

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

static int ts590_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    int retval;

    ENTERFUNC;

    switch (token)
    {
    case TOK_LEVEL_DSP_RX_EQUALIZER:
        if (val.i < 0 || val.i > 7)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 31, 1, val.i);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 37, 1, val.i);
        }

        break;

    case TOK_LEVEL_DSP_TX_EQUALIZER:
        if (val.i < 0 || val.i > 7)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 30, 1, val.i);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 36, 1, val.i);
        }

        break;

    case TOK_LEVEL_DSP_TX_SSB_AM_LOW_CUT_FILTER:
        if (val.i < 0 || val.i > 5)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 25, 1, val.i);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 31, 1, val.i);
        }

        break;

    case TOK_LEVEL_DSP_TX_SSB_AM_HIGH_CUT_FILTER:
        if (val.i < 0 || val.i > 5)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 26, 1, val.i);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 32, 1, val.i);
        }

        break;

    case TOK_LEVEL_DSP_TX_SSB_DATA_LOW_CUT_FILTER:
        if (val.i < 0 || val.i > 5)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 27, 1, val.i);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 33, 1, val.i);
        }

        break;

    case TOK_LEVEL_DSP_TX_SSB_DATA_HIGH_CUT_FILTER:
        if (val.i < 0 || val.i > 5)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 28, 1, val.i);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 34, 1, val.i);
        }

        break;

    case TOK_LEVEL_BEEP_VOLUME:
        if (val.f < 0 || val.f > 20)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 3, 2, (int) val.f);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 5, 2, (int) val.f);
        }

        break;

    case TOK_LEVEL_TX_SIDETONE_VOLUME:
        if (val.f < 0 || val.f > 20)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 4, 2, (int) val.f);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 6, 2, (int) val.f);
        }

        break;

    case TOK_LEVEL_ACC2_AUDIO_INPUT_LEVEL:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 66, 1, (int) val.f);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 73, 1, (int) val.f);
        }

        break;

    case TOK_LEVEL_ACC2_AUDIO_OUTPUT_LEVEL:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 67, 1, (int) val.f);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 74, 1, (int) val.f);
        }

        break;

    case TOK_LEVEL_USB_AUDIO_INPUT_LEVEL:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 64, 1, (int) val.f);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 71, 1, (int) val.f);
        }

        break;

    case TOK_LEVEL_USB_AUDIO_OUTPUT_LEVEL:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_set_ex_menu(rig, 65, 1, (int) val.f);
        }
        else
        {
            retval = ts590_set_ex_menu(rig, 72, 1, (int) val.f);
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static int ts590_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    int retval;
    int value;

    ENTERFUNC;

    switch (token)
    {
    case TOK_LEVEL_DSP_RX_EQUALIZER:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 31, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 37, 1, &value);
        }

        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_EQUALIZER:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 30, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 36, 1, &value);
        }

        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_SSB_AM_LOW_CUT_FILTER:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 25, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 31, 1, &value);
        }

        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_SSB_AM_HIGH_CUT_FILTER:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 26, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 32, 1, &value);
        }

        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_SSB_DATA_LOW_CUT_FILTER:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 27, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 33, 1, &value);
        }

        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_SSB_DATA_HIGH_CUT_FILTER:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 28, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 34, 1, &value);
        }

        val->i = value;
        break;

    case TOK_LEVEL_BEEP_VOLUME:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 3, 2, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 5, 2, &value);
        }

        val->f = value;
        break;

    case TOK_LEVEL_TX_SIDETONE_VOLUME:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 4, 2, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 6, 2, &value);
        }

        val->f = value;
        break;

    case TOK_LEVEL_ACC2_AUDIO_INPUT_LEVEL:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 66, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 73, 1, &value);
        }

        val->f = value;
        break;

    case TOK_LEVEL_ACC2_AUDIO_OUTPUT_LEVEL:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 67, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 74, 1, &value);
        }

        val->f = value;
        break;

    case TOK_LEVEL_USB_AUDIO_INPUT_LEVEL:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 64, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 71, 1, &value);
        }

        val->f = value;
        break;

    case TOK_LEVEL_USB_AUDIO_OUTPUT_LEVEL:
        if (rig->caps->rig_model == RIG_MODEL_TS590S)
        {
            retval = ts590_get_ex_menu(rig, 65, 1, &value);
        }
        else
        {
            retval = ts590_get_ex_menu(rig, 72, 1, &value);
        }

        val->f = value;
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static struct kenwood_filter_width ts590_filter_width[] =
{
    { RIG_MODE_CW | RIG_MODE_CWR, 50, 50 },
    { RIG_MODE_CW | RIG_MODE_CWR, 80, 80 },
    { RIG_MODE_CW | RIG_MODE_CWR, 100, 100 },
    { RIG_MODE_CW | RIG_MODE_CWR, 150, 150 },
    { RIG_MODE_CW | RIG_MODE_CWR, 200, 200 },
    { RIG_MODE_CW | RIG_MODE_CWR, 250, 250 },
    { RIG_MODE_CW | RIG_MODE_CWR, 300, 300 },
    { RIG_MODE_CW | RIG_MODE_CWR, 400, 400 },
    { RIG_MODE_CW | RIG_MODE_CWR, 500, 500 },
    { RIG_MODE_CW | RIG_MODE_CWR, 600, 600 },
    { RIG_MODE_CW | RIG_MODE_CWR, 1000, 1000 },
    { RIG_MODE_CW | RIG_MODE_CWR, 1500, 1500 },
    { RIG_MODE_CW | RIG_MODE_CWR, 2000, 2000 },
    { RIG_MODE_CW | RIG_MODE_CWR, 2500, 2500 },
    { RIG_MODE_RTTY | RIG_MODE_RTTYR, 250, 250 },
    { RIG_MODE_RTTY | RIG_MODE_RTTYR, 500, 500 },
    { RIG_MODE_RTTY | RIG_MODE_RTTYR, 1000, 1000 },
    { RIG_MODE_RTTY | RIG_MODE_RTTYR, 1500, 1500 },
    { RIG_MODE_SSB, 0, 2400 },
    { RIG_MODE_FM, 0, 12000 },
    { RIG_MODE_AM, 0, 6000 },
    { RIG_MODE_NONE, -1, -1 },
};

static struct kenwood_slope_filter ts590_slope_filter_high[] =
{
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 0, 1000 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 1, 1200 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 2, 1400 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 3, 1600 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 4, 1800 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 5, 2000 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 6, 2200 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 7, 2400 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 8, 2600 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 9, 2800 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 10, 3000 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 11, 3400 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 12, 4000 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 13, 5000 },
    { RIG_MODE_AM | RIG_MODE_PKTAM, 0, 0, 2500 },
    { RIG_MODE_AM | RIG_MODE_PKTAM, 0, 1, 3000 },
    { RIG_MODE_AM | RIG_MODE_PKTAM, 0, 2, 4000 },
    { RIG_MODE_AM | RIG_MODE_PKTAM, 0, 3, 5000 },
    { RIG_MODE_NONE, 0, -1, -1 },
};

static struct kenwood_slope_filter ts590_slope_filter_low[] =
{
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 0, 0 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 1, 50 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 2, 100 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 3, 200 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 4, 300 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 5, 400 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 6, 500 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 7, 600 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 8, 700 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 9, 800 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 10, 900 },
    { RIG_MODE_SSB | RIG_MODE_PKTSSB | RIG_MODE_FM | RIG_MODE_PKTFM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 11, 1000 },
    { RIG_MODE_AM | RIG_MODE_PKTAM, 0, 0, 0 },
    { RIG_MODE_AM | RIG_MODE_PKTAM, 0, 1, 100 },
    { RIG_MODE_AM | RIG_MODE_PKTAM, 0, 2, 200 },
    { RIG_MODE_AM | RIG_MODE_PKTAM, 0, 3, 300 },
    { RIG_MODE_NONE, 0, -1, -1 },
};

static struct kenwood_priv_caps ts590_priv_caps =
{
    .cmdtrm = EOM_KEN,
    .filter_width = ts590_filter_width,
    .slope_filter_high = ts590_slope_filter_high,
    .slope_filter_low = ts590_slope_filter_low,
};

/**
 * TS-590 rig capabilities
 */
const struct rig_caps ts590_caps =
{
    RIG_MODEL(RIG_MODEL_TS590S),
    .model_name = "TS-590S",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".5",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_HARDWARE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 3,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive = RIG_TRN_RIG,
    .agc_level_count = 6,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST, RIG_AGC_SUPERFAST, RIG_AGC_ON },

    .chan_list =  { /* TBC */
        {  0, 89, RIG_MTYPE_MEM,  TS590_CHANNEL_CAPS },
        { 90, 99, RIG_MTYPE_EDGE, TS590_CHANNEL_CAPS },
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        {kHz(30),   Hz(59999999), TS590_ALL_MODES, -1, -1, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(3500),  kHz(3800),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7200),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7200),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(30),   Hz(59999999), TS590_ALL_MODES, -1, -1, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(3500),  MHz(4) - 1, TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(5250),  kHz(5450),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(5250),  kHz(5450),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7300),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7300),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS590_ALL_MODES, kHz(1)},
        {TS590_ALL_MODES, Hz(2500)},
        {TS590_ALL_MODES, kHz(5)},
        {TS590_ALL_MODES, Hz(6250)},
        {TS590_ALL_MODES, kHz(10)},
        {TS590_ALL_MODES, Hz(12500)},
        {TS590_ALL_MODES, kHz(15)},
        {TS590_ALL_MODES, kHz(20)},
        {TS590_ALL_MODES, kHz(25)},
        {TS590_ALL_MODES, kHz(30)},
        {TS590_ALL_MODES, kHz(100)},
        {TS590_ALL_MODES, kHz(500)},
        {TS590_ALL_MODES, MHz(1)},
        {TS590_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .level_gran =
    {
#include "level_gran_kenwood.h"
        [LVL_RF] = { .min = { .f = 0 }, .max = { .f = 1.0 },  .step = { .f = 1.0f / 100.0f } },
        [LVL_AF] = { .min = { .f = 0 }, .max = { .f = 1.0 },  .step = { .f = 1.0f / 100.0f } },
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 30 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = {.min = {.i = 4}, .max = {.i = 60}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 400}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_BKIN_DLYMS] = {.min = {.i = 0}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_SLOPE_LOW] = {.min = {.i = 0}, .max = {.i = 2400}, .step = {.i = 10}},
        [LVL_SLOPE_HIGH] = {.min = {.i = 0}, .max = {.i = 5000}, .step = {.i = 10}},
    },

    .str_cal = TS590_STR_CAL,
    .swr_cal = TS590_SWR_CAL,

    .ext_tokens = ts590_ext_tokens,
    .extfuncs = ts590_ext_funcs,
    .extlevels = ts590_ext_levels,

    .priv = (void *)& ts590_priv_caps,
    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = ts590_set_rit,
    .get_rit = ts590_get_rit,
    .set_xit = ts590_set_rit,
    .get_xit = ts590_get_rit,
    .set_mode = ts590_set_mode,
    .get_mode = ts590_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .get_info = ts590_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan_ops =  TS590_SCAN_OPS,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS590_LEVEL_SET,
    .has_get_level = TS590_LEVEL_GET,
    .set_level = ts590_set_level,
    .get_level = ts590_get_level,
    .set_ext_level = ts590_set_ext_level,
    .get_ext_level = ts590_get_ext_level,
    .has_get_func = TS590_FUNC_ALL,
    .has_set_func = TS590_FUNC_ALL,
    .set_func = ts590_set_func,
    .get_func = ts590_get_func,
    .set_ext_func = ts590_set_ext_func,
    .get_ext_func = ts590_get_ext_func,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .ctcss_list =  kenwood38_ctcss_list,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .send_morse =  kenwood_send_morse,
    .stop_morse =  kenwood_stop_morse,
    .wait_morse =  rig_wait_morse,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .vfo_ops = TS590_VFO_OPS,
    .vfo_op =  kenwood_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/**
 * TS-590SG rig capabilities
 */
const struct rig_caps ts590sg_caps =
{
    RIG_MODEL(RIG_MODEL_TS590SG),
    .model_name = "TS-590SG",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".3",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_HARDWARE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 3,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive = RIG_TRN_RIG,
    .agc_level_count = 6,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST, RIG_AGC_SUPERFAST, RIG_AGC_ON },

    .chan_list =  { /* TBC */
        {  0, 89, RIG_MTYPE_MEM,  TS590_CHANNEL_CAPS },
        { 90, 99, RIG_MTYPE_EDGE, TS590_CHANNEL_CAPS },
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        {kHz(30),   Hz(59999999), TS590_ALL_MODES, -1, -1, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(3500),  kHz(3800),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7200),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7200),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(30),   Hz(59999999), TS590_ALL_MODES, -1, -1, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(3500),  MHz(4) - 1, TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(5250),  kHz(5450),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(5250),  kHz(5450),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7300),  TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(7),     kHz(7300),  TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(10100), kHz(10150), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(14),    kHz(14350), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(18068), kHz(18168), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(21),    kHz(21450), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {kHz(24890), kHz(24990), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(28),    kHz(29700), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_OTHER_TX_MODES, 5000, 100000, TS590_VFO, TS590_ANTS},
        {MHz(50),    kHz(52000), TS590_AM_TX_MODES, 5000, 25000, TS590_VFO, TS590_ANTS},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS590_ALL_MODES, kHz(1)},
        {TS590_ALL_MODES, Hz(2500)},
        {TS590_ALL_MODES, kHz(5)},
        {TS590_ALL_MODES, Hz(6250)},
        {TS590_ALL_MODES, kHz(10)},
        {TS590_ALL_MODES, Hz(12500)},
        {TS590_ALL_MODES, kHz(15)},
        {TS590_ALL_MODES, kHz(20)},
        {TS590_ALL_MODES, kHz(25)},
        {TS590_ALL_MODES, kHz(30)},
        {TS590_ALL_MODES, kHz(100)},
        {TS590_ALL_MODES, kHz(500)},
        {TS590_ALL_MODES, MHz(1)},
        {TS590_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(12)},
        RIG_FLT_END,
    },
    .level_gran = {
#include "level_gran_kenwood.h"
        [LVL_RF] = { .min = { .f = 0 }, .max = { .f = 1.0 },  .step = { .f = 1.0f / 100.0f } },
        [LVL_AF] = { .min = { .f = 0 }, .max = { .f = 1.0 },  .step = { .f = 1.0f / 100.0f } },
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 30 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = {.min = {.i = 4}, .max = {.i = 60}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 400}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_BKIN_DLYMS] = {.min = {.i = 0}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_SLOPE_LOW] = {.min = {.i = 0}, .max = {.i = 2400}, .step = {.i = 10}},
        [LVL_SLOPE_HIGH] = {.min = {.i = 0}, .max = {.i = 5000}, .step = {.i = 10}},
    },

    .str_cal = TS590_STR_CAL,
    .swr_cal = TS590_SWR_CAL,

    .ext_tokens = ts590_ext_tokens,
    .extfuncs = ts590_ext_funcs,
    .extlevels = ts590_ext_levels,

    .priv = (void *)& ts590_priv_caps,
    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = ts590_set_rit,
    .get_rit = ts590_get_rit,
    .set_xit = ts590_set_rit,
    .get_xit = ts590_get_rit,
    .set_mode = ts590_set_mode,
    .get_mode = ts590_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .get_info = ts590_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan_ops =  TS590_SCAN_OPS,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS590_LEVEL_SET,
    .has_get_level = TS590_LEVEL_GET,
    .set_level = ts590_set_level,
    .get_level = ts590_get_level,
    .set_ext_level = ts590_set_ext_level,
    .get_ext_level = ts590_get_ext_level,
    .has_get_func = TS590_FUNC_ALL,
    .has_set_func = TS590_FUNC_ALL,
    .set_func = ts590_set_func,
    .get_func = ts590_get_func,
    .set_ext_func = ts590_set_ext_func,
    .get_ext_func = ts590_get_ext_func,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .ctcss_list =  kenwood38_ctcss_list,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .send_morse =  kenwood_send_morse,
    .stop_morse =  kenwood_stop_morse,
    .wait_morse =  rig_wait_morse,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .vfo_ops = TS590_VFO_OPS,
    .vfo_op =  kenwood_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
