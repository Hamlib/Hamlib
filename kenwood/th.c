/*
 *  Hamlib Kenwood backend - TH handheld primitives
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: th.c,v 1.17 2004-01-15 22:03:18 fillods Exp $
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

const struct kenwood_priv_caps  th_priv_caps  = {
    .cmdtrm =  EOM_TH,
};
/* Note: Currently the code assumes the command termination is a
 * single character.
 */

#define EOM EOM_TH

/*
 * modes in use by the "MD" command
 */
#define MD_FM	'0'
#define MD_WFM	'1'		/* or is it AM on the THD7? */
#define MD_AM	'2'
#define MD_LSB	'3'
#define MD_USB	'4'
#define MD_CW	'5'

#define ACKBUF_LEN  64

/*
 * th_decode_event is called by sa_sigio, when some asynchronous
 * data has been received from the rig.
 */
int
th_decode_event (RIG *rig)
{
    char asyncbuf[128];
    int retval,async_len=128;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = kenwood_transaction(rig, NULL, 0, asyncbuf, &async_len);
    if (retval != RIG_OK)
        return retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: Decoding message\n", __FUNCTION__);

    /* --------------------------------------------------------------------- */
    if (async_len> 3 && asyncbuf[0] == 'B' && asyncbuf[1] == 'U' && asyncbuf[2] == 'F') {

        vfo_t vfo;
        freq_t freq, offset;
        rmode_t mode;
        int step, shift, rev, tone, ctcss, tonefq, ctcssfq;

        retval = sscanf(asyncbuf, "BUF %d,%"FREQFMT",%d,%d,%d,%d,%d,,%d,,%d,%"FREQFMT",%d",
                                  &vfo, &freq, &step, &shift, &rev, &tone,
                                  &ctcss, &tonefq, &ctcssfq, &offset, &mode);

        if (retval != 11) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected BUF message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }

        /* Calibration and conversions */
        vfo = (vfo == 0) ? RIG_VFO_A : RIG_VFO_B;
        mode = (mode == 0) ? RIG_MODE_FM : RIG_MODE_AM;

        rig_debug(RIG_DEBUG_TRACE, "%s: Buffer (vfo %d, freq %lld Hz, mode %d)\n", __FUNCTION__, vfo, freq, mode);

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
    int retval, step,ack_len=ACKBUF_LEN;
    long long f;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

      if(vfo!=RIG_VFO_CURR) {
        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_ENTARGET;
	}

    step = 1;
    f=(long long) freq;
    sprintf(freqbuf, "FQ %011lld,%1d"EOM, f, step);
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
    int retval, step,ack_len=ACKBUF_LEN;
    long long f;

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

    retval = sscanf(ackbuf, "FQ %lld,%d",&f,&step);
    if (retval != 2) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, freqbuf);
        return -RIG_ERJCTED;
    }

    *freq=(freq_t)f;
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
    int retval,ack_len=ACKBUF_LEN;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

      if(vfo!=RIG_VFO_CURR) {
        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_ENTARGET;
	}

    /* TODO: Find from rig->caps what modes can be supported. */
	switch (mode) {
    case RIG_MODE_FM:   kmode = '0'; break; /* TH-D7A(G) modes */
    case RIG_MODE_AM:   kmode = '1'; break;
	default:
        rig_debug(RIG_DEBUG_ERR,"%s: Unsupported Mode %d\n", __FUNCTION__, mode);
		return -RIG_EINVAL;
	}

    sprintf(mdbuf, "MD %c" EOM, kmode);
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
    int retval,ack_len=ACKBUF_LEN;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

      if(vfo!=RIG_VFO_CURR) {
        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_ENTARGET;
	}

    vch = '0';

    retval = kenwood_transaction (rig, "MD" EOM, 3, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

    if (strlen(ackbuf) < 4 ) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

	switch (ackbuf[3]) {
        case '0': *mode = RIG_MODE_FM; break;     /* TH-D7A(G) modes */
        case '1': *mode = RIG_MODE_AM; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Mode value '%c'\n", __FUNCTION__, ackbuf[3]);
            return -RIG_EINVAL;
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
    int retval,ack_len=ACKBUF_LEN;

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
            sprintf(vfobuf, "VMC 0,2" EOM);
            break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
            return -RIG_EVFO;
	}

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
    char ackbuf[ACKBUF_LEN];
    int retval,ack_len=ACKBUF_LEN;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = kenwood_transaction (rig, "BC" EOM, 3, ackbuf, &ack_len);
    if (retval != RIG_OK)
        return retval;

    if (ack_len < 4 ) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

    switch (ackbuf[3]) {
        case '0': *vfo = RIG_VFO_A; break;
        case '1': *vfo = RIG_VFO_B; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VFO value '%c'\n", __FUNCTION__, ackbuf[3]);
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
    int retval,ack_len=ACKBUF_LEN;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    sprintf(trnbuf, "AI %c" EOM, (trn == RIG_TRN_RIG) ? '1' : '0');
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
    int retval,ack_len=ACKBUF_LEN;

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
int
th_get_kenwood_func (RIG *rig, const char *cmd, int *status)
{
    char ackbuf[ACKBUF_LEN];
    int retval,ack_len=ACKBUF_LEN;
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

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (func) {
        case RIG_FUNC_MON:  /* which one? */
        case RIG_FUNC_SQL:
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

        case RIG_FUNC_FAGC:
        case RIG_FUNC_NB:
        case RIG_FUNC_COMP:
        case RIG_FUNC_VOX:
        case RIG_FUNC_SBKIN:
        case RIG_FUNC_FBKIN:
        case RIG_FUNC_ANF:
        case RIG_FUNC_NR:
        case RIG_FUNC_APF:
        case RIG_FUNC_MN:
        case RIG_FUNC_RNF:
        case RIG_FUNC_MUTE:
        case RIG_FUNC_VSC:
        case RIG_FUNC_ABM:
        case RIG_FUNC_BC:
        case RIG_FUNC_MBC:
            return -RIG_ENAVAIL;
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
    int retval, v, l,ack_len=ACKBUF_LEN;
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
            if (retval != 2 || l < 0 || l > 5) {
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

            retval = sscanf(ackbuf, "SQ %d,%d", &v, &l);
            if (retval != 2 || l < 0 || l > 5) {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
                return -RIG_ERJCTED;
            }

            /* range [0.0 ... 1.0] */
            val->f = l / 5.0;
		break;

        case RIG_LEVEL_RFPOWER:
            sprintf(lvlbuf, "PC 0" EOM); /* only VHF power ... */
            retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

            retval = sscanf(ackbuf, "PC 0,%d", &l);
            if (retval != 1 || l < 0 || l > 3) {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
                return -RIG_ERJCTED;
            }

            /* range [0.0 ... 1.0] */
            val->f = (3.0-l) / 3.0;
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
    int retval,ack_len=ACKBUF_LEN;
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

    if(level!=RIG_LEVEL_RFPOWER)
            return -RIG_EINVAL;

    sprintf(lvlbuf, "PC 0,%d" EOM, 3-(int)(val.f*3.0)); /* only VHF power ... */
    retval = kenwood_transaction (rig, lvlbuf, strlen(lvlbuf), ackbuf, &ack_len);

    return retval;
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
      unsigned char tonebuf[16], ackbuf[ACKBUF_LEN];
      int i, retval,ack_len=ACKBUF_LEN;

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
    int retval,ack_len=ACKBUF_LEN;
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
	static unsigned char firmbuf[16];
	int retval,firm_len=16;

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
	unsigned char vsel, membuf[16], ackbuf[16];
	int retval, ack_len = 16;
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

	retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

int
th_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	unsigned char *membuf, ackbuf[ACKBUF_LEN];
	int retval,ack_len=ACKBUF_LEN;
	vfo_t tvfo;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	if(vfo==RIG_VFO_CURR) tvfo=rig->state.current_vfo;
	else tvfo=vfo;

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

	return RIG_OK;
}

int
th_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
        unsigned char *membuf;
        int retval;

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

        retval = kenwood_transaction(rig, membuf, strlen(membuf), NULL, NULL);
        if (retval != RIG_OK)
                return retval;

	return RIG_OK;
}

int th_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
        unsigned char *membuf, ackbuf[ACKBUF_LEN];
        int retval, ack_len=ACKBUF_LEN;

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

	unsigned char *membuf,ackbuf[ACKBUF_LEN];
	int retval,ack_len=ACKBUF_LEN;

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
		default:
                	return -RIG_EINVAL;
	}

        retval = kenwood_transaction(rig, membuf, strlen(membuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
                return retval;
        return RIG_OK;
}
