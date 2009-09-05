/*
 *  Hamlib KIT backend - SoftRock / Si570 AVR USB tuner description
 *  Copyright (c) 2009 by Stephane Fillod
 *
 *  Derived from usbsoftrock-0.5:
 *  Copyright (C) 2009 Andrew Nilsson (andrew.nilsson@gmail.com)
 *
 *	$Id: si570avrusb.c,v 1.4 2008-04-26 09:00:30 fillods Exp $
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "hamlib/rig.h"
#include "token.h"

#include "kit.h"

/*
 * Compile this model only if libusb is available
 */
#if defined(HAVE_LIBUSB)

#include <errno.h>
#include <usb.h>

#include "si570avrusb.h"

static int si570avrusb_init(RIG *rig);
static int si570avrusb_cleanup(RIG *rig);
static int si570avrusb_open(RIG *rig);
static int si570avrusb_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int si570avrusb_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int si570avrusb_set_ptt(RIG * rig, vfo_t vfo, ptt_t ptt);
static int si570avrusb_set_conf(RIG *rig, token_t token, const char *val);
static int si570avrusb_get_conf(RIG *rig, token_t token, char *val);
static const char *si570avrusb_get_info(RIG *rig);



#define USBDEV_SHARED_VID	0x16C0  /* VOTI */
#define USBDEV_SHARED_PID	0x05DC  /* Obdev's free shared PID */
/* Use obdev's generic shared VID/PID pair and follow the rules outlined
 * in firmware/usbdrv/USBID-License.txt.
 */

#define VENDOR_NAME			"www.obdev.at"
#define PRODUCT_NAME			"DG8SAQ-I2C"


#define AVRUSB_WRITE_TIMEOUT 5000 /* from caps/state ? */


#define TOK_OSCFREQ	TOKEN_BACKEND(1)
#define TOK_MULTIPLIER	TOKEN_BACKEND(3)

static const struct confparams si570avrusb_cfg_params[] = {
	{ TOK_OSCFREQ, "osc_freq", "Oscillator freq", "Oscillator frequency in Hz",
			"114285000", RIG_CONF_NUMERIC, { .n = { 1, MHz(300), 1 } }
	},
	{ TOK_MULTIPLIER, "multiplier", "Freq Multiplier", "Frequency multiplier",
		"4", RIG_CONF_NUMERIC, { .n = { 0.000001, 100 } }
	},
	{ RIG_CONF_END, NULL, }
};


/*
 * Common data struct
 */
struct si570avrusb_priv_data {

	unsigned short version;		/* >=0x0f00 is PE0FKO's */

	double osc_freq;	/* MHz */
	double multiplier;	/* default to 4 for QSD/QSE */
};

#define SI570AVRUSB_MODES (RIG_MODE_USB)	/* USB is for SDR */

#define SI570AVRUSB_FUNC (RIG_FUNC_NONE)

#define SI570AVRUSB_LEVEL_ALL (RIG_LEVEL_NONE)

	/* TODO: BPF as a parm */
#define SI570AVRUSB_PARM_ALL (RIG_PARM_NONE)

#define SI570AVRUSB_VFO (RIG_VFO_A)

#define SI570AVRUSB_ANT (RIG_ANT_1)


/*
 * SoftRock Si570 AVR-USB description
 *
 * Heavily based on SoftRock USB-I2C Utility usbsoftrock-0.5,
 * author Andrew Nilsson VK6JBL:
 *   http://groups.yahoo.com/group/softrock40/files/VK6JBL/
 *
 */

