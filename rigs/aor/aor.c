/*
 *  Hamlib AOR backend - main file
 *  Copyright (c) 2000-2010 by Stephane Fillod
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
#include <ctype.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "idx_builtin.h"

#include "aor.h"



/*
 * acknowledge is CR
 * Is \r portable enough?
 */
#define CR '\r'
#define EOM "\r"

#define BUFSZ 256

/*
 * modes in use by the "MD" command of AR8000 and AR8200
 */
#define AR8K_WFM    '0'
#define AR8K_NFM    '1'
#define AR8K_AM '2'
#define AR8K_USB    '3'
#define AR8K_LSB    '4'
#define AR8K_CW '5'
#define AR8K_SFM    '6'
#define AR8K_WAM    '7'
#define AR8K_NAM    '8'


/*
 * aor_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 * TODO: error case handling
 */
static int aor_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                           int *data_len)
{
    int retval;
    struct rig_state *rs;
    char ackbuf[BUFSZ];
    int ack_len;

    rs = &rig->state;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!data)
    {
        data = ackbuf;
    }

    if (!data_len)
    {
        data_len = &ack_len;
    }

    /*
     * Do wait for a reply
     */
    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, EOM,
                         strlen(EOM), 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    /* chop LF head when present */
    if (retval >= 1 && data[0] == '\x0a')
    {
        retval--;
        memmove(data, data + 1, retval);
    }

    *data_len = retval;

    if (*data_len < BUFSZ)
    {
        data[*data_len] = '\0';
    }
    else
    {
        data[BUFSZ - 1] = '\0';
    }

    if (retval >= 1 && data[0] == '?')
    {
        /* command failed? resync with radio */
        write_block(&rs->rigport, (unsigned char *) EOM, 1);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * aor_close
 * Assumes rig!=NULL
 */
int aor_close(RIG *rig)
{
    /*
     * terminate remote operation via the RS-232
     * Note: use write_block() instead of aor_transaction
     * since no reply is to be expected.
     */

    return write_block(&rig->state.rigport, (unsigned char *) "EX" EOM, 3);
}

static int format_freq(char *buf, int buf_len, freq_t freq)
{
    int lowhz;
    int64_t f = (int64_t)freq;

    /*
     * actually, frequency must be like nnnnnnnnm0,
     * where m must be 0 or 5 (for 50Hz).
     */
    lowhz = f % 100;
    f /= 100;

    if (lowhz < 25)
    {
        lowhz = 0;
    }
    else if (lowhz < 75)
    {
        lowhz = 50;
    }
    else
    {
        lowhz = 100;
    }

    f = f * 100 + lowhz;

    // cppcheck-suppress *
    SNPRINTF(buf, buf_len, "RF%010"PRIll, f);
    return strlen(buf);
}

/*
 * aor_set_freq
 * Assumes rig!=NULL
 */
int aor_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];
    int freq_len;

    freq_len = format_freq(freqbuf, sizeof(freqbuf), freq);
    strcpy(freqbuf + freq_len, EOM);
    freq_len += strlen(EOM);

    return aor_transaction(rig, freqbuf, freq_len, NULL, NULL);
}

/*
 * aor_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int aor_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char *rfp;
    int freq_len, retval;
    char freqbuf[BUFSZ];

    retval = aor_transaction(rig, "RX" EOM, 3, freqbuf, &freq_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    rfp = strstr(freqbuf, "RF");

    if (!rfp && rig->caps->rig_model == RIG_MODEL_AR8000)
    {
        rfp = strstr(freqbuf, "VA");
    }

    if (!rfp && rig->caps->rig_model == RIG_MODEL_AR8000)
    {
        rfp = strstr(freqbuf, "VB");
    }

    if (!rfp)
    {
        rig_debug(RIG_DEBUG_WARN, "NO RF in returned string in aor_get_freq: '%s'\n",
                  freqbuf);
        return -RIG_EPROTO;
    }

    sscanf(rfp + 2, "%"SCNfreq, freq);

    return RIG_OK;
}

/*
 * aor_set_vfo
 * Assumes rig!=NULL
 */
