/*
 *  Hamlib PCR backend - main file
 *  Copyright (c) 2001-2002 by Stephane Fillod
 *
 *		$Id: pcr.c,v 1.15 2002-03-26 08:05:51 fillods Exp $
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
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>

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
#define FLT_2_8kHz '0'
#define FLT_6kHz '1'
#define FLT_15kHz '2'
#define FLT_50kHz '3'
#define FLT_230kHz '4'

#define CRLF "\x0d\x0a"
#define EOM CRLF

/* as returned by GE? */
#define COUNTRY_JAPAN	0x08
#define	COUNTRY_USA		0x01
#define	COUNTRY_UK		0x02	/* TBC */
#define	COUNTRY_EUAUCA	0x0a
#define	COUNTRY_FGA		0x0b
#define	COUNTRY_DEN		0x0c

/* as returned by GD? */
#define OPT_UT106 (1<<0)
#define OPT_UT107 (1<<4)

/*
 * CTCSS sub-audible tones for PCR100 and PCR1000
 * Don't even touch a single bit! indexes will be used in the protocol!
 */
const int pcr1_ctcss_list[] = {
		670,  693, 710,  719,  744,  770,  797,  825,  854,  885,  915,
		948,  974, 1000, 1035, 1072, 1109, 1148, 1188,  1230, 1273,
		1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
		1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
		2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
		0,
};

/*
 * pcr_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
int pcr_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
	int retval;
	struct rig_state *rs;

	rs = &rig->state;

	serial_flush(&rs->rigport);

	retval = write_block(&rs->rigport, cmd, cmd_len);
	if (retval != RIG_OK)
			return retval;

	/* eat the first ack */
#ifdef WANT_READ_STRING
	retval = read_string(&rs->rigport, data, 1, "\x0a", 1);
	if (retval < 0)
			return retval;
	if (retval != 1)
			return -RIG_EPROTO;
#else
	retval = read_block(&rs->rigport, data, 1);
#endif

	/* here is the real response */
#ifdef WANT_READ_STRING
	*data_len = read_string(&rs->rigport, data, *data_len, "\x0a", 1);
#else
	*data_len = read_block( &rs->rigport, data, *data_len );
#endif

	return RIG_OK;
}

/*
 * Basically, it sets up *priv
 */