const struct rig_caps si570avrusb_caps = {
.rig_model =  RIG_MODEL_SI570AVRUSB,
.model_name = "Si570 AVR-USB",
.mfg_name =  "SoftRock",
.version =  "0.1",
.copyright =  "GPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TUNER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_USB,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  5,
.retry = 0,

.has_get_func =  SI570AVRUSB_FUNC,
.has_set_func =  SI570AVRUSB_FUNC,
.has_get_level =  SI570AVRUSB_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(SI570AVRUSB_LEVEL_ALL),
.has_get_parm =  SI570AVRUSB_PARM_ALL,
.has_set_parm =  RIG_PARM_SET(SI570AVRUSB_PARM_ALL),
.level_gran =  {},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END },
.attenuator =   { RIG_DBLST_END },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  { RIG_CHAN_END, },

.rx_range_list1 =  {
    /* probably higher upper range, depending on type (CMOS, LVDS, ..) */
    {kHz(800),MHz(53.7),SI570AVRUSB_MODES,-1,-1,SI570AVRUSB_VFO},
	RIG_FRNG_END,
  },
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
    {kHz(800),MHz(53.7),SI570AVRUSB_MODES,-1,-1,SI570AVRUSB_VFO},
	RIG_FRNG_END,
  },
.tx_range_list2 =  { RIG_FRNG_END, },
.tuning_steps =  {
	 {SI570AVRUSB_MODES,Hz(1)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		RIG_FLT_END,
	},
.cfgparams =  si570avrusb_cfg_params,

.rig_init =     si570avrusb_init,
.rig_cleanup =  si570avrusb_cleanup,
.rig_open =     si570avrusb_open,
.set_conf =  si570avrusb_set_conf,
.get_conf =  si570avrusb_get_conf,

.set_freq    =  si570avrusb_set_freq,
.get_freq    =  si570avrusb_get_freq,
.set_ptt    =  si570avrusb_set_ptt,
.get_info    =  si570avrusb_get_info,

};



int si570avrusb_init(RIG *rig)
{
	hamlib_port_t *rp = &rig->state.rigport;
	struct si570avrusb_priv_data *priv;

	priv = (struct si570avrusb_priv_data*)calloc(sizeof(struct si570avrusb_priv_data), 1);
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	priv->osc_freq = SI570_NOMINAL_XTALL_FREQ;
	priv->multiplier = 4;

	rp->parm.usb.vid = USBDEV_SHARED_VID;
	rp->parm.usb.pid = USBDEV_SHARED_PID;
	rp->parm.usb.conf = 1;
	rp->parm.usb.iface = 0;
	rp->parm.usb.alt = 0;	/* necessary ? */

#if 0
	rp->parm.usb.vendor_name = VENDOR_NAME;
	rp->parm.usb.product = PRODUCT_NAME;
#endif

	rig->state.priv = (void*)priv;

	return RIG_OK;
}

int si570avrusb_cleanup(RIG *rig)
{
	if (!rig)
		return -RIG_EINVAL;

	if (rig->state.priv)
		free(rig->state.priv);
	rig->state.priv = NULL;

	return RIG_OK;
}

int si570avrusb_set_conf(RIG *rig, token_t token, const char *val)
{
	struct si570avrusb_priv_data *priv;
	freq_t freq;
	double multiplier;

	priv = (struct si570avrusb_priv_data*)rig->state.priv;

	switch(token) {
		case TOK_OSCFREQ:
			sscanf(val, "%"SCNfreq, &freq);
			priv->osc_freq = (double)freq/1e6;
			break;
		case TOK_MULTIPLIER:
			sscanf(val, "%lf", &multiplier);
			if (multiplier == 0.)
				return -RIG_EINVAL;
			priv->multiplier = multiplier;
			break;
		default:
			return -RIG_EINVAL;
	}
	return RIG_OK;
}

int si570avrusb_get_conf(RIG *rig, token_t token, char *val)
{
	struct si570avrusb_priv_data *priv;

	priv = (struct si570avrusb_priv_data*)rig->state.priv;

	switch(token) {
		case TOK_OSCFREQ:
			sprintf(val, "%"PRIfreq, (freq_t)(priv->osc_freq*1e6));
			break;
		case TOK_MULTIPLIER:
			sprintf(val, "%f", priv->multiplier);
			break;
		default:
			return -RIG_EINVAL;
	}
	return RIG_OK;
}



