/*
 *  Hamlib Kenwood backend - TH handheld primitives
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
 *	$Id: th.c,v 1.36 2009-01-23 03:24:42 n0nb Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "kenwood.h"
#include "th.h"
#include "serial.h"
#include "misc.h"

const struct kenwood_priv_caps  th_priv_caps  = {
    .cmdtrm =  EOM_TH,
};
/* Note: Currently the code assumes the command termination is a
 * single character.
 */

#define EOM "\r" /* XXX */


#define ACKBUF_LEN  64

/*
 * th_decode_event is called by sa_sigio, when some asynchronous
 * data has been received from the rig.
 */
int
th_decode_event (RIG *rig)
{
    char asyncbuf[128];
    int retval;
    size_t async_len=128;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = kenwood_transaction(rig, NULL, 0, asyncbuf, &async_len);
    if (retval != RIG_OK)
        return retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: Decoding message\n", __FUNCTION__);

    /* --------------------------------------------------------------------- */
    if (async_len> 3 && asyncbuf[0] == 'B' && asyncbuf[1] == 'U' && asyncbuf[2] == 'F') {

        vfo_t vfo;
        freq_t freq, offset;
        int mode;
        int step, shift, rev, tone, ctcss, tonefq, ctcssfq;

        retval = sscanf(asyncbuf, "BUF %d,%"SCNfreq",%X,%d,%d,%d,%d,,%d,,%d,%"SCNfreq",%d",
                                  &vfo, &freq, &step, &shift, &rev, &tone,
                                  &ctcss, &tonefq, &ctcssfq, &offset, &mode);

        if (retval != 11) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected BUF message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }

        /* Calibration and conversions */
        vfo = (vfo == 0) ? RIG_VFO_A : RIG_VFO_B;
        mode = (mode == 0) ? RIG_MODE_FM : RIG_MODE_AM;

        rig_debug(RIG_DEBUG_TRACE, "%s: Buffer (vfo %d, freq %"PRIfreq" Hz, mode %d)\n", __FUNCTION__, vfo, freq, mode);

        /* Callback execution */
        if (rig->callbacks.vfo_event) {
            rig->callbacks.vfo_event(rig, vfo, rig->callbacks.vfo_arg);
        }
        if (rig->callbacks.freq_event) {
            rig->callbacks.freq_event(rig, vfo, freq, rig->callbacks.freq_arg);
        }
        if (rig->callbacks.mode_event) {
            rig->callbacks.mode_event(rig, vfo, mode, RIG_PASSBAND_NORMAL,
							rig->callbacks.mode_arg);
        }

    /* --------------------------------------------------------------------- */
    } else if (async_len> 2 && asyncbuf[0] == 'S' && asyncbuf[1] == 'M') {

        vfo_t vfo;
        int lev;
        retval = sscanf(asyncbuf, "SM %d,%d", &vfo, &lev);
        if (retval != 2) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected SM message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }

        /* Calibration and conversions */
        vfo = (vfo == 0) ? RIG_VFO_A : RIG_VFO_B;

        rig_debug(RIG_DEBUG_TRACE, "%s: Signal strength event - signal = %.3f\n", __FUNCTION__, (float)(lev / 5.0));

        /* Callback execution */
#if STILLHAVETOADDCALLBACK
        if (rig->callbacks.strength_event)
            rig->callbacks.strength_event(rig, vfo,(float)(lev / 5.0),
							rig->callbacks.strength_arg);
