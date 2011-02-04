/*  $Id:                                                                      $
 *
 *  Hamlib KIT backend - FiFi-SDR Receiver(/Tuner) description
 *  Copyright (c) 2010 by Rolf Meeser
 *
 *  Derived from si570avrusb backend:
 *  Copyright (C) 2004-2010 Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "hamlib/rig.h"
#include "token.h"

#include "kit.h"

/*
 * Compile this model only if libusb is available
 */
#if defined(HAVE_LIBUSB) && defined(HAVE_USB_H)

#include <errno.h>
#include <usb.h>



/* Selected request codes of the original AVR USB Si570 firmware */
#define REQUEST_READ_VERSION			0x00
#define REQUEST_SET_DDRB			0x01
#define REQUEST_SET_PORTB			0x04
#define REQUEST_READ_EEPROM			0x11
#define REQUEST_FILTERS				0x17
#define REQUEST_SET_FREQ			0x30
#define REQUEST_SET_FREQ_BY_VALUE	0x32
#define REQUEST_SET_XTALL_FREQ		0x33
#define REQUEST_SET_STARTUP_FREQ	0x34
#define REQUEST_READ_MULTIPLY_LO	0x39
#define REQUEST_READ_FREQUENCY		0x3A
#define REQUEST_READ_SMOOTH_TUNE_PPM    0x3B
#define REQUEST_READ_STARTUP		0x3C
#define REQUEST_READ_XTALL		0x3D
#define REQUEST_READ_REGISTERS		0x3F
#define REQUEST_READ_KEYS			0x51

/* FiFi-SDR specific requests */
#define REQUEST_FIFISDR_READ		(0xAB)
#define REQUEST_FIFISDR_WRITE		(0xAC)


/* USB VID/PID, vendor name (idVendor), product name (idProduct).
 * Use obdev's generic shared VID/PID pair and follow the rules outlined
 * in firmware/usbdrv/USBID-License.txt.
 */
#define USBDEV_SHARED_VID			0x16C0  /* VOTI */
#define USBDEV_SHARED_PID			0x05DC  /* Obdev's free shared PID */
#define FIFISDR_VENDOR_NAME			"www.ov-lennestadt.de"
#define FIFISDR_PRODUCT_NAME		"FiFi-SDR"



static int fifisdr_init(RIG *rig);
static int fifisdr_cleanup(RIG *rig);
static int fifisdr_open(RIG *rig);
static int fifisdr_set_freq_by_value(RIG *rig, vfo_t vfo, freq_t freq);
static int fifisdr_get_freq_by_value(RIG *rig, vfo_t vfo, freq_t *freq);
static const char *fifisdr_get_info(RIG *rig);
static int fifisdr_set_conf(RIG *rig, token_t token, const char *val);
static int fifisdr_get_conf(RIG *rig, token_t token, char *val);
static int fifisdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int fifisdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t * width);
static int fifisdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int fifisdr_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val);




/* Private tokens. */
#define TOK_PARAM_MULTIPLIER	TOKEN_BACKEND(1)	/* Factor between VCO and RX frequency */

#define TOK_LVL_FMCENTER		TOKEN_BACKEND(20)	/* FM center frequency deviation */


static const struct confparams fifisdr_cfg_params[] = {
	{ TOK_PARAM_MULTIPLIER, "multiplier", "Freq Multiplier", "Frequency multiplier",
		"4", RIG_CONF_NUMERIC, { .n = { 0.000001, 100 } }
	},
	{ RIG_CONF_END, NULL, }
};


/* Extra levels definitions */
static const struct confparams fifisdr_ext_levels[] = {
	{ TOK_LVL_FMCENTER, "fmcenter", "FM center", "Center frequency deviation of FM signal",
		NULL, RIG_CONF_NUMERIC, { .n = { -kHz(50), kHz(50), Hz(1) } }
	},
	{ RIG_CONF_END, NULL, }
};



/*
 * Common data struct
 */
struct fifisdr_priv_data {

	double osc_freq;	/* MHz */
	double multiplier;	/* default to 4 for QSD/QSE */
};




/* FiFi-SDR receiver description.
 *
 * Based on AVR USB description, but uses different vendor and product strings.
 */
