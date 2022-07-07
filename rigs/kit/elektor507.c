/*
 *  Hamlib KIT backend - Elektor SDR USB (5/07) receiver description
 *  Copyright (c) 2007-2010 by Stephane Fillod
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "hamlib/rig.h"
#include "token.h"

#include "kit.h"

#ifdef _WIN32
#define USE_FTDI_DLL
#elif defined(HAVE_LIBUSB) && (defined(HAVE_LIBUSB_H) || defined(HAVE_LIBUSB_1_0_LIBUSB_H))
#define USE_LIBUSB
#endif

/*
 * Compile this model only if libusb is available
 * or if .DLL is available under Windows
 */
#if defined(USE_FTDI_DLL) || defined(USE_LIBUSB)


static int elektor507_init(RIG *rig);
static int elektor507_cleanup(RIG *rig);
static int elektor507_open(RIG *rig);
static int elektor507_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int elektor507_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int elektor507_set_level(RIG *rig, vfo_t vfo, setting_t level,
                                value_t val);
static int elektor507_get_level(RIG *rig, vfo_t vfo, setting_t level,
                                value_t *val);
static int elektor507_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);
static int elektor507_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                              ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);
static int elektor507_set_conf(RIG *rig, token_t token, const char *val);
static int elektor507_get_conf(RIG *rig, token_t token, char *val);


static const char *elektor507_get_info(RIG *rig);

/*
 * I2C addresses
 */
#define CY_I2C_RAM_ADR  210
#define CY_I2C_EEPROM_ADR  208

/*
 * I2C registers
 */
#define CLKOE_REG 0x09
#define DIV1_REG 0x0c
#define DIV2_REG 0x47
#define XTALCTL_REG 0x12
#define CAPLOAD_REG 0x13
#define PUMPCOUNTERS_REG 0x40
#define CLKSRC_REG 0x44


static int cy_update_pll(RIG *rig, unsigned char IICadr);
static int i2c_write_regs(RIG *rig, unsigned char IICadr, int reg_count,
                          unsigned char reg_adr,
                          unsigned char reg_val1, unsigned char reg_val2, unsigned char reg_val3);
#define i2c_write_reg(rig, IICadr, reg_adr, reg_val) \
        i2c_write_regs(rig, IICadr, 1, reg_adr, reg_val, 0, 0)


#ifdef USE_FTDI_DLL

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#ifdef HAVE_WINBASE_H
#include <winbase.h>
#endif

#include <math.h>

#define ELEKTOR507_DLL "FTD2XX.dll"


/* Some type definitions needed for dll access */
typedef enum
{
    FT_OK = 0,
    FT_INVALID_HANDLE = 1,
    FT_DEVICE_NOT_FOUND = 2,
    FT_DEVICE_NOT_OPENED = 3,
    FT_IO_ERROR = 4,
    FT_INSUFFICIENT_RESOURCES = 5,
    FT_INVALID_PARAMETER = 6,
    FT_SUCCESS = FT_OK,
    FT_INVALID_BAUD_RATE = 7,
    FT_DEVICE_NOT_OPENED_FOR_ERASE = 8,
    FT_DEVICE_NOT_OPENED_FOR_WRITE = 9,
    FT_FAILED_TO_WRITE_DEVICE = 10,
    FT_EEPROM_READ_FAILED = 11,
    FT_EEPROM_WRITE_FAILED = 12,
    FT_EEPROM_ERASE_FAILED = 13,
    FT_EEPROM_NOT_PRESENT = 14,
    FT_EEPROM_NOT_PROGRAMMED = 15,
    FT_INVALID_ARGS = 16,
    FT_OTHER_ERROR = 17,
} FT_Result;

typedef FT_Result(__stdcall *FNCFT_Open)(int Index, unsigned long *ftHandle);
typedef FT_Result(__stdcall *FNCFT_Close)(unsigned long ftHandle);
typedef FT_Result(__stdcall *FNCFT_SetBitMode)(unsigned long ftHandle,
        unsigned char Mask, unsigned char Enable);
typedef FT_Result(__stdcall *FNCFT_SetBaudRate)(unsigned long ftHandle,
        unsigned long BaudRate);
typedef FT_Result(__stdcall *FNCFT_Write)(unsigned long ftHandle,
        void *FTOutBuf, unsigned long BufferSize, int *ResultPtr);


struct elektor507_extra_priv_data
{
    HMODULE dll;

    FNCFT_Open FT_Open;
    FNCFT_Close FT_Close;
    FNCFT_SetBitMode FT_SetBitMode;
    FNCFT_SetBaudRate FT_SetBaudRate;
    FNCFT_Write FT_Write;

    unsigned long ftHandle;
};

#elif defined(USE_LIBUSB)


#include <errno.h>

