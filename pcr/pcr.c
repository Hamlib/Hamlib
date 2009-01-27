/*
 *  Hamlib PCR backend - main file
 *  Copyright (c) 2001-2005 by Stephane Fillod and Darren Hatcher
 *  Copyright (C) 2007-09 by Alessandro Zummo <a.zummo@towertech.it>
 *
 *	$Id: pcr.c,v 1.24 2009-01-27 19:05:59 fillods Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* String function definitions */
#include <unistd.h>		/* UNIX standard function definitions */
#include <math.h>
#include <errno.h>
#include <ctype.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "cal.h"
#include "pcr.h"

/*
 * modes in use by the "MD" command
 */
#define MD_LSB	'0'
#define MD_USB	'1'
#define MD_AM	'2'
#define MD_CW	'3'
#define MD_FM	'5'
#define MD_WFM	'6'

/* define 2.8kHz, 6kHz, 15kHz, 50kHz, and 230kHz */
#define FLT_2_8kHz	'0'
#define FLT_6kHz	'1'
#define FLT_15kHz	'2'
#define FLT_50kHz	'3'
#define FLT_230kHz	'4'

/* as returned by GD? */
#define OPT_UT106 (1 << 0)
#define OPT_UT107 (1 << 4)

/*
 * CTCSS sub-audible tones for PCR100 and PCR1000
 * Don't even touch a single bit! indexes will be used in the protocol!
 * 51 tones, the 60.0 Hz tone is missing.
 */
const tone_t pcr_ctcss_list[] = {
	670,  693,  710,  719,  744,  770,  797,  825,  854,  885,  915,
	948,  974,  1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
	1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
	1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
	2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
	0,
};


struct pcr_country
{
	int id;
	char *name;
};

struct pcr_country pcr_countries[] = {
	{ 0x00, "Japan" },
	{ 0x01, "USA" },
	{ 0x02, "EUR/AUS" },
	{ 0x03, "FRA" },
	{ 0x04, "DEN" },
	{ 0x05, "CAN" },
	{ 0x06, "Generic 1" },
	{ 0x07, "Generic 2" },
	{ 0x08, "FCC Japan" },
	{ 0x09, "FCC USA" },
	{ 0x0A, "FCC EUR/AUS" },
	{ 0x0B, "FCC FRA" },
	{ 0x0C, "FCC DEN" },
	{ 0x0D, "FCC CAN" },
	{ 0x0E, "FCC Generic 1" },
	{ 0x0F, "FCC Generic 2" },
};

#define PCR_COUNTRIES (sizeof(pcr_countries) / sizeof(struct pcr_country))

static int
pcr_read_block(RIG *rig, char *rxbuffer, size_t count)
{
	int err;
	int read = 0, tries = 4;

	struct rig_state *rs = &rig->state;
        struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

	rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

	/* already in sync? */
	if (priv->sync)
		return read_block(&rs->rigport, rxbuffer, count);

	/* read first char */
	do {
		char *c = &rxbuffer[0];

		err = read_block(&rs->rigport, &rxbuffer[0], 1);
		if (err < 0)
			return err;

		if (err != 1)
			return -RIG_EPROTO;

		/* validate char */
		if (!(*c == 'I' || *c == 'G' || *c == 'N' || *c == 'H'))
			continue;

		/* sync ok, read remaining chars */
		read++;
		count--;

		err = read_block(&rs->rigport, &rxbuffer[1], count);
		if (err < 0) {
			rig_debug(RIG_DEBUG_ERR, "%s: read failed - %s\n",
				__func__, strerror(errno));

			return err;
		}

		if (err == count) {
			read += err;
			priv->sync = 1;
		}

		rig_debug(RIG_DEBUG_TRACE, "%s: RX %d bytes\n", __func__, read);

		return read;

	} while (--tries > 0);

	return -RIG_EPROTO;
}

