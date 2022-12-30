/*
 *  Hamlib Kenwood backend - TS-480 description
 *  Copyright (c) 2000-2004 by Stephane Fillod and Juergen Rinas
 *  Copyright (c) 2021 by Mikael Nousiainen
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

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>

#include <hamlib/rig.h>
#include "cal.h"
#include "idx_builtin.h"
#include "iofunc.h"
#include "token.h"
#include "kenwood.h"

#define TS480_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define SDRUNO_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTUSB)
#define PS8000A_ALL_MODES (RIG_MODE_AM|RIG_MODE_AMS|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)

#define TS480_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS480_AM_TX_MODES RIG_MODE_AM
#define TS480_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS480_LEVEL_GET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_MICGAIN|RIG_LEVEL_STRENGTH|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH| \
    RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_NR|RIG_LEVEL_PREAMP|RIG_LEVEL_COMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_VOXGAIN|RIG_LEVEL_BKIN_DLYMS| \
    RIG_LEVEL_METER|RIG_LEVEL_SWR|RIG_LEVEL_COMP_METER|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW)

#define TS480_LEVEL_SET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH| \
    RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_NB|RIG_LEVEL_NR|RIG_LEVEL_PREAMP|RIG_LEVEL_COMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_VOXGAIN|RIG_LEVEL_BKIN_DLYMS| \
    RIG_LEVEL_METER|RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW)

#define TS480_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_BC2|RIG_FUNC_RIT|RIG_FUNC_XIT| \
    RIG_FUNC_TUNER|RIG_FUNC_MON|RIG_FUNC_FBKIN|RIG_FUNC_LOCK)

#define TS480_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|RIG_OP_CPY|RIG_OP_TUNE)

// TS-480 S-meter calibration table based on LCD display bars
#define TS480_STR_CAL { 9, \
    { \
        {   0, -60 }, \
        {   3, -48 }, \
        {   5, -36 }, \
        {   7, -24 }, \
        {   9, -12 }, \
        {  11,  0 }, \
        {  14,  20 }, \
        {  17,  40 }, \
        {  20,  60 } \
    } }

// TS-480 SWR calibration table based approximately on LCD display bars
#define TS480_SWR_CAL { 5, \
    { \
        {   0, 1.0f }, \
        {   4, 1.5f }, \
        {   8, 2.0f }, \
        {   12, 3.0f }, \
        {   20, 10.0f } \
    } }

#define TOK_FUNC_NOISE_REDUCTION_2 TOKEN_BACKEND(102)
#define TOK_FUNC_TX_AUDIO_FROM_DATA_INPUT TOKEN_BACKEND(103)
#define TOK_LEVEL_DSP_RX_EQUALIZER TOKEN_BACKEND(104)
#define TOK_LEVEL_DSP_TX_EQUALIZER TOKEN_BACKEND(105)
#define TOK_LEVEL_DSP_TX_BANDWIDTH TOKEN_BACKEND(106)
#define TOK_LEVEL_BEEP_VOLUME TOKEN_BACKEND(107)
#define TOK_LEVEL_TX_SIDETONE_VOLUME TOKEN_BACKEND(108)
#define TOK_LEVEL_AF_INPUT_LEVEL TOKEN_BACKEND(109)
#define TOK_LEVEL_AF_OUTPUT_LEVEL TOKEN_BACKEND(110)
#define TOK_LEVEL_DIGITAL_NOISE_LIMITER TOKEN_BACKEND(111)
#define TOK_FUNC_CW_IF_FOR_SSB_RX TOKEN_BACKEND(112)

int ts480_ext_tokens[] =
{
    TOK_FUNC_NOISE_REDUCTION_2, TOK_FUNC_FILTER_WIDTH_DATA, TOK_FUNC_TX_AUDIO_FROM_DATA_INPUT,
    TOK_LEVEL_DSP_RX_EQUALIZER, TOK_LEVEL_DSP_TX_EQUALIZER, TOK_LEVEL_DSP_TX_BANDWIDTH,
    TOK_LEVEL_BEEP_VOLUME, TOK_LEVEL_TX_SIDETONE_VOLUME,
    TOK_LEVEL_AF_INPUT_LEVEL, TOK_LEVEL_AF_OUTPUT_LEVEL,
    TOK_LEVEL_DIGITAL_NOISE_LIMITER, TOK_FUNC_CW_IF_FOR_SSB_RX,
    TOK_BACKEND_NONE,
};

const struct confparams ts480_ext_funcs[] =
{
    {
        TOK_FUNC_NOISE_REDUCTION_2, "NR2", "Noise reduction 2", "Noise reduction 2",
        NULL, RIG_CONF_CHECKBUTTON,
    },
    {
        TOK_FUNC_CW_IF_FOR_SSB_RX, "CW_IF_FOR_SSB_RX", "CW IF filter for SSB", "Use CW IF filter for SSB reception",
        NULL, RIG_CONF_CHECKBUTTON,
    },
    {
        TOK_FUNC_FILTER_WIDTH_DATA, "FILTER_WIDTH_DATA", "Filter bandwidth for data", "Filter bandwidth for data communications",
        NULL, RIG_CONF_CHECKBUTTON,
    },
    {
        TOK_FUNC_TX_AUDIO_FROM_DATA_INPUT, "TX_AUDIO_FROM_DATA_INPUT", "TX audio from data input", "Transmit with audio input from the data terminal",
        NULL, RIG_CONF_CHECKBUTTON,
    },
    { RIG_CONF_END, NULL, }
};

const struct confparams ts480_ext_levels[] =
{
    {
        TOK_LEVEL_DIGITAL_NOISE_LIMITER, "DIGITAL_NOISE_LIMITER", "Digital Noise Limiter", "Digital Noise Limiter",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "OFF", "DNL Level 1", "DNL Level 2", "DNL Level 3", NULL } } }
    },
    {
        TOK_LEVEL_DSP_RX_EQUALIZER, "DSP_RX_EQUALIZER", "DSP RX equalizer", "DSP RX equalizer type",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "OFF", "Hb1", "Hb2", "FP", "bb1", "bb2", "c", "U", NULL } } }
    },
    {
        TOK_LEVEL_DSP_TX_EQUALIZER, "DSP_TX_EQUALIZER", "DSP TX equalizer", "DSP TX equalizer type",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "OFF", "Hb1", "Hb2", "FP", "bb1", "bb2", "c", "U", NULL } } }
    },
    {
        TOK_LEVEL_DSP_TX_BANDWIDTH, "DSP_TX_BANDWIDTH", "DSP TX bandwidth", "DSP TX bandwidth for SSB and AM",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "2.0 kHz", "2.4 kHz", NULL } } }
    },
    {
        TOK_LEVEL_BEEP_VOLUME, "BEEP_VOLUME", "Beep volume", "Beep volume",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    {
        TOK_LEVEL_TX_SIDETONE_VOLUME, "TX_SIDETONE_VOLUME", "TX sidetone volume", "TX sidetone volume",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    {
        TOK_LEVEL_AF_INPUT_LEVEL, "AF_INPUT_LEVEL", "AF input level", "AF input level for data communications",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    {
        TOK_LEVEL_AF_OUTPUT_LEVEL, "AF_OUTPUT_LEVEL", "AF output level", "AF output level for data communications",
        NULL, RIG_CONF_NUMERIC, { .n = { .min = 0, .max = 9, .step = 1 } }
    },
    { RIG_CONF_END, NULL, }
};

/*
 * kenwood_ts480_get_info
 * Assumes rig!=NULL
 */
