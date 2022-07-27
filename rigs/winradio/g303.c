/*
 *  Hamlib WiNRADiO backend - WR-G303 description
 *  Copyright (c) 2001-2004 by Stephane Fillod
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


#include <hamlib/rig.h>
#include "winradio.h"


#ifdef _WIN32

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#ifdef HAVE_WINBASE_H
#include <winbase.h>
#endif

/*
 * Winradio G3 capabilities.
 *
 * TODO: rig_probe, rig_scan
 */

#define WRG3DLL "wrg3api.dll"

#define G303_FUNC  RIG_FUNC_NONE
#define G303_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AGC | RIG_LEVEL_RF | RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR)

#define G303_MODES (RIG_MODE_USB)   /* FIXME? */

static int g3_init(RIG *rig);
static int g3_cleanup(RIG *rig);
static int g3_open(RIG *rig);
static int g3_close(RIG *rig);
static int g3_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int g3_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int g3_set_powerstat(RIG *rig, powerstat_t status);
static int g3_get_powerstat(RIG *rig, powerstat_t *status);
static int g3_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int g3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static const char *g3_get_info(RIG *rig);

/* #pragma pack(1)       // set byte packing */
typedef struct
{
    int bLength;
    char    szSerNum[9];
    char    szProdName[9];
    DWORD   dwMinFreq;
    DWORD   dwMaxFreq;
    BYTE    bNumBands;
    DWORD   dwBandFreq[16];
    DWORD   dwLOfreq;
    BYTE    bNumVcos;
    DWORD   dwVcoFreq[8];
    WORD    wVcoDiv[8];
    BYTE    bVcoBits[8];
    DWORD   dwRefClk1;
    DWORD   dwRefClk2;
    BYTE    IF1DAC[8];
} __attribute__((packed)) RADIO_INFO;
/* #pragma pack()       // set back the default packing */

/* Some type definitions needed for dll access */
typedef int (__stdcall *FNCOpenRadioDevice)(int iDeviceNum);
typedef BOOL (__stdcall *FNCCloseRadioDevice)(int hRadio);
typedef BOOL (__stdcall *FNCG3SetFrequency)(int hRadio, DWORD dwFreq);
typedef DWORD (__stdcall *FNCG3GetFrequency)(int hRadio);
typedef BOOL (__stdcall *FNCSetPower)(int hRadio, BOOL rPower);
typedef BOOL (__stdcall *FNCGetPower)(int hRadio);
typedef BOOL (__stdcall *FNCSetAtten)(int hRadio, BOOL rAtten);
typedef BOOL (__stdcall *FNCGetAtten)(int hRadio);
typedef BOOL (__stdcall *FNCSetAGC)(int hRadio, int rAGC);
typedef int (__stdcall *FNCGetAGC)(int hRadio);
typedef BOOL (__stdcall *FNCSetIFGain)(int hRadio, int rIFGain);
typedef int (__stdcall *FNCGetIFGain)(int hRadio);
typedef int (__stdcall *FNCGetSignalStrengthdBm)(int hRadio);
typedef int (__stdcall *FNCGetRawSignalStrength)(int hRadio);
typedef BOOL (__stdcall *FNCG3GetInfo)(int hRadio, RADIO_INFO *info);

struct g3_priv_data
{
    HMODULE dll;
    int hRadio;

    FNCOpenRadioDevice OpenRadioDevice;
    FNCCloseRadioDevice CloseRadioDevice;
    FNCG3SetFrequency G3SetFrequency;
    FNCG3GetFrequency G3GetFrequency;
    FNCSetPower SetPower;
    FNCGetPower GetPower;
    FNCSetAtten SetAtten;
    FNCGetAtten GetAtten;
    FNCSetAGC SetAGC;
    FNCGetAGC GetAGC;
    FNCSetIFGain SetIFGain;
    FNCGetIFGain GetIFGain;
    FNCGetSignalStrengthdBm GetSignalStrengthdBm;
    FNCGetRawSignalStrength GetRawSignalStrength;
    FNCG3GetInfo G3GetInfo;
};


