/*
 *  Hamlib PCR backend - main file
 *  Copyright (c) 2001-2003 by Stephane Fillod and Darren Hatcher
 *
 *	$Id: pcr.c,v 1.19 2003-10-01 19:31:59 fillods Exp $
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
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "pcr.h"

/* baud rates used */
#define B_300   0
#define B_1200  1
#define B_4800  2
#define B_9600  3
#define B_19200 4
#define B_38400 5

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
		const char *rate_cmd;

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



/* *********************************************************************** */
/* load of new stuff added in by Darren Hatcher - G0WCW                    */
/* *********************************************************************** */

/*
 * pcr_set_level called by generic set level handler
 *
 * We are missing a way to set the BFO on/off here,
 */

int pcr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	int retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_level called\npcr: values = %f %ld, level  = %d\n", val.f, val.i,level);

	/* split by float/int types */
	if (!RIG_LEVEL_IS_FLOAT(level)){
	/* ints first */
		switch( level ) {

		case RIG_LEVEL_ATT:
			/* This is only on or off, but hamlib forces to use set level
			 * and pass as a float. Here we'll use 0 for off and 1 for on.
			 * If someone finds out how to set the ATT for the PCR in dB, let us
			 * know and the function can be changed to allow a true set level.
			 *
			 * Experiment shows it seems to have an effect, but unsure by how many db
			 */
			retval = pcr_set_Attenuator(rig, val.i);
			break;

		case RIG_LEVEL_IF:
			/*	.... rig supports 0 to FF, with 0x80 in the middle */
			retval = pcr_set_IF_shift(rig, val.i);
			break;

		case RIG_LEVEL_AGC:
			/* this is implemented as a level even though it is a binary function
			 * as far as PCR is concerned. There is no AGC on/off for a "set func",
			 * so done here is a set level
			 */
			retval = pcr_set_AGC(rig, val.i);
			break;

   		default:
			rig_debug(RIG_DEBUG_VERBOSE, "pcr: Integer rig level default not found ...\n");
      			return -RIG_EINVAL;
   		}
	}
	if (RIG_LEVEL_IS_FLOAT(level)){
	/* floats */
		switch( level ) {

		case RIG_LEVEL_AF:
			/* "val" can be 0.0 to 1.0 float which is 0 to 255 levels
			 * 0.3 seems to be ok in terms of loudness
			 */
			retval = pcr_set_volume(rig, (val.f * 0xFF));
			break;

		case RIG_LEVEL_SQL:
			/* "val" can be 0.0 to 1.0 float
			 *	.... rig supports 0 to FF - look at function for
			 *	squelch "bands"
			 */
			retval = pcr_set_squelch(rig, (val.f * 0xFF));
			break;

		case RIG_LEVEL_NR:
			/* This selectss the DSP unit - this isn't a level per se,
			 * but in the manual it says that we have to switch it on first
			 * we'll assume 1 is for the UT-106, and anything else as off
			 *
			 * Later on we can set if the DSP features are on or off in set_func
			 */
			retval = pcr_set_DSP(rig, (int) val.f);
			break;
   		default:
			rig_debug(RIG_DEBUG_VERBOSE, "pcr: Float  rig level default not found ...\n");
      			return -RIG_EINVAL;
   		}
	}

	return RIG_OK;
}

/*
 * pcr_get_level ....
 *
 * This needs a set of stored variables as the PCR doesn't return the current status of settings.
 * So we'll need to store them as we go along and keep them in sync.
 */

int pcr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_get_level called\n");

	/* stuff needs putting in here ... */

	return -RIG_ENIMPL;
}


/*
 * pcr_set_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * This is missing a way to call the set DSP noise reducer, as we don't have a func to call it
 * based on the flags in rig.h -> see also missing a flag for setting the BFO.
 */
int pcr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
	int retval;

	rig_debug(RIG_DEBUG_TRACE,
			"pcr: pcr_set_func called\npcr: status = %ld, func = %d\n", status ,func);

	switch( func ) {

		case RIG_FUNC_NR: /* sets DSP noise reduction on or off */
			/* status = 00 for off or 01 for on
			 * Note that the user should switch the DSP unit on first
			 *  using the set level function RIG_LEVEL_NR
			 */
			if (status == 1)
				retval = pcr_set_DSP_state(rig, 1);
			else
				retval = pcr_set_DSP_state(rig, 0);
			break;

		case RIG_FUNC_ANF: /* DSP auto notch filter */
			if (status == 1)
				retval = pcr_set_DSP_auto_notch(rig, 1);
			else
				retval = pcr_set_DSP_auto_notch(rig, 0);
			break;

		case RIG_FUNC_NB: /* noise blanker */
			/* status = 00 for off or 01 for on */
			if (status == 0)
				retval = pcr_set_NB(rig, 0);
			else
				retval = pcr_set_NB(rig, 1);

			break;

   		default:
			rig_debug(RIG_DEBUG_VERBOSE, "pcr: Rig function default not found ...\n");
      			return -RIG_EINVAL;
   	}

	return RIG_OK;
}

