/*
 *  Hamlib KIT backend - FUNcube Dongle USB tuner description
 *  Copyright (c) 2009-2011 by Stephane Fillod
 *
 *  Derived from usbsoftrock-0.5:
 *  Copyright (C) 2009 Andrew Nilsson (andrew.nilsson@gmail.com)
 *
 *  Author: Stefano Speretta, Innovative Solutions In Space BV
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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include "hamlib/rig.h"
#include "token.h"
#include "misc.h"

#include "kit.h"

#define BACKEND_VER "20210830"

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

#include "funcube.h"

static int funcube_hid_cmd(RIG *rig, unsigned char *au8BufOut,
                           unsigned char *au8BufIn, int inputSize);

static int funcube_init(RIG *rig);
static int funcubeplus_init(RIG *rig);
static int funcube_cleanup(RIG *rig);
static int funcube_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int funcube_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int funcube_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int funcube_get_level(RIG *rig, vfo_t vfo, setting_t level,
                             value_t *val);

static int funcubepro_set_level(RIG *rig, vfo_t vfo, setting_t level,
                                value_t val);
static int funcubepro_get_level(RIG *rig, vfo_t vfo, setting_t level,
                                value_t *val);

static const char *funcube_get_info(RIG *rig);
static int funcube_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width);

static const struct confparams funcube_cfg_params[] =
{
    { RIG_CONF_END, NULL, }
};

// functions used set / read frequency, working on FUNcube version 0 and 1
int set_freq_v0(libusb_device_handle *udh, unsigned int f, int timeout);
int set_freq_v1(libusb_device_handle *udh, unsigned int f, int timeout);

/*
 * Common data struct
 */
struct funcube_priv_data
{

    freq_t freq;    /* Hz */
};

/*
 * FUNcube Dongle description
 *
 * Based on Jan Axelson HID examples
 *   http://www.lvr.com/
 *
 */

