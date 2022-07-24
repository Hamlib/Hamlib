/*
 *  Hamlib JRC backend - main file
 *  Copyright (c) 2001-2010 by Stephane Fillod
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
#include <string.h>  /* String function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"

#include "jrc.h"


/*
 * Carriage return
 */
#define EOM "\r"

#define BUFSZ 32

/*
 * modes in use by the "2G" command
 */
#define MD_RTTY '0'
#define MD_CW   '1'
#define MD_USB  '2'
#define MD_LSB  '3'
#define MD_AM   '4'
#define MD_FM   '5'
#define MD_AMS  '6'
#define MD_FAX  '6'
#define MD_ECSS_USB '7'
#define MD_ECSS_LSB '8'
#define MD_WFM  '9'


/*
 * jrc_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
int jrc_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                    int *data_len)
{
    int retval;
    struct rig_state *rs;

    rs = &rig->state;

    rig_flush(&rs->rigport);

    set_transaction_active(rig);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        set_transaction_inactive(rig);
        return retval;
    }

    if (!data || !data_len)
    {
        set_transaction_inactive(rig);
        return 0;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, EOM,
                         strlen(EOM), 0, 1);

    set_transaction_inactive(rig);

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}

static int jrc2rig_mode(RIG *rig, char jmode, char jwidth,
                        rmode_t *mode, pbwidth_t *width)
{
    switch (jmode)
    {
    case MD_RTTY:   *mode = RIG_MODE_RTTY; break;

    case MD_CW: *mode = RIG_MODE_CW; break;

    case MD_USB:    *mode = RIG_MODE_USB; break;

    case MD_LSB:    *mode = RIG_MODE_LSB; break;

    case MD_AM: *mode = RIG_MODE_AM; break;

    case MD_FM: *mode = RIG_MODE_FM; break;

    case MD_AMS:    if (rig->caps->rig_model == RIG_MODEL_NRD535)
        {
            *mode = RIG_MODE_FAX;
        }
        else
        {
            *mode = RIG_MODE_AMS;
        }

        break;

    case MD_ECSS_USB:   *mode = RIG_MODE_ECSSUSB; break;

    case MD_ECSS_LSB:   *mode = RIG_MODE_ECSSLSB; break;

    case MD_WFM:        *mode = RIG_MODE_WFM; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported mode %c\n",
                  __func__, jmode);
        *mode = RIG_MODE_NONE;
        return -RIG_EINVAL;
    }

    /*
     * determine passband
     */
    switch (jwidth)
    {
    case '0':
        *width = s_Hz(6000); //wide
        break;

    case '1':
        *width = s_Hz(2000); //inter
        break;

    case '2':
        *width = s_Hz(1000); //narr
        break;

    case '3':
        *width = s_Hz(12000); //aux - nrd535 only
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported width %c\n",
                  __func__, jwidth);
        *width = RIG_PASSBAND_NORMAL;
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int rig2jrc_mode(RIG *rig, rmode_t mode, pbwidth_t width,
                        char *jmode, char *jwidth)
{
    switch (mode)
    {
    case RIG_MODE_RTTY: *jmode = MD_RTTY; break;

    case RIG_MODE_CW:       *jmode = MD_CW; break;

    case RIG_MODE_USB:      *jmode = MD_USB; break;

    case RIG_MODE_LSB:      *jmode = MD_LSB; break;

    case RIG_MODE_AM:       *jmode = MD_AM; break;

    case RIG_MODE_FM:       *jmode = MD_FM; break;

    case RIG_MODE_AMS:  *jmode = MD_AMS; break;

    case RIG_MODE_FAX:  *jmode = MD_FAX; break;

    case RIG_MODE_ECSSUSB:  *jmode = MD_ECSS_USB; break;

    case RIG_MODE_ECSSLSB:  *jmode = MD_ECSS_LSB; break;

    case RIG_MODE_WFM:  *jmode = MD_WFM; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported mode %s\n", __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (RIG_PASSBAND_NOCHANGE == width)
    {
        *jwidth = '1';
        return RIG_OK;
    }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    if (width <= s_Hz(1500))
    {
        *jwidth = '2';    /*narr*/
    }
    else if (width <= s_Hz(4000))
    {
        *jwidth = '1';    /*inter*/
    }
    else if (width <= s_Hz(9000))
    {
        *jwidth = '0';    /*wide*/
    }
    else if (rig->caps->rig_model == RIG_MODEL_NRD535)
    {
        *jwidth = '3';    /*aux - nrd535 only*/
    }
    else
    {
        *jwidth = '1';    /*inter*/
    }

    return RIG_OK;
}


int jrc_open(RIG *rig)
{
    int retval;

    /*
     * Turning computer control ON,
     * Turn continuous mode on (for "I" query)
     */

    if (rig->caps->rig_model == RIG_MODEL_NRD535)
    {
        retval = jrc_transaction(rig, "H1" EOM, 3, NULL, NULL);
    }
    else
    {
        retval = jrc_transaction(rig, "H1" EOM "I1"EOM, 6, NULL, NULL);
    }

    return retval;
}

int jrc_close(RIG *rig)
{
    int retval;

    /* Turning computer control OFF */

    retval = jrc_transaction(rig, "H0" EOM, 3, NULL, NULL);
    return retval;
}


/*
 * jrc_set_freq
 * Assumes rig!=NULL
 */
int jrc_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    char freqbuf[BUFSZ];

    if (freq >= (freq_t)pow(10, priv->max_freq_len))
    {
        return -RIG_EINVAL;
    }

    // cppcheck-suppress *
    // suppressing bogus cppcheck error in ver 1.90
    SNPRINTF(freqbuf, sizeof(freqbuf), "F%0*"PRIll EOM, priv->max_freq_len,
             (int64_t)freq);

    return jrc_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);
}