#ifdef HAVE_LIBUSB_H
# include <libusb.h>
#elif defined HAVE_LIBUSB_1_0_LIBUSB_H
# include <libusb-1.0/libusb.h>
#endif


#define USB_VID_FTDI        0x0403  /* Future Technology Devices International */
#define USB_PID_FTDI_FT232  0x6001  /* FT232R 8-bit FIFO */

#define FTDI_IN_EP 0x02
#define FTDI_USB_WRITE_TIMEOUT 5000


struct elektor507_extra_priv_data
{
    /* empty with libusb */
};

#endif


/* defaults */
#define OSCFREQ         10000 /* kHz unit -> MHz(10) */
#define XTAL_CAL        128

#define TOK_OSCFREQ TOKEN_BACKEND(1)
#define TOK_XTALCAL TOKEN_BACKEND(2)

static const struct confparams elektor507_cfg_params[] =
{
    {
        TOK_OSCFREQ, "osc_freq", "Oscillator freq", "Oscillator frequency in Hz",
        "10000000", RIG_CONF_NUMERIC, { .n = { 0, MHz(30), 1 } }
    },
    {
        TOK_XTALCAL, "xtal_cal", "Xtal Cal", "Crystal calibration",
        "132", RIG_CONF_NUMERIC, { .n = { 0, 255, 1 } }
    },
    { RIG_CONF_END, NULL, }
};


/*
 * Common data struct
 */
struct elektor507_priv_data
{
    struct elektor507_extra_priv_data extra_priv;

    unsigned xtal_cal;  /* 0..255 (-150ppm..150ppm) */
    unsigned osc_freq;  /* kHz */

#define ANT_AUTO    RIG_ANT_1
#define ANT_EXT     RIG_ANT_2
#define ANT_TEST_CLK    RIG_ANT_3
    ant_t ant;      /* current antenna */

    /* CY PLL stuff.
     * This is Qtotal & Ptotal values here.
     */
    int P, Q, Div1N;

    /* FTDI comm stuff */
    unsigned char FT_port;
    int Buf_adr;
#define FT_OUT_BUFFER_MAX 1024
    unsigned char FT_Out_Buffer[FT_OUT_BUFFER_MAX];
};



#ifdef USE_FTDI_DLL

int elektor507_init(RIG *rig)
{
    struct elektor507_priv_data *priv;
    struct elektor507_extra_priv_data *extra_priv;

    priv = (struct elektor507_priv_data *)calloc(sizeof(struct
            elektor507_priv_data), 1);

    if (!priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv->xtal_cal = XTAL_CAL;
    priv->osc_freq = OSCFREQ;
    priv->ant = ANT_AUTO;

    /* DIV1N set to safe default */
    priv->Div1N = 8;
    priv->P = 8;
    priv->Q = 2;

    extra_priv = &priv->extra_priv;

    /* Try to load required dll */
    extra_priv->dll = LoadLibrary(ELEKTOR507_DLL);

    if (!extra_priv->dll)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to LoadLibrary %s\n",
                  __func__, ELEKTOR507_DLL);
        free(priv);
        return -RIG_EIO;    /* huh! */
    }

    /*
     * Get process addresses from dll for function access
     */

    /* Open_USB_Device */
    extra_priv->FT_Open =
        (FNCFT_Open) GetProcAddress(extra_priv->dll, "FT_Open");
    /* Close_USB_Device */
    extra_priv->FT_Close =
        (FNCFT_Close) GetProcAddress(extra_priv->dll, "FT_Close");
    /* Set_USB_Device_BitMode */
    extra_priv->FT_SetBitMode =
        (FNCFT_SetBitMode) GetProcAddress(extra_priv->dll, "FT_SetBitMode");
    /* Set_USB_Device_BaudRate */
    extra_priv->FT_SetBaudRate =
        (FNCFT_SetBaudRate) GetProcAddress(extra_priv->dll, "FT_SetBaudRate");
    /* Write_USB_Device_Buffer */
    extra_priv->FT_Write =
        (FNCFT_Write) GetProcAddress(extra_priv->dll, "FT_Write");

    rig->state.priv = (void *)priv;

    return RIG_OK;
}