const struct rig_caps fifisdr_caps = {
	.rig_model = RIG_MODEL_FIFISDR,
	.model_name = "FiFi-SDR",
	.mfg_name = "FiFi",
	.version = "0.3",
	.copyright = "GPL",
	.status = RIG_STATUS_BETA,

	.rig_type = RIG_TYPE_RECEIVER,
	.ptt_type = RIG_PTT_NONE,
	.dcd_type = RIG_DCD_NONE,
	.port_type = RIG_PORT_USB,

	.write_delay = 0,
	.post_write_delay = 0,
	.timeout = 500,
	.retry = 0,

	.has_get_func = RIG_FUNC_NONE,
	.has_set_func = RIG_FUNC_NONE,
	.has_get_level = RIG_LEVEL_STRENGTH,
	.has_set_level = RIG_LEVEL_SET(RIG_LEVEL_NONE),
	.has_get_parm = RIG_PARM_NONE,
	.has_set_parm = RIG_PARM_SET(RIG_PARM_NONE),

	.level_gran = {},
	.parm_gran = {},

	.extparms = NULL,
	.extlevels = fifisdr_ext_levels,

	.preamp = { RIG_DBLST_END },
	.attenuator = { RIG_DBLST_END },
	.max_rit = Hz(0),
	.max_xit = Hz(0),
	.max_ifshift = Hz(0),

	.vfo_ops = RIG_OP_NONE,
	.scan_ops = RIG_SCAN_NONE,
	.targetable_vfo = 0,
	.transceive = RIG_TRN_OFF,

	.bank_qty = 0,
	.chan_desc_sz = 0,
	.chan_list = {
		RIG_CHAN_END,
	},

	.rx_range_list1 = {
		{.start = kHz(39.1), .end = MHz(175.0),
		 .modes = RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_FM,
		 .low_power = -1, .high_power = -1,
		 .vfo = RIG_VFO_A, .ant = RIG_ANT_1},
		RIG_FRNG_END,
	},
	.tx_range_list1 = { RIG_FRNG_END, },
	.rx_range_list2 =  {
		{.start = kHz(39.1), .end = MHz(175.0),
		 .modes = RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_FM,
		 .low_power = -1, .high_power = -1,
		 .vfo = RIG_VFO_A, .ant = RIG_ANT_1},
		RIG_FRNG_END,
	},
	.tx_range_list2 = { RIG_FRNG_END, },

	.tuning_steps = {
		{RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_FM, Hz(1)},
		RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
	.filters = {
		{RIG_MODE_SSB, kHz(2.7)},
		{RIG_MODE_AM,  kHz(6.0)},
		{RIG_MODE_FM,  kHz(12.5)},
		RIG_FLT_END,
	},

	.cfgparams = fifisdr_cfg_params,

	.rig_init = fifisdr_init,
	.rig_cleanup = fifisdr_cleanup,
	.rig_open = fifisdr_open,
	.rig_close = NULL,

	.set_freq = fifisdr_set_freq_by_value,
	.get_freq = fifisdr_get_freq_by_value,
	.set_mode = fifisdr_set_mode,
	.get_mode = fifisdr_get_mode,

	.get_level = fifisdr_get_level,
	.get_ext_level = fifisdr_get_ext_level,

	.set_conf = fifisdr_set_conf,
	.get_conf = fifisdr_get_conf,

	.get_info = fifisdr_get_info,
};




/* Conversion to/from little endian */
static uint32_t fifisdr_tole32 (uint32_t x)
{
	return
		(((((x) /        1ul) % 256ul) <<  0) |
		 ((((x) /      256ul) % 256ul) <<  8) |
		 ((((x) /    65536ul) % 256ul) << 16) |
		 ((((x) / 16777216ul) % 256ul) << 24));
}

static uint32_t fifisdr_fromle32 (uint32_t x)
{
	return
		(((((x) >> 24) & 0xFF) * 16777216ul) +
		 ((((x) >> 16) & 0xFF) *    65536ul) +
		 ((((x) >>  8) & 0xFF) *      256ul) +
		 ((((x) >>  0) & 0xFF) *        1ul));
}


/** fifisdr_usb_write. */
static int fifisdr_usb_write (RIG *rig,
							  int request, int value, int index,
							  char *bytes, int size)
{
	int ret;
	struct usb_dev_handle *udh = rig->state.rigport.handle;

	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
			request, value, index,
			bytes, size, rig->state.rigport.timeout);

	if (ret != size) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg (%d/%d) failed: %s\n",
					__func__,
				   request, value,
					usb_strerror ());
		return -RIG_EIO;
	}

	return RIG_OK;
}