/*
 * pcr_get_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * This will need similar variables/flags as get_level. The PCR doesn't offer much in the way of
 *  confirmation of current settings (according to the docs).
 */
int pcr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
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

int pcr_check_ok(RIG *rig)
{
	unsigned char ackbuf[16];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_check_ok called\n");

	ack_len = 6;
	retval = pcr_transaction (rig, "G0?" EOM, 5, ackbuf, &ack_len);
	if (retval != RIG_OK)
			return retval;

	if (ack_len != 6) {
			rig_debug(RIG_DEBUG_ERR,"pcr_check_ok: ack NG, len=%d\n",
							ack_len);
			return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not acked ok
			return -RIG_EPROTO;
	return RIG_OK;
}


/*
 * Sets the volume ofthe rig to the level specified in the level integer.
 *
 * Format is J40xx - where xx is 00 to FF in hex, and specifies 255 volume levels
 *
 */

int pcr_set_volume(RIG *rig, int level)
{
	unsigned char ackbuf[16], vol_cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_volume called - %d\n",level);

	// check that volume valid
	if (level < 0x0)
	{
			rig_debug(RIG_DEBUG_ERR,
				"pcr_set_volume: rig level too low: %d\n", level);
			return -RIG_EINVAL;
	}
	else
		if (level > 0xff)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_volume: rig level too high: %d\n", level);
			return -RIG_EINVAL;
		}
	// it  must be valid ...
	sprintf(vol_cmd,"J40%0X\r\n", level);

	ack_len = 6;
	retval = pcr_transaction (rig, vol_cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_volume: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;
}

/*
 * Sets the serial rate of the rig RS232
 *
 * Format is G1xx - where xx is defined as:
 * 		00=1200, 01=2400, 02=4800, 03=9600, 04=19200, 05=38400, 06..FF=38400
 *
 * baud_rate: 0=1200 ... 5=38400
 */

int pcr_set_comm_rate(RIG *rig, int baud_rate)
{
	unsigned char ackbuf[16], baud_cmd[8];
	struct pcr_priv_data *priv;

	int ack_len, retval;
	struct rig_state *rs;

	rig_debug(RIG_DEBUG_VERBOSE, "pcr: pcr_set_comm_rate called\n");

	// check that rate valid
	if (baud_rate < 0)
	{
		rig_debug(RIG_DEBUG_ERR,"pcr_set_comm_rate: rig rate too low: %d\n", baud_rate);
		return -RIG_EINVAL;
	}
	else
		if (baud_rate > B_38400) 
			baud_rate = B_38400; //enforce to 38400

	// it  must be valid ...

	sprintf(baud_cmd,"G10%0d" EOM, baud_rate);

	ack_len = 6;
	retval = pcr_transaction (rig, baud_cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	rs = &rig->state;
	priv = (struct pcr_priv_data *)rs->priv;

	// Note: the rate will be ok if we get to here ...
	switch ( baud_rate ) {

		case B_300:
			rs->rigport.parm.serial.rate = 300; // work ok!
			break;
		case B_1200:
			rs->rigport.parm.serial.rate = 1200; //work ok!
			break;
		case B_4800:
			rs->rigport.parm.serial.rate = 4800; // work ok!
			break;
		case B_9600:
			rs->rigport.parm.serial.rate = 9600; // work ok!
			break;
		case B_19200:
			rs->rigport.parm.serial.rate = 19200;  // work ok!
			break;
		case B_38400:
			rs->rigport.parm.serial.rate = 38400;  // work ok!
			break;
		default:
			rs->rigport.parm.serial.rate = 38400;
			break;
	}


	// re-setup the port
	serial_setup(&rs->rigport);

	// The return value is at the new speed. However the old message may be garbled ...
	// so to sync ok, we need to just do an ack
	retval = pcr_check_ok(rig);
	if (retval != RIG_OK)
		return retval; // hmmm ... busted

	// ok .. good stuff

	return RIG_OK;
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
 *
 *
 */

int pcr_set_squelch(RIG *rig, int level)
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_squelch called - %d\n",level);

	// check that level valid
	if (level < 0x0)
	{
			rig_debug(RIG_DEBUG_ERR,
						"pcr_set_squelch: rig level too low: %d\n", level);
			return -RIG_EINVAL;
	}
	else
		if (level > 0xff)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_squelch: rig level too high: %d\n", level);
			return -RIG_EINVAL;
		}
	// it  must be valid ...

	sprintf(cmd,"J41%0X\r\n", level);

	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_squelch: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;

}

/*
 * pcr_set_IF_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the IF shift  of the rig to the level specified in the level integer.
 * 	IF-SHIFT position (in 256 stages, 80 = centre):
 *
 * 		< 80	Minus shift (in 10 Hz steps)
 * 		> 80	Plus shift (in 10 Hz steps)
 * 		  80	Centre
 *
 * Format is J43xx - where xx is 00 to FF in hex, and specifies 255 squelch levels
 *
 */
int pcr_set_IF_shift(RIG *rig, int shift)
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_IF_shift called - %d\n",shift);

	// check that level valid
	if (shift < 0x0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_IF_shift: rig shift too low: %d\n", shift);
			return -RIG_EINVAL;
	}
	else
		if (shift > 0xff)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_IF_shift: rig shift too high: %d\n", shift);
			return -RIG_EINVAL;
		}
	// it  must be valid ...

	sprintf(cmd,"J43%0X\r\n", shift);

	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_IF_shift: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;

}
/*
 * pcr_set_AGC
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the AGC on or off based on the level specified in the level integer.
 * 	00 = off, 01 (non zero) is on
 *
 * Format is J45xx - where xx is 00 to FF in hex
 *
 */
