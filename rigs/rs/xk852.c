#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "num_stdio.h"

#include "xk852.h"

#define RESPSZ 64

#define LF "\x0a"
#define CR "\x0d"
#define BOM LF
#define EOM CR

#define XK852_MODES (RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_CW|RIG_MODE_AM)

#define XK852_FUNC (RIG_FUNC_NONE)

#define XK852_LEVEL_ALL (RIG_LEVEL_SQL | RIG_LEVEL_RFPOWER)

#define XK852_PARM_ALL (RIG_PARM_NONE)

#define XK852_VFO (RIG_VFO_A)

#define XK852_VFO_OPS (RIG_OP_NONE)

#define XK852_ANTS (RIG_ANT_1)

#define XK852_MEM_CAP {    \
        .freq = 1,      \
        .mode = 1,      \
        .width = 1,     \
        .ant = 1,     \
        .funcs = XK852_FUNC, \
        .levels = RIG_LEVEL_SET(XK852_LEVEL_ALL), \
        .channel_desc=1, \
        .flags = RIG_CHFLAG_SKIP, \
}

int
xk852_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                  int *data_len)
{
    int retval;
    hamlib_port_t *rp = RIGPORT(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: len=%d,cmd=%s\n", __func__, cmd_len,
              cmd);

    rig_flush(rp);

    rig_debug(RIG_DEBUG_VERBOSE, "xk852_transaction: len=%d,cmd=%s\n",
              cmd_len, cmd);
    retval = write_block(rp, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* no data expected */
    if (!data || !data_len)
    {
        return RIG_OK;
    }

    retval = read_string(rp, (unsigned char *) data, RESPSZ,
                         CR, 1, 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}

// Send command discard return
int
xk852_send_command(RIG *rig, const char *cmd, int cmd_len)
{
    char buf[RESPSZ];
    int len;
    int retval = 0;
    retval = xk852_transaction(rig, cmd, cmd_len, buf, &len);
    return retval;
}

int
xk852_parse_state(const char *msg, xk852_state *state)
{
    int ret;

    ret = sscanf(msg, BOM "*F%7u" SCNfreq, &state -> freq);

    if (ret != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unable to parse frequency from '%s'\n", __func__,
                  msg);
        return -RIG_EPROTO;
    };

    ret = sscanf(msg, "%*13cI%1u", &state -> mode);

    if (ret != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unable to parse mode from '%s'\n", __func__, msg);
        return -RIG_EPROTO;
    };

    ret = sscanf(msg, "%*23cN%1u", &state -> noise_blank);

    if (ret != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unable to parse noise blanker state from '%s'\n",
                  __func__, msg);
        return -RIG_EPROTO;
    };

    ret = sscanf(msg, "%*31cS%1u", &state -> op_mode);

    if (ret != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unable to parse op mode state from '%s'\n",
                  __func__, msg);
        return -RIG_EPROTO;
    };

    return RIG_OK;
};

int
xk852_query_state(RIG *rig, xk852_state *state)
{
    char buf[RESPSZ];
    int buf_len, ret;
#define STATE_QUERY   BOM "*O1" EOM
    ret = xk852_transaction(rig, STATE_QUERY, strlen(STATE_QUERY), buf, &buf_len);

    if (ret < 0)
    {
        return ret;
    }

    ret = xk852_parse_state(buf, state);

    return ret;
};

/*
 * xk852_set_freq
 * Assumes rig!=NULL
 */
int
xk852_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[32];
    int retval;
    char *fmt = BOM "*F%.7" PRIll EOM;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s,freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    SNPRINTF(freqbuf, sizeof(freqbuf), fmt, (int64_t)((freq + 5) / 10));
    retval = xk852_send_command(rig, freqbuf, strlen(freqbuf));

    return retval;
}

/*
 * xk852_get_freq
 * Assumes rig!=NULL
 */
