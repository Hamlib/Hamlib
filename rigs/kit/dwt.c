/*
 *  Hamlib KIT backend - Digital World Traveller DRM receiver description
 *  Copyright (c) 2005-2008 by Stephane Fillod
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

#include <stdio.h>
#include "hamlib/rig.h"

#include "kit.h"

#define BACKEND_VER "20200112"

/*
 * Compile only this model if libusb is available
 * or if .DLL is available under Windows
 */
#ifdef _WIN32

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#ifdef HAVE_WINBASE_H
#include <winbase.h>
#endif

#include <math.h>

/*
 * Coding Technologies Digital World Traveller DRM tuner.
 *
 * TODO: rig_scan, set/get FM mode (mono/stereo),
 *      get antenna mode (loop/wire),
 *      get low intermediate frequency (LIF) which may vary up to +-667 Hz
 *      and may be additionally increased of up to 2000 Hz
 */

#define DWTDLL "afgusbfe.dll"


/* Some type definitions needed for dll access */
typedef enum _tFrontendMode
{
    eFrontendModeUndefined  = 0,
    eFrontendModeDrm    = 1,
    eFrontendModeAm     = 2,
    eFrontendModeFm     = 6,
} tFrontendMode;

typedef short (__stdcall *FNCFrontendOpen)(void);
typedef short (__stdcall *FNCFrontendClose)(void);
typedef short (__stdcall *FNCFrontendGetId)(char *id);
typedef short (__stdcall *FNCFrontendSetMode)(tFrontendMode mode);
typedef tFrontendMode(__stdcall *FNCFrontendGetMode)(void);
typedef short (__stdcall *FNCFrontendSetFrequency)(double freq);
typedef double (__stdcall *FNCFrontendGetFrequency)(void);
typedef short (__stdcall *FNCFrontendGetRfLevel)(void);
typedef double (__stdcall *FNCFrontendGetLIF)(void);
typedef short (__stdcall *FNCFrontendStartScan)(short ScanMode,
        double WindowStartFreq, double WindowEndFreq, double DeltaFreq,
        double StepSize);
typedef short (__stdcall *FNCFrontendStopScan)(void);
typedef short (__stdcall *FNCFrontendGetScanStatus)(void);
typedef short (__stdcall *FNCFrontendSetRfAttenuator)(short n);
typedef short (__stdcall *FNCFrontendGetRfAttenuator)(void);
typedef short (__stdcall *FNCFrontendSetAntennaMode)(short n);
typedef short (__stdcall *FNCFrontendGetAntennaMode)(void);
typedef short (__stdcall *FNCFrontendSetFmMode)(short n);
typedef short (__stdcall *FNCFrontendGetFmMode)(void);


struct dwtdll_priv_data
{
    HMODULE dll;

    FNCFrontendOpen FrontendOpen;
    FNCFrontendClose FrontendClose;
    FNCFrontendGetId FrontendGetId;
    FNCFrontendSetMode FrontendSetMode;
    FNCFrontendGetMode FrontendGetMode;
    FNCFrontendSetFrequency FrontendSetFrequency;
    FNCFrontendGetFrequency FrontendGetFrequency;
    FNCFrontendGetRfLevel FrontendGetRfLevel;
    FNCFrontendGetLIF FrontendGetLIF;
    FNCFrontendStartScan FrontendStartScan;
    FNCFrontendStopScan FrontendStopScan;
    FNCFrontendGetScanStatus FrontendGetScanStatus;
    FNCFrontendSetRfAttenuator FrontendSetRfAttenuator;
    FNCFrontendGetRfAttenuator FrontendGetRfAttenuator;
    FNCFrontendSetAntennaMode FrontendSetAntennaMode;
    FNCFrontendGetAntennaMode FrontendGetAntennaMode;
    FNCFrontendSetFmMode FrontendSetFmMode;
    FNCFrontendGetFmMode FrontendGetFmMode;
};


#define DWT_MODES (RIG_MODE_AM|RIG_MODE_USB)    /* USB is for DRM */

#define DWT_FUNC (RIG_FUNC_NONE)

#define DWT_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_STRENGTH|RIG_LEVEL_RAWSTR)

#define DWT_PARM_ALL (RIG_PARM_NONE)

#define DWT_VFO (RIG_VFO_A)

#define DWT_ANT (RIG_ANT_1)

static int dwtdll_init(RIG *rig);
static int dwtdll_cleanup(RIG *rig);
static int dwtdll_open(RIG *rig);
static int dwtdll_close(RIG *rig);
static int dwtdll_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int dwtdll_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int dwtdll_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int dwtdll_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width);
static int dwtdll_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int dwtdll_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static const char *dwtdll_get_info(RIG *rig);