static int get_current_istate(RIG *rig, char *buf, int *buf_len)
{
    int retval;

    /*
     * JRCs use "I" to get information,
     */
    if (rig->caps->rig_model == RIG_MODEL_NRD535)
    {
        retval = jrc_transaction(rig, "I1" EOM "I0" EOM, 6, buf, buf_len);
    }
    else
    {
        retval = jrc_transaction(rig, "I" EOM, 2, buf, buf_len);
    }

    return retval;
}

/*
 * jrc_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int jrc_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    int retval;
    char freqbuf[BUFSZ];
    int freq_len;

    //note: JRCs use "I" to get information
    retval = get_current_istate(rig, freqbuf, &freq_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    //I command returns Iabdffffffffg<CR>
    if (freqbuf[0] != 'I' || freq_len != priv->info_len)
    {
        rig_debug(RIG_DEBUG_ERR, "jrc_get_freq: wrong answer %s, "
                  "len=%d\n", freqbuf, freq_len);
        return -RIG_ERJCTED;
    }

    freqbuf[4 + priv->max_freq_len] = '\0';

    /* extract freq */
    sscanf(freqbuf + 4, "%"SCNfreq, freq);

    return RIG_OK;
}

/*
 * jrc_set_vfo
 * Assumes rig!=NULL
 */
int jrc_set_vfo(RIG *rig, vfo_t vfo)
{
    unsigned char cmdbuf[16];
    int retval;
    char vfo_function;

    switch (vfo)
    {
    case RIG_VFO_VFO: vfo_function = 'F'; break;

    case RIG_VFO_MEM: vfo_function = 'C'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "jrc_set_vfo: unsupported VFO %s\n",
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "%c" EOM, vfo_function);

    retval = jrc_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf), NULL,
                             NULL);

    return retval;
}


/*
 * jrc_set_mode
 * Assumes rig!=NULL
 */
int jrc_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[BUFSZ];
    int retval;
    char amode, awidth;

    retval = rig2jrc_mode(rig, mode, width, &amode, &awidth);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(mdbuf, sizeof(mdbuf), "D" "%c" EOM, amode);
    retval = jrc_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        SNPRINTF(mdbuf, sizeof(mdbuf), "B" "%c" EOM, awidth);
        retval = jrc_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return RIG_OK;
}