/** fifisdr_usb_read. */
static int fifisdr_usb_read (RIG *rig,
							 int request, int value, int index,
							 char *bytes, int size)
{
	int ret;
	struct usb_dev_handle *udh = rig->state.rigport.handle;

	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
			request, value, index,
			bytes, size, rig->state.rigport.timeout);

	if (ret != size) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg (%d/%d) failed: %s\n",
					__func__,
				   request, value,
					usb_strerror ());
		return -RIG_EIO;
	}

	return RIG_OK;
}


int fifisdr_init(RIG *rig)
{
	hamlib_port_t *rp = &rig->state.rigport;
	struct fifisdr_priv_data *priv;

	priv = (struct fifisdr_priv_data*)calloc(sizeof(struct fifisdr_priv_data), 1);
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	priv->multiplier = 4;

	rp->parm.usb.vid = USBDEV_SHARED_VID;
	rp->parm.usb.pid = USBDEV_SHARED_PID;

	/* no usb_set_configuration() and usb_claim_interface() */
	rp->parm.usb.iface = -1;
	rp->parm.usb.conf = 1;
	rp->parm.usb.alt = 0;	/* necessary ? */

	rp->parm.usb.vendor_name = FIFISDR_VENDOR_NAME;
	rp->parm.usb.product = FIFISDR_PRODUCT_NAME;

	rig->state.priv = (void*)priv;

	return RIG_OK;
}


int fifisdr_cleanup(RIG *rig)
{
	if (!rig)
		return -RIG_EINVAL;

	if (rig->state.priv)
		free(rig->state.priv);
	rig->state.priv = NULL;

	return RIG_OK;
}

int fifisdr_set_conf(RIG *rig, token_t token, const char *val)
{
	struct fifisdr_priv_data *priv;
	double multiplier;

	priv = (struct fifisdr_priv_data*)rig->state.priv;

	switch(token) {
		case TOK_PARAM_MULTIPLIER:
			if (sscanf(val, "%lf", &multiplier) != 1)
				return -RIG_EINVAL;
			if (multiplier == 0.)
				return -RIG_EINVAL;
			priv->multiplier = multiplier;
			break;
		default:
			return -RIG_EINVAL;
	}
	return RIG_OK;
}

int fifisdr_get_conf(RIG *rig, token_t token, char *val)
{
	struct fifisdr_priv_data *priv = (struct fifisdr_priv_data*)rig->state.priv;

	switch(token) {
		case TOK_PARAM_MULTIPLIER:
			sprintf(val, "%f", priv->multiplier);
			break;
		default:
			return -RIG_EINVAL;
	}
	return RIG_OK;
}



int fifisdr_open(RIG *rig)
{
	rig_debug(RIG_DEBUG_TRACE,"%s called\n", __func__);

	return RIG_OK;
}


const char * fifisdr_get_info(RIG *rig)
{
	static char buf[64];
	int ret;
	uint32_t svn_version;
	int version;


	ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0, 0, (char *)&svn_version, sizeof(svn_version));
	if (ret != RIG_OK) {
		return NULL;
	}

	version = svn_version;
	snprintf(buf, sizeof(buf), "Firmware version: %d", svn_version);

	return buf;
}



int fifisdr_set_freq_by_value(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct fifisdr_priv_data *priv = (struct fifisdr_priv_data *)rig->state.priv;
	int ret;
	double f;
	uint32_t lfreq;

	f = (freq * priv->multiplier)/1e6;
	lfreq = fifisdr_tole32(round(f * 2097152.0));

	ret = fifisdr_usb_write(rig, REQUEST_SET_FREQ_BY_VALUE, 0, 0,
							(char *)&lfreq, sizeof(lfreq));
	if (ret != RIG_OK) {
		return -RIG_EIO;
	}

	return RIG_OK;
}



