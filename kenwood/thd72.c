/*
 *  Hamlib Kenwood TH-D72 backend
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (c) 2018 by Brian Lucas
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "kenwood.h"
#include "th.h"
#include "num_stdio.h"
#include "iofunc.h"
#include "serial.h"
#include "misc.h"


#define THD72_MODES	(RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_AM)
#define THD72_MODES_TX	(RIG_MODE_FM|RIG_MODE_FMN)

#define THD72_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_MON|    \
                       RIG_FUNC_SQL|    \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO)

#define THD72_LEVEL_ALL (RIG_LEVEL_RFPOWER|\
			RIG_LEVEL_SQL|\
			RIG_LEVEL_BALANCE|\
			RIG_LEVEL_VOXGAIN|\
                        RIG_LEVEL_VOXDELAY)

#define THD72_PARMS	(RIG_PARM_APO|\
			RIG_PARM_TIME)

#define THD72_VFO_OP (RIG_OP_NONE)

#define THD72_VFO (RIG_VFO_A|RIG_VFO_B)

static rmode_t thd72_mode_table[3] = {
    [0] = RIG_MODE_FM,	/* normal, but narrow compared to broadcast */
    [1] = RIG_MODE_FMN,	/* what kenwood calls narrow */
    [2] = RIG_MODE_AM,
};

static pbwidth_t thd72_width_table[3] = {
    [0] = 10000,	// +-5KHz
    [1] =  5000,	// +-2.5KHz
    [2] = 10000,	// what should this be?
};

static rptr_shift_t thd72_rshf_table[3] = {
    [0] = RIG_RPT_SHIFT_NONE,
    [1] = RIG_RPT_SHIFT_PLUS,
    [2] = RIG_RPT_SHIFT_MINUS,
} ;
static int thd72voxdelay[7] = {
    [0] =  2500,
    [1] =  5000,
    [2] =  7500,
    [3] = 10000,
    [4] = 15000,
    [5] = 20000,
    [6] = 30000
};

static float thd72sqlevel[6] = {
    [0] = 0.0,		/* open */
    [1] = 0.2,
    [2] = 0.4,
    [3] = 0.6,
    [4] = 0.8,
    [5] = 1.0
};

static int thd72apo[4] = {
    [0] = 0,
    [1] = 15,
    [2] = 30,
    [3] = 60
};

static struct kenwood_priv_caps thd72_priv_caps = {
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = thd72_mode_table,
};



int thd72_open(RIG *rig)
{
    int ret;
    struct kenwood_priv_data *priv = rig->state.priv;
    strcpy (priv->verify_cmd, "ID\r");

    ret = kenwood_transaction(rig, "", NULL, 0);

    return ret;
}

static int thd72_set_vfo(RIG *rig, vfo_t vfo)
{
    const char *cmd;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (vfo) {
	case RIG_VFO_A:
	case RIG_VFO_VFO:
	case RIG_VFO_MAIN:
	    cmd = "BC 0";
	    break;

	case RIG_VFO_B:
	    cmd = "BC 1";
	    break;
	default:
	    rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %d\n", __func__, vfo);
	    return -RIG_ENTARGET;
    }
    return kenwood_simple_transaction(rig, cmd, 4);
}

static int thd72_get_vfo(RIG *rig, vfo_t *vfo)
{
    int retval;
    char c, buf[10];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = kenwood_transaction(rig, "BC", buf, sizeof (buf));
    if (retval != RIG_OK)
	return retval;
    size_t length = strlen (buf);
    if (length == 4) {
	c = buf[3];
    } else {
	rig_debug(RIG_DEBUG_ERR, "%s: Unexpected answer length '%c'\n", __func__, length);
	return -RIG_EPROTO;
    }
    switch (c) {
	case '0': *vfo = RIG_VFO_A; break;
	case '1': *vfo = RIG_VFO_B; break;
	default:
	    rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %d\n", __func__, vfo);
	    return -RIG_EVFO;
    }
    return RIG_OK;
}

static int thd72_vfoc(RIG *rig, vfo_t vfo, char *vfoc)
{
    vfo = (vfo == RIG_VFO_CURR) ? rig->state.current_vfo : vfo;
    switch (vfo) {
	case RIG_VFO_A: *vfoc = '0'; break;
	case RIG_VFO_B: *vfoc = '1'; break;
	default:
	    rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %d\n", __func__, vfo);
	    return -RIG_ENTARGET;
    }
    return RIG_OK;
}