int pcr_set_AGC(RIG *rig, int level)
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_AGC called - %d\n",level);

	// check that level valid
	if (level < 0x0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_AGC: AGC too low: %d\n", level);
			return -RIG_EINVAL;
	}
	else
		if (level > 0xff)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_AGC: rig AGC too high: %d\n", level);
			return -RIG_EINVAL;
		}
	// it  must be valid ...
	if (level == 0)
		sprintf(cmd,"J4500\r\n"); /* off */
	else
		sprintf(cmd,"J4501\r\n"); /* else must be on */

	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_AGC: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;

}
/*
 * pcr_set_NB(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the noise blanker on or off based on the level specified in the level integer.
 * 	00 = off, 01 (non zero) is on
 *
 * Format is J46xx - where xx is 00 to FF in hex
 *
 */
int pcr_set_NB(RIG *rig, int level)
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_NB called - %d\n",level);

	// check that level valid
	if (level < 0x0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_NB: NB too low: %d\n", level);
			return -RIG_EINVAL;
	}
	else
		if (level > 0xff)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_NB: rig NB too high: %d\n", level);
			return -RIG_EINVAL;
		}
	// it  must be valid ...
	if (level == 0)
		sprintf(cmd,"J4600\r\n"); /* off */
	else
		sprintf(cmd,"J4601\r\n"); /* else must be on */

	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_NB: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;

}

/*
 * pcr_set_NB(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the attenuator on or off based on the level specified in the level integer.
 * 	00 = off, 01 (non zero) is on
 *		the downer is I don't know where to set the attenatuor level ....!
 *
 * Format is J47xx - where xx is 00 to FF in hex
 *
 */
int pcr_set_Attenuator(RIG *rig, int level)
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_Att called - Atten level = %d\n",level);

	// check that level valid
	if (level < 0x0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_Att: too low: %d\n", level);
			return -RIG_EINVAL;
	}
	else
		if (level > 0xff)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_Att: rig too high: %d\n", level);
			return -RIG_EINVAL;
		}
	// it  must be valid ...
	if (level == 0)
		sprintf(cmd,"J4700\r\n"); /* off */
	else
		sprintf(cmd,"J4701\r\n"); /* else must be on */

	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_Att: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	rig_debug(RIG_DEBUG_VERBOSE,"pcr_set_Att: all ok\n");
	return RIG_OK;

}

/*
 * pcr_set_BFO
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the BFO of the rig to the level specified in the level integer.
 * 	BFO-SHIFT position (in 256 stages, 80 = centre):
 *
 * 		< 80	Minus shift (in 10 Hz steps)
 * 		> 80	Plus shift (in 10 Hz steps)
 * 		  80	Centre
 *
 * Format is J43xx - where xx is 00 to FF in hex, and specifies 255 squelch levels
 *
 */
int pcr_set_BFO(RIG *rig, int shift) // J4Axx
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_BFO_shift called - %d\n",shift);

	// check that level valid
	if (shift < 0x0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_BFO_shift: rig shift too low: %d\n", shift);
			return -RIG_EINVAL;
	}
	else
		if (shift > 0xff)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_BFO_shift: rig shift too high: %d\n", shift);
			return -RIG_EINVAL;
		}
	// it  must be valid ...

	sprintf(cmd,"J4A%0X\r\n", shift);

	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_BFO_shift: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;

}