#endif

    /* --------------------------------------------------------------------- */
    } else if (async_len > 2 && asyncbuf[0] == 'B' && asyncbuf[1] == 'Y') {

        vfo_t vfo;
        int busy;

        retval = sscanf(asyncbuf, "BY %d,%d", &vfo, &busy);
        if (retval != 2) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected BY message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }
        vfo = (vfo == 0) ? RIG_VFO_A : RIG_VFO_B;
        rig_debug(RIG_DEBUG_TRACE, "%s: Busy event - status = '%s'\n",
				__FUNCTION__, (busy == 0) ? "OFF" : "ON" );
        return -RIG_ENIMPL;
        /* This event does not have a callback. */

    /* --------------------------------------------------------------------- */
    } else if (async_len > 2 && asyncbuf[0] == 'B' && asyncbuf[1] == 'C') {

        vfo_t vfo;
        retval = sscanf(asyncbuf, "BC %d", &vfo);
        if (retval != 1) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected BC message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }
        vfo = (vfo == 0) ? RIG_VFO_A : RIG_VFO_B;

        rig_debug(RIG_DEBUG_TRACE, "%s: VFO event - vfo = %d\n", __FUNCTION__, vfo);
        if (rig->callbacks.vfo_event)
            rig->callbacks.vfo_event(rig, vfo, rig->callbacks.vfo_arg);

    /* --------------------------------------------------------------------- */
    } else {

        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported transceive cmd '%s'\n", __FUNCTION__, asyncbuf);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

/*
 * th_set_freq
 * Assumes rig!=NULL
 */
int
th_set_freq (RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[ACKBUF_LEN], ackbuf[ACKBUF_LEN];
    int retval, step;
    size_t ack_len;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

      if(vfo!=RIG_VFO_CURR) {
        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_ENTARGET;
	}

    /* Step needs to be at least 10kHz on higher band */
    step = freq >= MHz(470) ? 4 : 1;
    sprintf(freqbuf, "FQ %011"PRIll",%X"EOM, (long long)freq, step);
    ack_len = ACKBUF_LEN;
    retval = kenwood_transaction(rig, freqbuf, strlen(freqbuf), ackbuf, &ack_len);
    if (retval != RIG_OK)
        return retval;

    return RIG_OK;
}

/*
 * th_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int
th_get_freq (RIG *rig, vfo_t vfo, freq_t *freq)
{
    char freqbuf[24], ackbuf[ACKBUF_LEN];
    int retval, step;
    size_t ack_len=ACKBUF_LEN;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

      if(vfo!=RIG_VFO_CURR) {
        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_ENTARGET;
	}

    *freq = 0;

    sprintf(freqbuf, "FQ"EOM);
    retval = kenwood_transaction (rig, freqbuf, strlen(freqbuf), ackbuf, &ack_len);
    if (retval != RIG_OK)
        return retval;

    retval = sscanf(ackbuf, "FQ %"SCNfreq",%x",freq,&step);
    if (retval != 2) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, freqbuf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * th_set_mode
 * Assumes rig!=NULL
 */
int
th_set_mode (RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char kmode, mdbuf[24], ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len;
    const struct kenwood_priv_caps *priv=(const struct kenwood_priv_caps *)rig->caps->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

      if(vfo!=RIG_VFO_CURR) {
        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_ENTARGET;
	}

    if (priv->mode_table)
    {
	    kmode = rmode2kenwood(mode, priv->mode_table);
	    if (kmode == -1) {
            	rig_debug(RIG_DEBUG_WARN, "%s: Unsupported Mode value '%s'\n",
				__FUNCTION__, rig_strrmode(mode));
            	return -RIG_EINVAL;
	    }
	    kmode += '0';
    }
    else
    {
	switch (mode) {
    	case RIG_MODE_FM:   kmode = '0'; break; /* TH-D7A(G) modes */
	case RIG_MODE_AM:   kmode = '1'; break;
	default:
        rig_debug(RIG_DEBUG_ERR,"%s: Unsupported Mode %d\n", __FUNCTION__, mode);
		return -RIG_EINVAL;
	}
    }

    sprintf(mdbuf, "MD %c" EOM, kmode);
    ack_len = ACKBUF_LEN;
    retval = kenwood_transaction (rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);
    if (retval != RIG_OK)
        return retval;

    return RIG_OK;
}

/*
 * th_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int
th_get_mode (RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char vch, ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len=ACKBUF_LEN;
    const struct kenwood_priv_caps *priv=(const struct kenwood_priv_caps *)rig->caps->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

      if(vfo!=RIG_VFO_CURR) {
        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_ENTARGET;
	}

    vch = '0';

    retval = kenwood_transaction (rig, "MD" EOM, 3, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

    if (strlen(ackbuf) < 4 || ackbuf[3] < '0' || ackbuf[3] > '9') {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

    if (priv->mode_table)
    {
	    *mode = kenwood2rmode(ackbuf[3]-'0', priv->mode_table);
	    if (*mode == RIG_MODE_NONE) {
            	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Mode (table)value '%c'\n",
				__FUNCTION__, ackbuf[3]);
            	return -RIG_EINVAL;
	    }
    }
    else
    {
	switch (ackbuf[3]) {
        case '0': *mode = RIG_MODE_FM; break;     /* TH-D7A(G) modes */
        case '1': *mode = RIG_MODE_AM; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Mode value '%c'\n", __FUNCTION__, ackbuf[3]);
            return -RIG_EINVAL;
	}
    }

    if (width)
        *width = RIG_PASSBAND_NORMAL;

    return RIG_OK;
}

/*
 * th_set_vfo
 * Assumes rig!=NULL
 */
int
th_set_vfo (RIG *rig, vfo_t vfo)
{
    char vfobuf[16], ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	switch (vfo) {
        case RIG_VFO_A:
            sprintf(vfobuf, "VMC 0,0" EOM);
            break;
        case RIG_VFO_B:
            sprintf(vfobuf, "VMC 1,0" EOM);
            break;
        case RIG_VFO_VFO:
            sprintf(vfobuf, "VMC 0,0" EOM);
            break;
        case RIG_VFO_MEM:
    	    if (rig->caps->rig_model == RIG_MODEL_THF7E)
            	sprintf(vfobuf, "VMC 0,1" EOM);
	    else
            	sprintf(vfobuf, "VMC 0,2" EOM);
            break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
            return -RIG_EVFO;
	}

    ack_len = ACKBUF_LEN;
    retval = kenwood_transaction(rig, vfobuf, strlen(vfobuf), ackbuf, &ack_len);
	if (retval != RIG_OK)
        return retval;

	switch (vfo) {
        case RIG_VFO_A:
            sprintf(vfobuf, "BC 0" EOM);
            break;
        case RIG_VFO_B:
            sprintf(vfobuf, "BC 1" EOM);
            break;
	default:
		return RIG_OK;
	}

    ack_len = ACKBUF_LEN;
    retval = kenwood_transaction(rig, vfobuf, strlen(vfobuf), ackbuf, &ack_len);
	if (retval != RIG_OK)
        return retval;

    return RIG_OK;
}

/*
 * th_get_vfo
 * Assumes rig!=NULL
 */
int
th_get_vfo (RIG *rig, vfo_t *vfo)
{
    char cmdbuf[16], ackbuf[ACKBUF_LEN],vfoc;
    int retval;
    size_t ack_len;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    ack_len=ACKBUF_LEN;
    retval = kenwood_transaction (rig, "BC" EOM, 3, ackbuf, &ack_len);
    if (retval != RIG_OK)
        return retval;

    if (ack_len < 4 ) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

    vfoc=ackbuf[3];
    switch (vfoc) {
        case '0': *vfo = RIG_VFO_A; break;
        case '1': *vfo = RIG_VFO_B; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VFO value '%c'\n", __FUNCTION__, ackbuf[3]);
            return -RIG_EVFO;
    }

    sprintf(cmdbuf,"VMC %c" EOM,vfoc);
    ack_len=ACKBUF_LEN;
    retval = kenwood_transaction (rig, cmdbuf, strlen(cmdbuf), ackbuf, &ack_len);
    if (retval != RIG_OK)
        return retval;

    if (ack_len < 8 ) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }
    switch(ackbuf[6]) {
	case '0' :
	case '1' :
		break;
	case '2' :
		*vfo = RIG_VFO_MEM;
		break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VFO value '%c'\n", __FUNCTION__, ackbuf[6]);
            return -RIG_EVFO;
     }

     return RIG_OK;
}


/*
 * th_set_trn
 * Assumes rig!=NULL
 */