/* expects a 4 byte buffer to parse */
static int
pcr_parse_answer(RIG *rig, char *buf, int len)
{
	struct rig_state *rs = &rig->state;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

	rig_debug(RIG_DEBUG_TRACE, "%s: len = %d\n", __func__, len);

	if (len != 4) {
		priv->sync = 0;
		return -RIG_EPROTO;
	}

	if (strncmp("G000", buf, 4) == 0)
		return RIG_OK;

	if (strncmp("G001", buf, 4) == 0)
		return -RIG_ERJCTED;

	if (strncmp("H101", buf, 4) == 0)
		return RIG_OK;

	if (buf[0] == 'I' && buf[1] == '1') {
		switch (buf[1]) {
		case '1':
			sscanf(buf, "I1%02X", &priv->raw_level);
			return RIG_OK;

		case '3':
			rig_debug(RIG_DEBUG_WARN, "%s: DTMF %c\n",
				__func__, buf[3]);
			return RIG_OK;
		}
	} else if (buf[0] == 'G') {
		switch (buf[1]) {
		case '2': /* G2 */
			sscanf((char *) buf, "G2%d", &priv->protocol);
			return RIG_OK;

		case '4': /* G4 */
			sscanf((char *) buf, "G4%d", &priv->firmware);
			return RIG_OK;

		case 'D': /* GD */
			sscanf((char *) buf, "GD%d", &priv->options);
			return RIG_OK;

		case 'E': /* GE */
			sscanf((char *) buf, "GE%d", &priv->country);
			return RIG_OK;
		}
	}

	priv->sync = 0;

	return -RIG_EPROTO;

	/* XXX bandscope */
}

static int
pcr_send(RIG * rig, const char *cmd)
{
	int err;
	struct rig_state *rs = &rig->state;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

	int len = strlen(cmd);

	rig_debug(RIG_DEBUG_TRACE, "%s: cmd = %s, len = %d\n",
		__func__, cmd, len);

	/* XXX check max len */
	memcpy(priv->cmd_buf, cmd, len);

	/* append cr lf */
	/* XXX not required in auto update mode? */
	priv->cmd_buf[len+0] = 0x0d;
	priv->cmd_buf[len+1] = 0x0a;

	rs->hold_decode = 1;

	serial_flush(&rs->rigport);

	err = write_block(&rs->rigport, priv->cmd_buf, len + 2);

	rs->hold_decode = 0;

	return err;
}


static int
pcr_transaction(RIG * rig, const char *cmd)
{
	int err;
	char buf[4];
	struct rig_state *rs = &rig->state;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

	rig_debug(RIG_DEBUG_TRACE, "%s: cmd = %s\n",
		__func__, cmd);

	pcr_send(rig, cmd);

	/* the pcr does not give ack in auto update mode */
	if (priv->auto_update)
		return RIG_OK;

	err = pcr_read_block(rig, buf, 4);
	if (err < 0) {
		rig_debug(RIG_DEBUG_ERR,
			  "%s: read error, %s\n", __func__, strerror(errno));
		return err;
	}

	if (err != 4) {
		priv->sync = 0;
		return -RIG_EPROTO;
	}

	rig_debug(RIG_DEBUG_TRACE,
		  "%s: got %c%c%c%c\n", __func__, buf[0], buf[1], buf[2], buf[3]);

	return pcr_parse_answer(rig, buf, err);
}

static int
pcr_set_comm_speed(RIG *rig, int rate)
{
	int err;
	const char *rate_cmd;

	/* limit maximum rate */
	if (rate > 38400)
		rate = 38400;

	switch (rate) {
	case 300:
		rate_cmd = "G100";
		break;
	case 1200:
		rate_cmd = "G101";
		break;
	case 2400:
		rate_cmd = "G102";
		break;
	case 9600:
	default:
		rate_cmd = "G103";
		break;
	case 19200:
		rate_cmd = "G104";
		break;
	case 38400:
		rate_cmd = "G105";
		break;
	}

	rig_debug(RIG_DEBUG_VERBOSE, "%s: setting speed to %d with %s\n",
		__func__, rate, rate_cmd);

	/* the answer will be sent at the new baudrate,
	 * so we do not use pcr_transaction
	 */
	err = pcr_send(rig, rate_cmd);
	if (err != RIG_OK)
		return err;

	rig->state.rigport.parm.serial.rate = rate;
	serial_setup(&rig->state.rigport);

	/* check if the pcr is still alive */
	return pcr_check_ok(rig);
}


