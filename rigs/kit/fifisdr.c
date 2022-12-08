/*
 *  Hamlib KIT backend - FiFi-SDR Receiver(/Tuner) description
 *  Copyright (c) 2010 by Rolf Meeser
 *
 *  Derived from si570avrusb backend:
 *  Copyright (C) 2004-2010 Stephane Fillod
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


/* Selected request codes of the original AVR USB Si570 firmware */
#define REQUEST_SET_FREQ_BY_VALUE   (0x32)
#define REQUEST_SET_XTALL_FREQ      (0x33)
#define REQUEST_READ_MULTIPLY_LO    (0x39)
#define REQUEST_READ_FREQUENCY      (0x3A)

/* FiFi-SDR specific requests */
#define REQUEST_FIFISDR_READ        (0xAB)
#define REQUEST_FIFISDR_WRITE       (0xAC)


/* USB VID/PID, vendor name (idVendor), product name (idProduct).
 * Use obdev's generic shared VID/PID pair and follow the rules outlined
 * in firmware/usbdrv/USBID-License.txt.
 */
#define USBDEV_SHARED_VID           0x16C0  /* VOTI */
#define USBDEV_SHARED_PID           0x05DC  /* Obdev's free shared PID */
#define FIFISDR_VENDOR_NAME         "www.ov-lennestadt.de"
#define FIFISDR_PRODUCT_NAME        "FiFi-SDR"


/* All level controls */
#define FIFISDR_LEVEL_ALL           (0                      \
                                    | RIG_LEVEL_PREAMP      \
                                    | RIG_LEVEL_STRENGTH    \
                                    | RIG_LEVEL_AF          \
                                    | RIG_LEVEL_AGC         \
                                    | RIG_LEVEL_SQL         \
                                    )



static int fifisdr_init(RIG *rig);
static int fifisdr_cleanup(RIG *rig);
static int fifisdr_open(RIG *rig);
static int fifisdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int fifisdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static const char *fifisdr_get_info(RIG *rig);
static int fifisdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int fifisdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width);
static int fifisdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int fifisdr_get_level(RIG *rig, vfo_t vfo, setting_t level,
                             value_t *val);
static int fifisdr_get_ext_level(RIG *rig, vfo_t vfo, token_t token,
                                 value_t *val);




/* Private tokens. */
#define TOK_LVL_FMCENTER        TOKEN_BACKEND(1)    /* FM center frequency deviation */


/* Extra levels definitions */
static const struct confparams fifisdr_ext_levels[] =
{
    {
        TOK_LVL_FMCENTER, "fmcenter", "FM center", "Center frequency deviation of FM signal",
        NULL, RIG_CONF_NUMERIC, { .n = { -kHz(5), kHz(5), Hz(1) } }
    },
    { RIG_CONF_END, NULL, }
};


/** Private instance data. */
struct fifisdr_priv_instance_data
{
    double multiplier;
};



/** FiFi-SDR receiver description. */
const struct rig_caps fifisdr_caps =
{
    RIG_MODEL(RIG_MODEL_FIFISDR),
    .model_name = "FiFi-SDR",
    .mfg_name = "FiFi",
    .version = "20200112.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,

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
    .has_get_level = FIFISDR_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(FIFISDR_LEVEL_ALL),
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_SET(RIG_PARM_NONE),

    .level_gran = {},
    .parm_gran = {},

    .extparms = NULL,
    .extlevels = fifisdr_ext_levels,

    .preamp = { 6, RIG_DBLST_END },
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
        {
            .startf = kHz(39.1), .endf = MHz(175.0),
            .modes = RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_FM,
            .low_power = -1, .high_power = -1,
            .vfo = RIG_VFO_A, .ant = RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {
            .startf = kHz(39.1), .endf = MHz(175.0),
            .modes = RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_FM,
            .low_power = -1, .high_power = -1,
            .vfo = RIG_VFO_A, .ant = RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END, },

    .tuning_steps = {
        {RIG_MODE_SSB, Hz(1)},
        {RIG_MODE_SSB, Hz(10)},
        {RIG_MODE_SSB, Hz(50)},
        {RIG_MODE_AM, Hz(10)},
        {RIG_MODE_AM, Hz(50)},
        {RIG_MODE_AM, Hz(100)},
        {RIG_MODE_FM, kHz(0.1)},
        {RIG_MODE_FM, kHz(5)},
        {RIG_MODE_FM, kHz(6.25)},
        {RIG_MODE_FM, kHz(10)},
        {RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_FM, kHz(20)},
        {RIG_MODE_FM, kHz(25)},
        {RIG_MODE_FM, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_SSB, kHz(2.7)},   /* normal */
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_SSB, kHz(3.3)},
        {RIG_MODE_AM,  kHz(8.0)},   /* normal */
        {RIG_MODE_AM,  kHz(6.2)},
        {RIG_MODE_AM,  kHz(10.0)},
        {RIG_MODE_FM,  kHz(9.0)},   /* normal */
        {RIG_MODE_FM,  kHz(6.0)},
        {RIG_MODE_FM,  kHz(12.5)},
        RIG_FLT_END,
    },

    .cfgparams = NULL,

    .rig_init = fifisdr_init,
    .rig_cleanup = fifisdr_cleanup,
    .rig_open = fifisdr_open,
    .rig_close = NULL,

    .set_freq = fifisdr_set_freq,
    .get_freq = fifisdr_get_freq,
    .set_mode = fifisdr_set_mode,
    .get_mode = fifisdr_get_mode,

    .set_level = fifisdr_set_level,
    .get_level = fifisdr_get_level,
    .get_ext_level = fifisdr_get_ext_level,

    .get_info = fifisdr_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};