int aor_set_vfo(RIG *rig, vfo_t vfo)
{
    char *vfocmd;

    switch (vfo)
    {
    case RIG_VFO_VFO:
        if (rig->caps->rig_model == RIG_MODEL_AR8000)
        {
            vfocmd = "RF" EOM;
        }
        else
        {
            vfocmd = "VF" EOM;
        }

        break;

    case RIG_VFO_A: vfocmd = "VA" EOM; break;

    case RIG_VFO_B: vfocmd = "VB" EOM; break;

    case RIG_VFO_C: vfocmd = "VC" EOM; break;

    case RIG_VFO_N(3): vfocmd = "VD" EOM; break;

    case RIG_VFO_N(4): vfocmd = "VE" EOM; break;

    case RIG_VFO_MEM: vfocmd = "MR" EOM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "aor_set_vfo: unsupported vfo %d\n",
                  vfo);
        return -RIG_EINVAL;
    }

    return aor_transaction(rig, vfocmd, strlen(vfocmd), NULL, NULL);
}

/*
 * aor_get_vfo
 * Assumes rig!=NULL, freq!=NULL
 */
int aor_get_vfo(RIG *rig, vfo_t *vfo)
{
    int vfo_len, retval;
    char vfobuf[BUFSZ];

    retval = aor_transaction(rig, "RX" EOM, 3, vfobuf, &vfo_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (rig->caps->rig_model == RIG_MODEL_AR8000)
    {
        switch (vfobuf[0])
        {
        case 'S':
        case 'D':
            *vfo = RIG_VFO_VFO;
            break;

        case 'V':
            *vfo = RIG_VFO_N(vfobuf[4] - 'A');
            break;

        case 'M':
            *vfo = RIG_VFO_MEM;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "aor_get_vfo: unknown vfo %s\n",
                      vfobuf);
            return -RIG_EINVAL;
        }
    }
    else
    {
        switch (vfobuf[1])
        {
        case 'S':
        case 'V':
        case 'F': *vfo = RIG_VFO_VFO; break;

        case 'A': *vfo = RIG_VFO_A; break;

        case 'B': *vfo = RIG_VFO_B; break;

        case 'C': *vfo = RIG_VFO_C; break;

        case 'D': *vfo = RIG_VFO_N(3); break;

        case 'E': *vfo = RIG_VFO_N(4); break;

        case 'R': *vfo = RIG_VFO_MEM; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "aor_get_vfo: unknown vfo %c\n",
                      vfobuf[1]);
            return -RIG_EINVAL;
        }
    }

    return RIG_OK;
}

