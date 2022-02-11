/*
 *  Hamlib AOR backend - SR2200 description
 *
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *
 *  Author: Stefano Speretta, Innovative Solutions In Space BV
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
#include <string.h>
#include <hamlib/rig.h>
#include "serial.h"
#include "idx_builtin.h"
#include "misc.h"
#include "aor.h"

#define SR2200_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define SR2200_FUNC_ALL ()

#define SR2200_LEVEL_GET (RIG_LEVEL_ATT | RIG_LEVEL_AGC | RIG_LEVEL_STRENGTH | RIG_LEVEL_AF | RIG_LEVEL_PREAMP )
#define SR2200_LEVEL_SET (RIG_LEVEL_ATT | RIG_LEVEL_AGC | RIG_LEVEL_AF | RIG_LEVEL_PREAMP)

#define SR2200_PARM (RIG_PARM_NONE)

#define SR2200_VFO_OPS (RIG_OP_MCL | RIG_OP_UP | RIG_OP_DOWN)
#define SR2200_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_PROG|RIG_SCAN_SLCT)

#define SR2200_VFO (RIG_VFO_A | RIG_VFO_B | RIG_VFO_C | RIG_VFO_N(3) | RIG_VFO_N(4) \
                    | RIG_VFO_N(5) | RIG_VFO_N(6) | RIG_VFO_N(7) | RIG_VFO_N(8) \
                    | RIG_VFO_N(9))

#define SR2200_PREAMP 10

/*   The data available on http://www.aoruk.com did not match very well on HF */
#define SR2200_STR_CAL { 16, { \
        {   0, -60 }, \
        {   3, -48 }, \
        {  14, -42 }, \
        {  26, -36 }, \
        {  34, -30 }, \
        {  42, -24 }, \
        {  52, -18 }, \
        {  62, -12 }, \
        {  74, -6 }, \
        {  87,  0 }, \
        { 101, 10 }, \
        { 117, 20 }, \
        { 135, 30 }, \
        { 152, 40 }, \
        { 202, 50 }, \
        { 255, 60 }, \
    } }


#define SR2200_MEM_CAP {    \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .bank_num = 1,  \
    .tuning_step = 1,   \
    .channel_desc = 1,  \
    .flags = 1, \
    .levels = RIG_LEVEL_ATT | RIG_LEVEL_AGC,    \
    .funcs = RIG_FUNC_ABM,    \
}

#define CR '\r'
#define LF '\n'
#define EOM "\r"

#define BUFSZ 256

static int sr2200_set_freq(RIG *rig, vfo_t vfo, freq_t freq);

static int parse_s2200_aor_mode(RIG *rig, char aormode, char aorwidth,
                                rmode_t *mode, pbwidth_t *width);

static int sr2200_transaction(RIG *rig, const char *cmd, int cmd_len,
                              char *data, int *data_len);

static int sr2200_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width);
static int sr2200_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);

static int sr2200_get_vfo(RIG *rig, vfo_t *vfo);
static int sr2200_set_vfo(RIG *rig, vfo_t vfo);

static int sr2200_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int sr2200_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

static const struct aor_priv_caps sr2200_priv_caps =
{
    .bank_base1 = '0',
    .bank_base2 = '0',
};

//  Derived from AOR AR8200 backend

/*
 * sr2200 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/
 *
 * TODO: retrieve BW info, and rest of commands
 */