int si570avrusb_open(RIG *rig)
{
	struct si570avrusb_priv_data *priv = (struct si570avrusb_priv_data *)rig->state.priv;
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;
	unsigned short version;

	rig_debug(RIG_DEBUG_TRACE,"%s called\n", __func__);

	/*
	 * Determine firmware
	 */

	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
			REQUEST_READ_VERSION, 0x0E00, 0,
			(char *) &version, sizeof(version), AVRUSB_WRITE_TIMEOUT);

	if (ret != 2) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg failed: %s\n", 
					__func__,
					usb_strerror ());
		return -RIG_EIO;
	}

	priv->version = version;
	if (version  >= 0x0F00)
	{
		unsigned int iFreq;

		rig_debug(RIG_DEBUG_VERBOSE,"%s: detected PE0FKO-like firmware\n", __func__);

		ret = usb_control_msg(udh,
				USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
				REQUEST_READ_XTALL, 0, 0, (char *) &iFreq, sizeof(iFreq),
				AVRUSB_WRITE_TIMEOUT);

		if (ret != 4)
			return -RIG_EIO;

		priv->osc_freq = (double)iFreq / (1UL<<24);
	}

	rig_debug(RIG_DEBUG_VERBOSE,"%s: using Xtall at %.3f MHz\n",
			__func__, priv->osc_freq);

	return RIG_OK;
}

const char * si570avrusb_get_info(RIG *rig)
{
	static char buf[64];
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	struct usb_device *q = usb_device(udh);
    int ret;
	unsigned short version;

	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
			REQUEST_READ_VERSION, 0x0E00, 0,
			(char *) &version, sizeof(version), AVRUSB_WRITE_TIMEOUT);

	if (ret != 2) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg failed: %s\n", 
					__func__,
					usb_strerror ());
		return NULL;
	}

	sprintf(buf, "USB dev %04d, version: %d.%d", q->descriptor.bcdDevice,
			(version & 0xFF00) >> 8, version & 0xFF);

	return buf;
}


static const int HS_DIV_MAP[] = {4,5,6,7,-1,9,-1,11};

static int calcDividers(RIG *rig, double f, struct solution* solution)
{
	struct si570avrusb_priv_data *priv = (struct si570avrusb_priv_data *)rig->state.priv;
	struct solution sols[8]; 
	int i;
	int imin;
	double fmin;
	double y;
	
	// Count down through the dividers
	for (i=7;i >= 0;i--) {
		
		if (HS_DIV_MAP[i] > 0) {
			sols[i].HS_DIV = i;
			y = (SI570_DCO_HIGH + SI570_DCO_LOW) / (2 * f);
			y = y / HS_DIV_MAP[i];
			if (y < 1.5) {
				y = 1.0;
			} else {
				y = 2 * round ( y / 2.0);
			}
			if (y > 128) {
				y = 128;
			}
			sols[i].N1 = trunc(y) - 1;
			sols[i].f0 = f * y * HS_DIV_MAP[i];
		} else {
			sols[i].f0 = 10000000000000000.0;
		}
	}
	imin = -1;
	fmin = 10000000000000000.0;
		
	for (i=0; i < 8; i++) {
		if ((sols[i].f0 >= SI570_DCO_LOW) && (sols[i].f0 <= SI570_DCO_HIGH)) {
			if (sols[i].f0 < fmin) {
				fmin = sols[i].f0;
				imin = i;
			}
		}
	}
		
	if (imin >= 0) {
		solution->HS_DIV = sols[imin].HS_DIV;
		solution->N1 = sols[imin].N1;
		solution->f0 = sols[imin].f0;
		solution->RFREQ = sols[imin].f0 / priv->osc_freq;

	    rig_debug(RIG_DEBUG_TRACE, "%s: solution: HS_DIV = %d, N1 = %d, f0 = %f, RFREQ = %f\n", 
			__func__, solution->HS_DIV, solution->N1, solution->f0, solution->RFREQ);

		return 1;
	} else {
		return 0;
	}
}