static int thd72_get_freq_info(RIG *rig, vfo_t vfo, char *buf)
{
    int retval, length;
    char c, cmd[8];

    retval = thd72_vfoc(rig, vfo, &c);
    if (retval != RIG_OK)
	return retval;
    sprintf(cmd, "FO %c", c);
    retval = kenwood_transaction(rig, cmd, buf, 53);
    length = strlen(buf);
    rig_debug(RIG_DEBUG_TRACE, "%s: length=%d\n", __func__, length);
    return RIG_OK;
}

static int thd72_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    char buf[64], fbuf[11];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    sprintf(fbuf, "%010ld", (int64_t)freq);
    memcpy(buf+5, fbuf, 10);
    retval = kenwood_simple_transaction(rig, buf, 52);
    return retval;
}

static int thd72_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    sscanf(buf+5, "%"SCNfreq, freq);
    return RIG_OK;
}

static int thd72_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    char modec, buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    switch (mode) {
	case RIG_MODE_FM:  modec = '0'; break;
	case RIG_MODE_FMN: modec = '1'; break;
	case RIG_MODE_AM:  modec = '2'; break;
	default:
	    return -RIG_EINVAL;
    }
    buf[51] = modec;
    retval = kenwood_simple_transaction(rig, buf, 52);
    return retval;
}

static int thd72_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    char modec, buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    modec = buf[51];
    if (modec >= '0' && modec <= '2') {
	*mode = thd72_mode_table[modec - '0'];
	*width = thd72_width_table[modec - '0'];
    }
    else
	return -RIG_EINVAL;
    return RIG_OK;
}

static int thd72_set_rptr_shft(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    int retval;
    char shftc, buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    switch (rptr_shift) {
	case RIG_RPT_SHIFT_NONE:  shftc = '0'; break;
	case RIG_RPT_SHIFT_PLUS:  shftc = '1'; break;
	case RIG_RPT_SHIFT_MINUS: shftc = '1'; break;
	default:
	    return  -RIG_EINVAL;
    }
    buf[18] = shftc;
    retval = kenwood_simple_transaction(rig, buf, 52);
    return retval;
}

static int thd72_get_rptr_shft(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    int retval;
    char shftc, buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    shftc = buf[18];
    if (shftc >= '0' && shftc <= '2')
	*rptr_shift = thd72_rshf_table[shftc - '0'];
    else
	return -RIG_EINVAL;
    return retval;
}


static int thd72_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    int retval;
    char boff[9], buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    sprintf(boff, "%08ld", offs);
    memcpy(buf+42, boff, 8);
    retval = kenwood_simple_transaction(rig, buf, 52);
    return retval;
}

static int thd72_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs)
{
    int retval;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    sscanf(buf+42, "%ld", offs);
    return RIG_OK;
}

static int thd72_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    int retval;

    retval = -RIG_EINVAL;	/** TEMP **/
    return retval;
}

static int thd72_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int retval;
    int tinx;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);
    if (retval != RIG_OK)
	return retval;
    if (buf[22] == '0')	/* no tone */
	*tone = 0;
    else {
	sscanf(buf+30, "%d", &tinx);
	if (tinx >= 0 && tinx <= 41)
	    *tone = kenwood42_ctcss_list[tinx];
	else
	    return -RIG_EINVAL;
    }
    return RIG_OK;
}

static int thd72_get_menu_info(RIG *rig, char *buf)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = kenwood_transaction(rig, "MU", buf, 41);
    if (retval != RIG_OK)
	return retval;
    if (strlen(buf) != 40)
	return -RIG_ERJCTED;
    return RIG_OK;
}

static int thd72_get_menu_item(RIG* rig, int item, char hi, char *val)
{
    int retval;
    char c, buf[48];

    retval = thd72_get_menu_info(rig, buf);
    if (retval != RIG_OK)
	return retval;
    c = buf[3 + 2*item];	/* "MU 0,1,2 ... */
    if (c < '0' || c > hi)
	return -RIG_EPROTO;
    *val = c;
    return RIG_OK;
}

static int thd72_set_menu_item(RIG* rig, int item, int val)
{
    int retval;
    char buf[48];

    retval = thd72_get_menu_info(rig, buf);
    if (retval != RIG_OK)
	return retval;
    buf[3 + 2*item] = '0' + val;
    return kenwood_simple_transaction(rig, buf, 40);
}