int
th_set_trn(RIG *rig, int trn)
{
    char trnbuf[16], ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    sprintf(trnbuf, "AI %c" EOM, (trn == RIG_TRN_RIG) ? '1' : '0');
    ack_len = ACKBUF_LEN;
    retval = kenwood_transaction (rig, trnbuf, strlen(trnbuf), ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

/*
 * th_get_trn
 * Assumes rig!=NULL
 */
int
th_get_trn (RIG *rig, int *trn)
{
    char ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len=ACKBUF_LEN;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = kenwood_transaction(rig, "AI" EOM, 3, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

    if (ack_len < 4 ) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
	}

    *trn = (ackbuf[3] != '0') ? RIG_TRN_RIG : RIG_TRN_OFF;

	return RIG_OK;
}


/*
 * th_get_kenwood_func
 * Assumes rig!=NULL, status!=NULL
 */
static int th_get_kenwood_func (RIG *rig, const char *cmd, int *status)
{
    char ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len=ACKBUF_LEN;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = kenwood_transaction (rig, cmd, strlen(cmd), ackbuf, &ack_len);
    if (retval != RIG_OK)
        return retval;

    if (ack_len < 4 ) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
	}

    if (strlen(cmd) == 4) {
        *status = (ackbuf[4] == '0') ? 0 : 1;
    } else {
        *status = (ackbuf[3] == '0') ? 0 : 1;
    }
	return RIG_OK;
};

/*
 * th_get_func
 * Assumes rig!=NULL, status!=NULL
 */
int
th_get_func (RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called (0x%04x)\n", __FUNCTION__, func);

    /* FIXME: What about the VFO? */

    switch (func) {
        case RIG_FUNC_MON:
            return th_get_kenwood_func(rig, "MON" EOM, status);
        case RIG_FUNC_TONE:
            return th_get_kenwood_func(rig, "TO" EOM, status);
        case RIG_FUNC_TSQL:
            return th_get_kenwood_func(rig, "CT" EOM, status);
        case RIG_FUNC_REV:
            return th_get_kenwood_func(rig, "REV" EOM, status);
        case RIG_FUNC_ARO:
            return th_get_kenwood_func(rig, "ARO" EOM, status);
        case RIG_FUNC_AIP:
            return th_get_kenwood_func(rig, "AIP" EOM, status);
        case RIG_FUNC_LOCK:
            return th_get_kenwood_func(rig, "LK" EOM, status);
        default:
            rig_debug(RIG_DEBUG_ERR,"%s: Unsupported function %#x", __FUNCTION__, func);
            return -RIG_EINVAL;
    }
    return RIG_OK;
}

/* --------------------------------------------------------------------- */
static int th_tburst(RIG *rig, vfo_t vfo, int status)
{
    char ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len;

    ack_len = ACKBUF_LEN;
    if(status==1) {
    retval = kenwood_transaction(rig, "TT"EOM, 3, ackbuf, &ack_len);
        if (retval != RIG_OK)
        	return retval;

	return RIG_OK;
    }
    if(status==0) {
	return rig_set_ptt(rig,vfo,RIG_PTT_OFF);
    }
    return -RIG_EINVAL;
}

/*
 * th_set_kenwood_func
 * Assumes rig!=NULL, status!=NULL
 */
static int th_set_kenwood_func (RIG *rig, const char *cmd, int status)
{
    char trbuf[16], ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    strncpy(trbuf,cmd,16);
    strncat(trbuf,(status)?" 1":" 0",15);
    strncat(trbuf,EOM,16);
    ack_len = ACKBUF_LEN;
    retval = kenwood_transaction (rig, cmd, strlen(cmd), ackbuf, &ack_len);
    if (retval != RIG_OK)
        return retval;

    if (ack_len < 4 ) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
	}
	return RIG_OK;
};


/*
 * th_get_func
 * Assumes rig!=NULL, status!=NULL
 */
int
th_set_func (RIG *rig, vfo_t vfo, setting_t func, int status)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called (0x%04x)\n", __FUNCTION__, func);

    /* FIXME: What about the VFO? */

    switch (func) {
        case RIG_FUNC_MON:
            return th_set_kenwood_func(rig, "MON", status);
        case RIG_FUNC_TONE:
            return th_set_kenwood_func(rig, "TO", status);
        case RIG_FUNC_TSQL:
            return th_set_kenwood_func(rig, "CT", status);
        case RIG_FUNC_REV:
            return th_set_kenwood_func(rig, "REV", status);
        case RIG_FUNC_ARO:
            return th_set_kenwood_func(rig, "ARO", status);
        case RIG_FUNC_AIP:
            return th_set_kenwood_func(rig, "AIP", status);
        case RIG_FUNC_LOCK:
            return th_set_kenwood_func(rig, "LK", status);
        case RIG_FUNC_TBURST:
            return th_tburst(rig, vfo, status);
        default:
            rig_debug(RIG_DEBUG_ERR,"%s: Unsupported function %#x", __FUNCTION__, func);
            return -RIG_EINVAL;
    }
    return RIG_OK;
}

/*
 * th_get_parm
 * Assumes rig!=NULL, status!=NULL
 */
int
th_get_parm (RIG *rig, setting_t parm, value_t *val)
{
    int status;
    int ret;

    rig_debug(RIG_DEBUG_TRACE, "%s: called (0x%04x)\n", __FUNCTION__, parm);

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (parm) {
        case RIG_PARM_BACKLIGHT:
            ret = th_get_kenwood_func(rig, "LMP" EOM, &status);
	    val->f = status ? 1.0 : 0;
            return ret;

        default:
            rig_debug(RIG_DEBUG_ERR,"%s: Unsupported parm %#x", __FUNCTION__, parm);
            return -RIG_EINVAL;
    }
    return RIG_OK;
}


