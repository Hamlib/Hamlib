/*
 *  Hamlib KIT backend - SoftRock / Si570 AVR USB tuner description
 *  Copyright (c) 2009-2010 by Stephane Fillod
 *
 *  Derived from usbsoftrock-0.5:
 *  Copyright (C) 2009 Andrew Nilsson (andrew.nilsson@gmail.com)
 *
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

#include <hamlib/config.h>

#define BACKEND_VER "20200112"

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
#if defined(HAVE_LIBUSB) && (defined(HAVE_LIBUSB_H) || defined(HAVE_LIBUSB_1_0_LIBUSB_H))

#include <errno.h>

#ifdef HAVE_LIBUSB_H
# include <libusb.h>
#elif defined HAVE_LIBUSB_1_0_LIBUSB_H
# include <libusb-1.0/libusb.h>
#endif

#include "si570avrusb.h"

static int si570avrusb_init(RIG *rig);
static int si570picusb_init(RIG *rig);
static int si570peaberry1_init(RIG *rig);
static int si570peaberry2_init(RIG *rig);
static int fasdr_init(RIG *rig);
static int si570xxxusb_cleanup(RIG *rig);
static int si570xxxusb_open(RIG *rig);
static int fasdr_open(RIG *rig);
static int si570xxxusb_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int si570xxxusb_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int si570xxxusb_set_freq_by_value(RIG *rig, vfo_t vfo, freq_t freq);
static int si570xxxusb_get_freq_by_value(RIG *rig, vfo_t vfo, freq_t *freq);
static int si570xxxusb_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int si570xxxusb_set_conf(RIG *rig, token_t token, const char *val);
static int si570xxxusb_get_conf(RIG *rig, token_t token, char *val);
static const char *si570xxxusb_get_info(RIG *rig);



#define USBDEV_SHARED_VID   0x16C0  /* VOTI */
#define USBDEV_SHARED_PID   0x05DC  /* Obdev's free shared PID */
/* Use obdev's generic shared VID/PID pair and follow the rules outlined
 * in firmware/usbdrv/USBID-License.txt.
 */

#define VENDOR_NAME         "www.obdev.at"
#define PEABERRY_VENDOR_NAME        "AE9RB"   /* Only for V1, V2 uses the 'standard' obdev name */
#define AVR_PRODUCT_NAME        "DG8SAQ-I2C"
#define PIC_PRODUCT_NAME        "KTH-SDR-KIT"
#define PEABERRY_PRODUCT_NAME       "Peaberry SDR"


#define TOK_OSCFREQ TOKEN_BACKEND(1)
#define TOK_MULTIPLIER  TOKEN_BACKEND(3)
#define TOK_I2C_ADDR    TOKEN_BACKEND(4)
#define TOK_BPF         TOKEN_BACKEND(5)

#define F_CAL_STATUS 1  //1 byte
#define F_CRYST 2       //4 byte