/*
 * jrc_get_mode
 * Assumes rig!=NULL, mode!=NULL, width!=NULL
 */
int jrc_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    int md_len, retval;
    char mdbuf[BUFSZ];
    char cmode;
    char cwidth;

    //note: JRCs use "I" to get information
    retval = get_current_istate(rig, mdbuf, &md_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    //I command returns Iabdffffffffg<CR>
    if (mdbuf[0] != 'I' || md_len != priv->info_len)
    {
        rig_debug(RIG_DEBUG_ERR, "jrc_get_mode: wrong answer %s, "
                  "len=%d\n", mdbuf, md_len);
        return -RIG_ERJCTED;
    }

    /* extract width and mode */
    cwidth = mdbuf[2];
    cmode = mdbuf[3];

    retval = jrc2rig_mode(rig, cmode, cwidth,
                          mode, width);

    return retval;
}

/*
 * jrc_set_func
 * Assumes rig!=NULL
 */
int jrc_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmdbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (func)
    {
    case RIG_FUNC_FAGC:
        /* FIXME: FAGC levels */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "G%d" EOM, status ? 1 : 2);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_FUNC_NB:
        /* FIXME: NB1 and NB2 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "N%d" EOM, status ? 1 : 0);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    /*
     * FIXME: which BB mode for NR and BC at same time ?
     */
    case RIG_FUNC_NR:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "BB%d" EOM, status ? 1 : 0);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_FUNC_BC:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "BB%d" EOM, status ? 2 : 0);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_FUNC_LOCK:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "DD%d" EOM, status ? 1 : 0);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_FUNC_MN:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "EE%d" EOM, status ? 1 : 0);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_func %s\n", rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * jrc_get_func
 * Assumes rig!=NULL, status!=NULL
 */
int jrc_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    int retval, func_len;
    char funcbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (func)
    {
    case RIG_FUNC_FAGC:
        /* FIXME: FAGC levels */
        //retval = jrc_transaction (rig, "G" EOM, 2, funcbuf, &func_len);
        retval = get_current_istate(rig, funcbuf, &func_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        //if (func_len != 3 || func_len != 6) {
        if (funcbuf[0] != 'I' || func_len != priv->info_len)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_func: wrong answer %s, "
                      "len=%d\n", funcbuf, func_len);
            return -RIG_ERJCTED;
        }

        //*status = funcbuf[1] != '2';
        *status = funcbuf[4 + priv->max_freq_len] != '2';

        return RIG_OK;

    case RIG_FUNC_NB:
        /* FIXME: NB1 and NB2 */
        retval = jrc_transaction(rig, "N" EOM, 2, funcbuf, &func_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (func_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_func: wrong answer %s, "
                      "len=%d\n", funcbuf, func_len);
            return -RIG_ERJCTED;
        }

        *status = funcbuf[1] != '0';

        return RIG_OK;

    /*
     * FIXME: which BB mode for NR and BC at same time ?
     */
    case RIG_FUNC_NR:
        retval = jrc_transaction(rig, "BB" EOM, 3, funcbuf, &func_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (func_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_func: wrong answer %s, "
                      "len=%d\n", funcbuf, func_len);
            return -RIG_ERJCTED;
        }

        *status = funcbuf[2] == '1';

        return RIG_OK;

    case RIG_FUNC_BC:
        retval = jrc_transaction(rig, "BB" EOM, 3, funcbuf, &func_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (func_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_func: wrong answer %s, "
                      "len=%d\n", funcbuf, func_len);
            return -RIG_ERJCTED;
        }

        *status = funcbuf[2] == '2';

        return RIG_OK;

    case RIG_FUNC_LOCK:
        retval = jrc_transaction(rig, "DD" EOM, 3, funcbuf, &func_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (func_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_func: wrong answer %s, "
                      "len=%d\n", funcbuf, func_len);
            return -RIG_ERJCTED;
        }

        *status = funcbuf[1] == '1';

        return RIG_OK;

    case RIG_FUNC_MN:
        retval = jrc_transaction(rig, "EE" EOM, 3, funcbuf, &func_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (func_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_func: wrong answer %s, "
                      "len=%d\n", funcbuf, func_len);
            return -RIG_ERJCTED;
        }

        *status = funcbuf[1] == '1';

        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_func %s\n", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * jrc_set_level
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sens though)
 */