int fifisdr_get_freq_by_value(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct fifisdr_priv_data *priv = (struct fifisdr_priv_data *)rig->state.priv;
	int ret;
	uint32_t iFreq;

	ret = fifisdr_usb_read(rig, REQUEST_READ_FREQUENCY, 0, 0, (char *)&iFreq, sizeof(iFreq));

	if (ret == RIG_OK) {
		iFreq = fifisdr_fromle32(iFreq);
		*freq = MHz(((double)iFreq / (1ul << 21)) / priv->multiplier);
	}

	return ret;
}





static int fifisdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	int ret;
	uint8_t fifi_mode;
	uint32_t fifi_width;

	/* Translate mode into FiFi-SDR language */
	fifi_mode = 0;
	switch (mode) {
		case RIG_MODE_AM:
			fifi_mode = 2;
			break;
		case RIG_MODE_LSB:
			fifi_mode = 0;
			break;
		case RIG_MODE_USB:
			fifi_mode = 1;
			break;
		case RIG_MODE_FM:
			fifi_mode = 3;
			break;
		default:
			return -RIG_EINVAL;
	}
	ret = fifisdr_usb_write(rig, REQUEST_FIFISDR_WRITE, 0,
							15,	/* Demodulator mode */
							(char *)&fifi_mode, sizeof(fifi_mode));
	if (ret != RIG_OK) {
		return -RIG_EIO;
	}

	/* Set filter width */
	fifi_width = fifisdr_tole32(width);
	ret = fifisdr_usb_write(rig, REQUEST_FIFISDR_WRITE, 0,
							16,	/* Filter width */
							(char *)&fifi_width, sizeof(fifi_width));
	if (ret != RIG_OK) {
		return -RIG_EIO;
	}

	return RIG_OK;
}



static int fifisdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t * width)
{
	int ret;
	uint8_t fifi_mode;
	uint32_t fifi_width;


	/* Read current mode */
	ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
						   15,	/* Demodulator mode */
						   (char *)&fifi_mode, sizeof(fifi_mode));

	if (ret != RIG_OK) {
		return -RIG_EIO;
	}

	/* Translate mode coding */
	*mode = RIG_MODE_NONE;
	switch (fifi_mode) {
		case 0:
			*mode = RIG_MODE_LSB;
			break;
		case 1:
			*mode = RIG_MODE_USB;
			break;
		case 2:
			*mode = RIG_MODE_AM;
			break;
		case 3:
			*mode = RIG_MODE_FM;
			break;
	}

	/* Read current filter width */
	ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
						   16,	/* Filter width */
						   (char *)&fifi_width, sizeof(fifi_width));
	if (ret != RIG_OK) {
		return -RIG_EIO;
	}

	*width = s_Hz(fifisdr_fromle32(fifi_width));

	return RIG_OK;
}





static int fifisdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	int ret = RIG_OK;
	uint32_t fifi_meter = 0;


	switch (level) {
		/* Signal strength */
		case RIG_LEVEL_STRENGTH:
			/* Read current signal strength */
			ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
								   17,	/* S-Meter */
								   (char *)&fifi_meter, sizeof(fifi_meter));
			if (ret == RIG_OK) {
				val->i = fifisdr_fromle32(fifi_meter);
			}
		break;

		/* Unsupported option */
		default:
			ret = -RIG_ENIMPL;
	}

	return ret;
}


static int fifisdr_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
	int ret = RIG_OK;
	uint32_t u32;


	switch (token) {
		/* Signal strength */
		case TOK_LVL_FMCENTER:
			/* Read current signal strength */
			ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
								   18,	/* FM center frequency */
								   (char *)&u32, sizeof(u32));
			if (ret == RIG_OK) {
				val->f = Hz((int32_t)fifisdr_fromle32(u32));
			}
		break;

		/* Unsupported option */
		default:
			ret = -RIG_ENIMPL;
	}

	return ret;
}

#endif	/* defined(HAVE_LIBUSB) && defined(HAVE_USB_H) */