const char *
kenwood_ts480_get_info(RIG *rig)
{
    char firmbuf[50];
    int retval;
    size_t firm_len;

    retval = kenwood_transaction(rig, "TY", firmbuf, sizeof(firmbuf));

    if (retval != RIG_OK)
    {
        return NULL;
    }

    firm_len = strlen(firmbuf);

    if (firm_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n", __func__, (int)firm_len);
        return NULL;
    }

    switch (firmbuf[4])
    {
    case '0':
        return "TS-480HX (200W)";

    case '1':
        return "TS-480SAT (100W + AT)";

    case '2':
        return "Japanese 50W type";

    case '3':
        return "Japanese 20W type";

    default:
        return "Firmware: unknown";
    }
}

static int ts480_set_ex_menu(RIG *rig, int number, int value_len, int value)
{
    char buf[20];

    ENTERFUNC;

    SNPRINTF(buf, sizeof(buf), "EX%03d0000%0*d", number, value_len, value);

    RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));
}

static int ts480_get_ex_menu(RIG *rig, int number, int value_len, int *value)
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

static int ts480_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
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

static int ts480_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
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
int kenwood_ts480_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int kenwood_val;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_RF:
        kenwood_val = val.f * 100;
        SNPRINTF(levelbuf, sizeof(levelbuf), "RG%03d", kenwood_val);
        break;

    case RIG_LEVEL_AF:
        return kenwood_set_level(rig, vfo, level, val);

    case RIG_LEVEL_SQL:
        kenwood_val = val.f * 255;
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ0%03d", kenwood_val);
        break;

    case RIG_LEVEL_AGC:
        /* hamlib argument is int, possible values rig.h:enum agc_level_e */
        /* possible values for TS480 000(=off), 001(=fast), 002(=slow) */
        rig_debug(RIG_DEBUG_VERBOSE, "%s TS480 RIG_LEVEL_AGC\n", __func__);

        switch (val.i)
        {
        case RIG_AGC_OFF:
            kenwood_val = 0;
            break;

        case RIG_AGC_FAST:
            kenwood_val = 1;
            break;

        case RIG_AGC_SLOW:
            kenwood_val = 2;
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

        RETURNFUNC(ts480_set_ex_menu(rig, 34, 2, (val.i - 400) / 50));

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    return kenwood_transaction(rig, levelbuf, NULL, 0);
}

