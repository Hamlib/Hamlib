/*
 *  Hamlib KIT backend - FUNcube Dongle USB tuner description
 *  Copyright (c) 2009-2011 by Stephane Fillod
 *
 *  Derived from usbsoftrock-0.5:
 *  Copyright (C) 2009 Andrew Nilsson (andrew.nilsson@gmail.com)
 *
 *  Author: Stefano Speretta, Innovative Solutions In Space BV
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

#include "funcube.h"

static int funcube_init(RIG *rig);
static int funcube_cleanup(RIG *rig);
static int funcube_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int funcube_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int funcube_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int funcube_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static const char *funcube_get_info(RIG *rig);

static const struct confparams funcube_cfg_params[] = {
	{ RIG_CONF_END, NULL, }
};

// functions used set / read frequency, working on FUNcube version 0 and 1
int set_freq_v0(usb_dev_handle *udh, unsigned int f, int timeout);
int set_freq_v1(usb_dev_handle *udh, unsigned int f, int timeout);

/*
 * Common data struct
 */
struct funcube_priv_data {

	freq_t freq;	/* Hz */
};

/*
 * FUNcube Dongle description
 *
 * Based on Jan Axelson HID examples
 *   http://www.lvr.com/
 *
 */

const struct rig_caps funcube_caps = {
.rig_model =  RIG_MODEL_FUNCUBEDONGLE,
.model_name = "FUNcube Dongle",
.mfg_name =  "AMSAT-UK",
.version =  "0.2",
.copyright =  "GPL",
.status =  RIG_STATUS_BETA,
.rig_type =  RIG_TYPE_TUNER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_USB,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  1000,
.retry = 0,

.has_get_func =  RIG_FUNC_NONE,
.has_set_func =  RIG_FUNC_NONE,
.has_get_level =  RIG_LEVEL_ATT | RIG_LEVEL_STRENGTH | RIG_LEVEL_PREAMP,
.has_set_level =  RIG_LEVEL_ATT | RIG_LEVEL_PREAMP,
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,
.level_gran =  {},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp = { 5, 10, 15, 20, 25, 30, RIG_DBLST_END, },
.attenuator = { 2, 5, RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  { RIG_CHAN_END, },

.rx_range_list1 =  {
    {MHz(50),MHz(2500),RIG_MODE_USB,-1,-1,RIG_VFO_A},
	RIG_FRNG_END,
  },
.tuning_steps =  {
	 {RIG_MODE_USB,kHz(1)},
	 RIG_TS_END,
	},
.filters =  {
	{RIG_MODE_USB, kHz(192)},
	RIG_FLT_END,
    },
.cfgparams =  funcube_cfg_params,

.rig_init =     funcube_init,
.rig_cleanup =  funcube_cleanup,
.set_freq    =  funcube_set_freq,
.get_freq    =  funcube_get_freq,
.get_level   =  funcube_get_level,
.set_level   =  funcube_set_level,
.get_info    =  funcube_get_info,
};

int funcube_init(RIG *rig)
{
	hamlib_port_t *rp = &rig->state.rigport;
	struct funcube_priv_data *priv;

	priv = (struct funcube_priv_data*)calloc(sizeof(struct funcube_priv_data), 1);
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	priv->freq = 0;

	rp->parm.usb.vid = VID;
	rp->parm.usb.pid = PID;
	rp->parm.usb.conf = FUNCUBE_CONFIGURATION;
	rp->parm.usb.iface = FUNCUBE_INTERFACE;
	rp->parm.usb.alt = FUNCUBE_ALTERNATIVE_SETTING;

	rp->parm.usb.vendor_name = VENDOR_NAME;
	rp->parm.usb.product = PRODUCT_NAME;

	rig->state.priv = (void*)priv;

	return RIG_OK;
}

int funcube_cleanup(RIG *rig)
{
	if (!rig)
		return -RIG_EINVAL;

	if (rig->state.priv)
		free(rig->state.priv);
	rig->state.priv = NULL;

	return RIG_OK;
}

const char * funcube_get_info(RIG *rig)
{
	static char buf[64];
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	struct usb_device *q = usb_device(udh);

	sprintf(buf, "USB dev %04d", q->descriptor.bcdDevice);

	return buf;
}

int set_freq_v0(usb_dev_handle *udh, unsigned int f, int timeout)
{
	int ret;

	char au8BufOut[64]; // endpoint size
	char au8BufIn[64];  // endpoint size

	// frequency is in Hz, while the dongle expects it in kHz
	f = f / 1e3;

	au8BufOut[0]=REQUEST_SET_FREQ; // Command to Set Frequency on dongle
	au8BufOut[1]=(char)f;
	au8BufOut[2]=(char)(f>>8);
	au8BufOut[3]=(char)(f>>16);

	rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
		__func__, (unsigned)au8BufOut[0] & 0xFF, (unsigned)au8BufOut[1] & 0xFF, (unsigned)au8BufOut[2] & 0xFF, (unsigned)au8BufOut[3] & 0xFF);

    ret = usb_interrupt_write(udh, OUTPUT_ENDPOINT, au8BufOut, sizeof(au8BufOut), timeout);

	if( ret < 0 )
    {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_write failed (%d): %s\n",
					__func__,ret, usb_strerror ());
					return -RIG_EIO;
    }

    ret = usb_interrupt_read(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn), timeout);

    if( ret != sizeof(au8BufIn) )
    {
	rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_read failed (%d): %s\n",
					__func__, ret, usb_strerror ());
					return -RIG_EIO;
    }

	rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x\n",
		__func__, (unsigned)au8BufIn[0] & 0xFF, (unsigned)au8BufIn[1] & 0xFF);

	if (au8BufIn[1] != FUNCUBE_SUCCESS) {
		rig_debug (RIG_DEBUG_ERR, "%s: REQUEST_SET_FREQ not supported\n",
					__func__);
		return -RIG_EIO;
	}

	return RIG_OK;
}