int jrc_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    char cmdbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level)
    {
    case RIG_LEVEL_ATT:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "A%d" EOM, val.i ? 1 : 0);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_RF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "HH%03d" EOM, (int)(val.f * 255.0));

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_AF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "JJ%03d" EOM, (int)(val.f * 255.0));

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_SQL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "LL%03d" EOM, (int)(val.f * 255.0));

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_NR:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "FF%03d" EOM, (int)(val.f * 255.0));

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

#if 0

    case RIG_LEVEL_TONE:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "KK%03d" EOM, (int)(val.f * 255.0));

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
#endif

    case RIG_LEVEL_NOTCHF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "GG%+04d" EOM, val.i);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

#if 0

    case RIG_LEVEL_BWC:
        if (priv->pbs_len == 3)
        {
            val.i /= 10;
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "W%0*d" EOM, priv->pbs_len,  val.i);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
#endif

    case RIG_LEVEL_AGC:
        if (val.i < 10)
        {
            SNPRINTF(cmdbuf, sizeof(cmdbuf), "G%d" EOM,
                     val.i == RIG_AGC_SLOW ? 0 :
                     val.i == RIG_AGC_FAST ? 1 : 2);
        }
        else
        {
            SNPRINTF(cmdbuf, sizeof(cmdbuf), "G3%03d" EOM, val.i / 20);
        }

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_CWPITCH:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "%s%+05d" EOM, priv->cw_pitch, val.i);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_IF:
        if (priv->pbs_len == 3)
        {
            val.i /= 10;
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "P%+0*d" EOM, priv->pbs_len + 1, val.i);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * jrc_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int jrc_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    int retval, lvl_len, lvl;
    char lvlbuf[BUFSZ];
    char cwbuf[BUFSZ];
    int cw_len;

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        /* read A/D converted value */
        retval = jrc_transaction(rig, "M" EOM, 2, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        lvlbuf[4] = '\0';
        val->i = atoi(lvlbuf + 1);
        break;

    case RIG_LEVEL_STRENGTH:
        /* read calibrated A/D converted value */
        retval = jrc_transaction(rig, "M" EOM, 2, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        lvlbuf[4] = '\0';
        val->i = (int)rig_raw2val(atoi(lvlbuf + 1), &rig->caps->str_cal);
        break;

    case RIG_LEVEL_ATT:
        retval = get_current_istate(rig, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'I' || lvl_len != priv->info_len)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        val->i = lvlbuf[1] == '1' ? 20 : 0;
        break;

    case RIG_LEVEL_AGC:
        retval = get_current_istate(rig, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'I' || lvl_len != priv->info_len)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        lvlbuf[priv->info_len - 1] = '\0';

        if (priv->info_len == 14)
        {
            switch (lvlbuf[priv->info_len - 2])
            {
            case '0' : val->i = RIG_AGC_SLOW; break;

            case '1' : val->i = RIG_AGC_FAST; break;

            case '2' : val->i = RIG_AGC_OFF; break;

            default : val->i = RIG_AGC_FAST;
            }
        }
        else
        {
            val->i = atoi(lvlbuf + priv->info_len - 4);
        }

        break;

    case RIG_LEVEL_RF:
        retval = jrc_transaction(rig, "HH" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        /*
         * 000..255
         */
        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = (float)lvl / 255.0;
        break;

    case RIG_LEVEL_AF:
        retval = jrc_transaction(rig, "JJ" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        /*
         * 000..255
         */
        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = (float)lvl / 255.0;
        break;

    case RIG_LEVEL_SQL:
        retval = jrc_transaction(rig, "LL" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        /*
         * 000..255
         */
        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = (float)lvl / 255.0;
        break;

    case RIG_LEVEL_NR:
        retval = jrc_transaction(rig, "FF" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        /*
         * 000..255
         */
        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = (float)lvl / 255.0;
        break;

#if 0

    case RIG_LEVEL_TONE:
        retval = jrc_transaction(rig, "KK" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%u", &lvl);
        val->f = (float)lvl / 255.0;
        break;
#endif

    case RIG_LEVEL_NOTCHF:
        retval = jrc_transaction(rig, "GG" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 8)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        /*
         * 000..255
         */
        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = (float)lvl / 255.0;
        break;

#if 0

    case RIG_LEVEL_BWC:
        retval = jrc_transaction(rig, "W" EOM, 2, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'W' || lvl_len != priv->pbs_len + 2)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 1, "%d", &lvl);

        if (priv->pbs_len == 3)
        {
            lvl *= 10;
        }

        val->i = lvl;
        break;
#endif

    case RIG_LEVEL_CWPITCH:
        SNPRINTF(cwbuf, sizeof(cwbuf), "%s" EOM, priv->cw_pitch);
        cw_len = strlen(cwbuf);

        retval = jrc_transaction(rig, cwbuf, strlen(cwbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != cw_len + 5)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + (cw_len - 1), "%05d", &lvl);
        val->i = lvl;
        break;

    case RIG_LEVEL_IF:
        retval = jrc_transaction(rig, "P" EOM, 2, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'P' || lvl_len != priv->pbs_info_len)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_level: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 1, "%d", &lvl);

        if (priv->pbs_len == 3)
        {
            lvl *= 10;
        }

        val->i = lvl;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * jrc_set_parm
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sens though)
 */
int jrc_set_parm(RIG *rig, setting_t parm, value_t val)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    char cmdbuf[BUFSZ];
    int minutes;

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (parm)
    {
    case RIG_PARM_BACKLIGHT:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "AA%d" EOM, val.f > 0.5 ? 0 : 1);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_PARM_BEEP:

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "U%0*d" EOM, priv->beep_len,
                 (priv->beep + val.i) ? 1 : 0);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_PARM_TIME:
        minutes = val.i / 60;
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "R1%02d%02d" EOM,
                 minutes / 60, minutes % 60);

        return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_parm %s\n", __func__,
                  rig_strparm(parm));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * jrc_get_parm
 * Assumes rig!=NULL, val!=NULL
 */
int jrc_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    int retval, lvl_len, i;
    char lvlbuf[BUFSZ];
    char cmdbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (parm)
    {
    case RIG_PARM_TIME:
        retval = jrc_transaction(rig, "R0" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* "Rhhmmss"CR */
        if (lvl_len != 8)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_parm: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        /* convert ASCII to numeric 0..9 */
        for (i = 1; i < 7; i++)
        {
            lvlbuf[i] -= '0';
        }

        val->i = ((10 * lvlbuf[1] + lvlbuf[2]) * 60 + /* hours */
                  10 * lvlbuf[3] + lvlbuf[4]) * 60 + /* minutes */
                 10 * lvlbuf[5] + lvlbuf[6]; /* secondes */
        break;

    case RIG_PARM_BEEP:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "U%d" EOM, priv->beep / 10);
        retval = jrc_transaction(rig, cmdbuf, strlen(cmdbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != priv->beep_len + 2)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_parm: wrong answer"
                      "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        val->i = lvlbuf[priv->beep_len] == 0 ? 0 : 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_parm %s\n", __func__,
                  rig_strparm(parm));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * jrc_get_dcd
 * Assumes rig!=NULL, dcd!=NULL
 */
int jrc_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char dcdbuf[BUFSZ];
    int dcd_len, retval;

    retval = jrc_transaction(rig, "Q" EOM, 2, dcdbuf, &dcd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (dcd_len != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "jrc_get_dcd: wrong answer %s, "
                  "len=%d\n", dcdbuf, dcd_len);
        return -RIG_ERJCTED;
    }

    *dcd = dcdbuf[1] == '0' ? RIG_DCD_ON : RIG_DCD_OFF;

    return RIG_OK;
}

/*
 * jrc_set_trn
 * Assumes rig!=NULL
 */
int jrc_set_trn(RIG *rig, int trn)
{
    char *trncmd;

    /* transceive(continuous) mode not available in remote mode
     * so switch back and forth upon entering/leaving
     */
    trncmd = trn == RIG_TRN_RIG ? "H0"EOM"I1"EOM : "H1"EOM"I1"EOM;

    return jrc_transaction(rig, trncmd, 6, NULL, NULL);
}

/*
 * jrc_set_powerstat
 * Assumes rig!=NULL
 */
int jrc_set_powerstat(RIG *rig, powerstat_t status)
{
    char pwrbuf[BUFSZ];

    SNPRINTF(pwrbuf, sizeof(pwrbuf), "T%d" EOM, status == RIG_POWER_ON ? 1 : 0);

    return jrc_transaction(rig, pwrbuf, strlen(pwrbuf), NULL, NULL);
}

/*
 * jrc_get_powerstat
 * Assumes rig!=NULL
 */
int jrc_get_powerstat(RIG *rig, powerstat_t *status)
{
    char pwrbuf[BUFSZ];
    int pwr_len, retval;

    if (rig->caps->rig_model == RIG_MODEL_NRD535)
    {
        retval = jrc_transaction(rig, "T" EOM, 2, pwrbuf, &pwr_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (pwr_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "jrc_get_powerstat: wrong answer %s, "
                      "len=%d\n", pwrbuf, pwr_len);
            return -RIG_ERJCTED;
        }

        *status = pwrbuf[1] == '0' ? RIG_POWER_OFF : RIG_POWER_ON;

        return RIG_OK;
    }
    else
    {
        retval = jrc_transaction(rig, "I" EOM, 2, pwrbuf, &pwr_len);

        *status = retval != RIG_OK ? RIG_POWER_OFF : RIG_POWER_ON;

        return retval;
    }

}

/*
 * jrc_reset
 * Assumes rig!=NULL
 */
int jrc_reset(RIG *rig, reset_t reset)
{
    char rstbuf[BUFSZ];
    char rst;

    switch (reset)
    {
    case RIG_RESET_MCALL: rst = '1'; break; /* mem clear */

    case RIG_RESET_VFO: rst = '2'; break;   /* user setup default */

    case RIG_RESET_MASTER: rst = '3'; break; /* 1 + 2 */

    default:
        rig_debug(RIG_DEBUG_ERR, "jrc_reset: unsupported reset %d\n",
                  reset);
        return -RIG_EINVAL;
    }

    SNPRINTF(rstbuf, sizeof(rstbuf), "Z%c" EOM, rst);

    return jrc_transaction(rig, rstbuf, strlen(rstbuf), NULL, NULL);
}

/*
 * jrc_set_mem
 * Assumes rig!=NULL
 */
int jrc_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char cmdbuf[BUFSZ];
    char membuf[BUFSZ];
    int mem_len;

    if (ch < 0 || ch > rig->caps->chan_list[0].endc)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "C%03d" EOM, ch);

    /* don't care about the Automatic response from receiver */

    return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), membuf, &mem_len);
}