int format8k_mode(RIG *rig, char *buf, int buf_len,  rmode_t mode,
                  pbwidth_t width)
{
    int aormode;

    switch (mode)
    {
    case RIG_MODE_AM:
        if (rig->caps->rig_model == RIG_MODEL_AR8000)
        {
            aormode = AR8K_AM;
        }
        else
        {
            switch (width)
            {
            case RIG_PASSBAND_NORMAL:
            case s_kHz(9): aormode = AR8K_AM; break;

            case s_kHz(12): aormode = AR8K_WAM; break;

            case s_kHz(3): aormode = AR8K_NAM; break;

            case RIG_PASSBAND_NOCHANGE: aormode = AR8K_AM; break;

            default:
                rig_debug(RIG_DEBUG_ERR,
                          "%s: unsupported passband %s %d\n",
                          __func__,
                          rig_strrmode(mode), (int)width);
                return -RIG_EINVAL;
            }
        }

        break;

    case RIG_MODE_CW:       aormode = AR8K_CW; break;

    case RIG_MODE_USB:      aormode = AR8K_USB; break;

    case RIG_MODE_LSB:      aormode = AR8K_LSB; break;

    case RIG_MODE_WFM:      aormode = AR8K_WFM; break;

    case RIG_MODE_FM:
        if (rig->caps->rig_model == RIG_MODEL_AR8000)
        {
            aormode = AR8K_NFM;
        }
        else
        {
            switch (width)
            {
            case RIG_PASSBAND_NORMAL:
            case s_kHz(12): aormode = AR8K_NFM; break;

            case s_kHz(9): aormode = AR8K_SFM; break;

            case RIG_PASSBAND_NOCHANGE: aormode = AR8K_NFM; break;

            default:
                rig_debug(RIG_DEBUG_ERR,
                          "%s: unsupported passband %s %d\n",
                          __func__,
                          rig_strrmode(mode), (int)width);
                return -RIG_EINVAL;
            }
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, buf_len, "MD%c", aormode);
    return strlen(buf);
}

/*
 * aor_set_mode
 * Assumes rig!=NULL
 */
int aor_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    char mdbuf[9];
    char mdbuf2[16] = "";
    int mdbuf2_len, retval;

    if (priv->format_mode(rig, mdbuf, sizeof(mdbuf), mode, width) <= 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: format_mode=%s failed?\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    strcat(mdbuf, EOM);

    switch (rig->caps->rig_model)
    {
    case RIG_MODEL_AR5000:
    case RIG_MODEL_AR5000A:
        SNPRINTF(mdbuf2, sizeof(mdbuf2), "%.3s",
                 mdbuf);      /* Extract first 'MD' part */
        mdbuf2_len = strlen(mdbuf2);
        strcpy(mdbuf2 + mdbuf2_len, EOM); /* Add delimiter */
        mdbuf2_len = strlen(mdbuf2);

        retval = aor_transaction(rig, mdbuf2, mdbuf2_len, NULL, NULL);

        if (retval != RIG_OK) { return retval; }

        strncpy(mdbuf2, mdbuf + 4, 3); /* Extract first 'BW' part */
        mdbuf2[3] = '\0'; // in case strnpy produces and un-terminated string
        mdbuf2_len = strlen(mdbuf2);

        retval = aor_transaction(rig, mdbuf2, mdbuf2_len, NULL, NULL);

        if (retval != RIG_OK) { return retval; }

        break;

    default:
        retval = aor_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);
    }

    return retval;
}

/*
 * parse8k_aor_mode * don't care about aorwidth,
 * because there's no such BW command
 */
int parse8k_aor_mode(RIG *rig, char aormode, char aorwidth, rmode_t *mode,
                     pbwidth_t *width)
{
    *width = RIG_PASSBAND_NORMAL;

    switch (aormode)
    {
    case AR8K_AM:       *mode = RIG_MODE_AM; break;

    case AR8K_NAM:
        *mode = RIG_MODE_AM;
        *width = rig_passband_narrow(rig, *mode);
        break;

    case AR8K_WAM:
        *mode = RIG_MODE_AM;
        *width = rig_passband_wide(rig, *mode);
        break;

    case AR8K_CW:       *mode = RIG_MODE_CW; break;

    case AR8K_USB:  *mode = RIG_MODE_USB; break;

    case AR8K_LSB:  *mode = RIG_MODE_LSB; break;

    case AR8K_WFM:  *mode = RIG_MODE_WFM; break;

    case AR8K_NFM:  *mode = RIG_MODE_FM; break;

    case AR8K_SFM:
        *mode = RIG_MODE_FM;
        *width = rig_passband_narrow(rig, *mode);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, aormode);
        return -RIG_EINVAL;
    }

    if (*width == RIG_PASSBAND_NORMAL)
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return RIG_OK;
}


/*
 * aor_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int aor_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    char ackbuf[BUFSZ], ackbuf2[BUFSZ];
    char *mdp, *mdp2;
    int ack_len, ack2_len, retval;


    retval = aor_transaction(rig, "MD" EOM, 3, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * search MD, because on the AR5000, AU is also returned
     * by MD request
     */
    mdp = strstr(ackbuf, "MD");

    if (!mdp)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no MD in returned string: '%s'\n",
                  __func__, ackbuf);
        return -RIG_EPROTO;
    }

    if (rig->caps->rig_model == RIG_MODEL_AR5000 ||
            rig->caps->rig_model == RIG_MODEL_AR5000A)
    {
        retval = aor_transaction(rig, "BW" EOM, 3, ackbuf2, &ack2_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        mdp2 = strstr(ackbuf2, "BW");
    }
    else
    {
        mdp2 = mdp;
    }

    retval = priv->parse_aor_mode(rig, mdp[2], mdp2[2], mode, width);

    return retval;
}

/*
 * aor_set_ts
 * Assumes rig!=NULL
 */