static void setLongWord(uint32_t value, char * bytes)
{
	bytes[0] = value & 0xff;
	bytes[1] = ((value & 0xff00) >> 8) & 0xff;
	bytes[2] = ((value & 0xff0000) >> 16) & 0xff;
	bytes[3] = ((value & 0xff000000) >> 24) & 0xff;
} 


int si570avrusb_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct si570avrusb_priv_data *priv = (struct si570avrusb_priv_data *)rig->state.priv;
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;
	char buffer[6];
	int request = REQUEST_SET_FREQ;
	int value = 0x700 + SI570_I2C_ADDR;
	int index = 0;
	double f;
	struct solution theSolution;
	int RFREQ_int;
	int RFREQ_frac;
	unsigned char fracBuffer[4];
	unsigned char intBuffer[4];

#if 0
	if (priv->version >= 0x0f00)
		return si570avrusb_set_freq_by_value(rig, vfo, freq);
#endif

	f = (freq * priv->multiplier)/1e6;

	calcDividers(rig, f, &theSolution);

	RFREQ_int = trunc(theSolution.RFREQ);
	RFREQ_frac = round((theSolution.RFREQ - RFREQ_int)*268435456);
	setLongWord(RFREQ_int, (char *) intBuffer);
	setLongWord(RFREQ_frac, (char *) fracBuffer);
	
	buffer[5] = fracBuffer[0];
	buffer[4] = fracBuffer[1];
	buffer[3] = fracBuffer[2];
	buffer[2] = fracBuffer[3];
	buffer[2] = buffer[2] | ((intBuffer[0] & 0xf) << 4);
	buffer[1] = RFREQ_int / 16;
	buffer[1] = buffer[1] + ((theSolution.N1 & 3) << 6);
	buffer[0] = theSolution.N1 / 4;
	buffer[0] = buffer[0] + (theSolution.HS_DIV << 5);

	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
			request, value, index, buffer, sizeof(buffer), AVRUSB_WRITE_TIMEOUT);

	rig_debug(RIG_DEBUG_TRACE, "%s: Freq=%.6f MHz, Real=%.6f MHz, buf=%02x%02x%02x%02x%02x%02x\n", 
			__func__, freq/1e6, f,
			buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);


	if (!ret) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg failed: %s\n", 
					__func__,
					usb_strerror ());
		return -RIG_EIO;
	}

	rig_debug(RIG_DEBUG_TRACE, "%s: Result buf=%02x%02x\n", 
			__func__, buffer[0], buffer[1]);

	return RIG_OK;
}

int si570avrusb_set_freq_by_value(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct si570avrusb_priv_data *priv = (struct si570avrusb_priv_data *)rig->state.priv;
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;

	char buffer[4];
	int request = REQUEST_SET_FREQ_BY_VALUE;
	int value = 0x700 + SI570_I2C_ADDR;
	int index = 0;
	double f;
       
	f = (freq * priv->multiplier)/1e6;

	setLongWord(round(f * 2097152.0), buffer);

	rig_debug(RIG_DEBUG_TRACE, "%s: Freq=%.6f MHz, Real=%.6f MHz, buf=%02x%02x%02x%02x\n", 
			__func__, freq/1e6, f,
			buffer[0], buffer[1], buffer[2], buffer[3]);

	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
			request, value, index, buffer, sizeof(buffer), AVRUSB_WRITE_TIMEOUT);

	if (!ret) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg failed: %s\n", 
					__func__,
					usb_strerror ());
		return -RIG_EIO;
	}

	rig_debug(RIG_DEBUG_TRACE, "%s: Result buf=%02x%02x\n", 
			__func__, buffer[0], buffer[1]);

	return RIG_OK;
}