static int thd72_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int retval, lvl;
    char c, lvlc, cmd[10];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_vfoc(rig, vfo, &c);
    if (retval != RIG_OK)
	return retval;
    switch (level) {
	case RIG_LEVEL_RFPOWER:
	    if (val.f <= 0.01) lvlc = '2';
	    else if (val.f <= 0.10) lvlc = '1';
	    else lvlc = '0';
	    sprintf(cmd, "PC %c,%c", c, lvlc);
	    return kenwood_simple_transaction(rig, cmd, 6);
	case RIG_LEVEL_VOXGAIN:
	    return thd72_set_menu_item(rig, 8, (int)(val.f*10.0 - 0.5));
	case RIG_LEVEL_VOXDELAY:
	    if (val.i > 20000) lvl = 6;
	    else if (val.i > 10000) lvl = val.i/10000 + 3;
	    else lvl = val.i/2500;
	    return thd72_set_menu_item(rig, 9, lvl);
	case RIG_LEVEL_SQL:
	    lvlc = '0' + (int)(val.f*5);
	    sprintf(cmd, "PC %c,%c", c, lvlc);
	    return kenwood_simple_transaction(rig, cmd, 6);
	case RIG_LEVEL_BALANCE:
	    /* FIXME - is balance 0.0 .. 1.0 or -1.0 .. 1.0? */
	    lvl = (int)(val.f*4.0);
	    return thd72_set_menu_item(rig, 13, lvl);
	default:
	    rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Level %d\n", __func__, level);
	    return -RIG_EINVAL;
    }
    return retval;
}

static int thd72_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval, v, l;
    char c, cmd[10], buf[48];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_vfoc(rig, vfo, &c);
    if (retval != RIG_OK)
	return retval;
    switch (level) {
	case RIG_LEVEL_RFPOWER:
	    sprintf(cmd, "PC %c", c);
	    retval = kenwood_transaction(rig, cmd, buf, sizeof (buf));
	    if (retval != RIG_OK)
		return retval;
	    retval = sscanf(buf, "PC %d,%d", &v, &l);
	    if (retval != 2 || l < 0 || l > 3) {
		rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
		return -RIG_ERJCTED;
	    }
	    switch (l) {
		case 0: val->f = 1.00; break;	/* 5.0 W */
		case 1: val->f = 0.10; break;	/* 500 mW */
		case 2: val->f = 0.01; break;	/* 50 mW */
	    }
	    break;
	case RIG_LEVEL_VOXGAIN:
	    retval = thd72_get_menu_item(rig, 8, '9', &c);
	    if (retval != RIG_OK)
		return retval;
	    /* FIXME - if VOX is off, what do we return */
	    val->f = (c - '0') / 9.0;
	    break;
	case RIG_LEVEL_VOXDELAY:
	    retval = thd72_get_menu_item(rig, 9, '7', &c);
	    if (retval != RIG_OK)
		return retval;
	    /* FIXME - if VOX is off, what do we return */
	    val->i = thd72voxdelay[c-'0'];
	    break;
	case RIG_LEVEL_SQL:
	    sprintf(cmd, "SQ %c", c);
	    retval = kenwood_transaction(rig, cmd, buf, sizeof (buf));
	    if (retval != RIG_OK)
		return retval;
	    retval = sscanf(buf, "SQ %d,%d", &v, &l);
	    if (retval != 2 || l < 0 || l > 6) {
		rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
		return -RIG_ERJCTED;
	    }
	    val->f = thd72sqlevel[l];
	    break;
	case RIG_LEVEL_BALANCE:
	    retval = thd72_get_menu_item(rig, 13, '4', &c);
	    if (retval != RIG_OK)
		return retval;
	    /* FIXME - is balance 0.0 .. 1.0 or -1.0 .. 1.0? */
		val->f = (float)(c-'4')/4.0;
	    break;
	default:
	    rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Level %d\n", __func__, level);
	    return -RIG_EINVAL;
	}
	return RIG_OK;
}

static int thd72_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int retval;
    char c;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (func) {
	case RIG_FUNC_AIP:
	    retval = thd72_vfoc(rig, vfo, &c);
	    if (retval != RIG_OK)
		return retval;
	    return thd72_set_menu_item(rig, c == '0'?5:6, status);
	default:
	    return -RIG_EINVAL;
    }
    return RIG_OK;
}

static int thd72_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int retval;
    char c;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (func) {
	case RIG_FUNC_AIP:
	    retval = thd72_vfoc(rig, vfo, &c);
	    if (retval != RIG_OK)
		return retval;
	    retval = thd72_get_menu_item(rig, c == '0'?5:6, '1', &c);
	    if (retval != RIG_OK)
		return retval;
	    *status = c - '0';
	    break;
	default:
	    return -RIG_EINVAL;
    }
    return RIG_OK;
}