/*
 * Coding Technologies Digital World Traveller DRM tuner.
 *
 * The receiver is controlled via USB, through a Windows DLL.
 *
 * see Winradio G303 as an example
 */

const struct rig_caps dwt_caps =
{
    RIG_MODEL(RIG_MODEL_DWT),
    .model_name =       "Digital World Traveller",
    .mfg_name =     "Coding Technologies",
    .version =      BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_PCRECEIVER,
    .ptt_type =     RIG_PTT_NONE,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      200,
    .retry =        0,

    .has_get_func =     DWT_FUNC,
    .has_set_func =     DWT_FUNC,
    .has_get_level =    DWT_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(DWT_LEVEL_ALL),
    .has_get_parm =     DWT_PARM_ALL,
    .has_set_parm =     RIG_PARM_SET(DWT_PARM_ALL),
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END },
    .attenuator =       { 20, RIG_DBLST_END, }, /* TBC */
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        {kHz(150), MHz(30) - kHz(1), DWT_MODES, -1, -1, DWT_VFO, DWT_ANT},
        {kHz(87.5), MHz(108), RIG_MODE_WFM, -1, -1, DWT_VFO, DWT_ANT},
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 = {
        {kHz(150), MHz(30) - kHz(1), DWT_MODES, -1, -1, DWT_VFO, DWT_ANT},
        {kHz(87.5), MHz(108), RIG_MODE_WFM, -1, -1, DWT_VFO, DWT_ANT},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   { RIG_FRNG_END, },
    .tuning_steps = {
        {DWT_MODES, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_USB, kHz(22)},    /* FIXME */
        {RIG_MODE_AM, kHz(9)},  /* FIXME */
        {RIG_MODE_WFM, kHz(230)},   /* FIXME */
        RIG_FLT_END,
    },

    .rig_init =     dwtdll_init,
    .rig_cleanup =      dwtdll_cleanup,
    .rig_open =     dwtdll_open,
    .rig_close =        dwtdll_close,

    .set_freq =     dwtdll_set_freq,
    .get_freq =     dwtdll_get_freq,
    .set_mode =     dwtdll_set_mode,
    .get_mode =     dwtdll_get_mode,
    .set_level =        dwtdll_set_level,
    .get_level =        dwtdll_get_level,
    .get_info =     dwtdll_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};



int dwtdll_init(RIG *rig)
{
    struct dwtdll_priv_data *priv;

    rig->state.priv = (struct dwtdll_priv_data *)calloc(1, sizeof(
                          struct dwtdll_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    /* Try to load required dll */
    priv->dll = LoadLibrary(DWTDLL);

    if (!priv->dll)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to LoadLibrary %s\n",
                  __func__, DWTDLL);
        free(rig->state.priv);
        return -RIG_EIO;    /* huh! */
    }



    /* Get process addresses from dll for function access */
    priv->FrontendOpen =
        (FNCFrontendOpen) GetProcAddress(priv->dll, "FrontendOpen");
    priv->FrontendClose =
        (FNCFrontendClose) GetProcAddress(priv->dll, "FrontendClose");
    priv->FrontendGetId =
        (FNCFrontendGetId) GetProcAddress(priv->dll, "FrontendGetId");
    priv->FrontendSetMode =
        (FNCFrontendSetMode) GetProcAddress(priv->dll, "FrontendSetMode");
    priv->FrontendGetMode =
        (FNCFrontendGetMode) GetProcAddress(priv->dll, "FrontendGetMode");
    priv->FrontendSetFrequency =
        (FNCFrontendSetFrequency) GetProcAddress(priv->dll, "FrontendSetFrequency");
    priv->FrontendGetFrequency =
        (FNCFrontendGetFrequency) GetProcAddress(priv->dll, "FrontendGetFrequency");
    priv->FrontendGetRfLevel =
        (FNCFrontendGetRfLevel) GetProcAddress(priv->dll, "FrontendGetRfLevel");
    priv->FrontendGetLIF =
        (FNCFrontendGetLIF) GetProcAddress(priv->dll, "FrontendGetLIF");
    priv->FrontendStartScan =
        (FNCFrontendStartScan) GetProcAddress(priv->dll, "FrontendStartScan");
    priv->FrontendStopScan =
        (FNCFrontendStopScan) GetProcAddress(priv->dll, "FrontendStopScan");
    priv->FrontendGetScanStatus =
        (FNCFrontendGetScanStatus) GetProcAddress(priv->dll, "FrontendGetScanStatus");
    priv->FrontendSetRfAttenuator =
        (FNCFrontendSetRfAttenuator) GetProcAddress(priv->dll,
                "FrontendSetRfAttenuator");
    priv->FrontendGetRfAttenuator =
        (FNCFrontendGetRfAttenuator) GetProcAddress(priv->dll,
                "FrontendGetRfAttenuator");
    priv->FrontendSetAntennaMode =
        (FNCFrontendSetAntennaMode) GetProcAddress(priv->dll, "FrontendSetAntennaMode");
    priv->FrontendGetAntennaMode =
        (FNCFrontendGetAntennaMode) GetProcAddress(priv->dll, "FrontendGetAntennaMode");
    priv->FrontendSetFmMode =
        (FNCFrontendSetFmMode) GetProcAddress(priv->dll, "FrontendSetFmMode");
    priv->FrontendGetFmMode =
        (FNCFrontendGetFmMode) GetProcAddress(priv->dll, "FrontendGetFmMode");

    return RIG_OK;
}