int elektor507_ftdi_write_data(RIG *rig, void *FTOutBuf,
                               unsigned long BufferSize)
{
    struct elektor507_extra_priv_data *extra_priv =
        &((struct elektor507_priv_data *)rig->state.priv)->extra_priv;
    FT_Result ret;
    int Result;

    rig_debug(RIG_DEBUG_TRACE, "%s called, %d bytes\n", __func__, (int)BufferSize);

    /* Open FTDI */
    ret = extra_priv->FT_Open(0, &extra_priv->ftHandle);

    if (ret != FT_OK)
    {
        return -RIG_EIO;
    }

    ret = extra_priv->FT_SetBitMode(extra_priv->ftHandle, 0xff, 1);

    if (ret != FT_OK)
    {
        return -RIG_EIO;
    }

    ret = extra_priv->FT_SetBaudRate(extra_priv->ftHandle, 38400);

    if (ret != FT_OK)
    {
        return -RIG_EIO;
    }

    ret = extra_priv->FT_Write(extra_priv->ftHandle, FTOutBuf, BufferSize, &Result);

    if (ret != FT_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "FT_Write failed: %d, Result: %d\n", ret, Result);
        return -RIG_EIO;
    }

    ret = extra_priv->FT_Close(extra_priv->ftHandle);

    if (ret != FT_OK)
    {
        return -RIG_EIO;
    }

    return RIG_OK;
}

int elektor507_cleanup(RIG *rig)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;

    /* Clean up the dll access */
    if (priv) { FreeLibrary(priv->extra_priv.dll); }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

const char *elektor507_get_info(RIG *rig)
{
    static char buf[64];

    SNPRINTF(buf, sizeof(buf), "Elektor SDR USB w/ FTDI DLL");

    return buf;
}



#elif defined(USE_LIBUSB)


/*
 * The libusb code is inspired by libftdi:
 *  http://www.intra2net.com/de/produkte/opensource/ftdi/
 */
int elektor507_init(RIG *rig)
{
    hamlib_port_t *rp = &rig->state.rigport;
    struct elektor507_priv_data *priv;

    rig->state.priv = (struct elektor507_priv_data *)calloc(sizeof(struct
                      elektor507_priv_data), 1);

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->xtal_cal = XTAL_CAL;
    priv->osc_freq = OSCFREQ;
    priv->ant = ANT_AUTO;

    /* DIV1N set to safe default */
    priv->Div1N = 8;
    priv->P = 8;
    priv->Q = 2;

    rp->parm.usb.vid = USB_VID_FTDI;
    rp->parm.usb.pid = USB_PID_FTDI_FT232;
    rp->parm.usb.conf = 1;
    rp->parm.usb.iface = 0;
    rp->parm.usb.alt = 0;   /* necessary ? */

    return RIG_OK;
}

int elektor507_cleanup(RIG *rig)
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
const char *elektor507_get_info(RIG *rig)
{
    static char buf[64];
    libusb_device_handle *udh = rig->state.rigport.handle;
    struct libusb_device_descriptor desc;

    /* always succeeds since libusb-1.0.16 */
    libusb_get_device_descriptor(libusb_get_device(udh), &desc);

    SNPRINTF(buf, sizeof(buf), "USB dev %04d", desc.bcdDevice);

    return buf;
}

int elektor507_libusb_setup(RIG *rig)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret;
    unsigned short index = 0, usb_val;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /* Reset the ftdi device */
#if 1
    ret =  libusb_control_transfer(udh, 0x40, 0, 0, index, NULL, 0,
                                   FTDI_USB_WRITE_TIMEOUT);

    if (ret != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer reset failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

#endif

    /*
     * Enable bitbang mode
     */
    usb_val = 0xff; /* low byte: bitmask */
    usb_val |= (0x01 << 8); /* Basic bitbang_mode: 0x01 */

    ret = libusb_control_transfer(udh, 0x40, 0x0B, usb_val, index, NULL, 0,
                                  FTDI_USB_WRITE_TIMEOUT);

    if (ret != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer bitbangmode failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    /*
     * Set baudrate
     * 9600 x4 because of bitbang mode
     */
    usb_val = 49230;    /* magic value for 38400 bauds */
    index = 0;
    ret = libusb_control_transfer(udh, 0x40, 3, usb_val, index, NULL, 0,
                                  FTDI_USB_WRITE_TIMEOUT);

    if (ret != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: libusb_control_transfer baudrate failed: %s\n",
                  __func__,
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    return RIG_OK;
}

int elektor507_ftdi_write_data(RIG *rig, void *FTOutBuf,
                               unsigned long BufferSize)
{
    libusb_device_handle *udh = rig->state.rigport.handle;
    int ret, actual_length;

    rig_debug(RIG_DEBUG_TRACE, "%s called, %lu bytes\n", __func__, BufferSize);

    ret = libusb_bulk_transfer(udh, FTDI_IN_EP, FTOutBuf, BufferSize,
                               &actual_length, FTDI_USB_WRITE_TIMEOUT);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "usb_bulk_write failed: %s\n",
                  libusb_error_name(ret));
        return -RIG_EIO;
    }

    return RIG_OK;
}
#endif  /* USE_LIBUSB */



#define ELEKTOR507_MODES (RIG_MODE_USB) /* USB is for SDR */