static int ts480_read_meters(RIG *rig, int *swr, int *comp, int *alc)
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

    // TS-480 returns values for all meters at the same time, for example: RM10000;RM20000;RM30000;

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


/*
 * kenwood_ts480_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int
kenwood_ts480_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
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

        val->f = levelint / (float) 100;
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

        switch (ackbuf[ack_len_expected - 1])
        {
        case '0':
            val->i = RIG_AGC_OFF;
            break;

        case '1':
            val->i = RIG_AGC_FAST;
            break;

        case '2':
            val->i = RIG_AGC_SLOW;
            break;

        default:
            return -RIG_EPROTO;
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
        int swr;
        int comp;
        int alc;

        retval = ts480_read_meters(rig, &swr, &comp, &alc);

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

        val->f = (float) raw_value / 20.0f;
        break;
    }

    case RIG_LEVEL_CWPITCH:
    {
        int raw_value;
        retval = ts480_get_ex_menu(rig, 34, 2, &raw_value);

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

static int ts480_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
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

static int ts480_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
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

static int ts480_set_ext_func(RIG *rig, vfo_t vfo, token_t token, int status)
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

    case TOK_FUNC_CW_IF_FOR_SSB_RX:
        if (status < 0 || status > 1)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 17, 1, status);
        break;

    case TOK_FUNC_FILTER_WIDTH_DATA:
        if (status < 0 || status > 1)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 45, 1, status);
        break;

    case TOK_FUNC_TX_AUDIO_FROM_DATA_INPUT:
        if (status < 0 || status > 1)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 60, 1, status);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static int ts480_get_ext_func(RIG *rig, vfo_t vfo, token_t token, int *status)
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

    case TOK_FUNC_CW_IF_FOR_SSB_RX:
        retval = ts480_get_ex_menu(rig, 17, 1, status);
        break;

    case TOK_FUNC_FILTER_WIDTH_DATA:
        retval = ts480_get_ex_menu(rig, 45, 1, status);
        break;

    case TOK_FUNC_TX_AUDIO_FROM_DATA_INPUT:
        retval = ts480_get_ex_menu(rig, 60, 1, status);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static int ts480_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    int retval;
    char cmdbuf[20];

    ENTERFUNC;

    switch (token)
    {
    case TOK_LEVEL_DIGITAL_NOISE_LIMITER:
        if (val.i < 0 || val.i > 3)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "DL%d%02d", val.i != 0 ? 1 : 0,
                 val.i > 0 ? val.i - 1 : 0);
        retval = kenwood_transaction(rig, cmdbuf, NULL, 0);
        break;

    case TOK_LEVEL_DSP_RX_EQUALIZER:
        if (val.i < 0 || val.i > 7)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 18, 1, val.i);
        break;

    case TOK_LEVEL_DSP_TX_EQUALIZER:
        if (val.i < 0 || val.i > 7)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 19, 1, val.i);
        break;

    case TOK_LEVEL_DSP_TX_BANDWIDTH:
        if (val.i < 0 || val.i > 1)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 20, 1, val.i);
        break;

    case TOK_LEVEL_BEEP_VOLUME:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 12, 1, (int) val.f);
        break;

    case TOK_LEVEL_TX_SIDETONE_VOLUME:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 13, 1, (int) val.f);
        break;

    case TOK_LEVEL_AF_INPUT_LEVEL:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 46, 1, (int) val.f);
        break;

    case TOK_LEVEL_AF_OUTPUT_LEVEL:
        if (val.f < 0 || val.f > 9)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        retval = ts480_set_ex_menu(rig, 47, 1, (int) val.f);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static int ts480_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    char ackbuf[20];
    int retval;
    int value;

    ENTERFUNC;

    switch (token)
    {
    case TOK_LEVEL_DIGITAL_NOISE_LIMITER:
    {
        int enabled;
        int level;

        retval = kenwood_safe_transaction(rig, "DL", ackbuf, sizeof(ackbuf), 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(ackbuf, "DL%1d%2d", &enabled, &level);

        val->i = enabled ? level + 1 : 0;
        break;
    }

    case TOK_LEVEL_DSP_RX_EQUALIZER:
        retval = ts480_get_ex_menu(rig, 18, 1, &value);
        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_EQUALIZER:
        retval = ts480_get_ex_menu(rig, 19, 1, &value);
        val->i = value;
        break;

    case TOK_LEVEL_DSP_TX_BANDWIDTH:
        retval = ts480_get_ex_menu(rig, 20, 1, &value);
        val->i = value;
        break;

    case TOK_LEVEL_BEEP_VOLUME:
        retval = ts480_get_ex_menu(rig, 12, 1, &value);
        val->f = value;
        break;

    case TOK_LEVEL_TX_SIDETONE_VOLUME:
        retval = ts480_get_ex_menu(rig, 13, 1, &value);
        val->f = value;
        break;

    case TOK_LEVEL_AF_INPUT_LEVEL:
        retval = ts480_get_ex_menu(rig, 46, 1, &value);
        val->f = value;
        break;

    case TOK_LEVEL_AF_OUTPUT_LEVEL:
        retval = ts480_get_ex_menu(rig, 47, 1, &value);
        val->f = value;
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(retval);
}

static struct kenwood_filter_width ts480_filter_width[] =
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
    { RIG_MODE_FM, 0, 12000 },
    { RIG_MODE_AM, 0, 6000 },
    { RIG_MODE_AM, 1, 2400 }, // NAR1 optional filter (?)
    { RIG_MODE_NONE, -1, -1 },
};

static struct kenwood_slope_filter ts480_slope_filter_high[] =
{
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 0, 1000 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 1, 1200 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 2, 1400 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 3, 1600 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 4, 1800 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 5, 2000 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 6, 2200 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 7, 2400 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 8, 2600 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 9, 2800 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 10, 3000 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 11, 3400 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 12, 4000 },
    { RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_RTTY | RIG_MODE_RTTYR, 0, 13, 5000 },
    { RIG_MODE_AM, 0, 0, 2500 },
    { RIG_MODE_AM, 0, 1, 3000 },
    { RIG_MODE_AM, 0, 2, 4000 },
    { RIG_MODE_AM, 0, 3, 5000 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 0, 1000 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 1, 1500 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 2, 2210 },
    { RIG_MODE_NONE, 0, -1, -1 },
};

static struct kenwood_slope_filter ts480_slope_filter_low[] =
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
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 0, 50 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 1, 100 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 2, 250 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 3, 500 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 4, 1000 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 5, 1500 },
    { RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_FM | RIG_MODE_AM, 1, 6, 2400 },
    { RIG_MODE_NONE, 0, -1, -1 },
};

static struct kenwood_priv_caps ts480_priv_caps =
{
    .cmdtrm = EOM_KEN,
    .filter_width = ts480_filter_width,
    .slope_filter_high = ts480_slope_filter_high,
    .slope_filter_low = ts480_slope_filter_low,
};

int ts480_init(RIG *rig)
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

    priv->ag_format = 2;
    priv->micgain_min = 0;
    priv->micgain_max = 100;

    RETURNFUNC(RIG_OK);
}

/*
 * TS-480 rig capabilities
 * Notice that some rigs share the same functions.
 */