const struct rig_caps funcube_caps =
{
    RIG_MODEL(RIG_MODEL_FUNCUBEDONGLE),
    .model_name =       "FUNcube Dongle",
    .mfg_name =     "AMSAT-UK",
    .version =      BACKEND_VER ".1",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TUNER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_USB,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      1000,
    .retry =        0,

    .has_get_func =     RIG_FUNC_NONE,
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    RIG_LEVEL_ATT | RIG_LEVEL_STRENGTH | RIG_LEVEL_PREAMP,
    .has_set_level =    RIG_LEVEL_ATT | RIG_LEVEL_PREAMP,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { 5, 10, 15, 20, 25, 30, RIG_DBLST_END, },
    .attenuator =       { 2, 5, RIG_DBLST_END, },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        {MHz(50), MHz(2500), RIG_MODE_USB, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tuning_steps = {
        {RIG_MODE_USB, kHz(1)},
        RIG_TS_END,
    },
    .filters = {
        {RIG_MODE_USB, kHz(192)},
        RIG_FLT_END,
    },
    .cfgparams =        funcube_cfg_params,

    .rig_init =     funcube_init,
    .rig_cleanup =      funcube_cleanup,
    .set_freq =     funcube_set_freq,
    .get_freq =     funcube_get_freq,
    .get_level =        funcube_get_level,
    .set_level =        funcube_set_level,
    .get_info =     funcube_get_info,
    .get_mode =     funcube_get_mode,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


const struct rig_caps funcubeplus_caps =
{
    RIG_MODEL(RIG_MODEL_FUNCUBEDONGLEPLUS),
    .model_name =       "FUNcube Dongle Pro+",
    .mfg_name =     "AMSAT-UK",
    .version =      BACKEND_VER ".1",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TUNER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_USB,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      1000,
    .retry =        0,

    .has_get_func =     RIG_FUNC_NONE,
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    RIG_LEVEL_PREAMP | RIG_LEVEL_RF,
    // RIG_LEVEL_PREAMP: 10dB=LNAon MixGainOff.   20dB=LNAoff, MixGainOn.  30dB=LNAOn, MixGainOn
    // RIG_LEVEL_RF 0..1 : IF gain 0 .. 59 dB
    .has_set_level =    RIG_LEVEL_PREAMP | RIG_LEVEL_RF,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =    { 10, 20, 30, RIG_DBLST_END, },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list = { RIG_CHAN_END, },

    .rx_range_list1 = {
        {kHz(150), MHz(1900), RIG_MODE_IQ, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tuning_steps = {
        {RIG_MODE_IQ, kHz(1)},
        RIG_TS_END,
    },
    .filters = {
        {RIG_MODE_IQ, kHz(192)},
        RIG_FLT_END,
    },
    .cfgparams =        funcube_cfg_params,

    .rig_init =     funcubeplus_init,
    .rig_cleanup =      funcube_cleanup,
    .set_freq =     funcube_set_freq,
    .get_freq =     funcube_get_freq,
    .get_level =        funcubepro_get_level,
    .set_level =        funcubepro_set_level,
    .get_info =     funcube_get_info,
    .get_mode =     funcube_get_mode,
};

int funcube_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct funcube_priv_data *priv;

    rig->state.priv = (struct funcube_priv_data *)calloc(sizeof(
                          struct funcube_priv_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->freq = 0;

    rp->parm.usb.vid = VID;
    rp->parm.usb.pid = PID;
    rp->parm.usb.conf = FUNCUBE_CONFIGURATION;
    rp->parm.usb.iface = FUNCUBE_INTERFACE;
    rp->parm.usb.alt = FUNCUBE_ALTERNATIVE_SETTING;

    rp->parm.usb.vendor_name = VENDOR_NAME;
    rp->parm.usb.product = PRODUCT_NAME;

    return RIG_OK;
}

int funcubeplus_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct funcube_priv_data *priv;

    rig->state.priv = (struct funcube_priv_data *)calloc(sizeof(
                          struct funcube_priv_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->freq = 0;

    rp->parm.usb.vid = VID;
    rp->parm.usb.pid = PIDPLUS;
    rp->parm.usb.conf = FUNCUBE_CONFIGURATION;
    rp->parm.usb.iface = FUNCUBE_INTERFACE;
    rp->parm.usb.alt = FUNCUBE_ALTERNATIVE_SETTING;

    rp->parm.usb.vendor_name = VENDOR_NAME;
    rp->parm.usb.product = PRODUCT_NAMEPLUS;

    return RIG_OK;
}

int funcube_cleanup(RIG *rig)
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

/* Rem: not reentrant */
const char *funcube_get_info(RIG *rig)
{
    static char buf[64];
    libusb_device_handle *udh = rig->state.rigport.handle;
    struct libusb_device_descriptor desc;

    /* always succeeds since libusb-1.0.16 */
    libusb_get_device_descriptor(libusb_get_device(udh), &desc);

    SNPRINTF(buf, sizeof(buf), "Dev %04d", desc.bcdDevice);

    return buf;
}

int set_freq_v0(libusb_device_handle *udh, unsigned int f, int timeout)
{
    int ret;
    int actual_length;

    unsigned char au8BufOut[64]; // endpoint size
    unsigned char au8BufIn[64];  // endpoint size

    // frequency is in Hz, while the dongle expects it in kHz
    f = f / 1000;

    au8BufOut[0] = REQUEST_SET_FREQ; // Command to Set Frequency on dongle
    au8BufOut[1] = (unsigned char)f;
    au8BufOut[2] = (unsigned char)(f >> 8);
    au8BufOut[3] = (unsigned char)(f >> 16);

    rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
              __func__, au8BufOut[0] & 0xFF, au8BufOut[1] & 0xFF, au8BufOut[2] & 0xFF,
              au8BufOut[3] & 0xFF);

    ret = libusb_interrupt_transfer(udh, OUTPUT_ENDPOINT, au8BufOut,
                                    sizeof(au8BufOut), &actual_length, timeout);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret, libusb_error_name(ret));
        return -RIG_EIO;
    }

    ret = libusb_interrupt_transfer(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn),
                                    &actual_length, timeout);

    if (ret < 0 || actual_length != sizeof(au8BufIn))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret, libusb_error_name(ret));
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x\n",
              __func__, au8BufIn[0] & 0xFF, au8BufIn[1] & 0xFF);

    if (au8BufIn[1] != FUNCUBE_SUCCESS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: REQUEST_SET_FREQ not supported\n",
                  __func__);
        return -RIG_EIO;
    }

    return RIG_OK;
}