int pcr_init(RIG *rig)
{
	struct pcr_priv_data *priv;

	if (!rig)
		return -RIG_EINVAL;

	priv = (struct pcr_priv_data*)malloc(sizeof(struct pcr_priv_data));

	if (!priv) {
				/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	/*
	 * FIXME: how can we retrieve initial status?
	 */
	priv->last_freq = MHz(145);
	priv->last_mode = MD_FM;
	priv->last_filter = FLT_15kHz;

	rig->state.priv = (rig_ptr_t)priv;

	return RIG_OK;
}

/*
 * PCR Generic pcr_cleanup routine
 * the serial port is closed by the frontend
 */
int pcr_cleanup(RIG *rig)
{
	if (!rig)
		return -RIG_EINVAL;

	if (rig->state.priv)
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
int pcr_open(RIG *rig)
{
	struct pcr_priv_data *priv;
	struct rig_state *rs;
	unsigned char ackbuf[16];
	int ack_len, retval;
	int wanted_serial_rate;
	const char *rate_cmd;

	rs = &rig->state;
	priv = (struct pcr_priv_data *)rs->priv;

	/* 
	 * initial communication is at 9600bps
	 * once the power is on, the serial speed can be changed with G1xx
	 */
	wanted_serial_rate = rs->rigport.parm.serial.rate;
	rs->rigport.parm.serial.rate = 9600;
	serial_setup(&rs->rigport);

	ack_len = 6;
	retval = pcr_transaction (rig, "H101" EOM, 6, ackbuf, &ack_len);
	if (retval != RIG_OK)
			return retval;

	ack_len = 6;
	retval = pcr_transaction (rig, "G300" EOM, 6, ackbuf, &ack_len);
	if (retval != RIG_OK)
			return retval;

#if 0
	if (wanted_serial_rate != 9600) {
		switch (wanted_serial_rate) {
			case 300:	rate_cmd = "G100" EOM; break;
			case 1200:	rate_cmd = "G101" EOM; break;
			case 2400:	rate_cmd = "G102" EOM; break;
			case 9600:	rate_cmd = "G103" EOM; break;
			case 19200:	rate_cmd = "G104" EOM; break;
			case 38400:	rate_cmd = "G105" EOM; break;
		}
		retval = pcr_transaction (rig, rate_cmd, 6, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;


		rs->rigport.parm.serial.rate = wanted_serial_rate;
		serial_setup(&rs->rigport);
		/* check communication state with "G0?" */
	}
#endif

	return RIG_OK;
}

/*
 * pcr_close
 * - send power off
 *
 * Assumes rig!=NULL
 */
int pcr_close(RIG *rig)
{
	unsigned char ackbuf[16];
	int ack_len, retval;

	ack_len = 6;
	retval = pcr_transaction (rig, "H100" EOM, 6, ackbuf, &ack_len);
	if (retval != RIG_OK)
			return retval;

	return retval;
}

/*
 * pcr_set_freq
 * Assumes rig!=NULL
 */
int pcr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		struct pcr_priv_data *priv;
		unsigned char freqbuf[32], ackbuf[16];
		int freq_len, ack_len, retval;

		priv = (struct pcr_priv_data *)rig->state.priv;

		freq_len = sprintf(freqbuf,"K0%010Ld0%c0%c00" EOM, freq, 
						priv->last_mode, priv->last_filter);

		ack_len = 6;
		retval = pcr_transaction (rig, freqbuf, freq_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 6 && ack_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"pcr_set_freq: ack NG, len=%d\n",
								ack_len);
				return -RIG_ERJCTED;
		}

		priv->last_freq = freq;

		return RIG_OK;
}

/*
 * hack! pcr_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int pcr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		struct pcr_priv_data *priv;

		priv = (struct pcr_priv_data *)rig->state.priv;

		*freq = priv->last_freq;

		return RIG_OK;
}

/*
 * pcr_set_mode
 * Assumes rig!=NULL
 */
int pcr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		struct pcr_priv_data *priv;
		unsigned char mdbuf[32],ackbuf[16];
		int mdbuf_len, ack_len, retval;
		int pcrmode, pcrfilter;

		priv = (struct pcr_priv_data *)rig->state.priv;

		/*
		 * not so sure about modes and filters
		 * as I write this from manual (no testing) --SF
		 */
		switch (mode) {
			case RIG_MODE_CW:	pcrmode = MD_CW; pcrfilter = FLT_2_8kHz; break;
			case RIG_MODE_USB:	pcrmode = MD_USB; pcrfilter = FLT_2_8kHz; break;
			case RIG_MODE_LSB:	pcrmode = MD_LSB; pcrfilter = FLT_2_8kHz; break;
			case RIG_MODE_AM:	pcrmode = MD_AM; pcrfilter = FLT_15kHz; break;
			case RIG_MODE_WFM:	pcrmode = MD_WFM; pcrfilter = FLT_230kHz; break;
			case RIG_MODE_FM:	pcrmode = MD_FM; pcrfilter = FLT_15kHz; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"pcr_set_mode: unsupported mode %d\n",
								mode);
				return -RIG_EINVAL;
		}

		switch (width) {
				/* nop, pcrfilter already set
				 * TODO: use rig_passband_normal instead?
				 */
			case RIG_PASSBAND_NORMAL: break;

			case kHz(2.8):	pcrfilter = FLT_2_8kHz; break;
			case kHz(6):	pcrfilter = FLT_6kHz; break;
			case kHz(15):	pcrfilter = FLT_15kHz; break;
			case kHz(50):	pcrfilter = FLT_50kHz; break;
			case kHz(230):	pcrfilter = FLT_230kHz; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"pcr_set_mode: unsupported "
							"width %d\n", width);
				return -RIG_EINVAL;
		}

		mdbuf_len = sprintf(mdbuf,"K0%010Ld0%c0%c00" EOM, priv->last_freq, 
						pcrmode, pcrfilter);

		ack_len = 6;
		retval = pcr_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 6 && ack_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"pcr_set_mode: ack NG, len=%d\n",
								ack_len);
				return -RIG_ERJCTED;
		}

		priv->last_mode = pcrmode;
		priv->last_filter = pcrfilter;

		return RIG_OK;
}