/*
 * th_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int
th_get_level (RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char vch, lvlbuf[32], ackbuf[ACKBUF_LEN];
    int retval, v, l;
    size_t ack_len=ACKBUF_LEN;
    vfo_t tvfo;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	if(vfo==RIG_VFO_CURR) tvfo=rig->state.current_vfo;
	else tvfo=vfo;

	switch (tvfo) {
        case RIG_VFO_A:
        case RIG_VFO_VFO:
        case RIG_VFO_MEM:
		 vch = '0'; break;
        case RIG_VFO_B: 
		 vch = '1'; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
            return -RIG_EVFO;
	}

    switch (level) {
        case RIG_LEVEL_RAWSTR:
            sprintf(lvlbuf, "SM %c" EOM, vch);
            retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
			if (retval != RIG_OK)
				return retval;

            retval = sscanf(ackbuf, "SM %d,%d", &v, &l);
            if (retval != 2 || l < rig->caps->level_gran[LVL_RAWSTR].min.i || l > rig->caps->level_gran[LVL_RAWSTR].max.i) {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
                return -RIG_ERJCTED;
            }

            val->i = l;
	    break;

        case RIG_LEVEL_SQL:
            sprintf(lvlbuf, "SQ %c" EOM, vch);
            retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

            retval = sscanf(ackbuf, "SQ %d,%x", &v, &l);
            if (retval != 2 || l < rig->caps->level_gran[LVL_SQL].min.i || l > rig->caps->level_gran[LVL_SQL].max.i) {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
                return -RIG_ERJCTED;
            }

            /* range [0.0 ... 1.0] */
            val->f = (float)(l-rig->caps->level_gran[LVL_SQL].min.i) / (float)(rig->caps->level_gran[LVL_SQL].max.i-rig->caps->level_gran[LVL_SQL].min.i);
		break;

        case RIG_LEVEL_AF:
            sprintf(lvlbuf, "AG %c" EOM, vch);
            retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

            retval = sscanf(ackbuf, "AG %d,%x", &v, &l);
            if (retval != 2 || l < rig->caps->level_gran[LVL_AF].min.i || l > rig->caps->level_gran[LVL_AF].max.i) {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
                return -RIG_ERJCTED;
            }

            /* range [0.0 ... 1.0] */
            val->f = (float)(l-rig->caps->level_gran[LVL_AF].min.i) / (float)(rig->caps->level_gran[LVL_AF].max.i-rig->caps->level_gran[LVL_AF].min.i);
		break;

        case RIG_LEVEL_RFPOWER:
            sprintf(lvlbuf, "PC %c" EOM,vch); 
            retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

            retval = sscanf(ackbuf, "PC %d,%d", &v, &l);
            if (retval != 2 || l < 0 || l > 3) {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
                return -RIG_ERJCTED;
            }

            /* range [0.0 ... 1.0] */
            val->f = (float)(l-rig->caps->level_gran[LVL_RFPOWER].min.i) / (float)(rig->caps->level_gran[LVL_RFPOWER].max.i-rig->caps->level_gran[LVL_AF].min.i);
		break;

        case RIG_LEVEL_BALANCE:
            sprintf(lvlbuf, "BAL" EOM);
            retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;
            if (ackbuf[4] < '0' || ackbuf[4] > '9')
		    return -RIG_EPROTO;
	    val->f = (float)('4'-ackbuf[4])/('4'-'0');
	    break;

        case RIG_LEVEL_ATT:
            sprintf(lvlbuf, "ATT" EOM);
            retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;
            if (ackbuf[4] < '0' || ackbuf[4] > '9')
		    return -RIG_EPROTO;
            if (ackbuf[4] == '0')
		    val->i = 0;
	    else
		    val->i = rig->state.attenuator[ackbuf[4]-'1'];
	    break;

        default:
            rig_debug(RIG_DEBUG_ERR,"%s: Unsupported Level %d", __FUNCTION__, level);
            return -RIG_EINVAL;
    }
    return RIG_OK;
}

int th_set_level (RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char vch, lvlbuf[32], ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len;
    vfo_t tvfo;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	if(vfo==RIG_VFO_CURR) tvfo=rig->state.current_vfo;
	else tvfo=vfo;

	switch (tvfo) {
        case RIG_VFO_A:
        case RIG_VFO_VFO:
        case RIG_VFO_MEM:
		 vch = '0'; break;
        case RIG_VFO_B: 
		 vch = '1'; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
            return -RIG_EVFO;
	}

    ack_len = ACKBUF_LEN;
    switch(level) {

	case RIG_LEVEL_RFPOWER :
	    sprintf(lvlbuf, "PC %c,%01d" EOM, vch,(int)(val.f*(rig->caps->level_gran[LVL_RFPOWER].max.i-rig->caps->level_gran[LVL_RFPOWER].min.i))+rig->caps->level_gran[LVL_RFPOWER].min.i);
	    retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
	    return retval;
	case RIG_LEVEL_SQL :
	    sprintf(lvlbuf, "SQ %c,%02x" EOM, vch,(int)(val.f*(rig->caps->level_gran[LVL_SQL].max.i-rig->caps->level_gran[LVL_SQL].min.i))+rig->caps->level_gran[LVL_SQL].min.i);
	    retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
	    return retval;
	case RIG_LEVEL_AF :
	    sprintf(lvlbuf, "AG %c,%02x" EOM, vch,(int)(val.f*32.0));
	    retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
	    return retval;
	case RIG_LEVEL_ATT :
	    sprintf(lvlbuf, "ATT %c" EOM, val.i ? '1' : '0');
	    retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
	    return retval;
	case RIG_LEVEL_BALANCE :
	    sprintf(lvlbuf, "BAL %c" EOM, '4' - (int) (val.f * ('4'-'0')));
	    retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
	    return retval;
        default:
            rig_debug(RIG_DEBUG_ERR,"%s: Unsupported Level %d", __FUNCTION__, level);
            return -RIG_EINVAL;
    }
    return RIG_OK;
}

#ifndef RIG_TONEMAX
#define RIG_TONEMAX	38
#endif

/*
 * th_set_ctcss_tone
 * Assumes rig!=NULL, rig->caps->ctcss_list != NULL
 */