int set_freq_v1(usb_dev_handle *udh, unsigned int f, int timeout)
{
	int ret;

	char au8BufOut[64]; // endpoint size
	char au8BufIn[64];  // endpoint size

	au8BufOut[0]=REQUEST_SET_FREQ_HZ; // Command to Set Frequency in Hz on dongle
	au8BufOut[1]=(char)f;
	au8BufOut[2]=(char)(f>>8);
	au8BufOut[3]=(char)(f>>16);
	au8BufOut[4]=(char)(f>>24);

	rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x%02x\n",
		__func__, (unsigned)au8BufOut[0] & 0xFF, (unsigned)au8BufOut[1] & 0xFF, (unsigned)au8BufOut[2] & 0xFF, (unsigned)au8BufOut[3] & 0xFF,
						(unsigned)au8BufOut[4] & 0xFF);

    ret = usb_interrupt_write(udh, OUTPUT_ENDPOINT, au8BufOut, sizeof(au8BufOut), timeout);

	if( ret < 0 )
    {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_write failed (%d): %s\n",
					__func__,ret,
					usb_strerror ());
					return -RIG_EIO;
    }

    ret = usb_interrupt_read(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn), timeout);

    if( ret != sizeof(au8BufIn) )
    {
	rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_read failed (%d): %s\n",
					__func__, ret,
					usb_strerror ());
					return -RIG_EIO;
    }

	rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x%02x%02x%02x%02x\n",
		__func__, (unsigned)au8BufIn[0] & 0xFF, (unsigned)au8BufIn[1] & 0xFF, (unsigned)au8BufIn[2] & 0xFF, (unsigned)au8BufIn[3] & 0xFF,
						(unsigned)au8BufIn[4] & 0xFF, (unsigned)au8BufIn[5] & 0xFF);

	if (au8BufIn[1] != FUNCUBE_SUCCESS) {
		rig_debug (RIG_DEBUG_ERR, "%s: REQUEST_SET_FREQ_HZ not supported\n",
					__func__);
		return -RIG_EIO;
	}

	return RIG_OK;
}

int funcube_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct funcube_priv_data *priv = (struct funcube_priv_data *)rig->state.priv;
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;

	if ((ret = set_freq_v1(udh, freq, rig->state.rigport.timeout)) != RIG_OK) {
		if ((ret = set_freq_v0(udh, freq, rig->state.rigport.timeout)) == RIG_OK) {
			priv->freq = freq;
		}
	}
	else {
		priv->freq = freq;
	}

	return ret;
}