#define ELEKTOR507_FUNC (RIG_FUNC_NONE)

#define ELEKTOR507_LEVEL_ALL (RIG_LEVEL_ATT)

#define ELEKTOR507_PARM_ALL (RIG_PARM_NONE)

#define ELEKTOR507_VFO (RIG_VFO_A)

/*
 * - Auto-filter antenna (K3)
 * - External antenna (PC1)
 * - Internal TEST_CLK (5 MHz)
 */
#define ELEKTOR507_ANT (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)


/*
 * Elektor SDR USB (5/07) receiver description
 *
 * This kit is a QSD based on a CY27EE16ZE PLL.
 * The receiver is controlled via USB (through FTDI FT232R).
 *
 * Original article:
 * http://www.elektor.com/magazines/2007/may/software-defined-radio.91527.lynkx
 *
 * Author (Burkhard Kainka) page, in german:
 * http://www.b-kainka.de/sdrusb.html
 */

const struct rig_caps elektor507_caps =
{
    RIG_MODEL(RIG_MODEL_ELEKTOR507),
    .model_name =       "Elektor SDR-USB",
    .mfg_name =     "Elektor",
    .version =      "20200112.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TUNER,
    .ptt_type =     RIG_PTT_NONE,
    .dcd_type =     RIG_DCD_NONE,
#ifdef USE_LIBUSB
    .port_type =        RIG_PORT_USB,
#else
    .port_type =        RIG_PORT_NONE,
#endif
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      200,
    .retry =        0,

    .has_get_func =     ELEKTOR507_FUNC,
    .has_set_func =     ELEKTOR507_FUNC,
    .has_get_level =    ELEKTOR507_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(ELEKTOR507_LEVEL_ALL),
    .has_get_parm =     ELEKTOR507_PARM_ALL,
    .has_set_parm =     RIG_PARM_SET(ELEKTOR507_PARM_ALL),
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       NULL,
    .dcs_list =     NULL,
    .preamp =       { RIG_DBLST_END },
    .attenuator =       { 10, 20, RIG_DBLST_END },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        {kHz(30), MHz(30) - kHz(1), ELEKTOR507_MODES, -1, -1, ELEKTOR507_VFO, ELEKTOR507_ANT},
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END, },
    .rx_range_list2 = {
        {kHz(30), MHz(30) - kHz(1), ELEKTOR507_MODES, -1, -1, ELEKTOR507_VFO, ELEKTOR507_ANT},
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END, },
    .tuning_steps = {
        {ELEKTOR507_MODES, kHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters = {
        {RIG_MODE_USB, kHz(24)},    /* bandpass may be more */
        RIG_FLT_END,
    },
    .cfgparams =        elektor507_cfg_params,

    .rig_init =     elektor507_init,
    .rig_cleanup =      elektor507_cleanup,
    .rig_open =     elektor507_open,
    .set_conf =     elektor507_set_conf,
    .get_conf =     elektor507_get_conf,


    .set_freq =     elektor507_set_freq,
    .get_freq =     elektor507_get_freq,
    .set_level =        elektor507_set_level,
    .get_level =        elektor507_get_level,
    .set_ant =      elektor507_set_ant,
    .get_ant =      elektor507_get_ant,
    .get_info =     elektor507_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


int elektor507_set_conf(RIG *rig, token_t token, const char *val)
{
    struct elektor507_priv_data *priv;
    freq_t freq;

    priv = (struct elektor507_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        sscanf(val, "%"SCNfreq, &freq);
        priv->osc_freq = freq / kHz(1);
        break;

    case TOK_XTALCAL:
        sscanf(val, "%u", &priv->xtal_cal);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int elektor507_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct elektor507_priv_data *priv;

    priv = (struct elektor507_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_OSCFREQ:
        SNPRINTF(val, val_len, "%"PRIfreq, priv->osc_freq * kHz(1));
        break;

    case TOK_XTALCAL:
        SNPRINTF(val, val_len, "%u", priv->xtal_cal);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int elektor507_get_conf(RIG *rig, token_t token, char *val)
{
    return elektor507_get_conf2(rig, token, val, 128);
}



int elektor507_open(RIG *rig)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    int ret;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /*
     * Setup the FT232R.
     */
#ifdef USE_LIBUSB
    ret = elektor507_libusb_setup(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

#endif

    /* Init the FT232R port to SCL/SDA high, Mux A0, Att 0 */
    priv->FT_port = 0x03;


    /*
     * Setup the CY27EE16ZE PLL.
     */

    /* Enable only CLOCK5. CLOCK3 will be on demand in set_ant() */
    ret = i2c_write_reg(rig, CY_I2C_RAM_ADR, CLKOE_REG, 0x20);

    if (ret != 0)
    {
        return ret;
    }

    /* DIV1N set to safe default */
    priv->Div1N = 8;
    ret = i2c_write_reg(rig, CY_I2C_RAM_ADR, DIV1_REG, priv->Div1N);

    if (ret != 0)
    {
        return ret;
    }

#if 0
    /* Xtal gain setting */
    ret = i2c_write_reg(rig, CY_I2C_RAM_ADR, XTALCTL_REG, 0x32);

    if (ret != 0)
    {
        return ret;
    }

    /* CapLoad set to middle */
    ret = i2c_write_reg(rig, CY_I2C_RAM_ADR, CAPLOAD_REG, priv->xtal_cal);

    if (ret != 0)
    {
        return ret;
    }

#endif
    /* CLKSRC: CLOCK3=DIV2CLK/2, CLOCK5=DIV1CLK/DIV1N */
    ret = i2c_write_regs(rig, CY_I2C_RAM_ADR, 3, CLKSRC_REG, 0x02, 0x8e, 0x47);

    if (ret != 0)
    {
        return ret;
    }

    /* DIV2SRC from REF */
    ret = i2c_write_reg(rig, CY_I2C_RAM_ADR, DIV2_REG, 0x88);

    if (ret != 0)
    {
        return ret;
    }

    return RIG_OK;
}


#define FREQ_ALGORITHM 3    /* use AC6SL version 3-Aug-2010 */

#if FREQ_ALGORITHM == 1     /* this used to be ORIG_ALGORITHM */
static void find_P_Q_DIV1N(struct elektor507_priv_data *priv, freq_t freq)
{
    int Freq;
    double Min, VCO;
    int p, q, q_max;

    Freq = freq / kHz(1);

    if (Freq > 19 && Freq < 60)
    {
        priv->Div1N = (2500 + Freq / 2) / Freq + 128;
        priv->P = 1000;
        priv->Q = 40;
        return;
    }
    else if (Freq > 59 && Freq < 801)
    {
        priv->Div1N = 125;
        priv->P = Freq * 2;
        priv->Q = 40;
        return;
    }
    else if (Freq > 800 && Freq < 2001)
    {
        priv->Div1N = 50;
        priv->P = Freq;
        priv->Q = 50;
        return;
    }
    else if (Freq > 2000 && Freq < 4001)
    {
        priv->Div1N = 25;
    }
    else if (Freq > 4000 && Freq < 10001)
    {
        priv->Div1N = 10;
    }
    else if (Freq > 10000 && Freq < 20001)
    {
        priv->Div1N = 5;
    }
    else if (Freq > 20000 && Freq < 30001)
    {
        priv->Div1N = 4;
    }

    Min = priv->osc_freq;
    freq /= kHz(1);

    /*
     * Q:2..129
     * P:8..2055, best 16..1023 (because of Pump)

       For stable operation:
       + REF/Qtotal must not fall below 250kHz (
       + P*(REF/Qtotal) must not be above 400 MHz or below 100 MHz
      */
#if 1
    q_max = priv->osc_freq / 250;
#else
    q_max = 100;
#endif

    for (q = q_max; q >= 10; q--)
    {
        for (p = 500; p <= 2000; p++)
        {
            VCO = ((double)priv->osc_freq / q) * p;

            if (fabs(4 * freq - VCO / priv->Div1N) < Min)
            {
                Min = fabs(4 * freq - VCO / priv->Div1N);
                priv->Q = q;
                priv->P = p;
            }
        }
    }

    VCO = ((double)priv->osc_freq / priv->Q) * priv->P;

    if (VCO < 100e3 || VCO > 400e3)
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Unstable parameters for VCO=%.1f\n",
                  __func__, VCO);
}
#endif  /* ORIG_ALGORITHM */

#if FREQ_ALGORITHM == 2     /* this used to be default alternative to ORIG_ALGORITHM */
static void find_P_Q_DIV1N(struct elektor507_priv_data *priv, freq_t freq)
{
    double Min, VCO, freq4;
    int div1n_min, div1n_max;
    int p, q, div1n, q_max;

    Min = priv->osc_freq;
    freq4 = freq * 4 / kHz(1);

#define vco_min 100e3
#define vco_max 500e3
    /*
     * Q:2..129
     * P:8..2055, best 16..1023 (because of Pump)

       For stable operation:
       + REF/Qtotal must not fall below 250kHz (
       + P*(REF/Qtotal) must not be above 400 MHz or below 100 MHz
      */
#if 1
    q_max = priv->osc_freq / 250;
#else
    q_max = 100;
#endif
    div1n_min = vco_min / freq4;

    if (div1n_min < 2)
    {
        div1n_min = 2;
    }
    else if (div1n_min > 127)
    {
        div1n_min = 127;
    }

    div1n_max = vco_max / freq4;

    if (div1n_max > 127)
    {
        div1n_max = 127;
    }
    else if (div1n_max < 2)
    {
        div1n_max = 2;
    }


    for (div1n = div1n_min; div1n <= div1n_max; div1n++)
    {
        // P/Qtotal = FREQ4*DIV1N/REF
        // (Q*int(r) + frac(r)*Q)/Q
        for (q = q_max; q >= 2; q--)
        {
            p = q * freq4 * div1n / priv->osc_freq;
#if 1

            if (p < 16 || p > 1023)
            {
                continue;
            }

#endif

            VCO = ((double)priv->osc_freq / q) * p;
#if 1

            if (VCO < vco_min || VCO > vco_max)
            {
                continue;
            }

#endif

            if (fabs(freq4 - VCO / div1n) < Min)
            {
                Min = fabs(freq4 - VCO / div1n);
                priv->Div1N = div1n;
                priv->Q = q;
                priv->P = p;
            }
        }
    }

    VCO = ((double)priv->osc_freq / priv->Q) * priv->P;

    if (VCO < vco_min || VCO > 400e3)
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Unstable parameters for VCO=%.1f\n",
                  __func__, VCO);
}
#endif  /* default alternative to ORIG_ALGORITHM */

#if FREQ_ALGORITHM == 3     /* AC6SL version 5-Aug-2010 */
/* search valid (P,Q,N) for closest match to requested frequency */
static void find_P_Q_DIV1N(
    struct elektor507_priv_data *priv,
    freq_t freq)
{
#define VCO_MIN 100000000
#define VCO_MAX 400000000
    int Ptotal;
    int Qtotal, Qmax = 40;
    int Div1N;
    double PmulREFdivQ;
    double Ref = priv->osc_freq * 1000.0;
    double freq4 = freq * 4;
    double newdelta, delta = fabs((priv->P * (Ref / priv->Q) / priv->Div1N) -
                                  freq4);

    /* For stable operation: Ref/Qtotal must not fall below 250kHz */
    /* Qmax = (int) ( Ref / 250000); */
    for (Qtotal = 2; Qtotal <= Qmax; Qtotal++)
    {
        double REFdivQ = (Ref / Qtotal);

        /* For stable operation: Ptotal*(Ref/Qtotal) must be ... */
        int Pmin = (int)(VCO_MIN / REFdivQ);   /* ... >= 100mHz */
        int Pmax = (int)(VCO_MAX / REFdivQ);   /* ... <= 400mHz */

        for (Ptotal = Pmin; Ptotal <= Pmax; Ptotal++)
        {
            PmulREFdivQ = Ptotal * REFdivQ;

            Div1N = (int)((PmulREFdivQ + freq4 / 2) / freq4);

            if (Div1N < 2)
            {
                Div1N = 2;
            }

            if (Div1N > 127)
            {
                Div1N = 127;
            }

            newdelta = fabs((PmulREFdivQ / Div1N) - freq4);

            if (newdelta < delta)
            {
                /* save best (P,Q,N) */
                delta = newdelta;
                priv->P = Ptotal;
                priv->Q = Qtotal;
                priv->Div1N = Div1N;
            }
        }
    }
}
#endif /* AC6SL version 5-Aug-2010 */

int elektor507_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    freq_t final_freq;
    int ret = 0;

    if (priv->ant == ANT_AUTO)
    {
        int Mux;

        /* Automatically select appropriate filter */
        if (freq <= kHz(1600))
        {
            /* Select A1, low pass, fc=1.6MHz */
            Mux = 1;
        }
        else
        {
            /* Select A2, high pass */
            Mux = 2;
        }

        priv->FT_port &= 0x63;  //0,1 = I2C, 2,3,4=MUX, 5,6=Attenuator
        priv->FT_port |= Mux << 2;
    }

    find_P_Q_DIV1N(priv, freq); /* Compute PLL parameters */

    elektor507_get_freq(rig, vfo, &final_freq);
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: Freq=%.0f kHz, delta=%d Hz, Div1N=%d, P=%d, Q=%d, FREQ_ALGORITHM=%d\n",
              __func__, freq / kHz(1), (int)(final_freq - freq), priv->Div1N, priv->P,
              priv->Q, FREQ_ALGORITHM);

    if ((double)priv->osc_freq / priv->Q < 250)
        rig_debug(RIG_DEBUG_WARN,
                  "%s: Unstable parameters for REF/Qtotal=%.1f\n",
                  __func__, (double)priv->osc_freq / priv->Q);

    ret = cy_update_pll(rig, CY_I2C_RAM_ADR);

    return (ret != 0) ? -RIG_EIO : RIG_OK;
}

int elektor507_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    double VCO;

    VCO = ((double)priv->osc_freq * kHz(1)) / priv->Q * priv->P;

    /* Div by 4 because of QSD */
    *freq = (VCO / priv->Div1N) / 4;

    return RIG_OK;
}