int aor_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    char tsbuf[BUFSZ];

    /*
     * actually, tuning step must be like nnnnm0,
     * where m must be 0 or 5 (for 50Hz).
     */
    SNPRINTF(tsbuf, sizeof(tsbuf), "ST%06ld" EOM, ts);

    return aor_transaction(rig, tsbuf, strlen(tsbuf), NULL, NULL);
}


/*
 * aor_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int aor_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct rig_state *rs;
    char lvlbuf[BUFSZ];
    int agc;

    rs = &rig->state;


    switch (level)
    {
    case RIG_LEVEL_ATT:
    {
        unsigned att = 0;

        unsigned i;

        for (i = 0; i < HAMLIB_MAXDBLSTSIZ && !RIG_IS_DBLST_END(rs->attenuator[i]); i++)
        {
            if (rs->attenuator[i] == val.i)
            {
                att = i + 1;
                break;
            }
        }

        /* should be caught by the front end */
        if ((val.i != 0) && (i >= HAMLIB_MAXDBLSTSIZ
                             || RIG_IS_DBLST_END(rs->attenuator[i])))
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AT%u" EOM, att);
        break;
    }

    case RIG_LEVEL_AGC: /* AR5000 & AR5000A */
        switch (val.i)
        {
        case RIG_AGC_FAST: agc = '0'; break;

        case RIG_AGC_MEDIUM: agc = '1'; break;

        case RIG_AGC_SLOW: agc = '2'; break;

        case RIG_AGC_OFF:
        default: agc = 'F';
        }

        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AC%c" EOM, agc);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported aor_set_level %d\n", (int)level);
        return -RIG_EINVAL;
    }

    return aor_transaction(rig, lvlbuf, strlen(lvlbuf), NULL, NULL);
}

/*
 * aor_get_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 */
int aor_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct rig_state *rs;
    char lvlbuf[BUFSZ], ackbuf[BUFSZ];
    int ack_len, retval;

    rs = &rig->state;

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "LM" EOM);
        break;

    case RIG_LEVEL_ATT:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AT" EOM);
        break;

    case RIG_LEVEL_AGC: /* AR5000 & AR5000A */
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AC" EOM);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported %s %d\n", __func__, (int)level);
        return -RIG_EINVAL;
    }

    retval = aor_transaction(rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        if (ack_len < 4 || ackbuf[0] != 'L' || ackbuf[1] != 'M')
        {
            return -RIG_EPROTO;
        }

        if (rig->caps->rig_model == RIG_MODEL_AR8000)
        {
            sscanf(ackbuf + 2, "%x", &val->i);
            val->i &= ~0x80; /* mask squelch status */
        }
        else if (rig->caps->rig_model == RIG_MODEL_AR8200 ||
                 rig->caps->rig_model == RIG_MODEL_AR8600)
        {
            sscanf(ackbuf + 3, "%d", &val->i);
        }
        else
        {
            sscanf(ackbuf + 3, "%x", &val->i);
        }

        break;

    case RIG_LEVEL_ATT:
    {
        unsigned att;

        if (ack_len < 4 || ackbuf[0] != 'A' || ackbuf[1] != 'T')
        {
            return -RIG_EPROTO;
        }

        if (rig->caps->rig_model == RIG_MODEL_AR8000)
        {
            att = ackbuf[2] - '0';
        }
        else
        {
            att = ackbuf[3] - '0';
        }

        if (att == 0)
        {
            val->i = 0;
            break;
        }

        if (att > HAMLIB_MAXDBLSTSIZ || rs->attenuator[att - 1] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "Unsupported att %s %d\n",
                      __func__, att);
            return -RIG_EPROTO;
        }

        val->i = rs->attenuator[att - 1];
        break;
    }

    case RIG_LEVEL_AGC:
        if (ack_len < 3 || ackbuf[0] != 'A' || ackbuf[1] != 'C')
        {
            return -RIG_EPROTO;
        }

        if (rig->caps->rig_model == RIG_MODEL_AR5000 ||
                rig->caps->rig_model == RIG_MODEL_AR5000A)
        {
            /* AR5000A requires switching to be made on
            3rd returned character. SM6PPS */

            switch (ackbuf[2])
            {
            case '0': val->i = RIG_AGC_FAST; break;

            case '1': val->i = RIG_AGC_MEDIUM; break;

            case '2': val->i = RIG_AGC_SLOW; break;

            case 'F':
            default: val->i = RIG_AGC_OFF;
            }
        }
        else
        {
            /* Left the switching on 4th position in case
            models other than AR5000(A) use this. SM6PPS */

            switch (ackbuf[3])
            {
            case '0': val->i = RIG_AGC_FAST; break;

            case '1': val->i = RIG_AGC_MEDIUM; break;

            case '2': val->i = RIG_AGC_SLOW; break;

            case 'F':
            default: val->i = RIG_AGC_OFF;
            }
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported %s %d\n", __func__, (int)level);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * aor_get_dcd
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 */
int aor_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char ackbuf[BUFSZ];
    int  ack_len, retval;

    retval = aor_transaction(rig, "LM" EOM, 3, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len < 2 || ackbuf[0] != 'L' || ackbuf[1] != 'M')
    {
        return -RIG_EPROTO;
    }

    *dcd = ackbuf[2] == '%' ? RIG_DCD_OFF : RIG_DCD_ON;

    return RIG_OK;
}