int get_freq_v0(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct funcube_priv_data *priv = (struct funcube_priv_data *)rig->state.priv;

	rig_debug(RIG_DEBUG_TRACE, "%s: frequency is not read from the device, the value shown is the last succesfully set.\n",__func__);
	*freq = priv->freq;

	return RIG_OK;
}

int get_freq_v1(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;
	unsigned int f;

	char au8BufOut[64]; // endpoint size
	char au8BufIn[64];  // endpoint size

	au8BufOut[0]=REQUEST_GET_FREQ_HZ; // Command to Set Frequency on dongle

	rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
		__func__, (unsigned)au8BufOut[0] & 0xFF, (unsigned)au8BufOut[1] & 0xFF, (unsigned)au8BufOut[2] & 0xFF, (unsigned)au8BufOut[3] & 0xFF);

    ret = usb_interrupt_write(udh, OUTPUT_ENDPOINT, au8BufOut, sizeof(au8BufOut), rig->state.rigport.timeout);

	if( ret < 0 )
    {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_write failed (%d): %s\n",
					__func__,ret,
					usb_strerror ());
    }

    ret = usb_interrupt_read(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn), rig->state.rigport.timeout);

    if( ret != sizeof(au8BufIn) )
    {
	rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_read failed (%d): %s\n",
					__func__, ret,
					usb_strerror ());
    }

	rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x%02x%02x%02x%02x\n",
		__func__, (unsigned)au8BufIn[0] & 0xFF, (unsigned)au8BufIn[1] & 0xFF, (unsigned)au8BufIn[2] & 0xFF, (unsigned)au8BufIn[3] & 0xFF,
									(unsigned)au8BufIn[4] & 0xFF, (unsigned)au8BufIn[5] & 0xFF);

	if (au8BufIn[1] != FUNCUBE_SUCCESS) {
		rig_debug (RIG_DEBUG_ERR, "%s: REQUEST_GET_FREQ_HZ not supported\n",
					__func__);
		return -RIG_EIO;
	}

	f = ((unsigned int)au8BufIn[2] & 0xFF) | (((unsigned int)au8BufIn[3] & 0xFF) << 8) |
				(((unsigned int)au8BufIn[4] & 0xFF) << 16) | (((unsigned int)au8BufIn[5] & 0xFF) << 24),

	*freq = f;

	return RIG_OK;
}

int funcube_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	int ret;

	if ((ret = get_freq_v1(rig, vfo, freq)) != RIG_OK) {
		ret = get_freq_v0(rig, vfo, freq);
	}

	return ret;
}