/* Basically, it sets up *priv */
int
pcr_init(RIG * rig)
{
	struct pcr_priv_data *priv;

	if (!rig)
		return -RIG_EINVAL;

	priv = (struct pcr_priv_data *) malloc(sizeof(struct pcr_priv_data));
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	memset(priv, 0x00, sizeof(struct pcr_priv_data));

	/*
	 * FIXME: how can we retrieve initial status?
	 * The protocol doesn't allow this.
	 */
	/* Some values are already at zero due to the memset above,
	 * but we reinitialize here for sake of completeness
	 */
	priv->country		= -1;
	priv->sync		= 0;
	priv->last_att		= 0;
	priv->last_agc		= 0;
	priv->last_ctcss_sql	= 0;
	priv->last_freq		= MHz(145);
	priv->last_mode		= MD_FM;
	priv->last_filter	= FLT_15kHz;

	rig->state.priv		= (rig_ptr_t) priv;
	rig->state.transceive	= RIG_TRN_OFF;

	return RIG_OK;
}

/*
 * PCR Generic pcr_cleanup routine
 * the serial port is closed by the frontend
 */
int
pcr_cleanup(RIG * rig)
{
	if (!rig)
		return -RIG_EINVAL;

	free(rig->state.priv);

	rig->state.priv = NULL;

	return RIG_OK;
}

/*
 * pcr_open
 * - send power on
 * - set auto update off
 *
 * Assumes rig!=NULL
 */
int
pcr_open(RIG * rig)
{
	struct rig_state *rs = &rig->state;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

	int err;
	int wanted_serial_rate;

	/* 
	 * initial communication is at 9600bps
	 * once the power is on, the serial speed can be changed with G1xx
	 */
	wanted_serial_rate = rs->rigport.parm.serial.rate;
	rs->rigport.parm.serial.rate = 9600;

	serial_setup(&rs->rigport);

	/* let the pcr settle and flush any remaining data*/
	usleep(100*1000);
	serial_flush(&rs->rigport);

	/* try powering on twice, sometimes the pcr answers H100 (off) */
	pcr_transaction(rig, "H101");
	err = pcr_transaction(rig, "H101");
	if (err != RIG_OK)
		return err;

	/* turn off auto update (just to be sure) */
	err = pcr_transaction(rig, "G300");
	if (err != RIG_OK)
		return err;

	/* turn squelch off */
	err = pcr_transaction(rig, "J4100");
	if (err != RIG_OK)
		return err;

	/* set volume to an acceptable default */
	err = pcr_set_volume(rig, 0.25);
	if (err != RIG_OK)
		return err;

	/* get device features */
	pcr_get_info(rig);

	/* tune to default last freq */
	pcr_set_freq(rig, 0, priv->last_freq);

	/* switch to different speed if requested */
	if (wanted_serial_rate != 9600 && wanted_serial_rate >= 300)
		return pcr_set_comm_speed(rig, wanted_serial_rate);

	return RIG_OK;
}

/*
 * pcr_close - send power off
 * Assumes rig!=NULL
 */
int
pcr_close(RIG * rig)
{
	/* when the pcr turns itself off sometimes we receive
	 * a malformed answer, so don't check for it.
	 */
	return pcr_send(rig, "H100");
}

/*
 * pcr_set_freq
 * Assumes rig!=NULL
 *
 * K0GMMMKKKHHHmmff00
 * GMMMKKKHHH is frequency GHz.MHz.KHz.Hz
 * mm is the mode setting
 *  00 = LSB
 *  01 = USB
 *  02 = AM
 *  03 = CW
 *  04 = Not used or Unknown
 *  05 = NFM
 *  06 = WFM
 * ff is the filter setting
 *  00 = 2.8 Khz (CW USB LSB AM)
 *  01 = 6.0 Khz (CW USB LSB AM NFM)
 *  02 = 15  Khz (AM NFM)
 *  03 = 50  Khz (AM NFM WFM)
 *  04 = 230 Khz (WFM)
 *
 */

int
pcr_set_freq(RIG * rig, vfo_t vfo, freq_t freq)
{
	struct pcr_priv_data *priv;
	unsigned char buf[20];
	int freq_len, err;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo = %d, freq = %.0f\n",
		  __func__, vfo, freq);

	priv = (struct pcr_priv_data *) rig->state.priv;

	freq_len = sprintf((char *) buf, "K0%010" PRIll "0%c0%c00",
			   (long long) freq,
			   priv->last_mode, priv->last_filter);

	buf[freq_len] = '\0';

	err = pcr_transaction(rig, (char *) buf);
	if (err != RIG_OK)
		return err;

	priv->last_freq = freq;

	return RIG_OK;
}