int
th_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
      const struct rig_caps *caps;
      char tonebuf[16],ackbuf[ACKBUF_LEN];
      int i, retval;
      size_t ack_len;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	caps = rig->caps;

	for (i = 0; caps->ctcss_list[i] != 0 && i < RIG_TONEMAX; i++) {
		if (caps->ctcss_list[i] == tone)
			break;
	}
	if (caps->ctcss_list[i] != tone)
		return -RIG_EINVAL;

    i += (i == 0) ? 1 : 2;  /* Correct for TH-7DA index anomally */
	sprintf(tonebuf, "CTN %02d" EOM, i);
    ack_len = ACKBUF_LEN;
    retval = kenwood_transaction(rig, tonebuf, strlen(tonebuf), ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

/*
 * thd7_get_ctcss_tone
 * Assumes rig!=NULL, rig->caps!=NULL
 */
int
th_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct rig_caps *caps;
    char ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len=ACKBUF_LEN;
    unsigned int tone_idx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	caps = rig->caps;

    retval = kenwood_transaction (rig, "CTN" EOM, 4, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

    retval = sscanf(ackbuf, "CTN %d", (int*)&tone_idx);
    if (retval != 1) {
		rig_debug(RIG_DEBUG_ERR, 
                        "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
		return -RIG_EPROTO;
    }

    /* verify tone index for TH-7DA rig */
	if (tone_idx <= 0 || tone_idx == 2 || tone_idx > 39) {
		rig_debug(RIG_DEBUG_ERR, "%s: Unexpected CTCSS no (%04d)\n",
				__FUNCTION__, tone_idx);
		return -RIG_EPROTO;
	}
    tone_idx -= (tone_idx == 1) ? 1 : 2; /* Correct for TH-7DA index anomaly */
	*tone = caps->ctcss_list[tone_idx];
	return RIG_OK;
}

const char *
th_get_info(RIG *rig)
{
	static char firmbuf[50];
	int retval;
	size_t firm_len=50;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	memset(firmbuf, 0, sizeof(firmbuf));
	retval = kenwood_transaction (rig, "ID" EOM, 3, firmbuf, &firm_len);
	if (retval != RIG_OK)
		return NULL;

	if (firm_len <3 ) {
       rig_debug(RIG_DEBUG_ERR, "%s: unexpected reply '%s', len=%d\n",
		       __FUNCTION__, firmbuf, firm_len);
	   return NULL;
	}

	return &firmbuf[2];
}

/*
 * th_set_mem
 * Assumes rig!=NULL
 */
int
th_set_mem(RIG *rig, vfo_t vfo, int ch)
{
	unsigned char vsel;
	char membuf[16], ackbuf[50];
	int retval;
	size_t ack_len;
	vfo_t tvfo;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	if(vfo==RIG_VFO_CURR) tvfo=rig->state.current_vfo;
	else tvfo=vfo;

	switch (tvfo) {
	  case RIG_VFO_VFO: 
	  case RIG_VFO_MEM: 
	  case RIG_VFO_A: 
			vsel = '0'; break;
	  case RIG_VFO_B:
			vsel = '1'; break;
	  default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_EVFO;
	}
	sprintf(membuf, "MC %c,%03i" EOM, vsel, ch);

	retval= rig_set_vfo(rig,RIG_VFO_MEM);
	if (retval != RIG_OK)
		return retval;

	ack_len = ACKBUF_LEN;
	retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

int
th_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	char *membuf, ackbuf[ACKBUF_LEN];
	int retval;
	size_t ack_len=ACKBUF_LEN;
	vfo_t tvfo,cvfo;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	cvfo=rig->state.current_vfo;
	if(vfo==RIG_VFO_CURR)
		 tvfo=cvfo;
	else
		 tvfo=vfo;

	switch (tvfo) {
	  case RIG_VFO_VFO:
	  case RIG_VFO_MEM: 
	  case RIG_VFO_A:
		membuf = "MC 0" EOM;
		break;
	  case RIG_VFO_B:
		membuf = "MC 1" EOM;
		break;
	  default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_EVFO;
	}

	retval= rig_set_vfo(rig,RIG_VFO_MEM);
	if (retval != RIG_OK)
		return retval;

	retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len <9 ) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected reply '%s', len=%d\n",
				__FUNCTION__, ackbuf, ack_len);
		return -RIG_ERJCTED;
	}

	ackbuf[8]='\0';
        *ch = atoi(&ackbuf[5]);

	retval= rig_set_vfo(rig,cvfo);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

int
th_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
	char *membuf, ackbuf[ACKBUF_LEN];
	int retval;
	size_t ack_len;

        rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

        switch(ptt) {
         case RIG_PTT_ON :
                membuf="TX" EOM;
                break;
         case RIG_PTT_OFF :
                membuf="RX" EOM;
                break;
         default:
                return -RIG_EINVAL;
        }
        ack_len = ACKBUF_LEN;
        retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
                return retval;

	return RIG_OK;
}

int
th_set_powerstat(RIG *rig, powerstat_t status)
{
        char *membuf, ackbuf[50];
        int retval;
        size_t ack_len;

        rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

        switch(status) {
         case RIG_POWER_ON :
                membuf="PS 1" EOM;
                break;
         case RIG_POWER_OFF :
                membuf="PS 0" EOM;
                break;
         default:
                return -RIG_EINVAL;
        }

        ack_len = ACKBUF_LEN;
        retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
                return retval;

	return RIG_OK;
}

int th_get_powerstat(RIG *rig, powerstat_t *status)
{
	char pwrbuf[50];
	size_t pwr_len = 50;
	int retval;

	retval = kenwood_transaction (rig, "PS" EOM, 3, pwrbuf, &pwr_len);
	if (retval != RIG_OK)
		return retval;

	if (pwr_len != 4)
	{
		rig_debug(RIG_DEBUG_ERR,"kenwood_get_powerstat: wrong answer "
			"len=%d\n", pwr_len);
		return -RIG_ERJCTED;
	}
	*status = pwrbuf[2] == '0' ? RIG_POWER_OFF : RIG_POWER_ON;
				            
	return RIG_OK;
}
                                                                                                                            