/*
 * pcr_set_DSP(RIG *rig, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP to UT106 (01) or off (non 01)
 *
 */
int pcr_set_DSP(RIG *rig, int level)
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_DSP called - level = %d\n",level);

	/* check that level valid - zero or one only */
	if (level < 0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_DSP: too low: %d\n", level);
			return -RIG_EINVAL;
	}
	else
		if (level > 1)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_DSP: rig too high: %d\n", level);
			return -RIG_EINVAL;
		}
	// it  must be valid ...
	if (level == 1)
		sprintf(cmd,"J8001\r\n"); /* else must be UT106 */
	else
		sprintf(cmd,"J8000\r\n"); /* off */

	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_DSP: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;

}

/*
 * pcr_set_DSP_state(RIG *rig, int state);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP on or off (1 = on, 0 = off)
 *
 */

int pcr_set_DSP_state(RIG *rig, int state)
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_DSP_state called - state = %d\n",state);

	/* check that state valid - zero or one only */
	if (state < 0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_DSP: too low: %d\n", state);
			return -RIG_EINVAL;
	}
	else
		if (state > 1)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_DSP: rig too high: %d\n", state);
			return -RIG_EINVAL;
		}
	// it  must be valid ...
	if (state == 1)
		sprintf(cmd,"J8101\r\n"); /* else must be set on */
	else
		sprintf(cmd,"J8100\r\n"); /* off */

	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_DSP_state: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;
}

/*
 * pcr_set_DSPnoise_reducer(RIG *rig, int state);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP noise reducer on or off (0x01 = on, 0x00 = off)
 *  the level of NR set by values 0x01 to 0x10 (1 to 16 inclusive)
 */

int pcr_set_DSP_noise_reducer(RIG *rig, int state) // J82xx
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_DSP_state called - state = %d\n",state);

	/* check that state valid - zero or one only */
	if (state < 0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_DSP_noise_reducer: too low: %d\n", state);
			return -RIG_EINVAL;
	}
	else
		if (state > 0x10)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_DSP_noise_reducer: rig too high: %d\n", state);
			return -RIG_EINVAL;
		}
	// it  must be valid ...
	if (state == 0)
		sprintf(cmd,"J8200\r\n"); /* off */
	else
		sprintf(cmd,"J82%0X\r\n",state); /* else must be set on */


	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_DSP_noise_reducer: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;

}

/*
 * pcr_set_DSP_auto_notch(RIG *rig, int state);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the auto notch on or off (1 = on, 0 = off)
 *
 */

int pcr_set_DSP_auto_notch(RIG *rig, int state) // J83xx
{
	unsigned char ackbuf[16], cmd[12];
	int ack_len, retval;

	rig_debug(RIG_DEBUG_TRACE, "pcr: pcr_set_DSP_auto_notch called - state = %d\n",state);

	/* check that state valid - zero or one only */
	if (state < 0)
	{
			rig_debug(RIG_DEBUG_ERR,
					"pcr_set_DSP_auto_notch: too low: %d\n", state);
			return -RIG_EINVAL;
	}
	else
		if (state > 1)
		{
			rig_debug(RIG_DEBUG_ERR, "pcr_set_DSP_auto_notch: rig too high: %d\n", state);
			return -RIG_EINVAL;
		}
	// it  must be valid ...
	if (state == 0)
		sprintf(cmd,"J8300\r\n"); /* off */
	else
		sprintf(cmd,"J8301\r\n"); /* else must be set on */


	ack_len = 6;
	retval = pcr_transaction (rig, cmd, 7, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len != 6 ){
		rig_debug(RIG_DEBUG_ERR,"pcr_set_DSP_auto_notch: ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (strcmp("G000" EOM, ackbuf) != 0) // then did not ack ok
		return -RIG_EPROTO;

	return RIG_OK;

}


int pcr_set_VSC(RIG *rig, int level) // J50xx
{
	/* Not sure what VSC for so skipping the function here ... */
	return -RIG_ENIMPL;
}

/* *********************************************************************************************
 * int pcr_set_comm_mode(RIG *rig, int mode_type);  // Set radio to fast/diagnostic mode  G3xx
 * int pcr_soft_reset(RIG *rig);                    // Ask rig to reset itself            H0xx
 ********************************************************************************************* */



/*
 * initrigs_pcr is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(pcr)
{
		rig_debug(RIG_DEBUG_VERBOSE, "pcr: _init called\n");

		rig_register(&pcr100_caps);
		rig_register(&pcr1000_caps);

		return RIG_OK;
}