/*
 * aor_set_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int aor_set_powerstat(RIG *rig, powerstat_t status)
{
    if (status == RIG_POWER_ON)
    {
        return aor_transaction(rig, "X" EOM, 2, NULL, NULL);
    }

    /* turn off power */
    return aor_transaction(rig, "QP" EOM, 3, NULL, NULL);
}

/*
 * aor_vfo_op
 * Assumes rig!=NULL
 */
int aor_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    char *aorcmd;

    switch (op)
    {
    case RIG_OP_UP: aorcmd = "\x1e" EOM; break;

    case RIG_OP_DOWN: aorcmd = "\x1f" EOM; break;

    case RIG_OP_RIGHT: aorcmd = "\x1c" EOM; break;

    case RIG_OP_LEFT: aorcmd = "\x1d" EOM; break;

    case RIG_OP_MCL: aorcmd = "MQ" EOM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "aor_vfo_op: unsupported op %d\n",
                  op);
        return -RIG_EINVAL;
    }

    return aor_transaction(rig, aorcmd, strlen(aorcmd), NULL, NULL);
}

/*
 * aor_scan, scan operation
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int aor_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    char *aorcmd;

    switch (scan)
    {
    case RIG_SCAN_STOP:

        /* Not sure how to stop the scanning.
         * Maye by going by to MEM/VFO mode?
         * Any clue? */
        if (vfo == RIG_VFO_CURR)
        {
            vfo = RIG_VFO_MEM;    /* supported by all the AOR rigs */
        }

        return rig_set_vfo(rig, vfo);

    case RIG_SCAN_MEM: aorcmd = "MS" EOM; break;

    case RIG_SCAN_SLCT: aorcmd = "SM" EOM; break;

    case RIG_SCAN_PROG: aorcmd = "VS" EOM; break; /* edges are VFO A & VFO B */

    case RIG_SCAN_VFO: aorcmd = "VV1" EOM;
        break; /* VFO scan mode, VV0 for 2-VFO mode */

    default:
        rig_debug(RIG_DEBUG_ERR, "aor_scan: unsupported scan %d\n",
                  scan);
        return -RIG_EINVAL;
    }

    return aor_transaction(rig, aorcmd, strlen(aorcmd), NULL, NULL);
}

/*
 * aor_set_mem
 * Assumes rig!=NULL
 */
int aor_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    char membuf[BUFSZ];
    int mem_num;
    char bank_base;

    /*
     * FIXME: we're assuming the banks are split 50/50.
     *  MW should be called the first time instead,
     *  and sizing memorized.
     */
    mem_num = ch % 100;

    if (mem_num >= 50 && priv->bank_base1 != priv->bank_base2)
    {
        bank_base = priv->bank_base2;
        mem_num -= 50;
    }
    else
    {
        bank_base = priv->bank_base1;
    }

    SNPRINTF(membuf, sizeof(membuf), "MR%c%02d" EOM,
             bank_base + ch / 100, mem_num);

    return aor_transaction(rig, membuf, strlen(membuf), NULL, NULL);
}