int th_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
        char *membuf, ackbuf[ACKBUF_LEN];
        int retval;
	size_t ack_len=ACKBUF_LEN;

	switch (vfo) {
	  case RIG_VFO_VFO:
	  case RIG_VFO_A:
		membuf = "BY 0" EOM;
		break;
	  case RIG_VFO_B:
		membuf = "BY 1" EOM;
		break;
	  default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_EVFO;
	}


        retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
                return retval;

	if (ack_len <6 ) {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected reply '%s', len=%d\n",
				__FUNCTION__, ackbuf, ack_len);
		return -RIG_ERJCTED;
	}

        switch(ackbuf[5]) {
		case '0' :
			*dcd=RIG_DCD_OFF;
			return RIG_OK;
		case '1' :
			*dcd=RIG_DCD_ON;
			return RIG_OK;
		default :
			rig_debug(RIG_DEBUG_ERR, "%s: unexpected reply '%s', len=%d\n", __FUNCTION__, ackbuf, ack_len);
			return -RIG_ERJCTED;
	}
	return -RIG_ERJCTED;
}

int th_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{

	char *membuf,ackbuf[ACKBUF_LEN];
	int retval;
	size_t ack_len;

        rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

      if(vfo!=RIG_VFO_CURR) {
        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_ENTARGET;
	}

	switch(op) {
		case RIG_OP_UP:
                	membuf="UP" EOM;
               		break;
		case RIG_OP_DOWN:
                	membuf="DW" EOM;
               		break;
		case RIG_OP_TO_VFO:
                	membuf="MSH" EOM;
               		break;
		default:
                	return -RIG_EINVAL;
	}

        ack_len = ACKBUF_LEN;
        retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
                return retval;
        return RIG_OK;
}

/* get and set channel tested on thg71&thf7e   	*/
/* must work on other th and tm kenwood rigs	*/
/* --------------------------------------------------------------------- */
int th_get_channel(RIG *rig, channel_t *chan)
{
    char membuf[64],ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len;
    freq_t freq,offset;
    char req[16],scf[128];
    int step, shift, rev, tone, ctcss, tonefq, ctcssfq, dcs, dcscode, mode, lockout;
    const char *mr_extra;
    int channel_num;
    vfo_t vfo;
    const struct kenwood_priv_caps *priv=(const struct kenwood_priv_caps *)rig->caps->priv;
    const chan_t *chan_caps;
   
    if (chan->vfo == RIG_VFO_MEM)
    {
    	chan_caps = rig_lookup_mem_caps(rig, chan->channel_num);
	if (!chan_caps)
		return -RIG_ECONF;
    }
    else
	    return -RIG_ENIMPL;

    channel_num = chan->channel_num;
    vfo = chan->vfo;
    memset(chan, 0, sizeof(channel_t));
    chan->channel_num = channel_num;
    chan->vfo = vfo;

    if (rig->caps->rig_model == RIG_MODEL_THF7E)
	    mr_extra = "";
    else
	    mr_extra = "0,";

    if (chan_caps)
    	channel_num -= chan_caps->start;
    switch (chan_caps->type) {
	    case RIG_MTYPE_MEM:
		if (chan_caps[1].type == RIG_MTYPE_PRIO) {
			/* Info */
    			sprintf(req,"MR %s0,I-%01d",mr_extra,channel_num);
		} else
    			sprintf(req,"MR %s0,%03d",mr_extra,channel_num);
		break;
	    case RIG_MTYPE_EDGE:
		if (chan_caps[1].type == RIG_MTYPE_EDGE) {
    			sprintf(req,"MR %s0,L%01d",mr_extra,channel_num);
        		sprintf(chan->channel_desc,"L%01d",channel_num);
		} else {
   			sprintf(req,"MR %s0,U%01d",mr_extra,channel_num);
        		sprintf(chan->channel_desc,"U%01d",channel_num);
		}
		break;
	    case RIG_MTYPE_PRIO:
		if (chan_caps->start == chan_caps->end) {
   			sprintf(req,"MR %s0,PR",mr_extra);
        		sprintf(chan->channel_desc,"Pr");
		} else {
   			sprintf(req,"MR %s0,PR%01d",mr_extra,channel_num+1);
        		sprintf(chan->channel_desc,"Pr%01d",channel_num+1);
		}
		break;
	    case RIG_MTYPE_CALL:
    		sprintf(req,"CR 0,%01d",channel_num);
        	if(chan->channel_num==chan_caps->start) sprintf(chan->channel_desc,"Call V");
		else if(chan->channel_num==chan_caps->end) sprintf(chan->channel_desc,"Call U");
		else sprintf(chan->channel_desc,"Call");
		break;
	    case RIG_MTYPE_BAND:
		sprintf(req,"VR %01X",channel_num);
		sprintf(chan->channel_desc,"BAND %01X",channel_num);
		break;
	    default:
		return -RIG_EINVAL;
    }

    sprintf(membuf,"%s"EOM,req);
    ack_len=ACKBUF_LEN;
    retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
        	return retval;

    /*
     * TODO: dcs/mode/lockout are not there on TH-G71
     */
    mode = RIG_MODE_NONE;
    rev = lockout = dcs = dcscode = 0;

    strcpy(scf,req);
    if (chan_caps->mem_caps.dcs_sql)
    {
	/* Step can be hexa
	 * Lockout is optional on some channels
	 */
    	strcat(scf,",%"SCNfreq",%x,%d,%d,%d,%d,%d,%d,%d,%d,%"SCNfreq",%d,%d");
	retval = sscanf(ackbuf, scf,
                    &freq, &step, &shift, &rev, &tone,
                    &ctcss, &dcs, &tonefq, &ctcssfq, &dcscode,
		    &offset, &mode, &lockout
		    );
    	if (retval < 12) {
    		rig_debug(RIG_DEBUG_WARN, "%s: sscanf failed %d\n", __FUNCTION__, retval);
		return -RIG_EPROTO;
	}
    }
    else
    {
	strcat(scf,",%"SCNfreq",%x,%d,%d,%d,%d,,%d,,%d,%"SCNfreq);
	retval = sscanf(ackbuf, scf,
                    &freq, &step, &shift, &rev, &tone,
		    &ctcss, &tonefq, &ctcssfq, &offset);
    	if (retval != 9) {
    		rig_debug(RIG_DEBUG_WARN, "%s: sscanf failed %d\n", __FUNCTION__, retval);
	}
    }

    chan->funcs = rev ? RIG_FUNC_REV : 0;
    chan->flags = lockout ? RIG_CHFLAG_SKIP : 0;
    chan->freq=freq;
    chan->vfo=RIG_VFO_MEM;
    chan->tuning_step=rig->state.tuning_steps[step].ts;

    if (priv->mode_table) {
	    chan->mode = kenwood2rmode(mode, priv->mode_table);
	    if (chan->mode == RIG_MODE_NONE) {
            	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Mode value '%d'\n",
				__FUNCTION__, mode);
            	return -RIG_EPROTO;
	    }
    }
    else
    {
	/* No mode info (TH-G71, TMV7,..),
	 * guess it from current freq
	 */
    	chan->mode = (freq < MHz(136)) ? RIG_MODE_AM : RIG_MODE_FM;
    }

    chan->width=rig_passband_normal(rig, chan->mode);

    switch(shift) {
	case 0 :
		chan->rptr_shift=RIG_RPT_SHIFT_NONE;
		break;
	case 1 :
		chan->rptr_shift=RIG_RPT_SHIFT_PLUS;
		break;
	case 2 :
		chan->rptr_shift=RIG_RPT_SHIFT_MINUS;
		offset = -offset;
		break;
	default:
            	rig_debug(RIG_DEBUG_ERR, "%s: not supported shift %d\n",
				__FUNCTION__, shift);
		chan->rptr_shift=RIG_RPT_SHIFT_NONE;
    }
    chan->rptr_offs=offset;

    /* FIXME: ctcss_list for t[fm]*.c */
	 //chan->ctcss_tone=rig->caps->ctcss_list[tonefq==1?0:tonefq-2];
	 // chan->ctcss_sql=rig->caps->ctcss_list[ctcssfq==1?0:ctcssfq-2];
    if(tone)
	 chan->ctcss_tone=rig->caps->ctcss_list[tonefq];
    else
	 chan->ctcss_tone=0;
    if(ctcss)
	 chan->ctcss_sql=rig->caps->ctcss_list[ctcssfq];
    else
	 chan->ctcss_sql=0;
    if(dcs)
	 chan->dcs_sql=chan->dcs_code=rig->caps->dcs_list[dcscode];
    else
	 chan->dcs_sql=chan->dcs_code=0;

    chan->tx_freq=RIG_FREQ_NONE;
    if(shift==RIG_RPT_SHIFT_NONE && 
		    ((chan_caps->type==RIG_MTYPE_MEM && chan_caps->start == 0) ||
			    chan_caps->type==RIG_MTYPE_CALL)) {
    	/* split ? */
	req[3+strlen(mr_extra)]='1';
        sprintf(membuf,"%s"EOM,req);
        ack_len=ACKBUF_LEN;
    	retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval == RIG_OK) {
    		strcpy(scf,req);
    		strcat(scf,",%"SCNfreq",%x");
    		retval = sscanf(ackbuf, scf, &freq, &step);
		chan->tx_freq=freq;
		chan->split=RIG_SPLIT_ON;
	}
    }

    /* If not set already by special channels.. */
    if(chan->channel_desc[0] == '\0') {
	if (chan_caps[1].type == RIG_MTYPE_PRIO)
    		sprintf(membuf,"MNA %sI-%01d"EOM,mr_extra,channel_num);
	else
    		sprintf(membuf,"MNA %s%03d"EOM,mr_extra,channel_num);
        ack_len=ACKBUF_LEN;
	/* Get memory name */
    	retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
        	return retval;
	if (ack_len > rig->caps->chan_desc_sz)
		ack_len = rig->caps->chan_desc_sz;
        strncpy(chan->channel_desc,ackbuf+strlen(membuf),ack_len);
	chan->channel_desc[ack_len] = '\0';
    }

    return RIG_OK;
}