int elektor507_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    int ret = 0;
    int att = 0;

    switch (level)
    {
    case RIG_LEVEL_ATT:

        /* val.i */
        /* FTDI: DSR, DCD */

        switch (val.i)
        {
        case 0: att = 0; break;

        case 10: att = 1; break;

        case 20: att = 2; break;

        default: return -RIG_EINVAL;
        }

        priv->FT_port &= 0x1f;
        priv->FT_port |= (att & 0x3) << 5;

        ret = elektor507_ftdi_write_data(rig, &priv->FT_port, 1);

        break;

    default:
        return -RIG_EINVAL;
    }

    return (ret != 0) ? -RIG_EIO : RIG_OK;
}

int elektor507_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    int ret = 0;

    switch (level)
    {
    case RIG_LEVEL_ATT:

        switch ((priv->FT_port >> 5) & 3)
        {
        case 0: val->i = 0; break;

        case 1: val->i = 10; break;

        case 2: val->i = 20; break;

        default:
            ret = -RIG_EINVAL;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return (ret != 0) ? -RIG_EIO : RIG_OK;
}


int elektor507_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    int ret, Mux;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /*
     * FTDI: RTS, CTS, DTR
     *
     * A4,A5,A6 are not connected
     *
     * ANT1->A1/A2, ANT2->A3, ANT3->A7
     */

    switch (ant)
    {
    case RIG_ANT_1: Mux = 0; break; /* Mux will be updated upon next set_freq */

    case RIG_ANT_2: Mux = 3; break; /* ANT_EXT */

    case RIG_ANT_3: Mux = 7; break; /* ANT_TEST_CLK */

    default:
        return -RIG_EINVAL;
    }

    priv->ant = ant;

    priv->FT_port &= 0x63;  //0,1 = I2C, 2,3,4=MUX, 5,6=Attenuator
    priv->FT_port |= Mux << 2;

#if 0
    ret = elektor507_ftdi_write_data(rig, &priv->FT_port, 1);
#else
    /* Enable CLOCK3 on demand */
    ret = i2c_write_reg(rig, CY_I2C_RAM_ADR, CLKOE_REG,
                        0x20 | (ant == RIG_ANT_3 ? 0x04 : 0));
#endif

    return (ret != 0) ? -RIG_EIO : RIG_OK;
}

