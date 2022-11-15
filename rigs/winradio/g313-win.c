/*
 *  Hamlib WiNRADiO backend - WR-G313
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

#include <stdlib.h>

#include <hamlib/rig.h>
#include "winradio.h"

#define G313_FUNC  RIG_FUNC_NONE
#define G313_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AGC | RIG_LEVEL_RF | RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR)

#define G313_MODES (RIG_MODE_USB)

#if defined (_WIN32) || !defined(OTHER_POSIX)

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


#define WAVEOUT_SOUNDCARDID 0x150901

const struct confparams g313_cfg_params[] =
{
    {
        WAVEOUT_SOUNDCARDID, "wodeviceid", "WaveOut Device ID", "Sound card device ID for playing IF signal from receiver",
        "-1", RIG_CONF_NUMERIC, { .n = { -3, 32, 1 } }
    },
    { RIG_CONF_END, NULL, }
};

#define WRG313DLL "wrg3130api.dll"

#define G313_FUNC  RIG_FUNC_NONE
#define G313_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AGC | RIG_LEVEL_RF | RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR)

static int g313_init(RIG *rig);
static int g313_cleanup(RIG *rig);
static int g313_open(RIG *rig);
static int g313_close(RIG *rig);
static int g313_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int g313_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int g313_set_powerstat(RIG *rig, powerstat_t status);
static int g313_get_powerstat(RIG *rig, powerstat_t *status);
static int g313_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int g313_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static const char *g313_get_info(RIG *rig);
int g313_set_conf(RIG *rig, token_t token, const char *val);
int g313_get_conf(RIG *rig, token_t token, char *val);

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

typedef MMRESULT(__stdcall *TwaveOutGetDevCaps)(UINT_PTR uDeviceID,
        LPWAVEOUTCAPS pwoc, UINT cbwoc);
typedef UINT(__stdcall *TwaveOutGetNumDevs)(void);


typedef HANDLE(__stdcall *TStartWaveOut)(LONG hRadio, LONG WaveOutDeviceIndex);
typedef void (__stdcall *TStopWaveOut)(HANDLE hWaveOut);

struct g313_priv_data
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


    HMODULE WinMM;
    TwaveOutGetDevCaps  waveOutGetDevCaps;
    TwaveOutGetNumDevs waveOutGetNumDevs;

    HMODULE hWRG313WO;

    int WaveOutDeviceID;

    HANDLE  hWaveOut;
    TStartWaveOut StartWaveOut;
    TStopWaveOut  StopWaveOut;

    int     Opened;
};


const struct rig_caps g313_caps =
{
    RIG_MODEL(RIG_MODEL_G313),
    .model_name =     "WR-G313",
    .mfg_name =       "Winradio",
    .version =        "20191204.0",
    .copyright =        "LGPL", /* This wrapper, not the G313 DLL */
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_PCRECEIVER,
    .port_type =      RIG_PORT_NONE,
    .targetable_vfo =    0,
    .ptt_type =       RIG_PTT_NONE,
    .dcd_type =       RIG_DCD_NONE,
    .has_get_func =   G313_FUNC,
    .has_set_func =   G313_FUNC,
    .has_get_level =  G313_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(G313_LEVEL),
    .has_get_parm =      RIG_PARM_NONE,
    .has_set_parm =      RIG_PARM_NONE,
    .ctcss_list =    NULL,
    .dcs_list =      NULL,
    .chan_list =     { RIG_CHAN_END },
    .transceive =     RIG_TRN_OFF,
    .max_ifshift =   kHz(2),
    .attenuator =     { 20, RIG_DBLST_END, },   /* TBC */
    .rx_range_list1 =  { {
            .startf = kHz(9), .endf = MHz(30), .modes = G313_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  { {
            .startf = kHz(9), .endf = MHz(30), .modes = G313_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  { {G313_MODES, 1},
        RIG_TS_END,
    },

    .filters =       { {G313_MODES, kHz(12)},
        RIG_FLT_END,
    },

    .cfgparams = g313_cfg_params,
    .set_conf = g313_set_conf,
    .get_conf = g313_get_conf,

    .rig_init =     g313_init,
    .rig_cleanup =  g313_cleanup,
    .rig_open =     g313_open,
    .rig_close =    g313_close,

    .set_freq =     g313_set_freq,
    .get_freq =     g313_get_freq,

    .set_powerstat =   g313_set_powerstat,
    .get_powerstat =   g313_get_powerstat,
    .set_level =     g313_set_level,
    .get_level =     g313_get_level,

    .get_info =      g313_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};



int g313_init(RIG *rig)
{
    struct g313_priv_data *priv;

    rig->state.priv = (struct g313_priv_data *)calloc(1, sizeof(
                          struct g313_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->WaveOutDeviceID = -1;

    priv->Opened = 0;
    priv->hWaveOut = NULL;

    priv->WinMM = LoadLibrary("WinMM.dll");

    if (priv->WinMM == NULL)
    {
        free(priv);
        return -RIG_EIO;
    }

    priv->hWRG313WO = LoadLibrary("WRG313WO.dll");

    if (priv->hWRG313WO == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to LoadLibrary WRG313WO.dll\n",
                  __func__);
        FreeLibrary(priv->WinMM);
        free(priv);
        return -RIG_EIO;
    }

    priv->StartWaveOut = (TStartWaveOut)GetProcAddress(priv->hWRG313WO,
                         "StartWaveOut");
    priv->StopWaveOut = (TStopWaveOut)GetProcAddress(priv->hWRG313WO,
                        "StopWaveOut");

    if (!priv->StartWaveOut || !priv->StopWaveOut)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to load valid WRG313WO.dll library\n",
                  __func__);
        FreeLibrary(priv->hWRG313WO);
        FreeLibrary(priv->WinMM);
        free(priv);
        return -RIG_EIO;
    }


    /* Try to load required dll */
    priv->dll = LoadLibrary(WRG313DLL);

    if (!priv->dll)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to LoadLibrary %s\n",
                  __func__, WRG313DLL);
        FreeLibrary(priv->hWRG313WO);
        FreeLibrary(priv->WinMM);
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


    if (!priv->OpenRadioDevice || !priv->CloseRadioDevice || !priv->G3SetFrequency
            ||
            !priv->G3GetFrequency || !priv->SetPower || !priv->GetPower || !priv->SetAtten
            ||
            !priv->GetAtten || !priv->SetAGC || !priv->GetAGC || !priv->SetIFGain
            || !priv->GetIFGain ||
            !priv->GetSignalStrengthdBm || !priv->GetRawSignalStrength)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to load valid %s library\n",
                  __func__, WRG313DLL);
        FreeLibrary(priv->dll);
        FreeLibrary(priv->hWRG313WO);
        FreeLibrary(priv->WinMM);
        free(priv);
        return -RIG_EIO;
    }

    priv->waveOutGetDevCaps = (TwaveOutGetDevCaps)GetProcAddress(priv->WinMM,
                              "waveOutGetDevCapsA");
    priv->waveOutGetNumDevs = (TwaveOutGetNumDevs)GetProcAddress(priv->WinMM,
                              "waveOutGetNumDevs");



    return RIG_OK;
}