static double calculateFrequency(RIG *rig, const unsigned char * buffer)
{
	struct si570avrusb_priv_data *priv = (struct si570avrusb_priv_data *)rig->state.priv;

	int RFREQ_int = ((buffer[2] & 0xf0) >> 4) + ((buffer[1] & 0x3f) * 16);
	int RFREQ_frac = (256 * 256 * 256 * (buffer[2] & 0xf)) + (256 * 256 * buffer[3]) + (256 * buffer[4]) + (buffer[5]);
	double RFREQ = RFREQ_int + (RFREQ_frac / 268435456.0);
	int N1 = ((buffer[1] & 0xc0 ) >> 6) + ((buffer[0] & 0x1f) * 4);
	int HS_DIV = (buffer[0] & 0xE0) >> 5;
	double fout = priv->osc_freq * RFREQ / ((N1 + 1) * HS_DIV_MAP[HS_DIV]);
	
	rig_debug (RIG_DEBUG_VERBOSE,
			"%s: Registers 7..13: %02x%02x%02x%02x%02x%02x\n",
			__func__,
			buffer[0],
			buffer[1],
			buffer[2],
			buffer[3],
			buffer[4],
			buffer[5]);

	rig_debug (RIG_DEBUG_VERBOSE,
			"%s: RFREQ = %f, N1 = %d, HS_DIV = %d, nHS_DIV = %d, fout = %f\n",
			__func__,
			RFREQ, N1, HS_DIV, HS_DIV_MAP[HS_DIV], fout);

	return fout;
}

int si570avrusb_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct si570avrusb_priv_data *priv = (struct si570avrusb_priv_data *)rig->state.priv;
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	unsigned char buffer[6];
	int ret;

#if 0
	if (priv->version >= 0x0f00)
		return si570avrusb_get_freq_by_value(rig, vfo, freq);
#endif

	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
			REQUEST_READ_REGISTERS, SI570_I2C_ADDR, 0,
			(char *)buffer, sizeof(buffer), AVRUSB_WRITE_TIMEOUT);

	if (ret <= 0) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg failed: %s\n", 
					__func__,
					usb_strerror ());
		return -RIG_EIO;
	}

	*freq = (calculateFrequency(rig, buffer) / priv->multiplier)*1e6;

	return RIG_OK;
}

int si570avrusb_get_freq_by_value(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct si570avrusb_priv_data *priv = (struct si570avrusb_priv_data *)rig->state.priv;
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;
	uint32_t iFreq;

	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
			REQUEST_READ_FREQUENCY, 0, 0,
			(char *)&iFreq, sizeof(iFreq), AVRUSB_WRITE_TIMEOUT);

	if (ret != 4) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg failed: %s\n", 
					__func__,
					usb_strerror ());
		return -RIG_EIO;
	}

	*freq = (((double)iFreq / (1UL<<21)) / priv->multiplier)/1e6;

	return RIG_OK;
}


int si570avrusb_set_ptt(RIG * rig, vfo_t vfo, ptt_t ptt)
{
	struct usb_dev_handle *udh = rig->state.rigport.handle;
	int ret;
	char buffer[3];

	rig_debug(RIG_DEBUG_TRACE,"%s called: %d\n", __func__, ptt);

	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 0;
	
	ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
			REQUEST_SET_PTT, (ptt == RIG_PTT_ON) ? 1 : 0, 0,
			(char *)buffer, sizeof(buffer), AVRUSB_WRITE_TIMEOUT);
	if (ret != 0) {
		rig_debug (RIG_DEBUG_ERR, "%s: usb_control_msg failed: %s\n", 
					__func__,
					usb_strerror ());
		return -RIG_EIO;
	}

	return RIG_OK;
}

#endif	/* defined(HAVE_LIBUSB) */