/*
 * jrc_get_mem
 * Assumes rig!=NULL
 */
int jrc_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    int mem_len, retval;
    char membuf[BUFSZ];
    int chan;

    retval = jrc_transaction(rig, "L" EOM, 2, membuf, &mem_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* need to handle vacant memories LmmmV<cr>, len = 6 */
    if ((mem_len != priv->mem_len) && (mem_len != 6))
    {
        rig_debug(RIG_DEBUG_ERR, "jrc_get_mem: wrong answer %s, "
                  "len=%d\n", membuf, mem_len);
        return -RIG_ERJCTED;
    }

    membuf[4] = '\0';

    /*extract current channel*/
    sscanf(membuf + 1, "%d", &chan);
    *ch = chan;

    return RIG_OK;
}

/*
 * jrc_set_chan
 * Assumes rig!=NULL
 */
int jrc_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    char    cmdbuf[BUFSZ];
    int retval;
    rmode_t mode;
    pbwidth_t width;
    channel_t current;

    /* read first to get current values */
    current.channel_num = chan->channel_num;

    if ((retval = jrc_get_chan(rig, vfo, &current, 1)) != RIG_OK) { return retval; }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "K%03d000", chan->channel_num);

    if (chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i == 20)
    {
        cmdbuf[4] = '1';
    }

    mode = chan->mode;
    width = chan->width;

    if (RIG_MODE_NONE == mode) { mode = current.mode; }

    if (RIG_PASSBAND_NOCHANGE == width) { width = current.width; }

    retval = rig2jrc_mode(rig, mode, width, &cmdbuf[6], &cmdbuf[5]);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmdbuf + 7, sizeof(cmdbuf) - 7, "%0*"PRIll, priv->max_freq_len,
             (int64_t)chan->freq);

    if (priv->mem_len == 17)
    {
        switch (chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i)
        {
        case RIG_AGC_SLOW : cmdbuf[priv->mem_len - 2] = '0'; break;

        case RIG_AGC_FAST : cmdbuf[priv->mem_len - 2] = '1'; break;

        case RIG_AGC_OFF  : cmdbuf[priv->mem_len - 2] = '2'; break;

        default : cmdbuf[priv->mem_len - 2] = '1';
        }
    }
    else
    {
        SNPRINTF(cmdbuf + priv->mem_len - 4, sizeof(cmdbuf) - (priv->mem_len - 4),
                 "%03d",
                 chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i);
    }

    return jrc_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
}