static int thd72_set_parm(RIG *rig, setting_t parm, value_t val)
{
    int lvl;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (parm) {
	case RIG_PARM_APO:
	    if (val.i == 0) lvl = 0;
	    else if (val.i <= 15) lvl = 1;
	    else if (val.i <= 30) lvl = 2;
	    else lvl = 3;
	    return thd72_set_menu_item(rig, 3, lvl);
	case RIG_PARM_TIME:
	default:
	    return -RIG_EINVAL;
    }
    return RIG_OK;
}

static int thd72_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    int retval, hh, mm, ss;
    char c, buf[48];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (parm) {
	case RIG_PARM_APO:
	    retval = thd72_get_menu_item(rig, 3, '3', &c);
	    if (retval != RIG_OK)
		return retval;
	    val->i = thd72apo[c-'0'];
	    break;
	case RIG_PARM_TIME:
	    retval = kenwood_transaction(rig, "RT", buf, sizeof (buf));
	    if (retval != RIG_OK)
		return retval;
	    sscanf(buf+11, "%2d%2d%2d", &hh, &mm, &ss);
	    val->i = ss + 60*(mm + 60*hh);
	    break;
	default:
	    return -RIG_EINVAL;
    }
    return RIG_OK;
}

#define CMD_SZ 5
#define BLOCK_SZ 256
#define BLOCK_COUNT 256

static int thd72_get_block (RIG *rig, int block_num, char *block)
{
    hamlib_port_t *rp = &rig->state.rigport;
    char cmd[CMD_SZ] = "R\0\0\0\0";
    char resp[CMD_SZ];
    int ret;

    /* fetching block i */
    cmd[2] = block_num & 0xff;

    ret = write_block(rp, cmd, CMD_SZ);
    if (ret != RIG_OK)
        return ret;

    /* read response first */
    ret = read_block(rp, resp, CMD_SZ);
    if (ret != RIG_OK)
        return ret;

    if (resp[0] != 'W' || memcmp(cmd+1, resp+1, CMD_SZ-1))
        return -RIG_EPROTO;

    /* read block */
    ret = read_block(rp, block, BLOCK_SZ);
    if (ret != BLOCK_SZ)
        return ret;

    ret = write_block(rp, "\006", 1);
    if (ret != RIG_OK)
        return ret;

    ret = read_block(rp, resp, 1);
    if (ret != 1)
        return ret;

    if (resp[0] != 0x06)
        return -RIG_EPROTO;

    return RIG_OK;
}

int thd72_get_chan_all_cb (RIG * rig, chan_cb_t chan_cb, rig_ptr_t arg)
{
    int i, j, ret;
    hamlib_port_t *rp = &rig->state.rigport;
	channel_t *chan;
	chan_t *chan_list = rig->state.chan_list;
	int chan_next = chan_list[0].start;
    char block[BLOCK_SZ];
    char resp[CMD_SZ];

    ret = kenwood_transaction(rig, "0M PROGRAM", NULL, 0);
    if (ret != RIG_OK)
        return ret;

    rp->parm.serial.rate = 57600;

    serial_setup(rp);

    /* let the pcr settle and flush any remaining data*/
    usleep(100*1000);
    serial_flush(rp);

    /* setRTS or Hardware flow control? */
    ret = ser_set_rts(rp, 1);
    if (ret != RIG_OK)
        return ret;

	/*
	 * setting chan to NULL means the application
	 * has to provide a struct where to store data
	 * future data for channel channel_num
	 */
    chan = NULL;
    ret = chan_cb(rig, &chan, chan_next, chan_list, arg);
    if (ret != RIG_OK)
        return ret;
    if (chan == NULL)
        return -RIG_ENOMEM;


    for (i=0; i<BLOCK_COUNT; i++) {

        ret = thd72_get_block (rig, i, block);
        if (ret != RIG_OK)
            return ret;

        /*
         * Most probably, there's 64 bytes per channel (256*256 / 1000+)
         */
#define CHAN_PER_BLOCK 4

		for (j=0; j<CHAN_PER_BLOCK; j++) {

            char *block_chan = block + j*(BLOCK_SZ/CHAN_PER_BLOCK);

            memset(chan, 0, sizeof(channel_t));
			chan->vfo = RIG_VFO_MEM;
			chan->channel_num = i*CHAN_PER_BLOCK + j;

            /* What are the extra 64 channels ? */
            if (chan->channel_num >= 1000)
                break;

            /* non-empty channel ? */
            // if (block_chan[0] != 0xff) {
	    // since block_chan is *signed* char, this maps to -1
            if (block_chan[0] != -1) {

                memcpy(chan->channel_desc, block_chan, 8);
                /* TODO: chop off trailing chars */
                chan->channel_desc[8] = '\0';

                /* TODO: parse block and fill in chan */
            }

			/* notify the end? */
			chan_next = chan_next < chan_list[i].end ? chan_next+1 : chan_next;

			/*
			 * provide application with channel data,
			 * and ask for a new channel structure
			 */
			chan_cb(rig, &chan, chan_next, chan_list, arg);
        }
    }

    ret = write_block(rp, "E", 1);
    if (ret != RIG_OK)
        return ret;

    ret = read_block(rp, resp, 1);
    if (ret != 1)
        return ret;

    if (resp[0] != 0x06) {
        return -RIG_EPROTO;
    }

    /* setRTS?? getCTS needed? */
    ret = ser_set_rts(rp, 1);
    if (ret != RIG_OK)
        return ret;

    return RIG_OK;
}