const struct rig_caps ts480_caps =
{
    RIG_MODEL(RIG_MODEL_TS480),
    .model_name = "TS-480",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".2",
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
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 3,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_SLOW },

    .rx_range_list1 = {
        {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  kHz(3800),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  MHz(4) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS480_ALL_MODES, kHz(1)},
        {TS480_ALL_MODES, Hz(2500)},
        {TS480_ALL_MODES, kHz(5)},
        {TS480_ALL_MODES, Hz(6250)},
        {TS480_ALL_MODES, kHz(10)},
        {TS480_ALL_MODES, Hz(12500)},
        {TS480_ALL_MODES, kHz(15)},
        {TS480_ALL_MODES, kHz(20)},
        {TS480_ALL_MODES, kHz(25)},
        {TS480_ALL_MODES, kHz(30)},
        {TS480_ALL_MODES, kHz(100)},
        {TS480_ALL_MODES, kHz(500)},
        {TS480_ALL_MODES, MHz(1)},
        {TS480_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, Hz(270)},
        {RIG_MODE_SSB, Hz(500)},
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
        RIG_FLT_END,
    },
    .vfo_ops = TS480_VFO_OPS,
    .level_gran =
    {
#include "level_gran_kenwood.h"
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 30 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = {.min = {.i = 10}, .max = {.i = 60}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 400}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_BKIN_DLYMS] = {.min = {.i = 0}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_SLOPE_LOW] = {.min = {.i = 0}, .max = {.i = 2400}, .step = {.i = 10}},
        [LVL_SLOPE_HIGH] = {.min = {.i = 0}, .max = {.i = 5000}, .step = {.i = 10}},
    },
    .str_cal = TS480_STR_CAL,
    .swr_cal = TS480_SWR_CAL,

    .ext_tokens = ts480_ext_tokens,
    .extfuncs = ts480_ext_funcs,
    .extlevels = ts480_ext_levels,

    .priv = (void *)& ts480_priv_caps,
    .rig_init = ts480_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = ts480_set_rit,
    .get_rit = ts480_get_rit,
    .set_xit = ts480_set_rit,
    .get_xit = ts480_get_rit,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .get_info = kenwood_ts480_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS480_LEVEL_SET,
    .has_get_level = TS480_LEVEL_GET,
    .set_level = kenwood_ts480_set_level,
    .get_level = kenwood_ts480_get_level,
    .set_ext_level = ts480_set_ext_level,
    .get_ext_level = ts480_get_ext_level,
    .has_get_func = TS480_FUNC_ALL,
    .has_set_func = TS480_FUNC_ALL,
    .set_func = ts480_set_func,
    .get_func = ts480_get_func,
    .set_ext_func = ts480_set_ext_func,
    .get_ext_func = ts480_get_ext_func,
    .send_morse = kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .vfo_op = kenwood_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * QRPLabs TS-480 emulation rig capabilities
 * Notice that some rigs share the same functions.
 */