/*
 * jrc_get_chan
 * Assumes rig!=NULL
 */
int jrc_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    char    membuf[BUFSZ], cmdbuf[BUFSZ];
    int     mem_len, retval;

    chan->vfo = RIG_VFO_MEM;
    chan->ant = RIG_ANT_NONE;
    chan->freq = 0;
    chan->mode = RIG_MODE_NONE;
    chan->width = RIG_PASSBAND_NORMAL;
    chan->tx_freq = 0;
    chan->tx_mode = RIG_MODE_NONE;
    chan->tx_width = RIG_PASSBAND_NORMAL;
    chan->split = RIG_SPLIT_OFF;
    chan->tx_vfo = RIG_VFO_NONE;
    chan->rptr_shift = RIG_RPT_SHIFT_NONE;
    chan->rptr_offs = 0;
    chan->tuning_step = 0;
    chan->rit = 0;
    chan->xit = 0;
    chan->funcs = 0;
    chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_OFF;
    chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 0;
    chan->ctcss_tone = 0;
    chan->ctcss_sql = 0;
    chan->dcs_code = 0;
    chan->dcs_sql = 0;
    chan->scan_group = 0;
    chan->flags = RIG_CHFLAG_SKIP;
    strcpy(chan->channel_desc, "");

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "L%03d%03d" EOM, chan->channel_num,
             chan->channel_num);
    retval = jrc_transaction(rig, cmdbuf, strlen(cmdbuf), membuf, &mem_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* need to handle vacant memories LmmmV<cr>, len = 6 */
    if ((mem_len != priv->mem_len) && (mem_len != 6))
    {
        rig_debug(RIG_DEBUG_ERR, "jrc_get_mem: wrong answer %s, "
                  "len=%d\n", membuf, mem_len);
        return -RIG_ERJCTED;
    }

    if (mem_len != 6)
    {
        char freqbuf[BUFSZ];

        if (membuf[4] == '1')
        {
            chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 20;
        }

        jrc2rig_mode(rig, membuf[6], membuf[5],
                     &chan->mode, &chan->width);

        strncpy(freqbuf, membuf + 7, priv->max_freq_len);
        freqbuf[priv->max_freq_len] = 0x00;
        chan->freq = strtol(freqbuf, NULL, 10);

        if (priv->mem_len == 17)
        {
            switch (membuf[priv->mem_len - 2])
            {
            case '0' : chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_SLOW; break;

            case '1' : chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_FAST; break;

            case '2' : chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_OFF; break;

            default : chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_FAST;
            }
        }
        else
        {
            strncpy(freqbuf, membuf + priv->mem_len - 4, 3);
            chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = strtol(freqbuf, NULL, 10);
        }
    }

    return RIG_OK;
}