/*
 * pcr_get_freq
 * frequency can't be read back, so report the last one that was set.
 * Assumes rig != NULL, freq != NULL
 */
int
pcr_get_freq(RIG * rig, vfo_t vfo, freq_t * freq)
{
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	*freq = priv->last_freq;

	return RIG_OK;
}

/*
 * pcr_set_mode
 * Assumes rig != NULL
 */

int
pcr_set_mode(RIG * rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	unsigned char buf[20];
	int buf_len, err;
	int pcrmode, pcrfilter;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: mode = %d (%s), width = %d\n",
		  __func__, mode, rig_strrmode(mode), width);

	/* XXX? */
	if (mode == RIG_MODE_NONE)
		mode = RIG_MODE_FM;

	/*
	 * not so sure about modes and filters
	 * as I write this from manual (no testing) --SF
	 */
	switch (mode) {
	case RIG_MODE_CW:
		pcrmode = MD_CW;
		break;
	case RIG_MODE_USB:
		pcrmode = MD_USB;
		break;
	case RIG_MODE_LSB:
		pcrmode = MD_LSB;
		break;
	case RIG_MODE_AM:
		pcrmode = MD_AM;
		break;
	case RIG_MODE_WFM:
		pcrmode = MD_WFM;
		break;
	case RIG_MODE_FM:
		pcrmode = MD_FM;
		break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %d\n",
			  __func__, mode);
		return -RIG_EINVAL;
	}

	if (width == RIG_PASSBAND_NORMAL)
		width = rig_passband_normal(rig, mode);

	rig_debug(RIG_DEBUG_VERBOSE, "%s: will set to %d\n",
		  __func__, width);

	switch (width) {
		/* nop, pcrfilter already set
		 * TODO: use rig_passband_normal instead?
		 */
	case s_kHz(2.8):
		pcrfilter = FLT_2_8kHz;
		break;
	case s_kHz(6):
		pcrfilter = FLT_6kHz;
		break;
	case s_kHz(15):
		pcrfilter = FLT_15kHz;
		break;
	case s_kHz(50):
		pcrfilter = FLT_50kHz;
		break;
	case s_kHz(230):
		pcrfilter = FLT_230kHz;
		break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported width %d\n",
			  __func__, width);
		return -RIG_EINVAL;
	}

	rig_debug(RIG_DEBUG_VERBOSE, "%s: filter set to %d (%c)\n",
		  __func__, width, pcrfilter);

	buf_len = sprintf((char *) buf, "K0%010" PRIll "0%c0%c00",
			(long long) priv->last_freq, pcrmode, pcrfilter);

	err = pcr_transaction(rig, (char *) buf);
	if (err != RIG_OK)
		return err;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: saving values\n",
		  __func__);

	priv->last_mode = pcrmode;
	priv->last_filter = pcrfilter;

	return RIG_OK;
}

/*
 * hack! pcr_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int
pcr_get_mode(RIG * rig, vfo_t vfo, rmode_t * mode, pbwidth_t * width)
{
	struct pcr_priv_data *priv;

	priv = (struct pcr_priv_data *) rig->state.priv;

	rig_debug(RIG_DEBUG_VERBOSE, "%s, last_mode = %c, last_filter = %c\n",  __func__,
		priv->last_mode, priv->last_filter);

	switch (priv->last_mode) {
	case MD_CW:
		*mode = RIG_MODE_CW;
		break;
	case MD_USB:
		*mode = RIG_MODE_USB;
		break;
	case MD_LSB:
		*mode = RIG_MODE_LSB;
		break;
	case MD_AM:
		*mode = RIG_MODE_AM;
		break;
	case MD_WFM:
		*mode = RIG_MODE_WFM;
		break;
	case MD_FM:
		*mode = RIG_MODE_FM;
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,
			  "pcr_get_mode: unsupported mode %d\n",
			  priv->last_mode);
		return -RIG_EINVAL;
	}

	switch (priv->last_filter) {
	case FLT_2_8kHz:
		*width = kHz(2.8);
		break;
	case FLT_6kHz:
		*width = kHz(6);
		break;
	case FLT_15kHz:
		*width = kHz(15);
		break;
	case FLT_50kHz:
		*width = kHz(50);
		break;
	case FLT_230kHz:
		*width = kHz(230);
		break;
	default:
		rig_debug(RIG_DEBUG_ERR, "pcr_get_mode: unsupported "
			  "width %d\n", priv->last_filter);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

/*
 * pcr_get_info
 * Assumes rig!=NULL
 */