const struct rig_caps g303_caps =
{
    RIG_MODEL(RIG_MODEL_G303),
    .model_name =     "WR-G303",
    .mfg_name =       "Winradio",
    .version =        "0.2.1",
    .copyright =      "LGPL", /* This wrapper, not the G3 DLL */
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_PCRECEIVER,
    .port_type =      RIG_PORT_NONE,
    .targetable_vfo =      0,
    .ptt_type =       RIG_PTT_NONE,
    .dcd_type =       RIG_DCD_NONE,
    .has_get_func =   G303_FUNC,
    .has_set_func =   G303_FUNC,
    .has_get_level =  G303_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(G303_LEVEL),
    .has_get_parm =    RIG_PARM_NONE,
    .has_set_parm =    RIG_PARM_NONE,
    .ctcss_list =      NULL,
    .dcs_list =        NULL,
    .chan_list =   { RIG_CHAN_END },
    .transceive =     RIG_TRN_OFF,
    .max_ifshift =     kHz(2),
    .attenuator =     { 20, RIG_DBLST_END, }, /* TBC */
    .rx_range_list1 =  { {
            .startf = kHz(9), .endf = MHz(30), .modes = G303_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  { {
            .startf = kHz(9), .endf = MHz(30), .modes = G303_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  { {G303_MODES, 1},
        RIG_TS_END,
    },

    .filters =       { {G303_MODES, kHz(12)},
        RIG_FLT_END,
    },

    .rig_init =     g3_init,
    .rig_cleanup =  g3_cleanup,
    .rig_open =     g3_open,
    .rig_close =    g3_close,

    .set_freq =     g3_set_freq,
    .get_freq =     g3_get_freq,

    .set_powerstat =   g3_set_powerstat,
    .get_powerstat =   g3_get_powerstat,
    .set_level =     g3_set_level,
    .get_level =     g3_get_level,

    .get_info =      g3_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};



int g3_init(RIG *rig)
{
    struct g3_priv_data *priv;

    rig->state.priv = (struct g3_priv_data *)calloc(1, sizeof(struct g3_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    /* Try to load required dll */
    priv->dll = LoadLibrary(WRG3DLL);

    if (!priv->dll)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to LoadLibrary %s\n",
                  __func__, WRG3DLL);
        free(priv);
        return -RIG_EIO;    /* huh! */
    }

    /* Get process addresses from dll for function access */
    priv->OpenRadioDevice =
        (FNCOpenRadioDevice) GetProcAddress(priv->dll, "OpenRadioDevice");
    priv->CloseRadioDevice =
        (FNCCloseRadioDevice) GetProcAddress(priv->dll, "CloseRadioDevice");
    priv->G3SetFrequency =
        (FNCG3SetFrequency) GetProcAddress(priv->dll, "SetFrequency");
    priv->G3GetFrequency =
        (FNCG3GetFrequency) GetProcAddress(priv->dll, "GetFrequency");
    priv->SetPower = (FNCSetPower) GetProcAddress(priv->dll, "SetPower");
    priv->GetPower = (FNCGetPower) GetProcAddress(priv->dll, "GetPower");
    priv->SetAtten = (FNCSetAtten) GetProcAddress(priv->dll, "SetAtten");
    priv->GetAtten = (FNCGetAtten) GetProcAddress(priv->dll, "GetAtten");
    priv->SetAGC = (FNCSetAGC) GetProcAddress(priv->dll, "SetAGC");
    priv->GetAGC = (FNCGetAGC) GetProcAddress(priv->dll, "GetAGC");
    priv->SetIFGain = (FNCSetIFGain) GetProcAddress(priv->dll, "SetIFGain");
    priv->GetIFGain = (FNCGetIFGain) GetProcAddress(priv->dll, "GetIFGain");
    priv->GetSignalStrengthdBm =
        (FNCGetSignalStrengthdBm) GetProcAddress(priv->dll, "GetSignalStrengthdBm");
    priv->GetRawSignalStrength =
        (FNCGetRawSignalStrength) GetProcAddress(priv->dll, "GetRawSignalStrength");
    priv->G3GetInfo = (FNCG3GetInfo) GetProcAddress(priv->dll, "G3GetInfo");

    return RIG_OK;
}

int g3_open(RIG *rig)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;
    int device_num;

    device_num = atoi(rig->state.rigport.pathname);

    /* Open Winradio receiver handle */
    priv->hRadio = priv->OpenRadioDevice(device_num);

    if (priv->hRadio == 0)
    {
        return -RIG_EIO;    /* huh! */
    }

    /* Make sure the receiver is switched on */
    priv->SetPower(priv->hRadio, TRUE);

    return RIG_OK;
}

int g3_close(RIG *rig)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;

    priv->CloseRadioDevice(priv->hRadio);

    return RIG_OK;
}

int g3_cleanup(RIG *rig)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;

    /* Clean up the dll access */
    if (priv) { FreeLibrary(priv->dll); }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

int g3_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;
    int ret;

    ret = priv->G3SetFrequency(priv->hRadio, (DWORD)(freq));
    ret = ret == TRUE ? RIG_OK : -RIG_EIO;

    return ret;
}

int g3_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;

    *freq = (freq_t) priv->G3GetFrequency(priv->hRadio);

    return *freq != 0 ? RIG_OK : -RIG_EIO;
}