int
xk852_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    xk852_state state;
    int ret;

    ret = xk852_query_state(rig, &state);

    *freq = (freq_t)(state.freq * 10);
    return ret;
}

/*
 * xk852_set_mode
 * Assumes rig!=NULL
 */
int
xk852_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char buf[32];
    xk852_mode smode;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s, mode=%s, width=%d\n", __func__,
              rig_strvfo(vfo), rig_strvfo(mode), (int)width);

    switch (mode)
    {
    case RIG_MODE_AM:
        smode = XK852_MODE_AME;
        break;

    case RIG_MODE_USB:
        smode = XK852_MODE_USB;
        break;

    case RIG_MODE_LSB:
        smode = XK852_MODE_LSB;
        break;

    case RIG_MODE_CW:
        smode = XK852_MODE_CW;
        break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), BOM "*I%1u" EOM, smode);
    retval = xk852_send_command(rig, buf, strlen(buf));

    return retval;
}

/*
 * xk852_get_mode
 * Assumes rig!=NULL
 */
int
xk852_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    xk852_state state;
    int ret;

    ret = xk852_query_state(rig, &state);

    if (ret != RIG_OK)
    {
        return ret;
    }

    switch (state.mode)
    {
    case XK852_MODE_AME: *mode = RIG_MODE_AM; break;

    case XK852_MODE_USB: *mode = RIG_MODE_USB; break;

    case XK852_MODE_LSB: *mode = RIG_MODE_LSB; break;

    case XK852_MODE_CW: *mode = RIG_MODE_CW; break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int
xk852_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[64];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        if (val.f >= 0.5)
            SNPRINTF(buf, sizeof(buf), BOM "*S4" EOM)
            else if (val.f >= 0.1)
                SNPRINTF(buf, sizeof(buf), BOM "*S3" EOM)
                else if (val.f >= 0.001)
                    SNPRINTF(buf, sizeof(buf), BOM "*S2" EOM)
                    else
                    {
                        SNPRINTF(buf, sizeof(buf), BOM "*S1" EOM);
                    }

        break;

    case RIG_LEVEL_SQL:
        if (val.f <= 0.5)
            SNPRINTF(buf, sizeof(buf), BOM "*N0" EOM)
            else
            {
                SNPRINTF(buf, sizeof(buf), BOM "*N1" EOM);
            }

        break;

    case RIG_LEVEL_AGC:
    case RIG_LEVEL_AF:
        return -RIG_ENIMPL;

    default:
        return -RIG_EINVAL;
    }

    retval = xk852_send_command(rig, buf, strlen(buf));

    return retval;
}

int
xk852_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int ret;
    xk852_state state;

    ret = xk852_query_state(rig, &state);

    if (ret != RIG_OK)
    {
        return ret;
    }

    switch (level)
    {
    case RIG_LEVEL_SQL:
        switch (state.noise_blank)
        {
        case XK852_NOISE_BLANK_OFF:
            val->f = 1;
            return RIG_OK;
            break;

        case XK852_NOISE_BLANK_ON:
            val->f = 0;
            return RIG_OK;
            break;
        }

    case RIG_LEVEL_RFPOWER:
        switch (state.op_mode)
        {
        case XK852_OP_MODE_OFF:
            val->f = 0;
            return RIG_OK;
            break;

        case XK852_OP_MODE_RX:
            val->f = 0;
            return RIG_OK;
            break;

        case XK852_OP_MODE_TX_LOW:
            val->f = 0.099;
            return RIG_OK;
            break;

        case XK852_OP_MODE_TX_MID:
            val->f = 0.499;
            return RIG_OK;
            break;

        case XK852_OP_MODE_TX_FULL:
            val->f = 1;
            return RIG_OK;
            break;
        }

    default:
        return -RIG_ENIMPL;
        break;
    }

    return -RIG_EINVAL;
}