int g313_findVSC(struct g313_priv_data *priv)
{
    int OutIndex;
    WAVEOUTCAPS Caps;
    int Count;
    int i;

    OutIndex = -1;
    Count = priv->waveOutGetNumDevs();

    for (i = 0; i < Count; i++)
    {
        if (priv->waveOutGetDevCaps(i, &Caps, sizeof(Caps)) == MMSYSERR_NOERROR)
        {
            if (strncmp(Caps.szPname, "WiNRADiO Virtual Sound Card", 27) == 0)
            {
                OutIndex = i;
                break;
            }
        }
    }

    return OutIndex;
}

int g313_open(RIG *rig)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int device_num;
    int Count;
    int id;

    device_num = atoi(rig->state.rigport.pathname);

    Count = priv->waveOutGetNumDevs();

    if (Count == 0)
    {
        return -RIG_EIO;
    }

    if (priv->WaveOutDeviceID == -2)
    {
        id = g313_findVSC(priv);
    }
    else
    {
        id = priv->WaveOutDeviceID;
    }


    /* Open Winradio receiver handle */
    priv->hRadio = priv->OpenRadioDevice(device_num);

    if (priv->hRadio == 0)
    {
        return -RIG_EIO;    /* huh! */
    }

    /* Make sure the receiver is switched on */
    priv->SetPower(priv->hRadio, TRUE);

    if (id > -3)
    {
        priv->hWaveOut = priv->StartWaveOut(priv->hRadio, id);

        if (priv->hWaveOut == NULL)
        {
            priv->CloseRadioDevice(priv->hRadio);
            return -RIG_EIO;
        }
    }
    else
    {
        priv->hWaveOut = NULL;
    }


    priv->Opened = 1;

    return RIG_OK;
}