int elektor507_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                       ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;

    *ant_curr = priv->ant;

    return RIG_OK;
}


/*
 * Update the PLL counters
 */
static int cy_update_pll(RIG *rig, unsigned char IICadr)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    int P0, R40, R41, R42;
    unsigned char Div1N;
    unsigned char Clk3_src;
    int Pump;
    int ret;

    /*
     * PLL Pump setting according to table 9
     */
    if (priv->P < 45)
    {
        Pump = 0;
    }
    else if (priv->P < 480)
    {
        Pump = 1;
    }
    else if (priv->P < 640)
    {
        Pump = 2;
    }
    else if (priv->P < 800)
    {
        Pump = 3;
    }
    else
    {
        Pump = 4;
    }

    P0 = priv->P & 0x01;
    R40 = (((priv->P >> 1) - 4) >> 8) | (Pump << 2) | 0xc0;
    R41 = ((priv->P >> 1) - 4) & 0xff;
    R42 = (priv->Q - 2) | (P0 << 7);


    ret = i2c_write_regs(rig, IICadr, 3, PUMPCOUNTERS_REG, R40, R41, R42);

    if (ret != 0)
    {
        return ret;
    }

    switch (priv->Div1N)
    {
    case 2:
        /* Fixed /2 divider option */
        Clk3_src = 0x80;
        Div1N = 8;
        break;

    case 3:
        /* Fixed /3 divider option */
        Clk3_src = 0xc0;
        Div1N = 6;
        break;

    default:
        Div1N = priv->Div1N;
        Clk3_src = 0x40;
    }

    ret = i2c_write_reg(rig, IICadr, DIV1_REG, Div1N);

    if (ret != 0)
    {
        return ret;
    }


    /* Set 2 low bits of CLKSRC for CLOCK5. DIV1CLK is set already */
    ret = i2c_write_reg(rig, IICadr, CLKSRC_REG + 2, Clk3_src | 0x07);

    if (ret != 0)
    {
        return ret;
    }

    return RIG_OK;
}