static int find_tone_index(const tone_t *tone_list, tone_t tone)
{
	int i;

    	for (i = 0; tone_list[i] != 0 && i < RIG_TONEMAX; i++) {
		if (tone_list[i] == tone)
			return i;
	}
	return -1;
}

/* --------------------------------------------------------------------- */
int th_set_channel(RIG *rig, const channel_t *chan)
{
    char membuf[ACKBUF_LEN],ackbuf[ACKBUF_LEN];
    int retval;
    size_t ack_len;
    char req[64];
    char lockoutstr[8];
    int channel_num, step, shift, rev, tone, ctcss, tonefq, ctcssfq, dcs, dcscode, mode, lockout;
    const char *mr_extra;
    const struct kenwood_priv_caps *priv=(const struct kenwood_priv_caps *)rig->caps->priv;
    const chan_t *chan_caps;
    const char *channel_desc;

    channel_num = chan->channel_num;

    for(step=0; rig->state.tuning_steps[step].ts!=0;step++)
	if(chan->tuning_step==rig->state.tuning_steps[step].ts) break;

    switch(chan->rptr_shift) {
	case RIG_RPT_SHIFT_NONE :
		shift=0;
		break;
	case  RIG_RPT_SHIFT_PLUS:
		shift=1;
		break;
	case RIG_RPT_SHIFT_MINUS:
		shift=2;
		break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: not supported shift %d\n",
			    __FUNCTION__, chan->rptr_shift);
            return -RIG_EINVAL;
    }

    if(chan->ctcss_tone==0) {
	 tone=0;tonefq=8;
    } else {
	tone=1;
	tonefq = find_tone_index(rig->caps->ctcss_list, chan->ctcss_tone);
	if (tonefq == -1)
		return -RIG_EINVAL;
	tonefq++;
    }
    if(chan->ctcss_sql==0) {
	ctcss=0;ctcssfq=8;
    } else {
	ctcss=1;
	ctcssfq = find_tone_index(rig->caps->ctcss_list, chan->ctcss_sql);
	if (tonefq == -1)
		return -RIG_EINVAL;
	ctcssfq++;
    }

    if(chan->dcs_code==0 && chan->dcs_sql==0) {
	dcs=0;dcscode=0;
    } else {
	dcs=1;
	dcscode = find_tone_index(rig->caps->dcs_list, chan->dcs_sql);
	if (dcscode == -1)
		return -RIG_EINVAL;
    }

    if (chan->vfo == RIG_VFO_MEM)
    {
    	chan_caps = rig_lookup_mem_caps(rig, chan->channel_num);
	if (!chan_caps)
		return -RIG_ECONF;
    	channel_num -= chan_caps->start;
    }
    else
	    return -RIG_ENIMPL;

    if (rig->caps->rig_model == RIG_MODEL_THF7E)
	    mr_extra = "";
    else
	    mr_extra = "0,";

    channel_desc = NULL;
    switch (chan_caps->type) {
	    case RIG_MTYPE_MEM:
		if (chan_caps[1].type == RIG_MTYPE_PRIO) {
			/* Info */
    			sprintf(req,"MW %s0,I-%01d",mr_extra,channel_num);
    			channel_desc = chan->channel_desc;
		} else {
			/* Regular */
    			sprintf(req,"MW %s0,%03d",mr_extra,channel_num);
    			channel_desc = chan->channel_desc;
		}
		break;
	    case RIG_MTYPE_EDGE:
		if (chan_caps[1].type == RIG_MTYPE_EDGE) {
    			sprintf(req,"MW %s0,L%01d",mr_extra,channel_num);
		} else {
   			sprintf(req,"MW %s0,U%01d",mr_extra,channel_num);
		}
		break;
	    case RIG_MTYPE_PRIO:
		if (chan_caps->start == chan_caps->end) {
   			sprintf(req,"MW %s0,PR",mr_extra);
		} else {
   			sprintf(req,"MW %s0,PR%01d",mr_extra,channel_num+1);
		}
		break;
	    case RIG_MTYPE_CALL:
    		sprintf(req,"CW 0,%01d",channel_num);
		break;
	    case RIG_MTYPE_BAND:
    		sprintf(req,"VW %01X",channel_num);
		break;
	    default:
		return -RIG_EINVAL;
    }

    rev = chan->funcs & RIG_FUNC_REV ? 1 : 0;
    lockout = chan->flags & RIG_CHFLAG_SKIP ? 1 : 0;

    if (chan_caps->mem_caps.flags)
	    sprintf(lockoutstr, ",%d", lockout);
    else
	    strcpy(lockoutstr, "");

    if (chan_caps->mem_caps.flags && chan_caps->mem_caps.dcs_sql)
    {
	if (!priv->mode_table) {
		rig_debug(RIG_DEBUG_ERR, "%s: Buggy backend, no mode_table '%d'\n",
				__FUNCTION__, chan->mode);
		return -RIG_ENIMPL;
	}
	mode = rmode2kenwood(chan->mode, priv->mode_table);
	if (mode == -1) {
		rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Mode value '%d'\n",
				__FUNCTION__, chan->mode);
		return -RIG_EINVAL;
	}

	/* Step can be hexa */
	retval = sprintf(membuf, "%s,%011"PRIll",%X,%d,%d,%d,%d,%d,%02d,%02d,%03d,%09"PRIll",%d%s"EOM,
			req, (long long)chan->freq, step, shift, rev, tone,
                    	ctcss, dcs, tonefq, ctcssfq, dcscode,
			(long long)abs(chan->rptr_offs), mode, lockoutstr
			);
    }
    else
    {
	/* Without DCS,mode */
	retval = sprintf(membuf, "%s,%011"PRIll",%X,%d,%d,%d,%d,,%02d,,%02d,%09"PRIll"%s" EOM,
			req, (long long)chan->freq, step, shift, rev, tone,
                    	ctcss, tonefq, ctcssfq, 
			(long long)abs(chan->rptr_offs), lockoutstr
			);
    }

    ack_len = ACKBUF_LEN;
    retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
    if (retval != RIG_OK)
	return retval;

    /* split ? */
    if (chan->tx_freq!=RIG_FREQ_NONE && 
		    ((chan_caps->type==RIG_MTYPE_MEM && chan_caps->start == 0) ||
			    chan_caps->type==RIG_MTYPE_CALL)) {
	req[3+strlen(mr_extra)]='1';
    	sprintf(membuf, "%s,%011"PRIll",%X"EOM, req,(long long)chan->tx_freq, step);
        ack_len=ACKBUF_LEN;
    	retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
        	return retval;
    }

    if(channel_desc) {
	/* Memory name */
        ack_len=ACKBUF_LEN;
	/* TODO: check strlen(channel_desc) < rig->caps->chan_desc_sz */
	if (chan_caps[1].type == RIG_MTYPE_PRIO)
    		sprintf(membuf,"MNA %sI-%01d,%s"EOM,mr_extra,channel_num,channel_desc);
	else
    		sprintf(membuf,"MNA %s%03d,%s"EOM,mr_extra,channel_num,channel_desc);
    	retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
        	return retval;
    }

    return RIG_OK;
}

