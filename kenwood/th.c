/*
 *  Hamlib Kenwood backend - TH handheld primitives
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: th.c,v 1.13 2003-10-20 22:15:02 fillods Exp $
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

#undef TH_ADD_CMDTRM      /* Let th_transaction add command termination */
#ifdef TH_ADD_CMDTRM
#define EOM
#else
#define EOM EOM_TH
#endif

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

#if 0
/**
 * th_transaction
 * Assumes rig!=NULL rig->state!=NULL rig->caps!=NULL
 *
 * cmdstr - Command to be sent to the rig. Cmdstr can also be NULL, indicating
 *          that only a reply is needed (nothing will be send).
 * data - Buffer for reply string.  Can be NULL, indicating that no reply is
 *        is needed and will return with RIG_OK after command was sent.
 * datasize - Size of buffer. It is the caller's responsibily to provide
 *            a large enough buffer for all possible replies for a command.
 *
 * returns:
 *   RIG_OK  -  if no error occured.
 *   RIG_EIO  -  if an I/O error occured while sending/receiving data.
 *   RIG_ETIMEOUT  -  if timeout expires without any characters received.
 *   RIG_REJECTED  -  if a negative acknowledge was received or command not
 *                    recognized by rig.
 */
int
th_transaction (RIG *rig, const char *cmdstr, char *data, size_t datasize)
{
    struct rig_state *rs;
    struct rig_caps *rc;
    int retval;
    const char *cmdtrm = "\r";  /* Default Command/Reply termination char */
    int retry_read = 0;
#define MAX_RETRY_READ  32

    rs = &rig->state;
    rs->hold_decode = 1;

    rc = rig->caps;
    if (rc->priv)
        cmdtrm = ((struct kenwood_priv_caps *)rc->priv)->cmdtrm;

    if (cmdstr) {
        retval = write_block(&rs->rigport, cmdstr, strlen(cmdstr));
        if (retval != RIG_OK)
            goto transaction_quit;
#ifdef TH_ADD_CMDTRM
        retval = write_block(&rs->rigport, cmdtrm, strlen(cmdtrm));
        if (retval != RIG_OK)
            goto transaction_quit;
#endif
    }

    if (data == NULL || datasize <= 0) {
        rig->state.hold_decode = 0;
        return RIG_OK;  /* don't want a reply */
    }

transaction_read:
    retval = read_string(&rs->rigport, data, datasize, cmdtrm, strlen(cmdtrm));
    if (retval < 0)
        goto transaction_quit;

    /* Check that command termination is correct */
    if (!strchr(cmdtrm, data[strlen(data)])) {
        if (retry_read++ < MAX_RETRY_READ)
            goto transaction_read;
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __FUNCTION__, data);
        retval = -RIG_EPROTO;
        goto transaction_quit;
    }

    /* Command recognised by rig but invalid data entered. */
    if (strlen(data) == 2 && data[0] == 'N') {
        rig_debug(RIG_DEBUG_ERR, "%s: NegAck for '%s'\n", __FUNCTION__, cmdstr);
        retval = -RIG_ERJCTED;
        goto transaction_quit;
    }

    /* Command not understood by rig */
    if (strlen(data) == 2 && data[0] == '?') {
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown command '%s'\n", __FUNCTION__, cmdstr);
        retval = -RIG_ERJCTED;
        goto transaction_quit;
    }

#define CONFIG_STRIP_CMDTRM 1
#ifdef CONFIG_STRIP_CMDTRM
    data[strlen(data)-1] = '\000';  /* not very elegant, but should work. */
#endif
    /*
     * Check that received the correct reply. The first two characters
     * should be the same as command.
     */
    if (cmdstr && (data[0] != cmdstr[0] || data[1] != cmdstr[1])) {
         /*
          * TODO: When RIG_TRN is enabled, we can pass the string
          *       to the decoder for callback. That way we don't ignore
          *       any commands.
          */
        if (retry_read++ < MAX_RETRY_READ)
            goto transaction_read;
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, data);
        retval =  -RIG_EPROTO;
        goto transaction_quit;
    }

    retval = RIG_OK;
transaction_quit:
    rs->hold_decode = 0;
    return retval;
}

#else

int th_transaction (RIG *rig, const char *cmdstr, char *data, size_t datasize)
{
		size_t ds = datasize;
		return kenwood_transaction(rig, cmdstr, strlen(cmdstr), 
						                data, &ds);
}
#endif

/*
 * th_decode_event is called by sa_sigio, when some asynchronous
 * data has been received from the rig.
 */