int funcube_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;
	char au8BufOut[64]; // endpoint size
	char au8BufIn[64];  // endpoint size

	switch (level) {
	case RIG_LEVEL_PREAMP:
		au8BufOut[0]=REQUEST_SET_LNA_GAIN; // Command to Set LNA gain

		switch (val.i) {
			case 5:
				au8BufOut[1]=6;
			break;
			case 10:
				au8BufOut[1]=8;
			break;
			case 15:
				au8BufOut[1]=10;
			break;
			case 20:
				au8BufOut[1]=12;
			break;
			case 25:
				au8BufOut[1]=13;
			break;
			case 30:
				au8BufOut[1]=14;
			break;
			default:
				au8BufOut[1]=4;
		}
	break;

	case RIG_LEVEL_ATT:
		au8BufOut[0]=REQUEST_SET_LNA_GAIN; // Command to Set LNA gain

		switch (val.i) {
			case 2:
				au8BufOut[1]=1;
			break;
			case 5:
				au8BufOut[1]=0;
			break;
			default:
				au8BufOut[1]=4;
		}
	break;

	default:
		rig_debug(RIG_DEBUG_ERR,"%s: Unsupported level %d\n", __func__, level);
		return -RIG_EINVAL;
	}

	rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
		__func__, (unsigned)au8BufOut[0] & 0xFF, (unsigned)au8BufOut[1] & 0xFF, (unsigned)au8BufOut[2] & 0xFF, (unsigned)au8BufOut[3] & 0xFF);

    ret = usb_interrupt_write(udh, OUTPUT_ENDPOINT, au8BufOut, sizeof(au8BufOut), rig->state.rigport.timeout);

	if( ret < 0 )
    {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_write failed (%d): %s\n",
					__func__,ret,
					usb_strerror ());
    }

    ret = usb_interrupt_read(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn), rig->state.rigport.timeout);

    if( ret != sizeof(au8BufIn) )
    {
	rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_read failed (%d): %s\n",
					__func__, ret,
					usb_strerror ());
    }

	rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x\n",
		__func__, (unsigned)au8BufIn[0] & 0xFF, (unsigned)au8BufIn[1] & 0xFF);

	if (au8BufIn[1] != FUNCUBE_SUCCESS) {
		rig_debug (RIG_DEBUG_ERR, "%s: REQUEST_GET_FREQ_HZ not supported\n",
					__func__);
		return -RIG_EIO;
	}

	return RIG_OK;
}
int funcube_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;
	char au8BufOut[64]; // endpoint size
	char au8BufIn[64];  // endpoint size

	switch (level) {
	case RIG_LEVEL_ATT:
	case RIG_LEVEL_PREAMP:
		au8BufOut[0]=REQUEST_GET_LNA_GAIN; // Command to Get LNA / ATT gain
	break;

	case RIG_LEVEL_STRENGTH:
		au8BufOut[0]=REQUEST_GET_RSSI;
		break;

	default:
		rig_debug(RIG_DEBUG_ERR,"%s: Unsupported level %d\n", __func__, level);
		return -RIG_EINVAL;
	}

	rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
		__func__, (unsigned)au8BufOut[0] & 0xFF, (unsigned)au8BufOut[1] & 0xFF, (unsigned)au8BufOut[2] & 0xFF, (unsigned)au8BufOut[3] & 0xFF);

    ret = usb_interrupt_write(udh, OUTPUT_ENDPOINT, au8BufOut, sizeof(au8BufOut), rig->state.rigport.timeout);

	if( ret < 0 )
    {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_write failed (%d): %s\n",
					__func__,ret,
					usb_strerror ());
    }

    ret = usb_interrupt_read(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn), rig->state.rigport.timeout);

    if( ret != sizeof(au8BufIn) )
    {
	rig_debug (RIG_DEBUG_ERR, "%s: usb_interrupt_read failed (%d): %s\n",
					__func__, ret,
					usb_strerror ());
    }

	rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x%02x\n",
		__func__, (unsigned)au8BufIn[0] & 0xFF, (unsigned)au8BufIn[1] & 0xFF, (unsigned)au8BufIn[2] & 0xFF);

	if (au8BufIn[1] != FUNCUBE_SUCCESS) {
		rig_debug (RIG_DEBUG_ERR, "%s: REQUEST_GET_FREQ_HZ not supported\n",
					__func__);
		return -RIG_EIO;
	}

	switch (level) {
		case RIG_LEVEL_PREAMP:
			switch (au8BufIn[2]) {
				case 6:
					val->i = 5;
				break;
				case 8:
					val->i = 10;
				break;
				case 10:
					val->i = 15;
				break;
				case 12:
					val->i = 20;
				break;
				case 13:
					val->i = 25;
				break;
				case 14:
					val->i = 30;
				break;
				default:
					val->i = 0;
			}
		break;

		case RIG_LEVEL_ATT:
			switch (au8BufIn[2]) {
				case 0:
					val->i = 5;
				break;
				case 1:
					val->i = 2;
				break;
				default:
					val->i = 0;
			}
		break;

		case RIG_LEVEL_STRENGTH:
			val->i = (int)((float)au8BufIn[2] * 2.8 - 35);
		break;

		default:
			rig_debug(RIG_DEBUG_ERR,"%s: Unsupported level %d\n", __func__, level);
			return -RIG_EINVAL;
	}

	return RIG_OK;
}
#endif	/* defined(HAVE_LIBUSB) && defined(HAVE_USB_H) */