const char *
pcr_get_info(RIG * rig)
{
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	char *country = NULL;

	pcr_transaction(rig, "G2?"); /* protocol */
	pcr_transaction(rig, "G4?"); /* firmware */
	pcr_transaction(rig, "GD?"); /* options */
	pcr_transaction(rig, "GE?"); /* country */

	/* translate country id to name */
	if (priv->country > -1) {
		int i;

		for (i = 0; i < PCR_COUNTRIES; i++) {
			if (pcr_countries[i].id == priv->country) {
				country = pcr_countries[i].name;
				break;
			}
		}

		if (country == NULL) {
			country = "Unknown";
			rig_debug(RIG_DEBUG_ERR,
				  "%s: unknown country code %#x, "
				  "please report to Hamlib maintainer\n",
				  __func__, priv->country);
		}
	} else {
		country = "Not queried yet";
	}

	sprintf(priv->info, "Firmware v%d.%d, Protocol v%d.%d, "
		"Optional devices:%s%s%s, Country: %s",
		priv->firmware / 10, priv->firmware % 10,
		priv->protocol / 10, priv->protocol % 10,
		priv->options & OPT_UT106 ? " DSP" : "",
		priv->options & OPT_UT107 ? " DARC" : "",
		priv->options ? "" : " none",
		country);

	rig_debug(RIG_DEBUG_VERBOSE, "%s: Firmware v%d.%d, Protocol v%d.%d, "
		"Optional devices:%s%s%s, Country: %s\n",
		__func__,
		priv->firmware / 10, priv->firmware % 10,
		priv->protocol / 10, priv->protocol % 10,
		priv->options & OPT_UT106 ? " DSP" : "",
		priv->options & OPT_UT107 ? " DARC" : "",
		priv->options ? "" : " none",
		country);

	return priv->info;
}



/* *********************************************************************** */
/* load of new stuff added in by Darren Hatcher - G0WCW                    */
/* *********************************************************************** */

/*
 * pcr_set_level called by generic set level handler
 *
 * We are missing a way to set the BFO on/off here,
 */

int
pcr_set_level(RIG * rig, vfo_t vfo, setting_t level, value_t val)
{
	int err = -RIG_ENIMPL;

	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	if (RIG_LEVEL_IS_FLOAT(level))
		rig_debug(RIG_DEBUG_VERBOSE, "%s: level = %d, val = %f\n",
				__func__, level, val.f);
        else
		rig_debug(RIG_DEBUG_VERBOSE, "%s: level = %d, val = %d\n",
				__func__, level, val.i);

	switch (level) {
	case RIG_LEVEL_ATT:
		/* This is only on or off, but hamlib forces to use set level
		 * and pass as a float. Here we'll use 0 for off and 1 for on.
		 * If someone finds out how to set the ATT for the PCR in dB, let us
		 * know and the function can be changed to allow a true set level.
		 *
		 * Experiment shows it seems to have an effect, but unsure by how many db
		 */
		return pcr_set_attenuator(rig, val.i);

	case RIG_LEVEL_IF:
		return pcr_set_if_shift(rig, val.i);

	case RIG_LEVEL_AGC:
		/* this is implemented as a level even though it is a binary function
		 * as far as PCR is concerned. There is no AGC on/off for a "set func",
		 * so done here is a set level
		 */
		return pcr_set_agc(rig, val.i);

	/* floats */

	case RIG_LEVEL_AF:
		/* "val" can be 0.0 to 1.0 float which is 0 to 255 levels
		 * 0.3 seems to be ok in terms of loudness
		 */
		return pcr_set_volume(rig, val.f);

	case RIG_LEVEL_SQL:
		/* "val" can be 0.0 to 1.0 float
		 *      .... rig supports 0 to FF - look at function for
		 *      squelch "bands"
		 */
		err = pcr_set_squelch(rig, (val.f * 0xFF));
		if (err == RIG_OK)
			priv->squelch = val.f;
		return err;

	case RIG_LEVEL_NR:
		/* This selectss the DSP unit - this isn't a level per se,
		 * but in the manual it says that we have to switch it on first
		 * we'll assume 1 is for the UT-106, and anything else as off
		 *
		 * Later on we can set if the DSP features are on or off in set_func
		 */
		return pcr_set_dsp(rig, (int) val.f);
	}

	return err;
}