int set_freq_v1(libusb_device_handle *udh, unsigned int f, int timeout)
{
    int ret;
    int actual_length;

    unsigned char au8BufOut[64]; // endpoint size
    unsigned char au8BufIn[64];  // endpoint size

    au8BufOut[0] = REQUEST_SET_FREQ_HZ; // Command to Set Frequency in Hz on dongle
    au8BufOut[1] = (unsigned char)f;
    au8BufOut[2] = (unsigned char)(f >> 8);
    au8BufOut[3] = (unsigned char)(f >> 16);
    au8BufOut[4] = (unsigned char)(f >> 24);

    rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x%02x\n",
              __func__, au8BufOut[0] & 0xFF, au8BufOut[1] & 0xFF, au8BufOut[2] & 0xFF,
              au8BufOut[3] & 0xFF,
              au8BufOut[4] & 0xFF);

    ret = libusb_interrupt_transfer(udh, OUTPUT_ENDPOINT, au8BufOut,
                                    sizeof(au8BufOut), &actual_length, timeout);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    ret = libusb_interrupt_transfer(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn),
                                    &actual_length, timeout);

    if (ret < 0 || actual_length != sizeof(au8BufIn))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x%02x%02x%02x%02x\n",
              __func__, au8BufIn[0] & 0xFF, au8BufIn[1] & 0xFF, au8BufIn[2] & 0xFF,
              au8BufIn[3] & 0xFF,
              au8BufIn[4] & 0xFF, au8BufIn[5] & 0xFF);

    if (au8BufIn[1] != FUNCUBE_SUCCESS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: REQUEST_SET_FREQ_HZ not supported\n",
                  __func__);
        return -RIG_EIO;
    }

    return RIG_OK;
}

int funcube_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct funcube_priv_data *priv = (struct funcube_priv_data *)rig->state.priv;
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;

    if ((ret = set_freq_v1(udh, freq, rig->state.rigport.timeout)) != RIG_OK)
    {
        if ((ret = set_freq_v0(udh, freq, rig->state.rigport.timeout)) == RIG_OK)
        {
            priv->freq = freq;
        }
    }
    else
    {
        priv->freq = freq;
    }

    return ret;
}

int get_freq_v0(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct funcube_priv_data *priv = (struct funcube_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE,
              "%s: frequency is not read from the device, the value shown is the last successfully set.\n",
              __func__);
    *freq = priv->freq;

    return RIG_OK;
}

int get_freq_v1(RIG *rig, vfo_t vfo, freq_t *freq)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    unsigned int f;
    int actual_length;

    unsigned char au8BufOut[64] = "\0\0\0\0"; // endpoint size
    unsigned char au8BufIn[64] = "\0\0\0\0";  // endpoint size

    au8BufOut[0] = REQUEST_GET_FREQ_HZ; // Command to Set Frequency on dongle

    rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
              __func__, au8BufOut[0] & 0xFF, au8BufOut[1] & 0xFF, au8BufOut[2] & 0xFF,
              au8BufOut[3] & 0xFF);

    ret = libusb_interrupt_transfer(udh, OUTPUT_ENDPOINT, au8BufOut,
                                    sizeof(au8BufOut), &actual_length, rig->state.rigport.timeout);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
    }

    ret = libusb_interrupt_transfer(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn),
                                    &actual_length, rig->state.rigport.timeout);

    if (ret < 0 || actual_length != sizeof(au8BufIn))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x%02x%02x%02x%02x\n",
              __func__, au8BufIn[0] & 0xFF, au8BufIn[1] & 0xFF, au8BufIn[2] & 0xFF,
              au8BufIn[3] & 0xFF,
              au8BufIn[4] & 0xFF, au8BufIn[5] & 0xFF);

    if (au8BufIn[1] != FUNCUBE_SUCCESS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: REQUEST_GET_FREQ_HZ not supported\n",
                  __func__);
        return -RIG_EIO;
    }

    f = (au8BufIn[2] & 0xFF) | ((au8BufIn[3] & 0xFF) << 8) |
        ((au8BufIn[4] & 0xFF) << 16) | ((au8BufIn[5] & 0xFF) << 24),

        *freq = f;

    return RIG_OK;
}

int funcube_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int ret;

    if ((ret = get_freq_v1(rig, vfo, freq)) != RIG_OK)
    {
        ret = get_freq_v0(rig, vfo, freq);
    }

    return ret;
}