/*
 * set the aerial/antenna to use
 */
int th_set_ant (RIG * rig, vfo_t vfo, ant_t ant)
{
  char ackbuf[ACKBUF_LEN];
  const char *cmd;
  size_t ack_len = ACKBUF_LEN;

  switch (ant) {
	  case RIG_ANT_1:
		cmd = "ANT 0"EOM;
	  case RIG_ANT_2:
		cmd = "ANT 1"EOM;
	  case RIG_ANT_3:
		cmd = "ANT 2"EOM;
	  default:
		return -RIG_EINVAL;
  }

  return kenwood_transaction (rig, cmd, strlen(cmd), ackbuf, &ack_len);
}


/*
 * get the aerial/antenna  in use
 */
int th_get_ant (RIG * rig, vfo_t vfo, ant_t * ant)
{
  char ackbuf[ACKBUF_LEN];
  size_t ack_len = ACKBUF_LEN;
  int retval;

  retval = kenwood_transaction (rig, "ANT"EOM, 4, ackbuf, &ack_len);
  if (RIG_OK != retval)
    return retval;
  if (4 != ack_len)
    return -RIG_EPROTO;

  if (ackbuf[2] <= '0' || ackbuf[2] > '9')
      return -RIG_EPROTO;

  *ant = RIG_ANT_N(ackbuf[2]-'0');

  return RIG_OK;
}