static const struct confparams si570xxxusb_cfg_params[] =
{
    {
        TOK_OSCFREQ, "osc_freq", "Oscillator freq", "Oscillator frequency in Hz",
        "114285000", RIG_CONF_NUMERIC, { .n = { 1, MHz(300), 1 } }
    },
    {
        TOK_MULTIPLIER, "multiplier", "Freq Multiplier", "Frequency multiplier",
        "4", RIG_CONF_NUMERIC, { .n = { 0.000001, 100 } }
    },
    {
        TOK_I2C_ADDR, "i2c_addr", "I2C Address", "Si570 I2C Address",
        "55", RIG_CONF_NUMERIC, { .n = { 0, 512 } }
    },
    {
        TOK_BPF, "bpf", "BPF", "Enable Band Pass Filter",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    { RIG_CONF_END, NULL, }
};


/*
 * Common data struct
 */
struct si570xxxusb_priv_data
{

    unsigned short version;     /* >=0x0f00 is PE0FKO's */

    double osc_freq;    /* MHz */
    double multiplier;  /* default to 4 for QSD/QSE */

    int i2c_addr;
    int bpf;    /* enable BPF? */
};

#define SI570AVRUSB_MODES (RIG_MODE_USB)    /* USB is for SDR */

#define SI570AVRUSB_FUNC (RIG_FUNC_NONE)

#define SI570AVRUSB_LEVEL_ALL (RIG_LEVEL_NONE)

/* TODO: BPF as a parm or ext_level? */
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

const struct rig_caps si570avrusb_caps =
{
    RIG_MODEL(RIG_MODEL_SI570AVRUSB),
    .model_name =       "Si570 AVR-USB",
    .mfg_name =     "SoftRock",
    .version =      BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TUNER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_USB,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      500,
    .retry =        0,

    .has_get_func =     SI570AVRUSB_FUNC,
    .has_set_func =     SI570AVRUSB_FUNC,
    .has_get_level =    SI570AVRUSB_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(SI570AVRUSB_LEVEL_ALL),
    .has_get_parm =     SI570AVRUSB_PARM_ALL,
    .has_set_parm =     RIG_PARM_SET(SI570AVRUSB_PARM_ALL),
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END },
    .attenuator =       { RIG_DBLST_END },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        /* probably higher upper range, depending on type (CMOS, LVDS, ..) */
        {kHz(800), MHz(53.7), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 = {
        {kHz(800), MHz(53.7), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END, },
    .tuning_steps = {
        {SI570AVRUSB_MODES, Hz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        RIG_FLT_END,
    },
    .cfgparams =        si570xxxusb_cfg_params,

    .rig_init =     si570avrusb_init,
    .rig_cleanup =      si570xxxusb_cleanup,
    .rig_open =     si570xxxusb_open,
    .set_conf =     si570xxxusb_set_conf,
    .get_conf =     si570xxxusb_get_conf,

    .set_freq =     si570xxxusb_set_freq,
    .get_freq =     si570xxxusb_get_freq,
    .set_ptt =      si570xxxusb_set_ptt,
    .get_info =     si570xxxusb_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Peaberry V1
 * http://www.ae9rb.com
 */
const struct rig_caps si570peaberry1_caps =
{
    RIG_MODEL(RIG_MODEL_SI570PEABERRY1),
    .model_name =       "Si570 Peaberry V1",
    .mfg_name =     "AE9RB",
    .version =      BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TUNER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_USB,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      500,
    .retry =        0,

    .has_get_func =     SI570AVRUSB_FUNC,
    .has_set_func =     SI570AVRUSB_FUNC,
    .has_get_level =    SI570AVRUSB_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(SI570AVRUSB_LEVEL_ALL),
    .has_get_parm =     SI570AVRUSB_PARM_ALL,
    .has_set_parm =     RIG_PARM_SET(SI570AVRUSB_PARM_ALL),
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END },
    .attenuator =       { RIG_DBLST_END },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        /* probably higher upper range, depending on type (CMOS, LVDS, ..) */
        {kHz(800), MHz(53.7), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 = {
        {kHz(800), MHz(53.7), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END, },
    .tuning_steps = {
        {SI570AVRUSB_MODES, Hz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        RIG_FLT_END,
    },
    .cfgparams =        si570xxxusb_cfg_params,

    .rig_init =     si570peaberry1_init,
    .rig_cleanup =      si570xxxusb_cleanup,
    .rig_open =     si570xxxusb_open,
    .set_conf =     si570xxxusb_set_conf,
    .get_conf =     si570xxxusb_get_conf,

    .set_freq =     si570xxxusb_set_freq,
    .get_freq =     si570xxxusb_get_freq,
    .set_ptt =      si570xxxusb_set_ptt,
    .get_info =     si570xxxusb_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Peaberry V2
 * http://www.ae9rb.com
 */
const struct rig_caps si570peaberry2_caps =
{
    RIG_MODEL(RIG_MODEL_SI570PEABERRY2),
    .model_name =       "Si570 Peaberry V2",
    .mfg_name =     "AE9RB",
    .version =      BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TUNER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_USB,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      500,
    .retry =        0,

    .has_get_func =     SI570AVRUSB_FUNC,
    .has_set_func =     SI570AVRUSB_FUNC,
    .has_get_level =    SI570AVRUSB_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(SI570AVRUSB_LEVEL_ALL),
    .has_get_parm =     SI570AVRUSB_PARM_ALL,
    .has_set_parm =     RIG_PARM_SET(SI570AVRUSB_PARM_ALL),
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END },
    .attenuator =       { RIG_DBLST_END },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        /* probably higher upper range, depending on type (CMOS, LVDS, ..) */
        {kHz(800), MHz(53.7), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 = {
        {kHz(800), MHz(53.7), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END, },
    .tuning_steps = {
        {SI570AVRUSB_MODES, Hz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        RIG_FLT_END,
    },
    .cfgparams =        si570xxxusb_cfg_params,

    .rig_init =     si570peaberry2_init,
    .rig_cleanup =      si570xxxusb_cleanup,
    .rig_open =     si570xxxusb_open,
    .set_conf =     si570xxxusb_set_conf,
    .get_conf =     si570xxxusb_get_conf,

    .set_freq =     si570xxxusb_set_freq,
    .get_freq =     si570xxxusb_get_freq,
    .set_ptt =      si570xxxusb_set_ptt,
    .get_info =     si570xxxusb_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * KTH-SDR-KIT Si570 PIC-USB description
 *
 * Same USB interface as AVR-USB, except different product string
 * and different multiplier.
 *
 *   http://home.kpn.nl/rw.engberts/sdr_kth.htm
 */
const struct rig_caps si570picusb_caps =
{
    RIG_MODEL(RIG_MODEL_SI570PICUSB),
    .model_name =       "Si570 PIC-USB",
    .mfg_name =     "KTH-SDR kit",
    .version =      BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TUNER,
    .ptt_type =     RIG_PTT_NONE,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_USB,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      500,
    .retry =        0,

    .has_get_func =     SI570AVRUSB_FUNC,
    .has_set_func =     SI570AVRUSB_FUNC,
    .has_get_level =    SI570AVRUSB_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(SI570AVRUSB_LEVEL_ALL),
    .has_get_parm =     SI570AVRUSB_PARM_ALL,
    .has_set_parm =     RIG_PARM_SET(SI570AVRUSB_PARM_ALL),
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END },
    .attenuator =       { RIG_DBLST_END }, /* TODO */
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        {kHz(800), MHz(550), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 = {
        {kHz(800), MHz(550), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END, },
    .tuning_steps = {
        {SI570AVRUSB_MODES, Hz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        RIG_FLT_END,
    },
    .cfgparams =        si570xxxusb_cfg_params,

    .rig_init =     si570picusb_init,
    .rig_cleanup =      si570xxxusb_cleanup,
    .rig_open =     si570xxxusb_open,
    .set_conf =     si570xxxusb_set_conf,
    .get_conf =     si570xxxusb_get_conf,

    .set_freq =     si570xxxusb_set_freq,
    .get_freq =     si570xxxusb_get_freq,
    .get_info =     si570xxxusb_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Funkamateur Sdr with Si570
 *
 * Same USB interface as AVR-USB, except different product string
 * and Oscillator correction may be stored on the device
 *
 *   http://www.funkamateur.de
 *   ( BX- 200 )
 */

const struct rig_caps fasdr_caps =
{
    RIG_MODEL(RIG_MODEL_FASDR),
    .model_name =       "FA-SDR",
    .mfg_name =     "Funkamateur",
    .version =      BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_BETA,
    .rig_type =     RIG_FLAG_TUNER | RIG_FLAG_TRANSMITTER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_USB,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      500,
    .retry =        0,

    .has_get_func =     SI570AVRUSB_FUNC,
    .has_set_func =     SI570AVRUSB_FUNC,
    .has_get_level =    SI570AVRUSB_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(SI570AVRUSB_LEVEL_ALL),
    .has_get_parm =     SI570AVRUSB_PARM_ALL,
    .has_set_parm =     RIG_PARM_SET(SI570AVRUSB_PARM_ALL),
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END },
    .attenuator =       { RIG_DBLST_END },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        /* probably higher upper range, depending on type (CMOS, LVDS, ..) */
        {kHz(1800), MHz(30), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 = {
        {kHz(1800), MHz(30), SI570AVRUSB_MODES, -1, -1, SI570AVRUSB_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END, },
    .tuning_steps = {
        {SI570AVRUSB_MODES, Hz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        RIG_FLT_END,
    },
    .cfgparams =        si570xxxusb_cfg_params,

    .rig_init =     fasdr_init,
    .rig_cleanup =      si570xxxusb_cleanup,
    .rig_open =     fasdr_open,
    .set_conf =     si570xxxusb_set_conf,
    .get_conf =     si570xxxusb_get_conf,

    .set_freq =     si570xxxusb_set_freq,
    .get_freq =     si570xxxusb_get_freq,
    .set_ptt =      si570xxxusb_set_ptt,
    .get_info =     si570xxxusb_get_info,

};

// libusb control transfer request types
#define REQUEST_TYPE_IN (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN)
#define REQUEST_TYPE_OUT (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT)

/*
 * some little endian (these devices are LE) to host endian converters
 */
static unsigned char *setLongWord(uint32_t value, unsigned char *bytes)
{
    bytes[0] = value & 0xff;
    bytes[1] = ((value & 0xff00) >> 8) & 0xff;
    bytes[2] = ((value & 0xff0000) >> 16) & 0xff;
    bytes[3] = ((value & 0xff000000) >> 24) & 0xff;
    return bytes;
}
static uint32_t getLongWord(unsigned char const *bytes)
{
    return bytes[0] + (bytes[1] << 8) + (bytes[2] << 16) + (bytes[3] << 24);
}

/*
 * AVR-USB model
 */
int si570avrusb_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct si570xxxusb_priv_data *priv;

    rig->state.priv = (struct si570xxxusb_priv_data *)calloc(sizeof(struct
                      si570xxxusb_priv_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->osc_freq = SI570_NOMINAL_XTALL_FREQ;
    /* QSD/QSE */
    priv->multiplier = 4;

    priv->i2c_addr = SI570_I2C_ADDR;
    /* disable BPF, because it may share PTT I/O line */
    priv->bpf = 0;

    rp->parm.usb.vid = USBDEV_SHARED_VID;
    rp->parm.usb.pid = USBDEV_SHARED_PID;

    /* no usb_set_configuration() and usb_claim_interface() */
    rp->parm.usb.iface = -1;
    rp->parm.usb.conf = 1;
    rp->parm.usb.alt = 0;   /* necessary ? */

    rp->parm.usb.vendor_name = VENDOR_NAME;
    rp->parm.usb.product = AVR_PRODUCT_NAME;

    return RIG_OK;
}

/*
 * Peaberry V1 model (AVR version with different vendor/product name)
 */
int si570peaberry1_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct si570xxxusb_priv_data *priv;

    rig->state.priv = (struct si570xxxusb_priv_data *)calloc(sizeof(struct
                      si570xxxusb_priv_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->osc_freq = SI570_NOMINAL_XTALL_FREQ;
    /* QSD/QSE */
    priv->multiplier = 4;

    priv->i2c_addr = SI570_I2C_ADDR;
    /* disable BPF, because it may share PTT I/O line */
    priv->bpf = 0;

    rp->parm.usb.vid = USBDEV_SHARED_VID;
    rp->parm.usb.pid = USBDEV_SHARED_PID;

    /* no usb_set_configuration() and usb_claim_interface() */
    rp->parm.usb.iface = -1;
    rp->parm.usb.conf = 1;
    rp->parm.usb.alt = 0;   /* necessary ? */

    rp->parm.usb.vendor_name = PEABERRY_VENDOR_NAME;
    rp->parm.usb.product = PEABERRY_PRODUCT_NAME;

    return RIG_OK;
}

/*
 * Peaberry V2 model (AVR version with different product name)
 */
int si570peaberry2_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct si570xxxusb_priv_data *priv;

    rig->state.priv = (struct si570xxxusb_priv_data *)calloc(sizeof(struct
                      si570xxxusb_priv_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->osc_freq = SI570_NOMINAL_XTALL_FREQ;
    /* QSD/QSE */
    priv->multiplier = 4;

    priv->i2c_addr = SI570_I2C_ADDR;
    /* disable BPF, because it may share PTT I/O line */
    priv->bpf = 0;

    rp->parm.usb.vid = USBDEV_SHARED_VID;
    rp->parm.usb.pid = USBDEV_SHARED_PID;

    /* no usb_set_configuration() and usb_claim_interface() */
    rp->parm.usb.iface = -1;
    rp->parm.usb.conf = 1;
    rp->parm.usb.alt = 0;   /* necessary ? */

    rp->parm.usb.vendor_name = VENDOR_NAME;
    rp->parm.usb.product = PEABERRY_PRODUCT_NAME;

    return RIG_OK;
}

/*
 * PIC-USB model
 */
int si570picusb_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct si570xxxusb_priv_data *priv;

    rig->state.priv = (struct si570xxxusb_priv_data *)calloc(sizeof(struct
                      si570xxxusb_priv_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->osc_freq = SI570_NOMINAL_XTALL_FREQ;
    /* QSD/QSE */
    priv->multiplier = 2;

    priv->i2c_addr = SI570_I2C_ADDR;
    /* enable BPF, because this is kit is receiver only */
    priv->bpf = 1;

    rp->parm.usb.vid = USBDEV_SHARED_VID;
    rp->parm.usb.pid = USBDEV_SHARED_PID;

    /* no usb_set_configuration() and usb_claim_interface() */
    rp->parm.usb.iface = -1;
    rp->parm.usb.conf = 1;
    rp->parm.usb.alt = 0;   /* necessary ? */

    rp->parm.usb.vendor_name = VENDOR_NAME;
    rp->parm.usb.product = PIC_PRODUCT_NAME;

    return RIG_OK;
}
/*
 * FA-SDR
 */

int fasdr_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct si570xxxusb_priv_data *priv;

    rig->state.priv = (struct si570xxxusb_priv_data *)calloc(sizeof(struct
                      si570xxxusb_priv_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->osc_freq = SI570_NOMINAL_XTALL_FREQ;
    /* QSD/QSE */
    priv->multiplier = 4;

    priv->i2c_addr = SI570_I2C_ADDR;
    /* disable BPF, because it may share PTT I/O line */
    priv->bpf = 0;

    rp->parm.usb.vid = USBDEV_SHARED_VID;
    rp->parm.usb.pid = USBDEV_SHARED_PID;

    /* no usb_set_configuration() and usb_claim_interface() */
    rp->parm.usb.iface = -1;
    rp->parm.usb.conf = 1;
    rp->parm.usb.alt = 0;   /* necessary ? */

    rp->parm.usb.vendor_name = VENDOR_NAME;
    rp->parm.usb.product = AVR_PRODUCT_NAME;

    return RIG_OK;
}


int fasdr_open(RIG *rig)
{
    struct si570xxxusb_priv_data *priv = (struct si570xxxusb_priv_data *)
                                         rig->state.priv;
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret, i;
    double f;
    unsigned char buffer[4];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);


    ret = libusb_control_transfer(udh, REQUEST_TYPE_IN,
                                  REQUEST_READ_VERSION, 0x0E00, 0,
                                  buffer, 2, rig->state.rigport.timeout);

    if (ret != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    priv->version = buffer[0] + (buffer[1] <<
                                 8); // Unsure how to get firmware version

    ret = libusb_control_transfer(udh,
                                  REQUEST_TYPE_IN,
                                  REQUEST_READ_EEPROM, F_CAL_STATUS, 0, buffer, 1,
                                  rig->state.rigport.timeout);

    if (ret != 1)
    {
        return -RIG_EIO;
    }


    rig_debug(RIG_DEBUG_VERBOSE, "%s: calibration byte %x", __func__, buffer[0]);

//        ret = libusb_control_transfer(udh,
//                REQUEST_TYPE_IN,
//                REQUEST_READ_XTALL, 0, 0, (unsigned char *) &iFreq, sizeof(iFreq),
//                rig->state.rigport.timeout);
    if (buffer[0] == 0xFF)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Device not calibrated", __func__);
        return RIG_OK;
    }

    for (i = 0; i < 4; i++)
    {
        ret = libusb_control_transfer(udh,
                                      REQUEST_TYPE_IN,
                                      REQUEST_READ_EEPROM, F_CRYST + i, 0, &buffer[i], 1,
                                      rig->state.rigport.timeout);

        if (ret != 1)
        {
            return -RIG_EIO;
        }
    }

    priv->osc_freq =  buffer[0];
    f = buffer[1];
    priv->osc_freq += f / 256.;
    f = buffer[2];
    priv->osc_freq += f / (256.*256.);
    f = buffer[3];
    priv->osc_freq += f / (256.*256.*256.);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: using Xtall at %.3f MHz\n",
              __func__, priv->osc_freq);

    return RIG_OK;
}


int si570xxxusb_cleanup(RIG *rig)
{
    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

int si570xxxusb_set_conf(RIG *rig, token_t token, const char *val)
{
    struct si570xxxusb_priv_data *priv;
    freq_t freq;
    double multiplier;
    unsigned int i2c_addr;

    priv = (struct si570xxxusb_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        if (sscanf(val, "%"SCNfreq, &freq) != 1)
        {
            return -RIG_EINVAL;
        }

        priv->osc_freq = (double)freq / 1e6;
        break;

    case TOK_MULTIPLIER:
        if (sscanf(val, "%lf", &multiplier) != 1)
        {
            return -RIG_EINVAL;
        }

        if (multiplier == 0.)
        {
            return -RIG_EINVAL;
        }

        priv->multiplier = multiplier;
        break;

    case TOK_I2C_ADDR:
        if (sscanf(val, "%x", &i2c_addr) != 1)
        {
            return -RIG_EINVAL;
        }

        if (i2c_addr >= (1 << 9))
        {
            return -RIG_EINVAL;
        }

        priv->i2c_addr = i2c_addr;
        break;

    case TOK_BPF:
        if (sscanf(val, "%d", &priv->bpf) != 1)
        {
            return -RIG_EINVAL;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int si570xxxusb_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct si570xxxusb_priv_data *priv;
    priv = (struct si570xxxusb_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        SNPRINTF(val, val_len, "%"PRIfreq, (freq_t)(priv->osc_freq * 1e6));
        break;

    case TOK_MULTIPLIER:
        SNPRINTF(val, val_len, "%f", priv->multiplier);
        break;

    case TOK_I2C_ADDR:
        SNPRINTF(val, val_len, "%x", priv->i2c_addr);
        break;

    case TOK_BPF:
        SNPRINTF(val, val_len, "%d", priv->bpf);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int si570xxxusb_get_conf(RIG *rig, token_t token, char *val)
{
    return si570xxxusb_get_conf2(rig, token, val, 128);
}


static int setBPF(RIG *rig, int enable)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    /* allocate enough space for up to 16 filters */
    unsigned short FilterCrossOver[16];
    int nBytes;

    // Does FilterCrossOver needs endianness ordering ?

    // first find out how may cross over points there are for the 1st bank, use 255 for index
    nBytes = libusb_control_transfer(udh, REQUEST_TYPE_IN,
                                     REQUEST_FILTERS, 0, 255,
                                     (unsigned char *) FilterCrossOver, sizeof(FilterCrossOver),
                                     rig->state.rigport.timeout);

    if (nBytes < 0)
    {
        return -RIG_EIO;
    }

    if (nBytes > 2)
    {
        int i;
        int retval = libusb_control_transfer(udh, REQUEST_TYPE_IN,
                                             REQUEST_FILTERS, enable, (nBytes / 2) - 1,
                                             (unsigned char *) FilterCrossOver, sizeof(FilterCrossOver),
                                             rig->state.rigport.timeout);

        if (retval < 2)
        {
            return -RIG_EIO;
        }

        nBytes = retval;

        rig_debug(RIG_DEBUG_TRACE, "%s: Filter Bank 1:\n", __func__);

        for (i = 0; i < (nBytes / 2) - 1; i++)
        {
            rig_debug(RIG_DEBUG_TRACE, "  CrossOver[%d] = %f\n",
                      i, (double) FilterCrossOver[i] / (1UL << 5));
        }

        rig_debug(RIG_DEBUG_TRACE, "  BPF Enabled: %d\n",
                  FilterCrossOver[(nBytes / 2) - 1]);
    }

    return RIG_OK;
}

int si570xxxusb_open(RIG *rig)
{
    struct si570xxxusb_priv_data *priv = (struct si570xxxusb_priv_data *)
                                         rig->state.priv;
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    unsigned char buffer[4];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /*
     * Determine firmware
     */

    ret = libusb_control_transfer(udh, REQUEST_TYPE_IN,
                                  REQUEST_READ_VERSION, 0x0E00, 0,
                                  buffer, 2, rig->state.rigport.timeout);

    if (ret != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    priv->version = buffer[0] + (buffer[1] << 8);

    if (priv->version  >= 0x0F00 || rig->caps->rig_model == RIG_MODEL_SI570PICUSB)
    {
        unsigned int iFreq;

        rig_debug(RIG_DEBUG_VERBOSE, "%s: detected PE0FKO-like firmware\n", __func__);

        ret = libusb_control_transfer(udh,
                                      REQUEST_TYPE_IN,
                                      REQUEST_READ_XTALL, 0, 0, buffer, sizeof(buffer),
                                      rig->state.rigport.timeout);

        if (ret != 4)
        {
            return -RIG_EIO;
        }

        iFreq = getLongWord(buffer);
        priv->osc_freq = (double)iFreq / (1UL << 24);

        if (priv->bpf)
        {
            ret = setBPF(rig, 1);

            if (ret != RIG_OK)
            {
                return ret;
            }
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: using Xtall at %.3f MHz\n",
              __func__, priv->osc_freq);

    return RIG_OK;
}


/* Rem: not reentrant */
const char *si570xxxusb_get_info(RIG *rig)
{
    static char buf[64];
    libusb_device_handle *udh = rig->state.rigport.handle;
    struct libusb_device_descriptor desc;
    int ret;
    unsigned char buffer[2];

    ret = libusb_control_transfer(udh, REQUEST_TYPE_IN,
                                  REQUEST_READ_VERSION, 0x0E00, 0,
                                  buffer, sizeof(buffer), rig->state.rigport.timeout);

    if (ret != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return NULL;
    }

    /* always succeeds since libusb-1.0.16 */
    libusb_get_device_descriptor(libusb_get_device(udh), &desc);

    SNPRINTF(buf, sizeof(buf), "USB dev %04d, version: %d.%d", desc.bcdDevice,
             buffer[1],
             buffer[0]);

    return buf;
}

static const int HS_DIV_MAP[] = {4, 5, 6, 7, -1, 9, -1, 11};

static int calcDividers(RIG *rig, double f, struct solution *solution)
{
    struct si570xxxusb_priv_data *priv = (struct si570xxxusb_priv_data *)
                                         rig->state.priv;
    struct solution sols[8];
    int i;
    int imin;
    double fmin;
    double y;

    // Count down through the dividers
    for (i = 7; i >= 0; i--)
    {

        if (HS_DIV_MAP[i] > 0)
        {
            sols[i].HS_DIV = i;
            y = (SI570_DCO_HIGH + SI570_DCO_LOW) / (2 * f);
            y = y / HS_DIV_MAP[i];

            if (y < 1.5)
            {
                y = 1.0;
            }
            else
            {
                y = 2 * round(y / 2.0);
            }

            if (y > 128)
            {
                y = 128;
            }

            sols[i].N1 = trunc(y) - 1;
            sols[i].f0 = f * y * HS_DIV_MAP[i];
        }
        else
        {
            sols[i].f0 = 10000000000000000.0;
        }
    }

    imin = -1;
    fmin = 10000000000000000.0;

    for (i = 0; i < 8; i++)
    {
        if ((sols[i].f0 >= SI570_DCO_LOW) && (sols[i].f0 <= SI570_DCO_HIGH))
        {
            if (sols[i].f0 < fmin)
            {
                fmin = sols[i].f0;
                imin = i;
            }
        }
    }

    if (imin >= 0)
    {
        solution->HS_DIV = sols[imin].HS_DIV;
        solution->N1 = sols[imin].N1;
        solution->f0 = sols[imin].f0;
        solution->RFREQ = sols[imin].f0 / priv->osc_freq;

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: solution: HS_DIV = %d, N1 = %d, f0 = %f, RFREQ = %f\n",
                  __func__, solution->HS_DIV, solution->N1, solution->f0, solution->RFREQ);

        return 1;
    }
    else
    {
        solution->HS_DIV = 0;
        solution->N1 = 0;
        solution->f0 = 0;
        solution->RFREQ = 0;
        rig_debug(RIG_DEBUG_TRACE, "%s: No solution\n", __func__);
        return 0;
    }
}

int si570xxxusb_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct si570xxxusb_priv_data *priv = (struct si570xxxusb_priv_data *)
                                         rig->state.priv;
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    unsigned char buffer[6];
    int request = REQUEST_SET_FREQ;
    int value = 0x700 + priv->i2c_addr;
    int index = 0;
    double f;
    struct solution theSolution;
    int RFREQ_int;
    int RFREQ_frac;
    unsigned char fracBuffer[4];
    unsigned char intBuffer[4];

    if (priv->version >= 0x0f00 || rig->caps->rig_model == RIG_MODEL_SI570PICUSB ||
            rig->caps->rig_model == RIG_MODEL_SI570PEABERRY1
            || rig->caps->rig_model == RIG_MODEL_SI570PEABERRY2)
    {
        return si570xxxusb_set_freq_by_value(rig, vfo, freq);
    }

    f = (freq * priv->multiplier) / 1e6;

    calcDividers(rig, f, &theSolution);

    RFREQ_int = trunc(theSolution.RFREQ);
    RFREQ_frac = round((theSolution.RFREQ - RFREQ_int) * 268435456);
    setLongWord(RFREQ_int, intBuffer);
    setLongWord(RFREQ_frac, fracBuffer);

    buffer[5] = fracBuffer[0];
    buffer[4] = fracBuffer[1];
    buffer[3] = fracBuffer[2];
    buffer[2] = fracBuffer[3];
    buffer[2] = buffer[2] | ((intBuffer[0] & 0xf) << 4);
    buffer[1] = RFREQ_int / 16;
    buffer[1] = buffer[1] + ((theSolution.N1 & 3) << 6);
    buffer[0] = theSolution.N1 / 4;
    buffer[0] = buffer[0] + (theSolution.HS_DIV << 5);

    ret = libusb_control_transfer(udh, REQUEST_TYPE_OUT,
                                  request, value, index, buffer, sizeof(buffer), rig->state.rigport.timeout);

    rig_debug(RIG_DEBUG_TRACE,
              "%s: Freq=%.6f MHz, Real=%.6f MHz, buf=%02x%02x%02x%02x%02x%02x\n",
              __func__, freq / 1e6, f,
              buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);


    if (!ret)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Result buf=%02x%02x\n",
              __func__, buffer[0], buffer[1]);

    return RIG_OK;
}

int si570xxxusb_set_freq_by_value(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct si570xxxusb_priv_data *priv = (struct si570xxxusb_priv_data *)
                                         rig->state.priv;
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;

    unsigned char buffer[4];
    int request = REQUEST_SET_FREQ_BY_VALUE;
    int value = 0x700 + priv->i2c_addr;
    int index = 0;
    double f;

    f = (freq * priv->multiplier) / 1e6;

    setLongWord(round(f * 2097152.0), buffer);

    rig_debug(RIG_DEBUG_TRACE,
              "%s: Freq=%.6f MHz, Real=%.6f MHz, buf=%02x%02x%02x%02x\n",
              __func__, freq / 1e6, f,
              buffer[0], buffer[1], buffer[2], buffer[3]);

    ret = libusb_control_transfer(udh, REQUEST_TYPE_OUT,
                                  request, value, index, buffer, sizeof(buffer), rig->state.rigport.timeout);

    if (!ret)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Result buf=%02x%02x\n",
              __func__, buffer[0], buffer[1]);

    return RIG_OK;
}

static double calculateFrequency(RIG *rig, const unsigned char *buffer)
{
    struct si570xxxusb_priv_data *priv = (struct si570xxxusb_priv_data *)
                                         rig->state.priv;

    int RFREQ_int = ((buffer[2] & 0xf0) >> 4) + ((buffer[1] & 0x3f) * 16);
    int RFREQ_frac = (256 * 256 * 256 * (buffer[2] & 0xf)) +
                     (256 * 256 * buffer[3]) + (256 * buffer[4]) + (buffer[5]);
    double RFREQ = RFREQ_int + (RFREQ_frac / 268435456.0);
    int N1 = ((buffer[1] & 0xc0) >> 6) + ((buffer[0] & 0x1f) * 4);
    int HS_DIV = (buffer[0] & 0xE0) >> 5;
    double fout = priv->osc_freq * RFREQ / ((N1 + 1) * HS_DIV_MAP[HS_DIV]);

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: Registers 7..13: %02x%02x%02x%02x%02x%02x\n",
              __func__,
              buffer[0],
              buffer[1],
              buffer[2],
              buffer[3],
              buffer[4],
              buffer[5]);

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: RFREQ = %f, N1 = %d, HS_DIV = %d, nHS_DIV = %d, fout = %f\n",
              __func__,
              RFREQ, N1, HS_DIV, HS_DIV_MAP[HS_DIV], fout);

    return fout;
}

int si570xxxusb_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct si570xxxusb_priv_data *priv = (struct si570xxxusb_priv_data *)
                                         rig->state.priv;
    libusb_device_handle *udh = rig->state.rigport.handle;
    unsigned char buffer[6];
    int ret;

    if (priv->version >= 0x0f00 || rig->caps->rig_model == RIG_MODEL_SI570PICUSB ||
            rig->caps->rig_model == RIG_MODEL_SI570PEABERRY1
            || rig->caps->rig_model == RIG_MODEL_SI570PEABERRY2)
    {
        return si570xxxusb_get_freq_by_value(rig, vfo, freq);
    }

    ret = libusb_control_transfer(udh, REQUEST_TYPE_IN,
                                  REQUEST_READ_REGISTERS, priv->i2c_addr, 0,
                                  buffer, sizeof(buffer), rig->state.rigport.timeout);

    if (ret <= 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    *freq = (calculateFrequency(rig, buffer) / priv->multiplier) * 1e6;

    return RIG_OK;
}

int si570xxxusb_get_freq_by_value(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct si570xxxusb_priv_data *priv = (struct si570xxxusb_priv_data *)
                                         rig->state.priv;
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    unsigned char buffer[4];
    uint32_t iFreq;

    ret = libusb_control_transfer(udh, REQUEST_TYPE_IN,
                                  REQUEST_READ_FREQUENCY, 0, 0,
                                  buffer, sizeof(buffer), rig->state.rigport.timeout);

    if (ret != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    iFreq = getLongWord(buffer);
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: Freq raw: %02x%02x%02x%02x endian converted: %u\n",
              __func__, buffer[0], buffer[1], buffer[2], buffer[3], iFreq);
    *freq = (((double)iFreq / (1UL << 21)) / priv->multiplier) * 1e6;

    return RIG_OK;
}


int si570xxxusb_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    unsigned char buffer[3];

    rig_debug(RIG_DEBUG_TRACE, "%s called: %d\n", __func__, ptt);

    buffer[0] = 0;
    buffer[1] = 0;
    buffer[2] = 0;

    ret = libusb_control_transfer(udh, REQUEST_TYPE_IN,
                                  REQUEST_SET_PTT, (ptt == RIG_PTT_ON) ? 1 : 0, 0,
                                  buffer, sizeof(buffer), rig->state.rigport.timeout);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    return RIG_OK;
}

#endif  /* defined(HAVE_LIBUSB) && defined(HAVE_LIBUSB_H) */