/** Convert from host endianness to FiFi-SDR little endian. */
static uint32_t fifisdr_tole32(uint32_t x)
{
    return
        (((((x) /        1ul) % 256ul) <<  0) |
         ((((x) /      256ul) % 256ul) <<  8) |
         ((((x) /    65536ul) % 256ul) << 16) |
         ((((x) / 16777216ul) % 256ul) << 24));
}



/** Convert FiFi-SDR little endian to host endianness. */
static uint32_t fifisdr_fromle32(uint32_t x)
{
    return
        (((((x) >> 24) & 0xFF) * 16777216ul) +
         ((((x) >> 16) & 0xFF) *    65536ul) +
         ((((x) >>  8) & 0xFF) *      256ul) +
         ((((x) >>  0) & 0xFF) *        1ul));
}



/** USB OUT transfer via vendor device command. */
static int fifisdr_usb_write(RIG *rig,
                             int request, int value, int index,
                             unsigned char *bytes, int size)
{
    int ret;
    libusb_device_handle *udh = rig->state.rigport.handle;

    ret = libusb_control_transfer(udh,
                                  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
                                  request, value, index,
                                  bytes, size, rig->state.rigport.timeout);

    if (ret != size)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer (%d/%d) failed: %s\n",
                  __func__,
                  request, value,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    return RIG_OK;
}



/** USB IN transfer via vendor device command. */
static int fifisdr_usb_read(RIG *rig,
                            int request, int value, int index,
                            unsigned char *bytes, int size)
{
    int ret;
    libusb_device_handle *udh = rig->state.rigport.handle;

    ret = libusb_control_transfer(udh,
                                  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                                  request, value, index,
                                  bytes, size, rig->state.rigport.timeout);

    if (ret != size)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer (%d/%d) failed: %s\n",
                  __func__,
                  request, value,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    return RIG_OK;
}



int fifisdr_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct fifisdr_priv_instance_data *priv;

    rig->state.priv = (struct fifisdr_priv_instance_data *)calloc(sizeof(
                          struct fifisdr_priv_instance_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->multiplier = 4;

    rp->parm.usb.vid = USBDEV_SHARED_VID;
    rp->parm.usb.pid = USBDEV_SHARED_PID;

    /* no usb_set_configuration() and usb_claim_interface() */
    rp->parm.usb.iface = -1;
    rp->parm.usb.conf = 1;
    rp->parm.usb.alt = 0;

    rp->parm.usb.vendor_name = FIFISDR_VENDOR_NAME;
    rp->parm.usb.product = FIFISDR_PRODUCT_NAME;

    return RIG_OK;
}



int fifisdr_cleanup(RIG *rig)
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



int fifisdr_open(RIG *rig)
{
    int ret;
    uint32_t multiply;
    struct fifisdr_priv_instance_data *priv;


    priv = (struct fifisdr_priv_instance_data *)rig->state.priv;

    /* The VCO is a multiple of the RX frequency. Typically 4 */
    ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
                           11, /* Read virtual VCO factor */
                           (unsigned char *)&multiply, sizeof(multiply));

    if (ret == RIG_OK)
    {
        priv->multiplier = fifisdr_fromle32(multiply);
    }

    return RIG_OK;
}



const char *fifisdr_get_info(RIG *rig)
{
    static char buf[64];
    int ret;
    uint32_t svn_version;

    ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0, 0,
                           (unsigned char *)&svn_version, sizeof(svn_version));

    if (ret != RIG_OK)
    {
        return NULL;
    }

    SNPRINTF(buf, sizeof(buf), "Firmware version: %u", svn_version);

    return buf;
}



int fifisdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct fifisdr_priv_instance_data *priv = (struct fifisdr_priv_instance_data *)
            rig->state.priv;
    int ret;
    double mhz;
    uint32_t freq1121;


    /* Need frequency in 11.21 format */
    mhz = (freq * priv->multiplier) / 1e6;
    freq1121 = fifisdr_tole32(round(mhz * 2097152.0));

    ret = fifisdr_usb_write(rig, REQUEST_SET_FREQ_BY_VALUE, 0, 0,
                            (unsigned char *)&freq1121, sizeof(freq1121));

    if (ret != RIG_OK)
    {
        return -RIG_EIO;
    }

    return RIG_OK;
}



int fifisdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct fifisdr_priv_instance_data *priv = (struct fifisdr_priv_instance_data *)
            rig->state.priv;
    int ret;
    uint32_t freq1121;


    ret = fifisdr_usb_read(rig, REQUEST_READ_FREQUENCY, 0, 0,
                           (unsigned char *)&freq1121, sizeof(freq1121));

    if (ret == RIG_OK)
    {
        freq1121 = fifisdr_fromle32(freq1121);
        *freq = MHz(((double)freq1121 / (1ul << 21)) / priv->multiplier);
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

    switch (mode)
    {
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
                            15, /* Demodulator mode */
                            (unsigned char *)&fifi_mode, sizeof(fifi_mode));

    if (ret != RIG_OK)
    {
        return -RIG_EIO;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return ret; }

    /* Set filter width */
    fifi_width = fifisdr_tole32(width);
    ret = fifisdr_usb_write(rig, REQUEST_FIFISDR_WRITE, 0,
                            16, /* Filter width */
                            (unsigned char *)&fifi_width, sizeof(fifi_width));

    if (ret != RIG_OK)
    {
        return -RIG_EIO;
    }

    return RIG_OK;
}



static int fifisdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width)
{
    int ret;
    uint8_t fifi_mode;
    uint32_t fifi_width;


    /* Read current mode */
    ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
                           15,  /* Demodulator mode */
                           (unsigned char *)&fifi_mode, sizeof(fifi_mode));

    if (ret != RIG_OK)
    {
        return -RIG_EIO;
    }

    /* Translate mode coding */
    *mode = RIG_MODE_NONE;

    switch (fifi_mode)
    {
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
                           16,  /* Filter width */
                           (unsigned char *)&fifi_width, sizeof(fifi_width));

    if (ret != RIG_OK)
    {
        return -RIG_EIO;
    }

    *width = s_Hz(fifisdr_fromle32(fifi_width));

    return RIG_OK;
}



static int fifisdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int ret = RIG_OK;
    uint8_t fifi_preamp;
    int16_t fifi_volume;
    uint8_t fifi_squelch;
    uint8_t fifi_agc;


    switch (level)
    {
    /* Preamplifier (ADC 0/+6dB switch) */
    case RIG_LEVEL_PREAMP:
        /* Value can be 0 (0 dB) or 1 (+6 dB) */
        fifi_preamp = 0;

        if (val.i == 6)
        {
            fifi_preamp = 1;
        }

        ret = fifisdr_usb_write(rig, REQUEST_FIFISDR_WRITE, 0,
                                19, /* Preamp */
                                (unsigned char *)&fifi_preamp, sizeof(fifi_preamp));
        break;

    /* RX volume control */
    case RIG_LEVEL_AF:
        /* Transform Hamlib value (float: 0...1) to an integer range (0...100) */
        fifi_volume = (int16_t)(val.f * 100.0f);

        if (fifi_volume < 0)
        {
            fifi_volume = 0;
        }

        if (fifi_volume > 100)
        {
            fifi_volume = 100;
        }

        ret = fifisdr_usb_write(rig, REQUEST_FIFISDR_WRITE, 0,
                                14, /* Demodulator volume */
                                (unsigned char *)&fifi_volume, sizeof(fifi_volume));
        break;

    /* Squelch level */
    case RIG_LEVEL_SQL:
        /* Transform Hamlib value (float: 0...1) to an integer range (0...100) */
        fifi_squelch = (uint8_t)(val.f * 100.0f);

        if (fifi_squelch > 100)
        {
            fifi_squelch = 100;
        }

        ret = fifisdr_usb_write(rig, REQUEST_FIFISDR_WRITE, 0,
                                20, /* Squelch control */
                                (unsigned char *)&fifi_squelch, sizeof(fifi_squelch));
        break;

    /* AGC */
    case RIG_LEVEL_AGC:
        /* Transform Hamlib enum value to FiFi-SDR selector */
        fifi_agc = 0;

        switch (val.i)
        {
        case RIG_AGC_OFF:       fifi_agc = 0;   break;

        case RIG_AGC_SUPERFAST: fifi_agc = 1;   break;

        case RIG_AGC_FAST:      fifi_agc = 2;   break;

        case RIG_AGC_SLOW:      fifi_agc = 3;   break;

        case RIG_AGC_USER:      fifi_agc = 4;   break;

        case RIG_AGC_MEDIUM:    fifi_agc = 5;   break;

        case RIG_AGC_AUTO:      fifi_agc = 6;   break;
        }

        ret = fifisdr_usb_write(rig, REQUEST_FIFISDR_WRITE, 0,
                                21, /* AGC template */
                                (unsigned char *)&fifi_agc, sizeof(fifi_agc));
        break;

    /* Unsupported option */
    default:
        ret = -RIG_ENIMPL;
    }

    return ret;
}