int funcube_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    int actual_length;
    unsigned char au8BufOut[64] = "\0\0\0\0"; // endpoint size
    unsigned char au8BufIn[64] = "\0\0\0\0";  // endpoint size

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        au8BufOut[0] = REQUEST_SET_LNA_GAIN; // Command to Set LNA gain

        switch (val.i)
        {
        case 5:
            au8BufOut[1] = 6;
            break;

        case 10:
            au8BufOut[1] = 8;
            break;

        case 15:
            au8BufOut[1] = 10;
            break;

        case 20:
            au8BufOut[1] = 12;
            break;

        case 25:
            au8BufOut[1] = 13;
            break;

        case 30:
            au8BufOut[1] = 14;
            break;

        default:
            au8BufOut[1] = 4;
        }

        break;

    case RIG_LEVEL_ATT:
        au8BufOut[0] = REQUEST_SET_LNA_GAIN; // Command to Set LNA gain

        switch (val.i)
        {
        case 2:
            au8BufOut[1] = 1;
            break;

        case 5:
            au8BufOut[1] = 0;
            break;

        default:
            au8BufOut[1] = 4;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
              __func__, au8BufOut[0] & 0xFF, au8BufOut[1] & 0xFF, au8BufOut[2] & 0xFF,
              au8BufOut[3] & 0xFF);

    ret = libusb_interrupt_transfer(udh, OUTPUT_ENDPOINT, au8BufOut,
                                    sizeof(au8BufOut), &actual_length, rig->state.rigport.timeout);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
    }

    ret = libusb_interrupt_transfer(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn),
                                    &actual_length, rig->state.rigport.timeout);

    if (ret < 0 || actual_length != sizeof(au8BufIn))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x\n",
              __func__, au8BufIn[0] & 0xFF, au8BufIn[1] & 0xFF);

    if (au8BufIn[1] != FUNCUBE_SUCCESS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: REQUEST_SET_LEVEL not supported\n",
                  __func__);
        return -RIG_EIO;
    }

    return RIG_OK;
}

int funcube_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    int actual_length;
    unsigned char au8BufOut[64] = "\0\0\0\0"; // endpoint size
    unsigned char au8BufIn[64] = "\0\0\0\0";  // endpoint size

    switch (level)
    {
    case RIG_LEVEL_ATT:
    case RIG_LEVEL_PREAMP:
        au8BufOut[0] = REQUEST_GET_LNA_GAIN; // Command to Get LNA / ATT gain
        break;

    case RIG_LEVEL_STRENGTH:
        au8BufOut[0] = REQUEST_GET_RSSI;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
              __func__, au8BufOut[0] & 0xFF, au8BufOut[1] & 0xFF, au8BufOut[2] & 0xFF,
              au8BufOut[3] & 0xFF);

    ret = libusb_interrupt_transfer(udh, OUTPUT_ENDPOINT, au8BufOut,
                                    sizeof(au8BufOut), &actual_length, rig->state.rigport.timeout);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
    }

    ret = libusb_interrupt_transfer(udh, INPUT_ENDPOINT, au8BufIn, sizeof(au8BufIn),
                                    &actual_length, rig->state.rigport.timeout);

    if (ret < 0 || actual_length != sizeof(au8BufIn))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x%02x\n",
              __func__, au8BufIn[0] & 0xFF, au8BufIn[1] & 0xFF, au8BufIn[2] & 0xFF);

    if (au8BufIn[1] != FUNCUBE_SUCCESS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: REQUEST_GET_LEVEL_x not supported\n",
                  __func__);
        return -RIG_EIO;
    }

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        switch (au8BufIn[2])
        {
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
        switch (au8BufIn[2])
        {
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
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int funcube_hid_cmd(RIG *rig, unsigned char *au8BufOut, unsigned char *au8BufIn,
                    int inputSize)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    int actual_length;
    rig_debug(RIG_DEBUG_TRACE, "%s: HID packet set to %02x%02x%02x%02x\n",
              __func__, au8BufOut[0] & 0xFF, au8BufOut[1] & 0xFF, au8BufOut[2] & 0xFF,
              au8BufOut[3] & 0xFF);

    ret = libusb_interrupt_transfer(udh, OUTPUT_ENDPOINT, au8BufOut,
                                    sizeof(au8BufOut), &actual_length, rig->state.rigport.timeout);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
    }

    ret = libusb_interrupt_transfer(udh, INPUT_ENDPOINT, au8BufIn, inputSize,
                                    &actual_length, rig->state.rigport.timeout);

    if (ret < 0 || actual_length != inputSize)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_interrupt_transfer failed (%d): %s\n",
                  __func__, ret,
                  libusb_error_name(ret));
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Answer buf=%02x%02x\n",
              __func__, au8BufIn[0] & 0xFF, au8BufIn[1] & 0xFF);

    if (au8BufIn[1] != FUNCUBE_SUCCESS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to perform FUNCube HID command %d.\n",
                  __func__, au8BufOut[0]);
        return -RIG_EIO;
    }

    return RIG_OK;
}