/*
 * aor_get_mem
 * Assumes rig!=NULL, freq!=NULL
 */
int aor_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    int mem_len, retval;
    char membuf[BUFSZ];

    retval = aor_transaction(rig, "MR" EOM, 3, membuf, &mem_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (membuf[0] == '?' || membuf[2] == '?')
    {
        return -RIG_ENAVAIL;
    }

    sscanf(membuf + 3, "%d", ch);

    /*
     * FIXME: we're assuming the banks are split 50/50.
     *  MW should be called the first time instead,
     *  and sizing memorized.
     */
    if (membuf[2] >= priv->bank_base2)
    {
        *ch += 100 * (membuf[2] - priv->bank_base2) + 50;
    }
    else
    {
        *ch += 100 * (membuf[2] - priv->bank_base1);
    }

    return RIG_OK;
}

/*
 * aor_set_bank
 * Assumes rig!=NULL
 */
int aor_set_bank(RIG *rig, vfo_t vfo, int bank)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    char membuf[BUFSZ];

    SNPRINTF(membuf, sizeof(membuf), "MR%c" EOM, (bank % 10) + (bank < 10 ?
             priv->bank_base1 : priv->bank_base2));

    return aor_transaction(rig, membuf, strlen(membuf), NULL, NULL);
}


int aor_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    char aorcmd[BUFSZ];
    int cmd_len;

    SNPRINTF(aorcmd, sizeof(aorcmd), "MX%c%02d ",
             chan->bank_num, chan->channel_num % 100);
    cmd_len = strlen(aorcmd);

    cmd_len += format_freq(aorcmd + cmd_len, sizeof(aorcmd) - cmd_len, chan->freq);

    /*
     * FIXME: automode
     */
    cmd_len += snprintf(aorcmd + cmd_len, sizeof(aorcmd) - cmd_len, " AU%d ST%06d ",
                        0, (int)chan->tuning_step);

    cmd_len += priv->format_mode(rig, aorcmd + cmd_len, sizeof(aorcmd) - cmd_len,
                                 chan->mode, chan->width);

    cmd_len += snprintf(aorcmd + cmd_len, sizeof(aorcmd) - cmd_len,
                        " AT%d TM%12s%s",
                        chan->levels[LVL_ATT].i ? 1 : 0, chan->channel_desc, EOM);

    return aor_transaction(rig, aorcmd, cmd_len, NULL, NULL);
}