static int fifisdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int ret = RIG_OK;
    uint32_t fifi_meter = 0;
    uint8_t fifi_preamp = 0;
    int16_t fifi_volume = 0;
    uint8_t fifi_squelch = 0;
    uint8_t fifi_agc = 0;


    switch (level)
    {
    /* Preamplifier (ADC 0/+6dB switch) */
    case RIG_LEVEL_PREAMP:
        ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
                               19,  /* Preamp */
                               (unsigned char *)&fifi_preamp, sizeof(fifi_preamp));

        if (ret == RIG_OK)
        {
            /* Value can be 0 (0 dB) or 1 (+6 dB) */
            val->i = 0;

            if (fifi_preamp != 0)
            {
                val->i = 6;
            }
        }

        break;

    /* RX volume control */
    case RIG_LEVEL_AF:
        ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
                               14,  /* Demodulator volume */
                               (unsigned char *)&fifi_volume, sizeof(fifi_volume));

        if (ret == RIG_OK)
        {
            /* Value is in % (0...100) */
            val->f = 0.0f;

            if ((fifi_volume >= 0) && (fifi_volume <= 100))
            {
                val->f = (float)fifi_volume / 100.0f;
            }
        }

        break;

    /* Squelch level */
    case RIG_LEVEL_SQL:
        ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
                               20,  /* Squelch control */
                               (unsigned char *)&fifi_squelch, sizeof(fifi_squelch));

        if (ret == RIG_OK)
        {
            /* Value is in % (0...100) */
            val->f = 0.0f;

            if (fifi_squelch <= 100)
            {
                val->f = (float)fifi_squelch / 100.0f;
            }
        }

        break;

    /* AGC */
    case RIG_LEVEL_AGC:
        ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
                               21,  /* AGC template */
                               (unsigned char *)&fifi_agc, sizeof(fifi_agc));

        if (ret == RIG_OK)
        {
            val->i = 0;

            switch (fifi_agc)
            {
            case 0: val->i = RIG_AGC_OFF;       break;

            case 1: val->i = RIG_AGC_SUPERFAST; break;

            case 2: val->i = RIG_AGC_FAST;      break;

            case 3: val->i = RIG_AGC_SLOW;      break;

            case 4: val->i = RIG_AGC_USER;      break;

            case 5: val->i = RIG_AGC_MEDIUM;    break;

            case 6: val->i = RIG_AGC_AUTO;      break;
            }
        }

        break;

    /* Signal strength */
    case RIG_LEVEL_STRENGTH:
        ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
                               17,  /* S-Meter */
                               (unsigned char *)&fifi_meter, sizeof(fifi_meter));

        if (ret == RIG_OK)
        {
            val->i = fifisdr_fromle32(fifi_meter);
        }

        break;

    /* Unsupported option */
    default:
        ret = -RIG_ENIMPL;
    }

    return ret;
}



static int fifisdr_get_ext_level(RIG *rig, vfo_t vfo, token_t token,
                                 value_t *val)
{
    int ret = RIG_OK;
    uint32_t u32;


    switch (token)
    {
    /* FM center frequency deviation */
    case TOK_LVL_FMCENTER:
        ret = fifisdr_usb_read(rig, REQUEST_FIFISDR_READ, 0,
                               18,  /* FM center frequency */
                               (unsigned char *)&u32, sizeof(u32));

        if (ret == RIG_OK)
        {
            val->f = Hz((int32_t)fifisdr_fromle32(u32));
        }

        break;

    /* Unsupported option */
    default:
        ret = -RIG_ENIMPL;
    }

    return ret;
}

#endif  /* defined(HAVE_LIBUSB) && defined(HAVE_LIBUSB_H) */