int
xk852_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval = 0;
    char cmd[32];

    int ptt_code = 2;

    switch (ptt)
    {
    case RIG_PTT_OFF:
        ptt_code = 2;
        break;

    case RIG_PTT_ON:
        ptt_code = 1;
        break;

    case RIG_PTT_ON_MIC:
        ptt_code = 1;
        break;

    case RIG_PTT_ON_DATA:
        ptt_code = 1;
        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    SNPRINTF(cmd, sizeof(cmd), BOM "*X%1d" EOM, ptt_code);
    retval = xk852_transaction(rig, cmd, strlen(cmd), NULL, NULL);
    return retval;
}

int
xk852_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    //    int retval = 0;
    //  int len;
    //  char buf[RESPSZ];
    //  char *cmd = BOM "X?" EOM;

    //    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    //    retval = xk852_transaction(rig, cmd, strlen(cmd), buf, &len);

    //    if (retval < 0)
    //    {
    //        return retval;
    //    }

    //    retval = (sscanf(buf, "%*cX%1u", ptt) == 1) ? RIG_OK : -RIG_EPROTO;

    *ptt = RIG_PTT_OFF;
    return RIG_OK;
}


/*
 * XK852 rig capabilities.
 */

struct rig_caps xk852_caps =
{
    RIG_MODEL(RIG_MODEL_XK852),
    .model_name = "XK852",
    .mfg_name = "Rohde&Schwarz",
    .version = BACKEND_VER ".0",
    .copyright = "LGPL",
    .status = RIG_STATUS_ALPHA,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    // Need to set RTS on for some reason
    // And HANDSHAKE_NONE even though HARDWARE is what is called for
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 9600, /* 7E1, RTS/CTS */
    .serial_data_bits = 7,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_EVEN,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 200, //nach senden warten // also see post_ptt_delay (in manpage)
    .timeout = 200,
    .retry = 3,
    .has_get_func = XK852_FUNC,
    .has_set_func = XK852_FUNC,
    .has_get_level = XK852_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(XK852_LEVEL_ALL),
    .has_get_parm = XK852_PARM_ALL,
    .has_set_parm = RIG_PARM_SET(XK852_PARM_ALL),
    .level_gran = {},
    .parm_gran = {},
    .ctcss_list = NULL,
    .dcs_list = NULL,
    .preamp = {RIG_DBLST_END},
    .attenuator = {RIG_DBLST_END},
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
    .targetable_vfo = 0,
    .transceive = RIG_TRN_RIG,
    .bank_qty = 0,
    .chan_desc_sz = 7,        /* FIXME */
    .vfo_ops = XK852_VFO_OPS,

    .chan_list = {
        {0, 99, RIG_MTYPE_MEM, XK852_MEM_CAP},
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        {
            kHz(1500), MHz(30), XK852_MODES, -1, -1, XK852_VFO,
            XK852_ANTS
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        {
            kHz(1500), MHz(30), XK852_MODES, 0, 150000, XK852_VFO,
            XK852_ANTS
        },
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {RIG_MODE_ALL, 10},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_WFM, kHz(150)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(15)},
        {XK852_MODES, kHz(2.4)},
        {XK852_MODES, kHz(1.5)},
        {XK852_MODES, Hz(150)},
        {XK852_MODES, Hz(300)},
        {XK852_MODES, Hz(600)},
        {XK852_MODES, kHz(6)},
        {XK852_MODES, kHz(9)},
        {XK852_MODES, kHz(15)},
        {XK852_MODES, kHz(30)},
        {XK852_MODES, kHz(50)},
        {XK852_MODES, kHz(120)},
        RIG_FLT_END,
    },
    .priv = NULL,

    .set_ptt = xk852_set_ptt,
    .get_ptt = xk852_get_ptt,
    .set_freq = xk852_set_freq,
    .get_freq = xk852_get_freq,
    .set_mode =  xk852_set_mode,
    .get_mode =  xk852_get_mode,
    .set_level =  xk852_set_level,
    .get_level =  xk852_get_level,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