int dwtdll_open(RIG *rig)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;
    short ret;

    /* Open DWT receiver */
    ret = priv->FrontendOpen();

    if (ret < 0)
    {
        return -RIG_EIO;    /* huh! */
    }

    /* default to DRM mode */
    ret = priv->FrontendSetMode(eFrontendModeDrm);

    if (ret < 0)
    {
        return -RIG_EIO;    /* huh! */
    }

    return RIG_OK;
}

int dwtdll_close(RIG *rig)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;
    short ret;

    /* Open DWT receiver */
    ret = priv->FrontendClose();

    if (ret < 0)
    {
        return -RIG_EIO;    /* huh! */
    }

    return RIG_OK;
}

int dwtdll_cleanup(RIG *rig)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;

    /* Clean up the dll access */
    if (priv) { FreeLibrary(priv->dll); }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

int dwtdll_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;
    short ret;

    ret = priv->FrontendSetFrequency((double) freq);

    return ret < 0 ? -RIG_EIO : RIG_OK;
}

int dwtdll_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;

    *freq = (freq_t) priv->FrontendGetFrequency();

    return RIG_OK;
}

int dwtdll_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;
    tFrontendMode dwtmode;
    short ret;

    switch (mode)
    {
    case RIG_MODE_USB: dwtmode = eFrontendModeDrm; break;

    case RIG_MODE_AM: dwtmode = eFrontendModeAm; break;

    case RIG_MODE_WFM: dwtmode = eFrontendModeFm; break;

    default:
        return -RIG_EINVAL;
    }

    ret = priv->FrontendSetMode(dwtmode);

    return ret < 0 ? -RIG_EIO : RIG_OK;
}

int dwtdll_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;
    tFrontendMode dwtmode;

    dwtmode = priv->FrontendGetMode();

    switch (dwtmode)
    {
    case eFrontendModeDrm: *mode = RIG_MODE_USB; break;

    case eFrontendModeAm: *mode = RIG_MODE_AM; break;

    case eFrontendModeFm: *mode = RIG_MODE_WFM; break;

    default:
        return -RIG_EPROTO;
    }

    *width = rig_passband_normal(rig, *mode);

    return RIG_OK;
}


int dwtdll_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;
    short ret = 0;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        ret = priv->FrontendSetRfAttenuator(val.i ? 1 : 0);
        break;

    default:
        return -RIG_EINVAL;
    }

    return ret < 0 ? -RIG_EIO : RIG_OK;
}

int dwtdll_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;
    signed short ret = 0;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        ret = priv->FrontendGetRfAttenuator();

        if (ret < 0)
        {
            break;
        }

        /* local vs. DX mode */
        val->i = ret ? 0 : rig->caps->attenuator[0];
        break;

    case RIG_LEVEL_STRENGTH:
        /* actual RMS signal strength in dBuV */
        ret = priv->FrontendGetRfLevel();

        if (ret < 0)
        {
            break;
        }

        /* return actual RMS signal strength in dBuV, -34 to get dB rel S9 */
        val->i = ret - 34;
        break;

    case RIG_LEVEL_RAWSTR:
        /* actual RMS signal strength in dBuV */
        ret = priv->FrontendGetRfLevel();

        if (ret < 0)
        {
            break;
        }

        val->i = ret;
        break;

    default:
        return -RIG_EINVAL;
    }

    return ret < 0 ? -RIG_EIO : RIG_OK;
}