const struct rig_caps sr2200_caps =
{
    RIG_MODEL(RIG_MODEL_SR2200),
    .model_name = "SR2200",
    .mfg_name =  "AOR",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_SCANNER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  19200,
    .serial_rate_max =  38400,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_XONXOFF,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  SR2200_LEVEL_GET,
    .has_set_level =  SR2200_LEVEL_SET,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,                /* FIXME: CTCSS list */
    .dcs_list =  NULL,
    .preamp =   { SR2200_PREAMP, RIG_DBLST_END, },
    .attenuator =   { 10, 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   10,
    .chan_desc_sz =  8,
    .vfo_ops =  SR2200_VFO_OPS,
    .scan_ops =  SR2200_SCAN_OPS,
    .str_cal = SR2200_STR_CAL,

    .chan_list =  {
        {   0,  999, RIG_MTYPE_MEM, SR2200_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {MHz(25), MHz(3000), SR2200_MODES, -1, -1, SR2200_VFO},
        RIG_FRNG_END,
    },

    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {MHz(25), MHz(3000), SR2200_MODES, -1, -1, SR2200_VFO},
        RIG_FRNG_END,
    },

    .tx_range_list2 =  { RIG_FRNG_END, },   /* no tx range, this is a scanner! */

    .tuning_steps =  {
        {SR2200_MODES, 1},
        {SR2200_MODES, 100},
        {SR2200_MODES, 500},
        {SR2200_MODES, kHz(1)},
        {SR2200_MODES, kHz(2)},
        {SR2200_MODES, kHz(5)},
        {SR2200_MODES, kHz(6.25)},
        {SR2200_MODES, kHz(9)},
        {SR2200_MODES, kHz(10)},
        {SR2200_MODES, kHz(12.5)},
        {SR2200_MODES, kHz(20)},
        {SR2200_MODES, kHz(25)},
        {SR2200_MODES, kHz(30)},
        {SR2200_MODES, kHz(50)},
        {SR2200_MODES, kHz(100)},
#if 0
        {SR2200_MODES, 0},  /* any tuning step */
#endif
        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(15)},
        {RIG_MODE_FM, kHz(15)},    /* narrow */
        {RIG_MODE_FM, kHz(6)},
        {RIG_MODE_WFM, kHz(300)}, /*  wide  */
        RIG_FLT_END,
    },

    .priv = (void *)& sr2200_priv_caps,

    .rig_init =  NULL,
    .rig_cleanup =  NULL,
    .rig_open =  NULL,
    .rig_close =  NULL,

    .set_freq =  sr2200_set_freq,
    .get_freq =  aor_get_freq,
    .set_mode =  sr2200_set_mode,
    .get_mode =  sr2200_get_mode,
    .set_vfo =  sr2200_set_vfo,
    .get_vfo =  sr2200_get_vfo,

    .set_level =  sr2200_set_level,
    .get_level =  sr2200_get_level,
    .get_dcd = aor_get_dcd,

    .set_ts =  aor_set_ts,
    .set_powerstat =  aor_set_powerstat,
    .vfo_op =  aor_vfo_op,
    .scan =  aor_scan,
    .get_info =  aor_get_info,

    .set_mem = aor_set_mem,
    .get_mem = aor_get_mem,
    .set_bank = aor_set_bank,

    .set_channel = aor_set_channel,
    .get_channel = aor_get_channel,

    .get_chan_all_cb = aor_get_chan_all_cb,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */


/*
 * modes in use by the "MD" command of SR2200
 */
#define SR2200_FM '0'
#define SR2200_WFM   '1'
#define SR2200_AM '2'
#define SR2200_SFM   '3'
#define SR2200_WAM   '4'

/*
 * sr2200_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 * TODO: error case handling
 */
static int sr2200_transaction(RIG *rig, const char *cmd, int cmd_len,
                              char *data, int *data_len)
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

    *data_len = retval;

    if (*data_len < BUFSZ)
    {
        data[*data_len] = '\0';
    }
    else
    {
        data[BUFSZ - 1] = '\0';
    }

    if (data[0] == '?')
    {
        /* command failed? resync with radio */
        write_block(&rs->rigport, (unsigned char *) EOM, 1);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}
/*
 * sr2200_set_freq
 * Assumes rig!=NULL
 */
int sr2200_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ], ackbuf[BUFSZ], *rfp;
    int freq_len, ret_freq_len;
    int retval;

    ret_freq_len = BUFSZ;

    if (freq < sr2200_caps.rx_range_list1[0].startf)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "Error in %s: frequency is lower than minimum supported value (%.0f Hz)\n",
                  __func__, sr2200_caps.rx_range_list1[0].startf);
        return -RIG_EPROTO;
    }

    if (freq > sr2200_caps.rx_range_list1[0].endf)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "Error in %s: frequency is higher than maximum supported value (%.0f Hz)\n",
                  __func__, sr2200_caps.rx_range_list1[0].endf);
        return -RIG_EPROTO;
    }

    SNPRINTF(freqbuf, sizeof(freqbuf), "RF%010.0f"EOM, freq);
    freq_len = strlen(freqbuf);

    strcpy(freqbuf + freq_len, EOM);
    freq_len += strlen(EOM);

    retval = sr2200_transaction(rig, freqbuf, freq_len, ackbuf, &ret_freq_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    rfp = strstr(ackbuf, "RF");

    if (!rfp)
    {
        rig_debug(RIG_DEBUG_WARN, "NO RF in returned string in %s: '%s'\n",
                  __func__, freqbuf);
        return -RIG_EPROTO;
    }

    sscanf(rfp + 2, "%"SCNfreq, &freq);

    return RIG_OK;

}