int g313_close(RIG *rig)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;


    if (!priv->Opened)
    {
        return RIG_OK;
    }

    priv->Opened = 0;

    if (priv->hWaveOut)
    {
        priv->StopWaveOut(priv->hWaveOut);
    }

    priv->CloseRadioDevice(priv->hRadio);

    return RIG_OK;
}

int g313_cleanup(RIG *rig)
{
    struct g313_priv_data *priv;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct g313_priv_data *)rig->state.priv;

    /* Clean up the dll access */
    FreeLibrary(priv->dll);
    FreeLibrary(priv->WinMM);
    FreeLibrary(priv->hWRG313WO);

    free(rig->state.priv);

    rig->state.priv = NULL;

    return RIG_OK;
}

int g313_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int ret;

    ret = priv->G3SetFrequency(priv->hRadio, (DWORD)(freq));
    ret = ret == TRUE ? RIG_OK : -RIG_EIO;

    return ret;
}

int g313_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

    *freq = (freq_t) priv->G3GetFrequency(priv->hRadio);

    return *freq != 0 ? RIG_OK : -RIG_EIO;
}

int g313_set_powerstat(RIG *rig, powerstat_t status)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int ret;

    ret = priv->SetPower(priv->hRadio, status == RIG_POWER_ON ? TRUE : FALSE);
    ret = ret == TRUE ? RIG_OK : -RIG_EIO;

    return ret;
}

int g313_get_powerstat(RIG *rig, powerstat_t *status)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int ret;

    ret = priv->GetPower(priv->hRadio);
    *status = ret == TRUE ? RIG_POWER_ON : RIG_POWER_OFF;

    return RIG_OK;
}

int g313_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int ret, agc;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        ret = priv->SetAtten(priv->hRadio, val.i != 0 ? TRUE : FALSE);
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_OFF:
            agc = 0;
            break;

        case RIG_AGC_SLOW:
            agc = 1;
            break;

        case RIG_AGC_MEDIUM:
            agc = 2;
            break;

        case RIG_AGC_FAST:
            agc = 3;
            break;

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

int g313_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
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
        case 0:
            val->i = RIG_AGC_OFF;
            break;

        case 1:
            val->i = RIG_AGC_SLOW;
            break;

        case 2:
            val->i = RIG_AGC_MEDIUM;
            break;

        case 3:
            val->i = RIG_AGC_FAST;
            break;

        case -1:
            ret = -RIG_EIO;
            break;

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


static const char *g313_get_info(RIG *rig)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    static RADIO_INFO info;

    info.bLength = sizeof(RADIO_INFO);

    if (priv->G3GetInfo(priv->hRadio, &info) == FALSE)
    {
        return NULL;
    }

    return info.szSerNum;
}


int g313_set_conf(RIG *rig, token_t token, const char *val)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int id;

    switch (token)
    {
    case WAVEOUT_SOUNDCARDID:
        if (val[0] == '0' && val[1] == 'x')
        {
            id = strtol(val, (char **)NULL, 16);
        }
        else
        {
            id = atoi(val);
        }

        if (id < -3 || id > 32)
        {
            return -RIG_EINVAL;
        }

        priv->WaveOutDeviceID = id;

        if (priv->Opened)
        {
            if (id == -2)
            {
                id = g313_findVSC(priv);
            }

            if (priv->hWaveOut)
            {
                priv->StopWaveOut(priv->hWaveOut);
            }

            if (id > -3)
            {
                priv->hWaveOut = priv->StartWaveOut(priv->hRadio, id);
            }
            else
            {
                priv->hWaveOut = NULL;
            }
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int g313_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

    switch (token)
    {
    case WAVEOUT_SOUNDCARDID:
        SNPRINTF(val, val_len, "%d", priv->WaveOutDeviceID);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int g313_get_conf(RIG *rig, token_t token, char *val)
{
    return g313_get_conf2(rig, token, val, 128);
}

#endif /* _WIN32 */