/*
 * pcr_get_level
 *
 * This needs a set of stored variables as the PCR doesn't return the current status of settings.
 * So we'll need to store them as we go along and keep them in sync.
 */

int
pcr_get_level(RIG * rig, vfo_t vfo, setting_t level, value_t * val)
{
	int err;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

//	rig_debug(RIG_DEBUG_TRACE, "%s: level = %d\n", __func__, level);

	switch (level) {
	case RIG_LEVEL_SQL:
		val->f = priv->squelch;
		return RIG_OK;

	case RIG_LEVEL_AF:
		val->f = priv->volume;
		return RIG_OK;

	case RIG_LEVEL_STRENGTH:
		if (priv->auto_update == 0) {
			err = pcr_transaction(rig, "I1?");
			if (err != RIG_OK)
				return err;
		}

		val->i = rig_raw2val(priv->raw_level, &rig->caps->str_cal);
/*		rig_debug(RIG_DEBUG_TRACE, "%s, raw = %d, converted = %d\n",
				 __func__, priv->raw_level, val->i);
*/
		return RIG_OK;

	case RIG_LEVEL_RAWSTR:
		if (priv->auto_update == 0) {
			err = pcr_transaction(rig, "I1?");
			if (err != RIG_OK)
				return err;
		}

		val->i = priv->raw_level;
		return RIG_OK;

	case RIG_LEVEL_IF:
		val->i = priv->last_shift;
		return RIG_OK;

	case RIG_LEVEL_ATT:
		val->i = priv->last_att;
		return RIG_OK;

	case RIG_LEVEL_AGC:
		val->i = priv->last_agc;
		return RIG_OK;
	}

	return -RIG_ENIMPL;
}


/*
 * pcr_set_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * This is missing a way to call the set DSP noise reducer, as we don't have a func to call it
 * based on the flags in rig.h -> see also missing a flag for setting the BFO.
 */
int
pcr_set_func(RIG * rig, vfo_t vfo, setting_t func, int status)
{
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %ld, func = %d\n", __func__,
		  status, func);

	switch (func) {
	case RIG_FUNC_NR: /* sets DSP noise reduction on or off */
		/* status = 00 for off or 01 for on
		 * Note that the user should switch the DSP unit on first
		 * using the set level function RIG_LEVEL_NR
		 */
		if (status == 1)
			return pcr_set_dsp_state(rig, 1);
		else
			return pcr_set_dsp_state(rig, 0);
		break;

	case RIG_FUNC_ANF: /* DSP auto notch filter */
		if (status == 1)
			return pcr_set_dsp_auto_notch(rig, 1);
		else
			return pcr_set_dsp_auto_notch(rig, 0);
		break;

	case RIG_FUNC_NB: /* noise blanker */
		if (status == 0)
			return pcr_set_nb(rig, 0);
		else
			return pcr_set_nb(rig, 1);

		break;

#if 0
	case RIG_FUNC_ANL: /* automatic noise limiter */
		if (status == 0)
			return pcr_set_anl(rig, 0);
		else
			return pcr_set_anl(rig, 1);

		break;
#endif

	case RIG_FUNC_TSQL:
		if (priv->last_mode != MD_FM)
			return -RIG_ERJCTED;

		if (status == 0)
			return pcr_set_ctcss_sql(rig, vfo, 0);
		else
			return pcr_set_ctcss_sql(rig, vfo, priv->last_ctcss_sql);

	default:
		rig_debug(RIG_DEBUG_VERBOSE, "%s: default\n");
		return -RIG_EINVAL;
	}
}

/*
 * pcr_get_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * This will need similar variables/flags as get_level. The PCR doesn't offer much in the way of
 *  confirmation of current settings (according to the docs).
 */
int
pcr_get_func(RIG * rig, vfo_t vfo, setting_t func, int *status)
{
	/* stub here ... */
	return -RIG_ENIMPL;
}

/* --------------------------------------------------------------------------------------- */
/* The next functions are all "helper types". These are called by the base functions above */
/* --------------------------------------------------------------------------------------- */
/*
 * Asks if the rig is ok = G0? response is G000 if ok or G001 if not
 *
 * Is only useful in fast transfer mode (when the CR/LF is stripped off all commands) ...
 * but also works on standard mode.
 */