/*
 * sr2200_set_mode
 * Assumes rig!=NULL
 */
int sr2200_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[BUFSZ];
    int aormode, retval;
    pbwidth_t normal_width;

    normal_width = rig_passband_normal(rig, mode);

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = normal_width;
    }

    switch (mode)
    {
    case RIG_MODE_AM:
        aormode = width > normal_width ?
                  SR2200_WAM : SR2200_AM;
        break;

    case RIG_MODE_FM:
        aormode = width >= normal_width ?
                  SR2200_FM : SR2200_SFM;
        break;

    case RIG_MODE_WFM:
        aormode = SR2200_WFM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(mdbuf, sizeof(mdbuf), "MD%c" EOM,  aormode);
    retval = sr2200_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);

    return retval;
}

/*
 * sr2200_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int sr2200_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char ackbuf[BUFSZ], *mdp;
    int ack_len, retval;

    retval = sr2200_transaction(rig, "MD" EOM, 3, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    mdp = strstr(ackbuf, "MD");

    if (!mdp)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no MD in returned string: '%s'\n",
                  __func__, ackbuf);
        return -RIG_EPROTO;
    }

    retval = parse_s2200_aor_mode(rig, mdp[2], mdp[2], mode, width);

    return retval;
}

int parse_s2200_aor_mode(RIG *rig, char aormode, char aorwidth, rmode_t *mode,
                         pbwidth_t *width)
{
    switch (aormode)
    {
    case SR2200_FM:     *mode = RIG_MODE_FM; break;

    case SR2200_AM:     *mode = RIG_MODE_AM; break;

    case SR2200_SFM:    *mode = RIG_MODE_FM; break;

    case SR2200_WAM:    *mode = RIG_MODE_AM; break;

    case SR2200_WFM:    *mode = RIG_MODE_WFM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, aormode);
        return -RIG_EPROTO;
    }

    switch (aorwidth)
    {
    case SR2200_FM:     *width = s_kHz(15); break;

    case SR2200_AM:     *width = s_kHz(6); break;

    case SR2200_SFM:    *width = s_kHz(6); break;

    case SR2200_WAM:    *width = s_kHz(15); break;

    case SR2200_WFM:    *width = s_kHz(300); break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported width %d\n",
                  __func__, aorwidth);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * sr2200_set_vfo
 * Assumes rig!=NULL
 */