static const char *dwtdll_get_info(RIG *rig)
{
    struct dwtdll_priv_data *priv = (struct dwtdll_priv_data *)rig->state.priv;
    static char info[22];

    if (priv->FrontendGetId(info) < 0)
    {
        return NULL;
    }

    info[21] = '\0';

    return info;
}


#elif defined(HAVE_LIBUSB) && (defined(HAVE_LIBUSB_H) || defined(HAVE_LIBUSB_1_0_LIBUSB_H))

#include <errno.h>

#ifdef HAVE_LIBUSB_H
# include <libusb.h>
#elif defined HAVE_LIBUSB_1_0_LIBUSB_H
# include <libusb-1.0/libusb.h>
#endif

#include "token.h"


#define USB_VID_CT  0x1539  /* AFG Engineering */
#define USB_PID_CT_DWT  0x1730  /* Digital World Traveller */


#define DWT_MODES (RIG_MODE_NONE)

#define DWT_FUNC (RIG_FUNC_NONE)

#define DWT_LEVEL_ALL (RIG_LEVEL_NONE)

#define DWT_PARM_ALL (RIG_PARM_NONE)

#define DWT_VFO (RIG_VFO_A)

static int dwt_init(RIG *rig);
static int dwt_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static const char *dwt_get_info(RIG *rig);

/*
 * Coding Technologies Digital World Traveller DRM tuner.
 *
 * The receiver is controlled via USB.
 *
 * see dsbr100.c as an example
 */

const struct rig_caps dwt_caps =
{
    RIG_MODEL(RIG_MODEL_DWT),
    .model_name =       "Digital World Traveller",
    .mfg_name =     "Coding Technologies",
    .version =      BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_ALPHA,
    .rig_type =     RIG_TYPE_TUNER,
    .ptt_type =     RIG_PTT_NONE,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_USB,
    .serial_rate_min =  9600,   /* don't care */
    .serial_rate_max =  9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      200,
    .retry =        0,

    .has_get_func =     DWT_FUNC,
    .has_set_func =     DWT_FUNC,
    .has_get_level =    DWT_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(DWT_LEVEL_ALL),
    .has_get_parm =     DWT_PARM_ALL,
    .has_set_parm =     RIG_PARM_SET(DWT_PARM_ALL),
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
        {kHz(150), MHz(30) - kHz(1), DWT_MODES, -1, -1, DWT_VFO},
        {kHz(87.5), MHz(108), RIG_MODE_WFM, -1, -1, DWT_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 = {
        {kHz(150), MHz(30) - kHz(1), DWT_MODES, -1, -1, DWT_VFO},
        {kHz(87.5), MHz(108), RIG_MODE_WFM, -1, -1, DWT_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END, },
    .tuning_steps = {
        {DWT_MODES, kHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_USB, kHz(22)},    /* FIXME */
        {RIG_MODE_AM, kHz(9)},  /* FIXME */
        {RIG_MODE_WFM, kHz(230)},   /* FIXME */
        RIG_FLT_END,
    },

    .rig_init =     dwt_init,

    .set_freq =     dwt_set_freq,
    .get_info =     dwt_get_info,

};


int dwt_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;

    rp->parm.usb.vid = USB_VID_CT;
    rp->parm.usb.pid = USB_PID_CT_DWT;
    rp->parm.usb.conf = 1;
    rp->parm.usb.iface = 0;
    rp->parm.usb.alt = 0;

    return RIG_OK;
}

#define MSG_LEN 16
int dwt_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    int request, value, index;
    unsigned char buf[MSG_LEN] = { 0x4a, 0x00, 0x03, 0x00, 0xff, 0xff, 0x32 };
    int requesttype, r;
    int ifreq = (int)(freq / 1000);

    /* FIXME */
    requesttype = 0x00;
    request = 0x00;
    value = 0x00;
    index = 0x00;

    buf[8] = ifreq & 0xff;
    buf[7] = (ifreq >> 8) & 0xff;

    r = libusb_control_transfer(udh, requesttype, request, value, index,
                                buf, 9, 1000);

    if (r < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "libusb_control_transfer failed: %s\n",
                  libusb_error_name(r));
        return -RIG_EIO;
    }

    return RIG_OK;
}

/* Rem: not reentrant */
const char *dwt_get_info(RIG *rig)
{
    static char buf[64];
    libusb_device_handle *udh = rig->state.rigport.handle;
    struct libusb_device_descriptor desc;

    /* always succeeds since libusb-1.0.16 */
    libusb_get_device_descriptor(libusb_get_device(udh), &desc);

    SNPRINTF(buf, sizeof(buf), "Dev %04d", desc.bcdDevice);

    return buf;
}

#endif  /* HAVE_LIBUSB */