static int
pcr_check_ok(RIG * rig)
{
	rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);
	return pcr_transaction(rig, "G0?");
}

static int
pcr_set_level_cmd(RIG * rig, char *base, int level)
{
	char buf[12];

	rig_debug(RIG_DEBUG_TRACE, "%s: base is %s, level is %d\n",
		  __func__, base, level);

	if (level < 0x0) {
		rig_debug(RIG_DEBUG_ERR, "%s: too low: %d\n",
			__func__, level);
		return -RIG_EINVAL;
	} else if (level > 0xff) {
		rig_debug(RIG_DEBUG_ERR, "%s: too high: %d\n",
			__func__, level);
		return -RIG_EINVAL;
	}

	sprintf(buf, "%s%02X", base, level);
	return pcr_transaction(rig, buf);
}

/*
 * Sets the volume of the rig to the level specified in the level integer.
 * Format is J40xx - where xx is 00 to FF in hex, and specifies 255 volume levels
 */

int
pcr_set_volume(RIG * rig, float level)
{
	int err;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	int hwlevel = level * 0xFF;

	rig_debug(RIG_DEBUG_TRACE, "%s: level = %f\n", __func__, level);

	err = pcr_set_level_cmd(rig, "J40", hwlevel);
	if (err == RIG_OK)
		priv->volume = level;

	return err;
}

/*
 * pcr_set_squelch
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Format is J41xx - where xx is 00 to FF in hex, and specifies 255 squelch levels
 *
 * Sets the squelch of the rig to the level specified in the level integer.
 * There are some bands though ...
 * 	00	Tone squelch clear and squelch open
 *	01-3f	Squelch open
 *	40-7f	Noise squelch
 *	80-ff	Noise squelch + S meter squelch ...
 *		 Comparative S level = (squelch setting - 128) X 2
 *
 *	Could do with some constatnts to add together to allow better (and more accurate)
 *	use of Hamlib API. Otherwise may get unexpected squelch settings if have to do by hand.
 */

int
pcr_set_squelch(RIG * rig, int level)
{
	rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);
	return pcr_set_level_cmd(rig, "J41", level);
}

/*
 * pcr_set_if_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the IF shift  of the rig to the level specified in the level integer.
 * 	IF-SHIFT position (in 256 stages, 80 = centre):
 *
 * 		< 80	Minus shift (in 10 Hz steps)
 * 		> 80	Plus shift (in 10 Hz steps)
 * 		  80	Centre
 *
 * Format is J43xx - where xx is 00 to FF in hex
 *
 */
int
pcr_set_if_shift(RIG * rig, int level)
{
	int err;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);

	err = pcr_set_level_cmd(rig, "J43", (level / 10) + 0x80);
	if (err == RIG_OK)
		priv->last_shift = level;

	return err;
}

/*
 * pcr_set_agc
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the AGC on or off based on the level specified in the level integer.
 * 	00 = off, 01 (non zero) is on
 *
 * Format is J45xx - where xx is 00 to 01 in hex
 *
 */
int
pcr_set_agc(RIG * rig, int status)
{
	int err;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);

	err = pcr_set_level_cmd(rig, "J45", status ? 1 : 0);
	if (err == RIG_OK)
		priv->last_agc = status ? 1 : 0;

	return err;
}

/*
 * pcr_set_nb(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the noise blanker on or off based on the level specified in the level integer.
 * 	00 = off, 01 (non zero) is on
 *
 * Format is J46xx - where xx is 00 to 01 in hex
 *
 */
int
pcr_set_nb(RIG * rig, int status)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);
	return pcr_set_level_cmd(rig, "J46", status ? 1 : 0);
}

/* Automatic Noise Limiter - J4Dxx - 00 off, 01 on */
int
pcr_set_anl(RIG * rig, int status)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);
	return pcr_set_level_cmd(rig, "J4D", status ? 1 : 0);
}

/*
 * pcr_set_attenuator(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the attenuator on or off based on the level specified in the level integer.
 * 	00 = off, 01 (non zero) is on
 * The attenuator seems to be fixed at ~ -20dB
 *
 * Format is J47xx - where xx is 00 to 01 in hex
 *
 */