static void ftdi_SCL(RIG *rig, int d)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;

    if (priv->Buf_adr >= FT_OUT_BUFFER_MAX)
    {
        return;
    }

    /*
     * FTDI RXD->SCL
     */

    if (d == 0)
    {
        priv->FT_port &= ~0x02;
    }
    else
    {
        priv->FT_port |= 0x02;
    }

    priv->FT_Out_Buffer[priv->Buf_adr++] = priv->FT_port;
}

static void ftdi_SDA(RIG *rig, int d)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;

    if (priv->Buf_adr >= FT_OUT_BUFFER_MAX)
    {
        return;
    }

    /*
     * FTDI TXD->SDA
     */

    if (d == 0)
    {
        priv->FT_port &= ~0x01;
    }
    else
    {
        priv->FT_port |= 0x01;
    }

    priv->FT_Out_Buffer[priv->Buf_adr++] = priv->FT_port;
}

static void ftdi_I2C_Init(RIG *rig)
{
    ftdi_SCL(rig, 1); ftdi_SDA(rig, 1);          /* SCL=1, SDA=1 */
}

static void ftdi_I2C_Start(RIG *rig)
{
    ftdi_SDA(rig, 0);          /* SDA=0 */
    ftdi_SCL(rig, 0);          /* SCL=0 */
}