static int parse_chan_line(RIG *rig, channel_t *chan, char *basep,
                           const channel_cap_t *mem_caps)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    char *tagp;
    int ts;

    /*
     * search for attribute tags in the line.
     * Using strstr enable support for various models
     * which may or may not have tag support.
     */

    tagp = strstr(basep, "---");

    if (tagp)
    {
        vfo_t vfo_save = chan->vfo;
        int ch_save = chan->channel_num;

        rig_debug(RIG_DEBUG_WARN, "%s: skipping, channel is empty: '%s'\n",
                  __func__, basep);

        memset(chan, 0, sizeof(channel_t));
        chan->vfo = vfo_save;
        chan->channel_num = ch_save;

        return -RIG_ENAVAIL;
    }

    /* bank_num */
    if (mem_caps->bank_num)
    {
        tagp = strstr(basep, "MX");

        if (!tagp)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: no MX in returned string: '%s'\n",
                      __func__, basep);
            return -RIG_EPROTO;
        }

        chan->bank_num = tagp[2] - (tagp[2] >= priv->bank_base2 ?
                                    priv->bank_base2 + 10 : priv->bank_base1);
    }

    /* pass */
    if (mem_caps->flags)
    {
        tagp = strstr(basep, "MP");

        if (!tagp)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: no MP in returned string: '%s'\n",
                      __func__, basep);
            return -RIG_EPROTO;
        }

        chan->flags = tagp[2] == '0' ? 0 : RIG_CHFLAG_SKIP;
    }

    /* frequency */
    if (mem_caps->freq)
    {
        tagp = strstr(basep, "RF");

        if (!tagp)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: no RF in returned string: '%s'\n",
                      __func__, basep);
            return -RIG_EPROTO;
        }

        sscanf(tagp + 2, "%"SCNfreq, &chan->freq);
    }

    /* channel desc */
    if (mem_caps->tuning_step)
    {
        tagp = strstr(basep, "ST");

        if (!tagp)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: no ST in returned string: '%s'\n",
                      __func__, basep);
            return -RIG_EPROTO;
        }

        ts = chan->tuning_step;
        sscanf(tagp + 2, "%d", &ts);
    }


    /* mode and width */
    if (mem_caps->mode && mem_caps->width)
    {
        int retval;
        char *tag2p;
        tagp = strstr(basep, "MD");

        if (!tagp && mem_caps->mode && mem_caps->width)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: no MD in returned string: '%s'\n",
                      __func__, basep);
            return -RIG_EPROTO;
        }

        /* "BW" only on AR5000 */
        tag2p = strstr(basep, "BW");

        if (!tag2p)
        {
            tag2p = tagp;
        }

        retval = priv->parse_aor_mode(rig, tagp[2], tag2p[2], &chan->mode,
                                      &chan->width);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    /* auto-mode */
    if (mem_caps->funcs & RIG_FUNC_ABM)
    {
        tagp = strstr(basep, "AU");

        if (!tagp)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: no AU in returned string: '%s'\n",
                      __func__, basep);
            return -RIG_EPROTO;
        }

        chan->funcs = tagp[2] == '0' ? 0 : RIG_FUNC_ABM;
    }


    /* attenuator */
    if (mem_caps->levels & LVL_ATT)
    {
        tagp = strstr(basep, "AT");

        if (!tagp)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: no AT in returned string: '%s'\n",
                      __func__, basep);
            return -RIG_EPROTO;
        }

        chan->levels[LVL_ATT].i = tagp[2] == '0' ? 0 :
                                  rig->caps->attenuator[tagp[2] - '0' - 1];
    }


    /* channel desc */
    if (mem_caps->channel_desc)
    {
        int i;

        tagp = strstr(basep, "TM");

        if (!tagp)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: no TM in returned string: '%s'\n",
                      __func__, basep);
            return -RIG_EPROTO;
        }

        strncpy(chan->channel_desc, tagp + 2, 12);
        chan->channel_desc[12] = '\0';

        /* chop off trailing spaces */
        for (i = 11; i > 0 && chan->channel_desc[i] == ' '; i--)
        {
            chan->channel_desc[i] = '\0';
        }

    }

    return RIG_OK;
}


int aor_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    char aorcmd[BUFSZ];
    int chan_len;
    char chanbuf[BUFSZ];
    int retval;
    channel_cap_t *mem_caps = NULL;
    chan_t *chan_list;
    int channel_num = chan->channel_num;

    chan_list = rig->caps->chan_list;

    if (chan->vfo == RIG_VFO_CURR)
    {
        /*
         * curr VFO mem_caps same as memory caps
         */
        mem_caps = &chan_list[0].mem_caps;
    }
    else
    {
        int mem_num;
        char bank_base;

        /*
         * find mem_caps in caps, we'll need it later
         */
        int i;

        for (i = 0; i < HAMLIB_CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++)
        {
            if (channel_num >= chan_list[i].startc &&
                    channel_num <= chan_list[i].endc)
            {
                mem_caps = &chan_list[i].mem_caps;
                break;
            }
        }

        if (!mem_caps)
        {
            return -RIG_EINVAL;
        }


        /*
         * FIXME: we're assuming the banks are split 50/50.
         *  MW should be called the first time instead,
         *  and sizing memorized.
         */
        mem_num = channel_num % 100;

        if (mem_num >= 50 && priv->bank_base1 != priv->bank_base2)
        {
            bank_base = priv->bank_base2;
            mem_num -= 50;
        }
        else
        {
            bank_base = priv->bank_base1;
        }

        SNPRINTF(aorcmd, sizeof(aorcmd), "MR%c%02d" EOM,
                 bank_base + channel_num / 100, mem_num);
        retval = aor_transaction(rig, aorcmd, strlen(aorcmd), chanbuf, &chan_len);

        /* is the channel empty? */
        if (retval == -RIG_EPROTO && chanbuf[0] == '?')
        {
            chan->freq = RIG_FREQ_NONE;
            return -RIG_ENAVAIL;
        }

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    SNPRINTF(aorcmd, sizeof(aorcmd), "RX" EOM);
    retval = aor_transaction(rig, aorcmd, strlen(aorcmd), chanbuf, &chan_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = parse_chan_line(rig, chan, chanbuf, mem_caps);

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        return -RIG_ENIMPL;
    }

    return retval;
}