int funcubepro_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    ENTERFUNC;
    int ret;
    unsigned char au8BufOut[64] = { 0 }; // endpoint size
    unsigned char au8BufIn[64] = { 0 };  // endpoint size

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        rig_debug(RIG_DEBUG_TRACE, "%s: Setting PREAMP state to %d.\n",
                  __func__, val.i);
        au8BufOut[0] = REQUEST_SET_LNA_GAIN; // Command to set LNA gain

        if (val.i == 10 || val.i == 30)
        {
            au8BufOut[1] = 1;
        }
        else
        {
            au8BufOut[1] = 0;
        }

        ret = funcube_hid_cmd(rig, au8BufOut, au8BufIn, sizeof(au8BufIn));

        if (ret < 0)
        {
            return ret;
        }

        au8BufOut[0] = REQUEST_SET_MIXER_GAIN; // Set mixer gain

        if (val.i == 20 || val.i == 30)
        {
            au8BufOut[1] = 1;
        }
        else
        {
            au8BufOut[1] = 0;
        }

        return funcube_hid_cmd(rig, au8BufOut, au8BufIn, sizeof(au8BufIn));

    case RIG_LEVEL_RF:
        au8BufOut[0] = REQUEST_SET_IF_GAIN; // Command to set IF gain
        au8BufOut[1] = (int)(val.f * 100) ;

        if (au8BufOut[1] > 59)
        {
            au8BufOut[1] = 59;
        }

        return funcube_hid_cmd(rig, au8BufOut, au8BufIn, sizeof(au8BufIn));

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }
}

int funcubepro_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    ENTERFUNC;
    int ret;
    int gain_state;
    unsigned char au8BufOut[64] = { 0 }; // endpoint size
    unsigned char au8BufIn[64] = { 0 };  // endpoint size

    switch (level)
    {

    case RIG_LEVEL_PREAMP:
        au8BufOut[0] = REQUEST_GET_MIXER_GAIN;   // Command to get mixer gain enabled
        ret = funcube_hid_cmd(rig, au8BufOut, au8BufIn, sizeof(au8BufIn));

        if (ret < 0)
        {
            return ret;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: Mixer gain state returned %d.\n",
                  __func__, au8BufIn[2] & 0xFF);

        gain_state = au8BufIn[2] & 0x1;

        au8BufOut[0] = REQUEST_GET_LNA_GAIN;   // Command to get LNA gain enabled

        ret = funcube_hid_cmd(rig, au8BufOut, au8BufIn, sizeof(au8BufIn));

        if (ret < 0)
        {
            return ret;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: LNA gain state returned %d.\n",
                  __func__, au8BufIn[2] & 0xFF);

        //Mixer gain is 20dB 0x2
        gain_state *= 2;

        //Add the LNA gain if present (10dB) 0x1
        gain_state += (au8BufIn[2] & 0x1);

        //Scale it to tens 1->10dB 2->20dB 3->30dB
        gain_state *= 10;

        rig_debug(RIG_DEBUG_TRACE, "%s: Calculated gain state is %d.\n",
                  __func__, gain_state);

        if (gain_state > 30 || gain_state < 0 || gain_state % 10 != 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unrecognized composite gain: %d\n", __func__,
                      gain_state);
            return -RIG_EINVAL;
        }

        val->i = gain_state;

        return RIG_OK;

    case RIG_LEVEL_RF:
        au8BufOut[0] = REQUEST_GET_IF_GAIN;
        ret = funcube_hid_cmd(rig, au8BufOut, au8BufIn, sizeof(au8BufIn));
        val->f = ((float)au8BufIn[2]) / 100.;
        return ret;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int funcube_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width)
{
    if (rig->caps->rig_model == RIG_MODEL_FUNCUBEDONGLE)
    {
        *mode = RIG_MODE_USB;
    }
    else
    {
        *mode = RIG_MODE_IQ;
    }

    *width = 192000;
    return RIG_OK;
}

#endif  /* defined(HAVE_LIBUSB) && defined(HAVE_LIBUSB_H) */