static void ftdi_I2C_Stop(RIG *rig)
{
    ftdi_SCL(rig, 0); ftdi_SDA(rig, 0);  /* SCL=0, SDA=0 */
    ftdi_SCL(rig, 1);          /* SCL=1 */
    ftdi_SDA(rig, 1);          /* SDA=1 */
}

/*
    Acknowledge:
      SCL=0, SDA=0
      SCL=1
      SCL=0

    No Acknowledge:
      SCL=0, SDA=1
      SCL=1
      SCL=0
 */

static void ftdi_I2C_Write_Byte(RIG *rig, unsigned char c)
{
    int i;

    for (i = 7; i >= 0; i--)
    {
        ftdi_SDA(rig, c & (1 << i));        /* SDA value */

        ftdi_SCL(rig, 1);
        ftdi_SCL(rig, 0);
    }

    ftdi_SDA(rig, 1);
    ftdi_SCL(rig, 1);
    ftdi_SCL(rig, 0);
}


int i2c_write_regs(RIG *rig, unsigned char IICadr, int reg_count,
                   unsigned char reg_adr,
                   unsigned char reg_val1, unsigned char reg_val2, unsigned char reg_val3)
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    int ret;

    /* Start with a new buffer */
    priv->Buf_adr = 0;

    ftdi_I2C_Init(rig);
    ftdi_I2C_Start(rig);
    ftdi_I2C_Write_Byte(rig, IICadr);
    ftdi_I2C_Write_Byte(rig, reg_adr);

    if (reg_count >= 1)
    {
        ftdi_I2C_Write_Byte(rig, reg_val1);
    }

    if (reg_count >= 2)
    {
        ftdi_I2C_Write_Byte(rig, reg_val2);
    }

    if (reg_count >= 3)
    {
        ftdi_I2C_Write_Byte(rig, reg_val3);
    }

    ftdi_I2C_Stop(rig);
    //usleep(10000);

    ret = elektor507_ftdi_write_data(rig, priv->FT_Out_Buffer, priv->Buf_adr);

    if (ret != 0)
    {
        return -RIG_EIO;
    }

    return 0;
}


#if 0
static const unsigned char ftdi_code[256] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x34, 0x08, 0x5a, 0x24/*0x6f*/, 0x00, 0x14, 0x0a, 0x00, 0x08, 0x88,
    0x50, 0x04, 0x32, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xd1, 0x2b, 0x17, 0x00, 0xfe, 0xfe, 0x7f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32,
    0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32,
    0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32,
    0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32,
    0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32,
    0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32,
    0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32,
    0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32, 0x10, 0x32
};

int load_ftdi_code(RIG *rig, unsigned char IICadr, const unsigned char code[])
{
    struct elektor507_priv_data *priv = (struct elektor507_priv_data *)
                                        rig->state.priv;
    int ret;
    int i, j;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    for (i = 0; i < 16; i++)
    {
        /* Start with a new buffer */
        priv->Buf_adr = 0;
        ftdi_I2C_Init(rig);
        ftdi_I2C_Start(rig);
        ftdi_I2C_Write_Byte(rig, IICadr);
        ftdi_I2C_Write_Byte(rig, i * 16);

        for (j = 0; j < 16; j++)
        {
            ftdi_I2C_Write_Byte(rig, code[i * 16 + j]);
        }

        ftdi_I2C_Stop(rig);

        ret = elektor507_ftdi_write_data(rig, priv->FT_Out_Buffer, priv->Buf_adr);

        if (ret != 0)
        {
            return -RIG_EIO;
        }
    }

    return RIG_OK;
}
#endif

#endif  /* defined(USE_FTDI_DLL) || defined(USE_LIBUSB) */