/*
 * jrc_vfo_op
 * Assumes rig!=NULL
 */
int jrc_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    const char *cmd;

    switch (op)
    {
    case RIG_OP_FROM_VFO: cmd = "E1" EOM; break;

    case RIG_OP_UP: cmd = "MM25" EOM; break;

    case RIG_OP_DOWN: cmd = "MM24" EOM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "jrc_vfo_op: unsupported op %#x\n",
                  op);
        return -RIG_EINVAL;
    }

    return jrc_transaction(rig, cmd, strlen(cmd), NULL, NULL);
}


/*
 * jrc_scan, scan operation
 * Assumes rig!=NULL
 *
 * Not really a scan operation so speaking.
 * You just make the rig increment frequency of decrement continuously,
 * depending on the sign of ch.
 * However, using DCD sensing, followed by a stop, you get it.
 */
int jrc_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    const char *scan_cmd = "";

    switch (scan)
    {
    case RIG_SCAN_STOP:
        scan_cmd = "Y0" EOM;
        break;

    case RIG_SCAN_SLCT:
        scan_cmd = ch > 0 ? "Y+" EOM : "Y-" EOM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported scan %#x", scan);
        return -RIG_EINVAL;
    }

    return jrc_transaction(rig, scan_cmd, 3, NULL, NULL);
}