#define LINES_PER_MA    10

int aor_get_chan_all_cb(RIG *rig, vfo_t vfo, chan_cb_t chan_cb, rig_ptr_t arg)
{
    struct aor_priv_caps *priv = (struct aor_priv_caps *)rig->caps->priv;
    int i, j, retval;
    chan_t *chan_list = rig->state.chan_list;
    channel_t *chan;
    int chan_count;
    char aorcmd[BUFSZ];
    int chan_len;
    char chanbuf[BUFSZ];
    int chan_next = chan_list[0].startc;


    chan_count = chan_list[0].endc - chan_list[0].startc + 1;

    /*
     * setting chan to NULL means the application
     * has to provide a struct where to store data
     * future data for channel channel_num
     */
    chan = NULL;
    retval = chan_cb(rig, vfo, &chan, chan_next, chan_list, arg);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (chan == NULL)
    {
        return -RIG_ENOMEM;
    }

    SNPRINTF(aorcmd, sizeof(aorcmd), "MA%c" EOM,
             priv->bank_base1);

    for (i = 0; i < chan_count / LINES_PER_MA; i++)
    {

        retval = aor_transaction(rig, aorcmd, strlen(aorcmd), chanbuf, &chan_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        for (j = 0; j < LINES_PER_MA; j++)
        {

            chan->vfo = RIG_VFO_MEM;
            chan->channel_num = i * LINES_PER_MA + j;

            retval = parse_chan_line(rig, chan, chanbuf, &chan_list[0].mem_caps);

            if (retval == -RIG_ENAVAIL)
            {
                retval = RIG_OK;
            }

            if (retval != RIG_OK)
            {
                return retval;
            }

            /* notify the end? */
            chan_next = chan_next < chan_list[i].endc ? chan_next + 1 : chan_next;

            /*
             * provide application with channel data,
             * and ask for a new channel structure
             */
            chan_cb(rig, vfo, &chan, chan_next, chan_list, arg);

            if (j >= LINES_PER_MA - 1)
            {
                break;
            }

            /*
             * get next line
             */
            retval = read_string(&rig->state.rigport, (unsigned char *) chanbuf, BUFSZ,
                                 EOM, strlen(EOM), 0, 1);

            if (retval < 0)
            {
                return retval;
            }
        }

        SNPRINTF(aorcmd, sizeof(aorcmd), "MA" EOM);
    }

    return RIG_OK;
}

/*
 * aor_get_info
 * Assumes rig!=NULL
 */
const char *aor_get_info(RIG *rig)
{
    static char infobuf[BUFSZ];
    int id_len, frm_len, retval;
    char idbuf[BUFSZ];
    char frmbuf[32]; // only expect 6 chars...please check

    retval = aor_transaction(rig, "\001" EOM, 2, idbuf, &id_len);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    if (retval > 2) { idbuf[2] = '\0'; }

    retval = aor_transaction(rig, "VR" EOM, 3, frmbuf, &frm_len);

    if (retval != RIG_OK || frm_len > 16)
    {
        return NULL;
    }

    frmbuf[frm_len] = '\0';
    SNPRINTF(infobuf, sizeof(infobuf), "Remote ID %c%c, Firmware version %s",
             idbuf[0], idbuf[1], frmbuf);

    return infobuf;
}


/*
 * initrigs_aor is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(aor)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&sr2200_caps);
    rig_register(&ar2700_caps);
    rig_register(&ar8200_caps);
    rig_register(&ar8000_caps);
    rig_register(&ar8600_caps);
    rig_register(&ar5000_caps);
    rig_register(&ar3000a_caps);
    rig_register(&ar7030_caps);
    rig_register(&ar3030_caps);
    rig_register(&ar5000a_caps);
    rig_register(&ar7030p_caps);

    return RIG_OK;
}