int g3_set_powerstat(RIG *rig, powerstat_t status)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;
    int ret;

    ret = priv->SetPower(priv->hRadio, status == RIG_POWER_ON ? TRUE : FALSE);
    ret = ret == TRUE ? RIG_OK : -RIG_EIO;

    return ret;
}

int g3_get_powerstat(RIG *rig, powerstat_t *status)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;
    int ret;

    ret = priv->GetPower(priv->hRadio);
    *status = ret == TRUE ? RIG_POWER_ON : RIG_POWER_OFF;

    return RIG_OK;
}

int g3_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;
    int ret, agc;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        ret = priv->SetAtten(priv->hRadio, val.i != 0 ? TRUE : FALSE);
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_OFF: agc = 0; break;

        case RIG_AGC_SLOW: agc = 1; break;

        case RIG_AGC_MEDIUM: agc = 2; break;

        case RIG_AGC_FAST: agc = 3; break;

        default:
            return -RIG_EINVAL;
        }

        ret = priv->SetAGC(priv->hRadio, agc);
        break;

    case RIG_LEVEL_RF:
        ret = priv->SetIFGain(priv->hRadio, (int)(val.f * 100));
        break;

    default:
        return -RIG_EINVAL;
    }

    ret = ret == TRUE ? RIG_OK : -RIG_EIO;
    return ret;
}

int g3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;
    int ret;

    ret = RIG_OK;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        val->i = priv->GetAtten(priv->hRadio) ? rig->caps->attenuator[0] : 0;
        break;

    case RIG_LEVEL_AGC:
        switch (priv->GetAGC(priv->hRadio))
        {
        case 0: val->i = RIG_AGC_OFF; break;

        case 1: val->i = RIG_AGC_SLOW; break;

        case 2: val->i = RIG_AGC_MEDIUM; break;

        case 3: val->i = RIG_AGC_FAST; break;

        case -1: ret = -RIG_EIO; break;

        default:
            return -RIG_EINVAL;
        }

        break;

    case RIG_LEVEL_STRENGTH:
        val->i = priv->GetSignalStrengthdBm(priv->hRadio) / 10 + 73;
        break;

    case RIG_LEVEL_RAWSTR:
        val->i = priv->GetRawSignalStrength(priv->hRadio);
        break;

    default:
        return -RIG_EINVAL;
    }

    return ret;
}


static const char *g3_get_info(RIG *rig)
{
    struct g3_priv_data *priv = (struct g3_priv_data *)rig->state.priv;
    static RADIO_INFO info;

    info.bLength = sizeof(RADIO_INFO);

    if (priv->G3GetInfo(priv->hRadio, &info) == FALSE)
    {
        return NULL;
    }

    return info.szSerNum;
}


#endif  /* _WIN32 */