int sr2200_set_vfo(RIG *rig, vfo_t vfo)
{
    char *vfocmd;

    switch (vfo)
    {
    case RIG_VFO_A: vfocmd = "VA" EOM; break;

    case RIG_VFO_B: vfocmd = "VB" EOM; break;

    case RIG_VFO_C: vfocmd = "VC" EOM; break;

    case RIG_VFO_N(3): vfocmd = "VD" EOM; break;

    case RIG_VFO_N(4): vfocmd = "VE" EOM; break;

    case RIG_VFO_N(5): vfocmd = "VF" EOM; break;

    case RIG_VFO_N(6): vfocmd = "VG" EOM; break;

    case RIG_VFO_N(7): vfocmd = "VH" EOM; break;

    case RIG_VFO_N(8): vfocmd = "VI" EOM; break;

    case RIG_VFO_N(9): vfocmd = "VJ" EOM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "aor_set_vfo: unsupported vfo %s\n",
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    return sr2200_transaction(rig, vfocmd, strlen(vfocmd), NULL, NULL);
}


/*
 * sr2200_get_vfo
 * Assumes rig!=NULL, freq!=NULL
 */
int sr2200_get_vfo(RIG *rig, vfo_t *vfo)
{
    int vfo_len, retval;
    char vfobuf[BUFSZ];

    retval = sr2200_transaction(rig, "RX" EOM, 3, vfobuf, &vfo_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (vfobuf[1])
    {

    case 'A': *vfo = RIG_VFO_A; break;

    case 'B': *vfo = RIG_VFO_B; break;

    case 'C': *vfo = RIG_VFO_C; break;

    case 'D': *vfo = RIG_VFO_N(3); break;

    case 'E': *vfo = RIG_VFO_N(4); break;

    case 'F': *vfo = RIG_VFO_N(5); break;

    case 'G': *vfo = RIG_VFO_N(6); break;

    case 'H': *vfo = RIG_VFO_N(7); break;

    case 'I': *vfo = RIG_VFO_N(8); break;

    case 'J': *vfo = RIG_VFO_N(9); break;

    default:
        rig_debug(RIG_DEBUG_ERR, "aor_get_vfo: unknown vfo %c\n",
                  vfobuf[1]);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * sr2200_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int sr2200_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct rig_state *rs;
    char lvlbuf[BUFSZ];
    unsigned i;
    int agc;
    unsigned att = 0;

    rs = &rig->state;

    switch (level)
    {
    case RIG_LEVEL_AF:
        if (val.f > 255.0F)
        {
            SNPRINTF(lvlbuf, sizeof(lvlbuf), "AG255" EOM);
        }
        else
        {
            SNPRINTF(lvlbuf, sizeof(lvlbuf), "AG%03d" EOM, (int)val.f);
        }

        break;

    case RIG_LEVEL_PREAMP:
        if (val.f > 0)
        {
            SNPRINTF(lvlbuf, sizeof(lvlbuf), "AM1" EOM);
        }
        else
        {
            SNPRINTF(lvlbuf, sizeof(lvlbuf), "AM0" EOM);
        }

        break;

    case RIG_LEVEL_ATT:
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

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_FAST: agc = '1'; break;

        case RIG_AGC_MEDIUM: agc = '3'; break;

        case RIG_AGC_SLOW: agc = '2'; break;

        case RIG_AGC_OFF:
        default: agc = '0';
        }

        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AC%c" EOM, agc);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported aor_set_level %s\n", rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return sr2200_transaction(rig, lvlbuf, strlen(lvlbuf), NULL, NULL);
}

/*
 * sr2200_get_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 */
int sr2200_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct rig_state *rs;
    char lvlbuf[BUFSZ], ackbuf[BUFSZ];
    int ack_len, retval;

    rs = &rig->state;

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "LB" EOM);
        break;

    case RIG_LEVEL_ATT:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AT" EOM);
        break;

    case RIG_LEVEL_AGC:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AC" EOM);
        break;

    case RIG_LEVEL_AF:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AG" EOM);
        break;

    case RIG_LEVEL_PREAMP:
        SNPRINTF(lvlbuf, sizeof(lvlbuf), "AM" EOM);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    retval = sr2200_transaction(rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
        float tmp;

    case RIG_LEVEL_STRENGTH:
        if (ack_len < 7 || ackbuf[0] != 'L' || ackbuf[1] != 'B')
        {
            return -RIG_EPROTO;
        }

        sscanf(ackbuf + 3, "%d", &val->i);
        // calibrate the S-meter -> values should be referred to S9 (-73 dBm)
        val->i += 73;
        break;

    case RIG_LEVEL_ATT:
    {
        unsigned att;

        if (ack_len < 4 || ackbuf[0] != 'A' || ackbuf[1] != 'T')
        {
            return -RIG_EPROTO;
        }

        att = ackbuf[2] - '0';

        if (att == 0)
        {
            val->i = 0;
            break;
        }

        if (att > HAMLIB_MAXDBLSTSIZ || rs->attenuator[att - 1] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "Unsupported att %s %u\n",
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

        switch (ackbuf[2])
        {
        case '1': val->i = RIG_AGC_FAST; break;

        case '3': val->i = RIG_AGC_MEDIUM; break;

        case '2': val->i = RIG_AGC_SLOW; break;

        case '0':
        default: val->i = RIG_AGC_OFF;
        }

        break;

    case RIG_LEVEL_AF:
        if (ack_len < 3 || ackbuf[0] != 'A' || ackbuf[1] != 'G')
        {
            return -RIG_EPROTO;
        }

        sscanf(ackbuf + 2, "%f", &val->f);
        break;

    case RIG_LEVEL_PREAMP:
        if (ack_len < 3 || ackbuf[0] != 'A' || ackbuf[1] != 'M')
        {
            return -RIG_EPROTO;
        }

        sscanf(ackbuf + 2, "%f", &tmp);

        if (tmp != 0.0)
        {
            val->i = SR2200_PREAMP;
        }
        else
        {
            val->i = 0;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}
