/*
 *  Hamlib CI-V backend - wrapper to get/set alternate AGC levels
 *  Copyright (c) 2000-2025 by Stephane Fillod
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

#include "icom.h"
#include "icom_defs.h"
#include "icom_alt_agc.h"
#include "frame.h"
#include "misc.h"

/*
 * Note:
 * This file contains a wrapper to utilize the original AGC settings routines
 * for specific radios that require this alternate mapping.
 *
 * In order to make use of it, modify your rig's backend with the following:
 *
 * 1. Add `#include icom_alt_agc.h`
 *
 * 2. Change
 *    `.set_level = icom_set_level,`
 *    To
 *    `.set_level = icom_rig_set_level,`
 *
 * 3. Change
 *    `.get_level = icom_get_level,`
 *    To
 *    `.get_level = icom_rig_get_level,`
 *
 * All other level set/get routines other than AGC should be unaffected.
 */

static int icom_check_ack(int ack_len, unsigned char *ackbuf)
{
    if ((ack_len >= 1 && ackbuf[0] != ACK) && (ack_len >= 2 && ackbuf[1] != NAK))
    {
        // if we don't get ACK/NAK some serial corruption occurred
        // so we'll call it a timeout for retry purposes
        rig_debug(RIG_DEBUG_WARN, "%s: command timed out (%#.2x)\n", __func__,
                  ackbuf[0]);
        return -RIG_ETIMEOUT;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: command not acknowledged (%#.2x), len=%d\n",
                  __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

static int icom_set_cmd(RIG *rig, vfo_t vfo, struct cmdparams *par, value_t val)
{
    ENTERFUNC;

    unsigned char cmdbuf[MAXFRAMELEN];
    int cmdlen = 0;
    unsigned char ackbuf[MAXFRAMELEN];
    int ack_len = 0;

    if (!(par->submod & SC_MOD_WR)) { RETURNFUNC(-RIG_EINVAL); }

    if ((par->submod & SC_MOD_RW12) == SC_MOD_RW12)
    {
        cmdbuf[0] = 0x01;
        cmdlen = 1;
    }
    else
    {
        cmdlen = par->sublen;
        memcpy(cmdbuf, par->subext, cmdlen);
    }

    int wrd = val.i;
    int i;

    switch (par->dattyp)
    {
    case CMD_DAT_WRD:
        for (i = 1; i <= par->datlen; i++)
        {
            cmdbuf[cmdlen + par->datlen - i] = wrd & 0xff;
            wrd >>= 8;
        }

        break;

    case CMD_DAT_BUF:
        memcpy(&cmdbuf[cmdlen], val.b.d, par->datlen);
        break;

    case CMD_DAT_INT:
    case CMD_DAT_BOL:
        to_bcd_be(&cmdbuf[cmdlen], val.i, (par->datlen * 2));
        break;

    case CMD_DAT_FLT:
        to_bcd_be(&cmdbuf[cmdlen], (int) val.f, (par->datlen * 2));
        break;

    case CMD_DAT_LVL:
        to_bcd_be(&cmdbuf[cmdlen], (int)(val.f * 255.0), (par->datlen * 2));
        break;

    case CMD_DAT_TIM: // returned as seconds since midnight
        to_bcd_be(&cmdbuf[cmdlen],
                  ((((int)val.i / 3600) * 100) + (((int)val.i / 60) % 60)), (par->datlen * 2));
        break;

    default:
        break;
    }

    cmdlen += par->datlen;
    RETURNFUNC(icom_transaction(rig, par->command, par->subcmd, cmdbuf, cmdlen,
                                ackbuf,
                                &ack_len));
}

static int icom_get_cmd(RIG *rig, vfo_t vfo, struct cmdparams *par, value_t *val)
{

    ENTERFUNC;

    unsigned char ssc = 0x02;
    unsigned char resbuf[MAXFRAMELEN];
    int reslen = sizeof(resbuf);
    int retval;

    if (!(par->submod & SC_MOD_RD)) { RETURNFUNC(-RIG_EINVAL); }

    if ((par->submod & SC_MOD_RW12) == SC_MOD_RW12)
    {
        retval = icom_get_raw_buf(rig, par->command, par->subcmd, 1, &ssc, &reslen,
                                  resbuf);
    }
    else
    {
        retval = icom_get_raw_buf(rig, par->command, par->subcmd,
                                  par->sublen, (unsigned char *)par->subext, &reslen, resbuf);
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    switch (par->dattyp)
    {
    case CMD_DAT_WRD:
    {
        int wrd = 0;
        int i;

        for (i = 0; i < par->datlen; i++)
        {
            wrd = (wrd << 8) + resbuf[i];
        }

        val->i = wrd;
    }
    break;

    case CMD_DAT_STR:
        if (strlen(val->s) < reslen)
        {
            RETURNFUNC(-RIG_EINTERNAL);
        }

        memcpy(val->s, resbuf, reslen);
        val->s[reslen] = 0;
        break;

    case CMD_DAT_BUF:
        if (reslen > val->b.l)
        {
            RETURNFUNC(-RIG_EINTERNAL);
        }

        memcpy(val->b.d, resbuf, reslen);
        val->b.l = reslen;
        break;

    case CMD_DAT_INT:
        val->i = from_bcd_be(resbuf, (reslen * 2));
        break;

    case CMD_DAT_FLT:
        val->f = (float) from_bcd_be(resbuf, (reslen * 2));
        break;

    case CMD_DAT_LVL:
        val->f = (float) from_bcd_be(resbuf, (reslen * 2)) / 255.0;
        break;

    case CMD_DAT_BOL:
        val->i = (from_bcd_be(resbuf, (reslen * 2)) == 0) ? 0 : 1;
        break;

    case CMD_DAT_TIM:
        val->i = (from_bcd_be(resbuf, 2) * 3600) + (from_bcd_be(&resbuf[1], 2) * 60);
        break;

    default:
        val->i = 0;
        break;
    }

    RETURNFUNC(RIG_OK);
}

/*
 * icom_rig_set_level
 * rig-specific wrapper for setting alternate AGC values only
 * Assumes rig!=NULL, STATE(rig)->priv!=NULL
 */
int icom_rig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int retval;

    if (level != RIG_LEVEL_AGC)
    {
        retval = icom_set_level(rig, vfo, level, val);
        RETURNFUNC(retval);
    }
    else
    {
        unsigned char cmdbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
        int cmd_len, ack_len = sizeof(ackbuf);
        int lvl_cn, lvl_sc;       /* Command Number, Subcommand */
        int icom_val;
        int i;
        const struct icom_priv_caps *priv_caps =
            (const struct icom_priv_caps *) rig->caps->priv;

        ENTERFUNC;

        const struct cmdparams *extcmds = priv_caps->extcmds;

        for (i = 0; extcmds && extcmds[i].id.s != 0; i++)
        {
            if (extcmds[i].cmdparamtype == CMD_PARAM_TYPE_LEVEL && extcmds[i].id.s == level)
            {
              RETURNFUNC(icom_set_cmd(rig, vfo, (struct cmdparams *)&extcmds[i], val));
            }
        }

        /*
         * Many levels of float type are in [0.0..1.0] range
         */
        if (RIG_LEVEL_IS_FLOAT(level))
        {
            icom_val = val.f * 255;
        }
        else
        {
            icom_val = val.i;
        }

        /*
         * Most of the time, the data field is a 3 digit BCD,
         * but in *big endian* order: 0000..0255
         * (from_bcd is little endian)
         */
        cmd_len = 2;
        to_bcd_be(cmdbuf, (long long) icom_val, cmd_len * 2);

        lvl_cn = C_CTL_FUNC;
        lvl_sc = S_FUNC_AGC;
        cmd_len = 1;

        if (priv_caps->agc_levels_present)
        {
            int found = 0;

            for (i = 0;
                    i <= HAMLIB_MAX_AGC_LEVELS
                    && priv_caps->agc_levels[i].level != RIG_AGC_LAST; i++)
            {
                if (priv_caps->agc_levels[i].level == val.i)
                {
                    cmdbuf[0] = priv_caps->agc_levels[i].icom_level;
                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                RETURNFUNC(-RIG_EINVAL);
            }
        }
        else
        {
            // Legacy mapping that does not apply to all rigs
            switch (val.i)
            {
            case RIG_AGC_SUPERFAST:
                cmdbuf[0] = ICOM_AGC_SUPERFAST;
                break;

            case RIG_AGC_FAST:
                cmdbuf[0] = ICOM_AGC_FAST;
                break;

            case RIG_AGC_SLOW:
                cmdbuf[0] = ICOM_AGC_SLOW;
                break;

            case RIG_AGC_MEDIUM:
                cmdbuf[0] = ICOM_AGC_MID;
                break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unsupported LEVEL_AGC %d\n",
                          __func__, val.i);
                RETURNFUNC(-RIG_EINVAL);
            }
        }

        retval = icom_transaction(rig, lvl_cn, lvl_sc, cmdbuf, cmd_len, ackbuf,
                                  &ack_len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if ((retval = icom_check_ack(ack_len, ackbuf)) != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        RETURNFUNC(RIG_OK);
    }
}

/*
 * icom_rig_get_level
 * rig-specific wrapper for getting alternate AGC values only
 * Assumes rig!=NULL, STATE(rig)->priv!=NULL, val!=NULL
 *
 */
int icom_rig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval;

    if (level != RIG_LEVEL_AGC)
    {
        retval = icom_get_level(rig, vfo, level, val);
        RETURNFUNC(retval);
    }   
    else
    {
        unsigned char cmdbuf[MAXFRAMELEN], respbuf[MAXFRAMELEN];
        int cmd_len, resp_len;
        int lvl_cn, lvl_sc;       /* Command Number, Subcommand */
        int icom_val;
        int cmdhead;
        const struct icom_priv_caps *priv_caps =
            (const struct icom_priv_caps *) rig->caps->priv;

        ENTERFUNC;

        const struct cmdparams *extcmds = priv_caps->extcmds;
        int i;

        for (i = 0; extcmds && extcmds[i].id.s != 0; i++)
        {
            //rig_debug(RIG_DEBUG_TRACE, "%s: i=%d\n", __func__, i);

            if (extcmds[i].cmdparamtype == CMD_PARAM_TYPE_LEVEL && extcmds[i].id.s == level)
            {
                RETURNFUNC(icom_get_cmd(rig, vfo, (struct cmdparams *)&extcmds[i], val));
            }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: no extcmd found\n", __func__);

        cmdbuf[0] = 0x00;
        cmd_len = 0;

        lvl_cn = C_CTL_FUNC;
        lvl_sc = S_FUNC_AGC;

        /* use cmdbuf and cmd_len for 'set mode' subcommand */
        retval = icom_transaction(rig, lvl_cn, lvl_sc, cmdbuf, cmd_len, respbuf,
                                  &resp_len);

        if (retval != RIG_OK)
        {
        RETURNFUNC(retval);
        }

        /*
         * strbuf should contain Cn,Sc,Data area
         */
        //cmdhead = ((lvl_sc == -1) ? 1 : 2) + cmd_len;
        cmdhead = 2 + cmd_len;
        resp_len -= cmdhead;

        if (respbuf[0] != lvl_cn)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                      respbuf[0], resp_len);
            RETURNFUNC(-RIG_ERJCTED);
        }

        /*
         * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
         * (from_bcd is little endian)
         */
        icom_val = from_bcd_be(respbuf + cmdhead, resp_len * 2);

        if (priv_caps->agc_levels_present)
        {
            int found = 0;

            for (i = 0;
                    i <= HAMLIB_MAX_AGC_LEVELS && priv_caps->agc_levels[i].level >= 0; i++)
            {
                if (priv_caps->agc_levels[i].icom_level == icom_val)
                {
                    val->i = priv_caps->agc_levels[i].level;
                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: unexpected AGC 0x%02x\n", __func__,
                          icom_val);
                RETURNFUNC(-RIG_EPROTO);
            }
        }
        else
        {
            switch (icom_val)
            {
            case ICOM_AGC_SUPERFAST:
                val->i = RIG_AGC_SUPERFAST;
                break;

            case ICOM_AGC_FAST:
                 val->i = RIG_AGC_FAST;
                 break;

            case ICOM_AGC_SLOW:
                 val->i = RIG_AGC_SLOW;
                 break;

            case ICOM_AGC_MID:
                 val->i = RIG_AGC_MEDIUM;
                 break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unexpected AGC 0x%02x\n", __func__,
                          icom_val);
                RETURNFUNC(-RIG_EPROTO);
            }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: %d %d %d %f\n", __func__, resp_len,
                  icom_val, val->i, val->f);

        RETURNFUNC(RIG_OK);
    }
}