int
pcr_set_attenuator(RIG * rig, int status)
{
	int err;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);

	err = pcr_set_level_cmd(rig, "J47", status ? 1 : 0);
	if (err == RIG_OK)
		priv->last_att = status;

	return err;
}

/*
 * pcr_set_bfo_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the BFO of the rig to the level specified in the level integer.
 * 	BFO-SHIFT position (in 256 stages, 80 = centre):
 *
 * 		< 80	Minus shift (in 10 Hz steps)
 * 		> 80	Plus shift (in 10 Hz steps)
 * 		  80	Centre
 *
 * Format is J4Axx - where xx is 00 to FF in hex, and specifies 255 squelch levels
 * XXX command undocumented?
 */
int
pcr_set_bfo_shift(RIG * rig, int level)
{
	rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);
	return pcr_set_level_cmd(rig, "J4A", level);
}

/*
 * pcr_set_dsp(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP to UT106 (01) or off (non 01)
 *
 */
int
pcr_set_dsp(RIG * rig, int level)
{
	rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);
	return pcr_set_level_cmd(rig, "J80", level);
}

/*
 * pcr_set_dsp_state(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP on or off (> 0 = on, 0 = off)
 *
 */

int
pcr_set_dsp_state(RIG * rig, int level)
{
	rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);
	return pcr_set_level_cmd(rig, "J81", level);
}

/*
 * pcr_set_dsp_noise_reducer(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP noise reducer on or off (0x01 = on, 0x00 = off)
 *  the level of NR set by values 0x01 to 0x10 (1 to 16 inclusive)
 */

int
pcr_set_dsp_noise_reducer(RIG * rig, int level)
{
	rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);
	return pcr_set_level_cmd(rig, "J82", level);
}

/*
 * pcr_set_dsp_auto_notch(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the auto notch on or off (1 = on, 0 = off)
 */

int
pcr_set_dsp_auto_notch(RIG * rig, int status) // J83xx
{
	rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, status);
	return pcr_set_level_cmd(rig, "J83", status ? 1 : 0);
}


int
pcr_set_vsc(RIG * rig, int status) // J50xx
{
	/* Not sure what VSC for so skipping the function here ... */
	return pcr_set_level_cmd(rig, "J50", status ? 1 : 0);
}

int pcr_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	*tone = priv->last_ctcss_sql;
	return RIG_OK;
}

int pcr_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
	int i, err;
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: tone = %d\n", __func__, tone);

	if (tone == 0)
		return pcr_transaction(rig, "J5100");

	for (i = 0; rig->caps->ctcss_list[i] != 0; i++) {
		if (rig->caps->ctcss_list[i] == tone)
                        break;
	}

	rig_debug(RIG_DEBUG_TRACE, "%s: index = %d, tone = %d\n",
			__func__, i, rig->caps->ctcss_list[i]);

	if (rig->caps->ctcss_list[i] != tone)
		return -RIG_EINVAL;

	err = pcr_set_level_cmd(rig, "J51", i + 1);
	if (err == RIG_OK)
		priv->last_ctcss_sql = tone;

	return RIG_OK;
}

int pcr_set_trn(RIG * rig, int trn)
{
	struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: trn = %d\n", __func__, trn);

	if (trn == RIG_TRN_OFF) {
		priv->auto_update = 0;
		return pcr_transaction(rig, "G300");
	}
	else if (trn == RIG_TRN_RIG) {
		priv->auto_update = 1;
		return pcr_send(rig, "G301");
	}
	else
		return -RIG_EINVAL;
}

int pcr_decode_event(RIG *rig)
{
	int err;
	char buf[4];

	err = pcr_read_block(rig, buf, 4);
	if (err == 4)
		return pcr_parse_answer(rig, buf, 4);

	return RIG_OK;
}


/* *********************************************************************************************
 * int pcr_set_comm_mode(RIG *rig, int mode_type);  // Set radio to fast/diagnostic mode  G3xx
 * int pcr_soft_reset(RIG *rig);                    // Ask rig to reset itself            H0xx
 ********************************************************************************************* */

DECLARE_INITRIG_BACKEND(pcr)
{
	rig_debug(RIG_DEBUG_VERBOSE, "pcr: init called\n");

	rig_register(&pcr100_caps);
	rig_register(&pcr1000_caps);

	return RIG_OK;
}