const struct rig_caps qrplabs_caps =
{
    RIG_MODEL(RIG_MODEL_QRPLABS),
    .model_name = "QCX/QDX",
    .mfg_name = "QRPLabs",
    .version = BACKEND_VER ".0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 3,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,
    .agc_level_count = 3,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_SLOW },

    .rx_range_list1 = {
        {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  kHz(3800),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  MHz(4) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS480_ALL_MODES, kHz(1)},
        {TS480_ALL_MODES, Hz(2500)},
        {TS480_ALL_MODES, kHz(5)},
        {TS480_ALL_MODES, Hz(6250)},
        {TS480_ALL_MODES, kHz(10)},
        {TS480_ALL_MODES, Hz(12500)},
        {TS480_ALL_MODES, kHz(15)},
        {TS480_ALL_MODES, kHz(20)},
        {TS480_ALL_MODES, kHz(25)},
        {TS480_ALL_MODES, kHz(30)},
        {TS480_ALL_MODES, kHz(100)},
        {TS480_ALL_MODES, kHz(500)},
        {TS480_ALL_MODES, MHz(1)},
        {TS480_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, Hz(270)},
        {RIG_MODE_SSB, Hz(500)},
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
        RIG_FLT_END,
    },
    .vfo_ops = TS480_VFO_OPS,
    .level_gran =
    {
#include "level_gran_kenwood.h"
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 30 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = {.min = {.i = 10}, .max = {.i = 60}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 400}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_BKIN_DLYMS] = {.min = {.i = 0}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_SLOPE_LOW] = {.min = {.i = 0}, .max = {.i = 2400}, .step = {.i = 10}},
        [LVL_SLOPE_HIGH] = {.min = {.i = 0}, .max = {.i = 5000}, .step = {.i = 10}},
    },
    .str_cal = TS480_STR_CAL,
    .swr_cal = TS480_SWR_CAL,

    .ext_tokens = ts480_ext_tokens,
    .extfuncs = ts480_ext_funcs,
    .extlevels = ts480_ext_levels,

    .priv = (void *)& ts480_priv_caps,
    .rig_init = ts480_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = ts480_set_rit,
    .get_rit = ts480_get_rit,
    .set_xit = ts480_set_rit,
    .get_xit = ts480_get_rit,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .get_info = kenwood_ts480_get_info,
    .reset = kenwood_reset,
    .has_set_level = TS480_LEVEL_SET,
    .has_get_level = TS480_LEVEL_GET,
    .set_level = kenwood_ts480_set_level,
    .get_level = kenwood_ts480_get_level,
    .set_ext_level = ts480_set_ext_level,
    .get_ext_level = ts480_get_ext_level,
    .has_get_func = TS480_FUNC_ALL,
    .has_set_func = TS480_FUNC_ALL,
    .set_func = ts480_set_func,
    .get_func = ts480_get_func,
    .set_ext_func = ts480_set_ext_func,
    .get_ext_func = ts480_get_ext_func,
    .send_morse = kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .vfo_op = kenwood_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Hilberling PS8000A TS480 emulation
 * Notice that some rigs share the same functions.
 */