/*
 * hack! pcr_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int pcr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		struct pcr_priv_data *priv;

		priv = (struct pcr_priv_data *)rig->state.priv;

		switch (priv->last_mode) {
			case MD_CW:		*mode = RIG_MODE_CW; break;
			case MD_USB:	*mode = RIG_MODE_USB; break;
			case MD_LSB:	*mode = RIG_MODE_LSB; break;
			case MD_AM:		*mode = RIG_MODE_AM; break;
			case MD_WFM:	*mode = RIG_MODE_WFM; break;
			case MD_FM:		*mode = RIG_MODE_FM; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"pcr_get_mode: unsupported mode %d\n",
								priv->last_mode);
				return -RIG_EINVAL;
		}
		switch(priv->last_filter) {
			case FLT_2_8kHz:	*width = kHz(2.8); break;
			case FLT_6kHz:		*width = kHz(6); break;
			case FLT_15kHz: 	*width = kHz(15); break;
			case FLT_50kHz:		*width = kHz(50); break;
			case FLT_230kHz:	*width = kHz(230); break;
			default:
					rig_debug(RIG_DEBUG_ERR,"pcr_get_mode: unsupported "
								"width %d\n", priv->last_filter);
					return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * pcr_get_info
 * Assumes rig!=NULL
 */
const char *pcr_get_info(RIG *rig)
{
		struct pcr_priv_data *priv;
		unsigned char ackbuf[16];
		static char buf[100];	/* FIXME: reentrancy */
		int ack_len, retval;
		int proto_version = 0, frmwr_version = 0;
		int options = 0, country_code = 0;
		char *country;

		priv = (struct pcr_priv_data *)rig->state.priv;

		/*
		 * protocol version
		 */
		ack_len = 6;
		retval = pcr_transaction (rig, "G2?" EOM, 5, ackbuf, &ack_len);
		if (retval != RIG_OK || ack_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"pcr_get_info: ack NG, len=%d\n",
								ack_len);
		} else {
			sscanf(ackbuf, "G2%d", &proto_version);
		}

		/*
		 * Firmware version
		 */
		ack_len = 6;
		retval = pcr_transaction (rig, "G4?" EOM, 5, ackbuf, &ack_len);
		if (retval != RIG_OK || ack_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"pcr_get_info: ack NG, len=%d\n",
								ack_len);
		} else {
			sscanf(ackbuf, "G4%d", &frmwr_version);
		}

		/*
		 * optional devices
		 */
		ack_len = 6;
		retval = pcr_transaction (rig, "GD?" EOM, 5, ackbuf, &ack_len);
		if (retval != RIG_OK || ack_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"pcr_get_info: ack NG, len=%d\n",
								ack_len);
		} else {
				sscanf(ackbuf, "GD%d", &options);
		}

		/*
		 * Country
		 */
		ack_len = 6;
		retval = pcr_transaction (rig, "GE?" EOM, 5, ackbuf, &ack_len);
		if (retval != RIG_OK || ack_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"pcr_get_info: ack NG, len=%d\n",
								ack_len);
		} else {
			sscanf(ackbuf, "GE%d", &country_code);
		}

		switch (country_code) {
		case COUNTRY_JAPAN: country = "Japan"; break;
		case COUNTRY_USA: country = "USA"; break;
		case COUNTRY_UK: country = "UK"; break;
		case COUNTRY_EUAUCA: country = "Europe/Australia/Canada"; break;
		case COUNTRY_FGA: country = "FGA?"; break;
		case COUNTRY_DEN: country = "DEN?"; break;
		default: 
			country = "Other";
			rig_debug(RIG_DEBUG_ERR,"pcr_get_info: unknown country code %#x, "
							"please retport to Hamlib maintainer\n",
								country_code);
		}


		sprintf(buf, "Firmware v%d.%d, Protocol v%d.%d, "
						"Optional devices:%s%s%s, Country: %s", 
						frmwr_version/10,frmwr_version%10,
						proto_version/10,proto_version%10,
						options&OPT_UT106 ? " DSP"  : "",
						options&OPT_UT107 ? " DARC" : "",
						options ? "" : " none",
						country);

		return buf;
}



/*
 * initrigs_pcr is called by rig_backend_load
 */
int initrigs_pcr(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "pcr: _init called\n");

		rig_register(&pcr100_caps);
		rig_register(&pcr1000_caps);

		return RIG_OK;
}