int
th_decode_event (RIG *rig)
{
    char asyncbuf[128];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = th_transaction(rig, NULL, asyncbuf, sizeof(asyncbuf));
    if (retval != RIG_OK)
        return retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: Decoding message\n", __FUNCTION__);

    /* --------------------------------------------------------------------- */
    if (asyncbuf[0] == 'B' && asyncbuf[1] == 'U' && asyncbuf[2] == 'F') {

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
    } else if (asyncbuf[0] == 'S' && asyncbuf[1] == 'M') {

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
    } else if (asyncbuf[0] == 'B' && asyncbuf[1] == 'Y') {

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
    } else if (asyncbuf[0] == 'B' && asyncbuf[1] == 'C') {

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
#if 0
    } else if (asyncbuf[0] == 'V' && asyncbuf[1] == 'M' && asyncbuf[2] == 'C') {

        vfo_t vfo;
        vfo_t bandmode;

        retval = sscanf(asyncbuf, "VMC %d,%d", &vfo, &bandmode);
        if (retval != 1) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VMC message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }

        /* FIXME: I am not sure how to encode this command into Hamlib at the moment */
        vfo = (vfo == 0) ? RIG_VFO_A : RIG_VFO_B;
        switch (bandmode) {
            case 0:     bandmode = RIG_VFO_VFO;  break;
            case 2:     bandmode = RIG_VFO_MEM;  break;
           //case 3:     bandmode = RIG_VFO_CALL; break;
            default:    bandmode = RIG_VFO_CURR; break; /*FIXME: what is a better value? */
        }
        rig_debug(RIG_DEBUG_TRACE, "%s: Mode of Band event - vfo = %d, %d\n", __FUNCTION__, vfo, bandmode);

        /* TODO: This event does not have a callback! */
        return -RIG_ENIMPL;
#endif
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
    char freqbuf[24], ackbuf[ACKBUF_LEN];
    int retval, step;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    step = 0;
    if (step < 0 || step > 9)
        return -RIG_EINVAL;

    sprintf(freqbuf, "FQ %011Ld,%d" EOM, (long long)freq, step);
    retval = th_transaction(rig, freqbuf, ackbuf, sizeof(ackbuf));
    if (retval != RIG_OK)
        return RIG_OK;

    if (ackbuf[0] != 'F' || ackbuf[1] != 'Q') {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * th_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int
th_get_freq (RIG *rig, vfo_t vfo, freq_t *freq)
{
    char vch, freqbuf[ACKBUF_LEN];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    *freq = 0;
    vch = '0';
    /* TODO: What about the VFO? */

    retval = th_transaction (rig, "FQ" EOM, freqbuf, sizeof(freqbuf));
    if (retval != RIG_OK)
        return retval;

    retval = sscanf(freqbuf, "FQ %"FREQFMT, freq);
    if (retval != 1) {
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
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	switch (vfo) {
      case RIG_VFO_CURR: break;
	  default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_EINVAL;
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
    retval = th_transaction (rig, mdbuf, ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK)
        return retval;

    if (ackbuf[0] != 'M' || ackbuf[1] != 'D') {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

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
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    /* TODO: What about the VFO? */
    vch = '0';

    retval = th_transaction (rig, "MD" EOM, ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK)
		return retval;

    if (strlen(ackbuf) < 4 || ackbuf[0] != 'M' || ackbuf[1] != 'D') {
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
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	switch (vfo) {
        case RIG_VFO_A:
            sprintf(vfobuf, "BC 0" EOM);
            break;
        case RIG_VFO_B:
            sprintf(vfobuf, "BC 1" EOM);
            break;

#if 0   /* FIXME: don't know if this is the right place */
        case RIG_VFO_VFO:
            sprintf(vfobuf, "VMC0,0" EOM);
            break;
        case RIG_VFO_MEM:
            sprintf(vfobuf, "VMC0,2" EOM);
            break;
        case RIG_VFO_CALL:
            sprintf(vfobuf, "VMC0,3" EOM);
            break;
#endif
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
            return -RIG_EINVAL;
	}

    retval = th_transaction(rig, vfobuf, ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK)
        return retval;

    if (ackbuf[0] != 'B' || ackbuf[1] != 'C') {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

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
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = th_transaction (rig, "BC" EOM, ackbuf, sizeof(ackbuf));
    if (retval != RIG_OK)
        return retval;

    if (strlen(ackbuf) < 4 || ackbuf[0] != 'B' || ackbuf[1] != 'C') {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

    switch (ackbuf[3]) {
        case '0': *vfo = RIG_VFO_A; break;
        case '1': *vfo = RIG_VFO_B; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VFO value '%c'\n", __FUNCTION__, ackbuf[3]);
            return -RIG_ERJCTED;
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
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    sprintf(trnbuf, "AI %c" EOM, (trn == RIG_TRN_RIG) ? '1' : '0');
    retval = th_transaction (rig, trnbuf, ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK)
		return retval;

	if (ackbuf[0] != 'A' || ackbuf[1] != 'I') {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
	}

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
	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = th_transaction(rig, "AI" EOM, ackbuf, sizeof(ackbuf));
	if (retval != RIG_OK)
		return retval;

    if (strlen(ackbuf) < 4 || ackbuf[0] != 'A' || ackbuf[1] != 'I') {
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
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = th_transaction (rig, cmd, ackbuf, sizeof(ackbuf));
    if (retval != RIG_OK)
        return retval;

    if (strlen(ackbuf) < 4 || ackbuf[0] != cmd[0] || ackbuf[1] != cmd[1]) {
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
    int retval, v, l;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	switch (vfo) {
        case RIG_VFO_A: vch = '0'; break;
        case RIG_VFO_B: vch = '1'; break;
        case RIG_VFO_CURR: vch = '0'; break;    // FIXME: which one do I choose
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
            return -RIG_EINVAL;
	}

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level) {
        case RIG_LEVEL_STRENGTH:
            sprintf(lvlbuf, "SM %c" EOM, vch);
            retval = th_transaction (rig, lvlbuf, ackbuf, sizeof(ackbuf));
			if (retval != RIG_OK)
				return retval;

            retval = sscanf(ackbuf, "SM %d,%d", &v, &l);
            if (retval != 2 || l < 0 || l > 5) {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
                return -RIG_ERJCTED;
            }

            /* FIXME: should be in dB ! */
            val->i = l;
			break;

        case RIG_LEVEL_SQL:
            sprintf(lvlbuf, "SQ %c" EOM, vch);
            retval = th_transaction (rig, lvlbuf, ackbuf, sizeof(ackbuf));
			if (retval != RIG_OK)
				return retval;

            retval = sscanf(ackbuf, "SQ %d,%d", &v, &l);
            if (retval != 2 || l < 0 || l > 5) {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
                return -RIG_ERJCTED;
            }

            /* range [0.0 ... 1.0] */
            val->f = l / 5;
			break;

        case RIG_LEVEL_BALANCE:
            return -RIG_ENIMPL;
        case RIG_LEVEL_METER:
            return -RIG_ENAVAIL;
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
    unsigned char tonebuf[16], ackbuf[ACKBUF_LEN];
    int i, retval;
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
    retval = th_transaction(rig, tonebuf, ackbuf, sizeof(ackbuf));
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
	unsigned int tone_idx;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	caps = rig->caps;

    retval = th_transaction (rig, "CTN" EOM, ackbuf, sizeof(ackbuf));
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
th_get_info(RIG *rig)		/* I hope this is the correct function for this command. */
{
	static unsigned char firmbuf[16];
	int retval, firm_len = 0;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	memset(firmbuf, 0, sizeof(firmbuf));
	retval = kenwood_transaction (rig, "ID" EOM, 3, firmbuf, &firm_len);
	if (retval != RIG_OK)
		return NULL;
#if 1
	if (firm_len != 6) {
		rig_debug(RIG_DEBUG_ERR,"%s: wrong answer len=%d\n",
			__FUNCTION__, firm_len);
        return NULL;
    }
#endif
	if (firmbuf[0] != 'I' || firmbuf[1] != 'D') {
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
	int retval, mem_len, ack_len = 0;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);
	memset(ackbuf, 0, sizeof(ackbuf));

	if (ch < 0 || ch > 200) {
		rig_debug(RIG_DEBUG_ERR, "%s: channel num out of range: %d\n", __FUNCTION__, ch);
		return -RIG_EINVAL;
	}
	switch (vfo) {
	  case RIG_VFO_A: vsel = '0'; break;
	  case RIG_VFO_B: vsel = '1'; break;
	  default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_EINVAL;
	}

	mem_len = sprintf(membuf, "MC %c,%i" EOM, vsel, ch);
	retval = kenwood_transaction(rig, membuf, mem_len, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ackbuf[0] == 'N' && ackbuf[1] == '\n') {
		rig_debug(RIG_DEBUG_ERR, "%s: negative acknowledgment\n", __FUNCTION__);
		return -RIG_ERJCTED;
	}

	if (ackbuf[0] != 'M' || ackbuf[1] != 'C') {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected reply '%s', len=%d\n",
				__FUNCTION__, ackbuf, ack_len);
		return -RIG_ERJCTED;
	}

	return RIG_OK;
}

int
th_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	unsigned char *membuf, ackbuf[16];
	int retval, mem_len, ack_len = 0;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);
	memset(ackbuf, 0, sizeof(ackbuf));

	switch (vfo) {
	  case RIG_VFO_A:
		membuf = "MC 0" EOM;
		break;
	  case RIG_VFO_B:
		membuf = "MC 1" EOM;
		break;
	  case RIG_VFO_CURR:
		membuf = "MC" EOM;
		break;
	  default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_EINVAL;
	}

	mem_len = strlen(membuf);
	retval = kenwood_transaction(rig, membuf, mem_len, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ackbuf[0] == 'N' && ackbuf[1] == '\n') {
		rig_debug(RIG_DEBUG_ERR, "%s: negative acknowledgment\n", __FUNCTION__);
		return -RIG_ERJCTED;
	}

	if (ackbuf[0] != 'M' || ackbuf[1] != 'C') {
		rig_debug(RIG_DEBUG_ERR, "%s: unexpected reply '%s', len=%d\n",
				__FUNCTION__, ackbuf, ack_len);
		return -RIG_ERJCTED;
	}

    *ch = atoi(&membuf[3]);
#if 0
	if (*ch < 0 || *ch > 200) {
		rig_debug(RIG_DEBUG_ERR, "%s: channel num out of range: %d\n",
				__FUNCTION__, ch);
		return -RIG_EINVAL;
	}
#endif
	return RIG_OK;
}

/* end of file */