/*
 * jrc_decode is called by sa_sigio, when some asynchronous
 * data has been received from the rig
 */
int jrc_decode_event(RIG *rig)
{
    struct jrc_priv_caps *priv = (struct jrc_priv_caps *)rig->caps->priv;
    struct rig_state *rs;
    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
    int count;
    char buf[BUFSZ];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: jrc_decode called\n", __func__);

    rs = &rig->state;

    /* "Iabdfg"CR */
    //#define SETUP_STATUS_LEN 17

    //count = read_string(&rs->rigport, buf, SETUP_STATUS_LEN, "", 0);
    count = read_string(&rs->rigport, (unsigned char *) buf, priv->info_len, "", 0,
                        0, 1);

    if (count < 0)
    {
        return count;
    }

    buf[31] = '\0'; /* stop run away.. */

    if (buf[0] != 'I')
    {
        rig_debug(RIG_DEBUG_WARN, "jrc: unexpected data: %s\n",
                  buf);
        return -RIG_EPROTO;
    }

    /*
     * TODO: Attenuator and AGC change notification.
     */

    if (rig->callbacks.freq_event)
    {

        //buf[14] = '\0'; /* side-effect: destroy AGC first digit! */
        buf[4 + priv->max_freq_len] = '\0'; /* side-effect: destroy AGC first digit! */
        sscanf(buf + 4, "%"SCNfreq, &freq);
        return rig->callbacks.freq_event(rig, RIG_VFO_CURR, freq,
                                         rig->callbacks.freq_arg);
    }

    if (rig->callbacks.mode_event)
    {
        jrc2rig_mode(rig, buf[3], buf[2], &mode, &width);
        return rig->callbacks.mode_event(rig, RIG_VFO_CURR, mode, width,
                                         rig->callbacks.freq_arg);
    }

    return RIG_OK;
}


/*
 * initrigs_jrc is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(jrc)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&nrd535_caps);
    rig_register(&nrd545_caps);
    rig_register(&nrd525_caps);
    rig_register(&jst145_caps);
    rig_register(&jst245_caps);

    return RIG_OK;
}