const struct rig_caps pt8000a_caps =
{
    RIG_MODEL(RIG_MODEL_PT8000A),
    .model_name = "PT-8000A",
    .mfg_name = "Hilberling",
    .version = BACKEND_VER ".1",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 57600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 20,
    .timeout = 500,
    .retry = 10,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,

    .rx_range_list1 = { // not region specific
        {kHz(9),   MHz(30), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_2 | RIG_ANT_3, "Generic"},
        {MHz(50),   MHz(54), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_1, "Generic"},
        {MHz(69.9),   MHz(70.5), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_1, "Generic"},
        {MHz(110),   MHz(143.99), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_1, "Generic"},
        {MHz(144),   MHz(148), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_1, "Generic"},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {MHz(1.8),  MHz(30),  TS480_OTHER_TX_MODES, 1000, 200000, TS480_VFO, RIG_ANT_2 | RIG_ANT_3, "Generic"}, /* 200W class */
        {MHz(1.8),  MHz(30),  TS480_AM_TX_MODES | RIG_MODE_AMS, 5000, 50000, TS480_VFO, RIG_ANT_2 | RIG_ANT_3, "Generic"},   /* 50W class */
        {MHz(50),  MHz(54),  TS480_OTHER_TX_MODES, 1000, 100000, TS480_VFO, RIG_ANT_1, "Generic"},   /* 100W class */
        {MHz(50),  MHz(54),  TS480_AM_TX_MODES | RIG_MODE_AMS, 5000, 25000, TS480_VFO, RIG_ANT_1, "Generic"},     /* 25W class */
        {MHz(69.9),  MHz(70.5),  TS480_OTHER_TX_MODES, 1000, 100000, TS480_VFO, RIG_ANT_1, "Generic"},   /* 100W class */
        {MHz(69.9),  MHz(70.5),  TS480_AM_TX_MODES | RIG_MODE_AMS, 5000, 25000, TS480_VFO, RIG_ANT_1, "Generic"},     /* 25W class */
        {MHz(144),  MHz(148),  TS480_OTHER_TX_MODES, 1000, 100000, TS480_VFO, RIG_ANT_1, "Generic"},   /* 100W class */
        {MHz(144),  MHz(148),  TS480_AM_TX_MODES | RIG_MODE_AMS, 5000, 25000, TS480_VFO, RIG_ANT_1, "Generic"},     /* 25W class */
        RIG_FRNG_END,
    },
    .tuning_steps =  {
        {PS8000A_ALL_MODES, Hz(1)},
        {PS8000A_ALL_MODES, Hz(10)},
        {PS8000A_ALL_MODES, Hz(100)},
        {PS8000A_ALL_MODES, Hz(1000)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(1.0)},
        {RIG_MODE_SSB, kHz(1.2)},
        {RIG_MODE_SSB, kHz(1.4)},
        {RIG_MODE_SSB, kHz(1.6)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(1.9)},
        {RIG_MODE_SSB, kHz(2.0)},
        {RIG_MODE_SSB, kHz(2.1)},
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_SSB, kHz(2.3)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(2.5)},
        {RIG_MODE_SSB, kHz(2.6)},
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_SSB, kHz(2.9)},
        {RIG_MODE_SSB, kHz(3.0)},
        {RIG_MODE_SSB, kHz(3.1)},
        {RIG_MODE_SSB, kHz(3.2)},
        {RIG_MODE_SSB, kHz(3.3)},
        {RIG_MODE_SSB, kHz(3.4)},
        {RIG_MODE_SSB, kHz(3.5)},
        {RIG_MODE_SSB, kHz(4.6)},
        {RIG_MODE_SSB, kHz(6.0)},
        {RIG_MODE_CW, Hz(50)},
        {RIG_MODE_CW, Hz(100)},
        {RIG_MODE_CW, Hz(200)},
        {RIG_MODE_CW, Hz(400)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_AM, kHz(2.5)},
        {RIG_MODE_AM, kHz(2.6)},
        {RIG_MODE_AM, kHz(2.7)},
        {RIG_MODE_AM, kHz(2.8)},
        {RIG_MODE_AM, kHz(2.9)},
        {RIG_MODE_AM, kHz(3.0)},
        {RIG_MODE_AM, kHz(3.1)},
        {RIG_MODE_AM, kHz(3.2)},
        {RIG_MODE_AM, kHz(3.3)},
        {RIG_MODE_AM, kHz(3.4)},
        {RIG_MODE_AM, kHz(3.5)},
        {RIG_MODE_AM, kHz(3.5)},
        {RIG_MODE_AM, kHz(4.6)},
        {RIG_MODE_AM, kHz(6.0)},
        {RIG_MODE_FM, kHz(2.4)},
        {RIG_MODE_FM, kHz(2.5)},
        {RIG_MODE_FM, kHz(2.6)},
        {RIG_MODE_FM, kHz(2.7)},
        {RIG_MODE_FM, kHz(2.8)},
        {RIG_MODE_FM, kHz(2.9)},
        {RIG_MODE_FM, kHz(3.0)},
        {RIG_MODE_FM, kHz(3.1)},
        {RIG_MODE_FM, kHz(3.2)},
        {RIG_MODE_FM, kHz(3.3)},
        {RIG_MODE_FM, kHz(3.4)},
        {RIG_MODE_FM, kHz(3.5)},
        {RIG_MODE_FM, kHz(3.5)},
        {RIG_MODE_FM, kHz(4.6)},
        {RIG_MODE_FM, kHz(6.0)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts480_priv_caps,
    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit,
    .get_rit = kenwood_get_rit,
    .set_xit = kenwood_set_xit,
    .get_xit = kenwood_get_xit,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .get_info = kenwood_ts480_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS480_LEVEL_SET,
    .has_get_level = TS480_LEVEL_GET,
    .set_level = kenwood_ts480_set_level,
    .get_level = kenwood_ts480_get_level,
    .has_get_func = TS480_FUNC_ALL,
    .has_set_func = TS480_FUNC_ALL,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * SDRPlay SDRUno rig capabilities
 * Notice that some rigs share the same functions.
 */
const struct rig_caps sdruno_caps =
{
    RIG_MODEL(RIG_MODEL_SDRUNO),
    .model_name = "SDRUno",
    .mfg_name = "SDRPlay",
    .version = BACKEND_VER ".2",
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
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 3,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,

    .rx_range_list1 = {
        {kHz(100),   Hz(59999999), SDRUNO_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  kHz(3800),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  MHz(4) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS480_ALL_MODES, kHz(1)},
        {TS480_ALL_MODES, Hz(2500)},
        {TS480_ALL_MODES, kHz(5)},
        {TS480_ALL_MODES, Hz(6250)},
        {TS480_ALL_MODES, kHz(10)},
        {TS480_ALL_MODES, Hz(12500)},
        {TS480_ALL_MODES, kHz(15)},
        {TS480_ALL_MODES, kHz(20)},
        {TS480_ALL_MODES, kHz(25)},
        {TS480_ALL_MODES, kHz(30)},
        {TS480_ALL_MODES, kHz(100)},
        {TS480_ALL_MODES, kHz(500)},
        {TS480_ALL_MODES, MHz(1)},
        {TS480_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, Hz(270)},
        {RIG_MODE_SSB, Hz(500)},
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
        {RIG_MODE_PKTUSB, kHz(4)},
        RIG_FLT_END,
    },
    .vfo_ops = TS480_VFO_OPS,
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 30 }, .step = { .i = 1 } },
        [LVL_KEYSPD] = {.min = {.i = 10}, .max = {.i = 60}, .step = {.i = 1}},
        [LVL_CWPITCH] = {.min = {.i = 400}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_BKIN_DLYMS] = {.min = {.i = 0}, .max = {.i = 1000}, .step = {.i = 50}},
        [LVL_SLOPE_LOW] = {.min = {.i = 0}, .max = {.i = 2400}, .step = {.i = 10}},
        [LVL_SLOPE_HIGH] = {.min = {.i = 0}, .max = {.i = 5000}, .step = {.i = 10}},
    },
    .str_cal = TS480_STR_CAL,
    .swr_cal = TS480_SWR_CAL,

    .ext_tokens = ts480_ext_tokens,
    .extfuncs = ts480_ext_funcs,
    .extlevels = ts480_ext_levels,

    .priv = (void *)& ts480_priv_caps,
    .rig_init = ts480_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = ts480_set_rit,
    .get_rit = ts480_get_rit,
    .set_xit = ts480_set_rit,
    .get_xit = ts480_get_rit,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .get_info = kenwood_ts480_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS480_LEVEL_SET,
    .has_get_level = TS480_LEVEL_GET,
    .set_level = kenwood_ts480_set_level,
    .get_level = kenwood_ts480_get_level,
    .set_ext_level = ts480_set_ext_level,
    .get_ext_level = ts480_get_ext_level,
    .has_get_func = TS480_FUNC_ALL,
    .has_set_func = TS480_FUNC_ALL,
    .set_func = ts480_set_func,
    .get_func = ts480_get_func,
    .set_ext_func = ts480_set_ext_func,
    .get_ext_func = ts480_get_ext_func,
    .send_morse = kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .vfo_op = kenwood_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct confparams malachite_cfg_parms[] =
{
    {
        // the Malachite SDR cannot handle sending ID; after FA; commands
        TOK_NO_ID, "no_id", "No ID", "If true do not send ID; with set commands",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    { RIG_CONF_END, NULL, }
};

int malachite_init(RIG *rig)
{
    struct kenwood_priv_data *priv;
    int retval;

    ENTERFUNC;

    retval = kenwood_init(rig);

    priv = rig->state.priv;

    priv->no_id = 1;  // the Malchite doesn't like the ID; verify cmd

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    RETURNFUNC(RIG_OK);
}

int malachite_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;

    // Malachite has a bug where it takes two freq set to make it work
    // under some band changes -- so we just do this all the time
    retval = kenwood_set_freq(rig, vfo, freq + 1);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    retval = kenwood_set_freq(rig, vfo, freq);

    RETURNFUNC2(retval);
}

/*
 * Malachite SDR rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps malachite_caps =
{
    RIG_MODEL(RIG_MODEL_MALACHITE),
    .model_name = "DSP",
    .mfg_name = "Malachite",
    .version = BACKEND_VER ".2",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_RECEIVER,
    .ptt_type = RIG_PTT_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 38400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 400,
    .timeout = 3000,
    .retry = 3,
    .preamp = {0, RIG_DBLST_END,},
    .attenuator = {0, RIG_DBLST_END,},
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_POLL,


    .rx_range_list1 = {
        {kHz(50),   MHz(250), TS480_ALL_MODES, -1, -1, RIG_VFO_A, RIG_ANT_CURR,  "Generic" },
        {MHz(400),   GHz(2), TS480_ALL_MODES, -1, -1, RIG_VFO_A, RIG_ANT_CURR,  "Generic" },
        RIG_FRNG_END,
    },
    .tuning_steps =  {
        {RIG_MODE_ALL, Hz(1)},
        RIG_TS_END
    },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .priv = (void *)& ts480_priv_caps,
    .rig_init = malachite_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = malachite_set_freq,
    .get_freq = kenwood_get_freq,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo, // Malachite only supports VFOA
    .get_vfo = kenwood_get_vfo_if,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