/*
 * th-d72a rig capabilities.
 */
const struct rig_caps thd72a_caps = {
.rig_model =  RIG_MODEL_THD72A,
.model_name = "TH-D72A",
.mfg_name =  "Kenwood",
.version =  TH_VER ".1",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_HANDHELD|RIG_FLAG_APRS|RIG_FLAG_TNC|RIG_FLAG_DXCLUSTER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_XONXOFF,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  250,
.retry =  3,

.has_get_func =  THD72_FUNC_ALL,
.has_set_func =  THD72_FUNC_ALL,
.has_get_level =  THD72_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(THD72_LEVEL_ALL),
.has_get_parm =  THD72_PARMS,
.has_set_parm =  THD72_PARMS,    /* FIXME: parms */
.level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 5 } },
        [LVL_SQL] = { .min = { .i = 0 }, .max = { .i = 5 } },
        [LVL_RFPOWER] = { .min = { .i = 2 }, .max = { .i = 0 } },
},
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.vfo_ops =  THD72_VFO_OP,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  6, /* TBC */

.chan_list =  {
                {  0,  999, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* TBC MEM */
                RIG_CHAN_END,
                   },

.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
    {MHz(118),MHz(174),THD72_MODES,-1,-1,THD72_VFO},
    {MHz(320),MHz(524),THD72_MODES,-1,-1,THD72_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {MHz(144),MHz(148),THD72_MODES_TX,W(0.05),W(5),THD72_VFO},
    {MHz(430),MHz(440),THD72_MODES_TX,W(0.05),W(5),THD72_VFO},
	RIG_FRNG_END,
  }, /* tx range */

.tuning_steps =  {
	 {THD72_MODES,kHz(5)},
	 {THD72_MODES,kHz(6.25)},
     /* kHz(8.33)  ?? */
	 {THD72_MODES,kHz(10)},
	 {THD72_MODES,kHz(12.5)},
	 {THD72_MODES,kHz(15)},
	 {THD72_MODES,kHz(20)},
	 {THD72_MODES,kHz(25)},
	 {THD72_MODES,kHz(30)},
	 {THD72_MODES,kHz(50)},
	 {THD72_MODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_FM, kHz(14)},
		{RIG_MODE_FMN, kHz(7)},
		{RIG_MODE_AM, kHz(9)},
		RIG_FLT_END,
	},
.priv =  (void *)&thd72_priv_caps,

.rig_init = kenwood_init,
.rig_cleanup = kenwood_cleanup,
.rig_open = thd72_open,
.set_freq = thd72_set_freq,
.get_freq = thd72_get_freq,
.set_mode = thd72_set_mode,
.get_mode = thd72_get_mode,
.set_vfo =  thd72_set_vfo,
.get_vfo =  thd72_get_vfo,
.set_rptr_shift = thd72_set_rptr_shft,
.get_rptr_shift = thd72_get_rptr_shft,
.set_rptr_offs = thd72_set_rptr_offs,
.get_rptr_offs = thd72_get_rptr_offs,
.set_ctcss_tone =  thd72_set_ctcss_tone,
.get_ctcss_tone =  thd72_get_ctcss_tone,
// set/get dcs_code
//.set_ctcss_sql =  th_set_ctcss_sql,
//.get_ctcss_sql =  th_get_ctcss_sql,
.set_level = thd72_set_level,
.get_level = thd72_get_level,
.set_func = thd72_set_func,
.get_func = thd72_get_func,
.set_parm = thd72_set_parm,
.get_parm = thd72_get_parm,
.get_chan_all_cb = thd72_get_chan_all_cb,

.get_info =  th_get_info,

};
