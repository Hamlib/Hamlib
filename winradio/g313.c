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

#define G313_MODES (RIG_MODE_NONE)

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


#define WAVEOUT_SOUNDCARDID 0x150901

const struct confparams g313_cfg_params[] = {
	{ WAVEOUT_SOUNDCARDID, "wodeviceid", "WaveOut Device ID", "Sound card device ID for playing IF signal from receiver",
			"-1", RIG_CONF_NUMERIC, { .n = { -3, 32, 1 } }
	},
	{ RIG_CONF_END, NULL, }
};

#define WRG313DLL "wrg3130api.dll"

#define G313_FUNC  RIG_FUNC_NONE
#define G313_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AGC | RIG_LEVEL_RF | RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR)

#define G313_MODES (RIG_MODE_NONE)

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
static const char* g313_get_info(RIG *rig);
int g313_set_conf(RIG *rig, token_t token, const char *val);
int g313_get_conf(RIG *rig, token_t token, char *val);

/* #pragma pack(1)       // set byte packing */
typedef struct {
	int	bLength;
	char	szSerNum[9];
	char	szProdName[9];
	DWORD	dwMinFreq;
	DWORD	dwMaxFreq;
	BYTE	bNumBands;
	DWORD	dwBandFreq[16];
	DWORD	dwLOfreq;
	BYTE	bNumVcos;
	DWORD	dwVcoFreq[8];
	WORD	wVcoDiv[8];
	BYTE	bVcoBits[8];
	DWORD	dwRefClk1;
	DWORD	dwRefClk2;
	BYTE	IF1DAC[8];
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
typedef BOOL (__stdcall *FNCG3GetInfo)(int hRadio,RADIO_INFO *info);

typedef MMRESULT (__stdcall *TwaveOutGetDevCaps)(UINT_PTR uDeviceID,LPWAVEOUTCAPS pwoc,UINT cbwoc);
typedef UINT (__stdcall *TwaveOutGetNumDevs)(void);


typedef HANDLE (__stdcall *TStartWaveOut)(LONG hRadio,LONG WaveOutDeviceIndex);
typedef void (__stdcall *TStopWaveOut)(HANDLE hWaveOut);

struct g313_priv_data {
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
	TwaveOutGetDevCaps	waveOutGetDevCaps;
	TwaveOutGetNumDevs waveOutGetNumDevs;

	HMODULE hWRG313WO;

	int	WaveOutDeviceID;

	HANDLE	hWaveOut;
	TStartWaveOut StartWaveOut;
	TStopWaveOut  StopWaveOut;

	int		Opened;
};


const struct rig_caps g313_caps = {
  .rig_model =      RIG_MODEL_G313,
  .model_name =     "WR-G313",
  .mfg_name =       "Winradio",
  .version =        "0.1",
  .copyright = 	    "LGPL",	/* This wrapper, not the G313 DLL */
  .status =         RIG_STATUS_BETA,
  .rig_type =       RIG_TYPE_PCRECEIVER,
  .port_type =      RIG_PORT_NONE,
  .targetable_vfo = 	 0,
  .ptt_type =       RIG_PTT_NONE,
  .dcd_type =       RIG_DCD_NONE,
  .has_get_func =   G313_FUNC,
  .has_set_func =   G313_FUNC,
  .has_get_level =  G313_LEVEL,
  .has_set_level =  RIG_LEVEL_SET(G313_LEVEL),
  .has_get_parm = 	 RIG_PARM_NONE,
  .has_set_parm = 	 RIG_PARM_NONE,
  .ctcss_list = 	 NULL,
  .dcs_list =   	 NULL,
  .chan_list = 	 { RIG_CHAN_END },
  .transceive =     RIG_TRN_OFF,
  .max_ifshift = 	 kHz(2),
  .attenuator =     { 20, RIG_DBLST_END, },	/* TBC */
  .rx_range_list1 =  { {.start = kHz(9),.end = MHz(30),.modes = G313_MODES,
		    	.low_power = -1,.high_power = -1,.vfo = RIG_VFO_A},
		    	RIG_FRNG_END, },
  .tx_range_list1 =  { RIG_FRNG_END, },
  .rx_range_list2 =  { {.start = kHz(9),.end = MHz(30),.modes = G313_MODES,
		    	.low_power = -1,.high_power = -1,.vfo = RIG_VFO_A},
		    	RIG_FRNG_END, },
  .tx_range_list2 =  { RIG_FRNG_END, },

  .tuning_steps =  { {G313_MODES,1},
  		  	RIG_TS_END, },

  .filters =       { {G313_MODES, kHz(12)},
                  RIG_FLT_END, },

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
};



int g313_init(RIG *rig)
{
	struct g313_priv_data *priv;

	priv = (struct g313_priv_data*)malloc(sizeof(struct g313_priv_data));
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	priv->WaveOutDeviceID=-1;

	priv->Opened=0;
	priv->hWaveOut=NULL;

	priv->WinMM=LoadLibrary("WinMM.dll");

	if(priv->WinMM==NULL)
	{
		free(priv);
		return -RIG_EIO;
	}

	priv->hWRG313WO=LoadLibrary("WRG313WO.dll");

	if(priv->hWRG313WO==NULL)
	{
		rig_debug(RIG_DEBUG_ERR, "%s: Unable to LoadLibrary WRG313WO.dll\n",
				__FUNCTION__);
		FreeLibrary(priv->WinMM);
		free(priv);
		return -RIG_EIO;
	}

	priv->StartWaveOut=(TStartWaveOut)GetProcAddress(priv->hWRG313WO,"StartWaveOut");
	priv->StopWaveOut=(TStopWaveOut)GetProcAddress(priv->hWRG313WO,"StopWaveOut");

	if(!priv->StartWaveOut || !priv->StopWaveOut)
	{
		rig_debug(RIG_DEBUG_ERR, "%s: Unable to load valid WRG313WO.dll library\n",
				__FUNCTION__);
		FreeLibrary(priv->hWRG313WO);
		FreeLibrary(priv->WinMM);
		free(priv);
		return -RIG_EIO;
	}


	/* Try to load required dll */
	priv->dll = LoadLibrary(WRG313DLL);

	if (!priv->dll) {
		rig_debug(RIG_DEBUG_ERR, "%s: Unable to LoadLibrary %s\n",
				__FUNCTION__, WRG313DLL);
		FreeLibrary(priv->hWRG313WO);
		FreeLibrary(priv->WinMM);
		free(priv);
		return -RIG_EIO;	/* huh! */
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


	if(!priv->OpenRadioDevice || !priv->CloseRadioDevice || !priv->G3SetFrequency ||
	   !priv->G3GetFrequency || !priv->SetPower || !priv->GetPower || !priv->SetAtten ||
	   !priv->GetAtten || !priv->SetAGC || !priv->GetAGC || !priv->SetIFGain || !priv->GetIFGain ||
	   !priv->GetSignalStrengthdBm || !priv->GetRawSignalStrength)
	{
		rig_debug(RIG_DEBUG_ERR, "%s: Unable to load valid %s library\n",
				__FUNCTION__, WRG313DLL);
		FreeLibrary(priv->dll);
		FreeLibrary(priv->hWRG313WO);
		FreeLibrary(priv->WinMM);
		free(priv);
		return -RIG_EIO;
	}

	priv->waveOutGetDevCaps=(TwaveOutGetDevCaps)GetProcAddress(priv->WinMM,"waveOutGetDevCapsA");
	priv->waveOutGetNumDevs=(TwaveOutGetNumDevs)GetProcAddress(priv->WinMM,"waveOutGetNumDevs");


	rig->state.priv = (void*)priv;


	return RIG_OK;
}

int g313_findVSC(struct g313_priv_data *priv)
{
 int OutIndex;
 WAVEOUTCAPS Caps;
 int Count;
 int i;

	OutIndex=-1;
	Count=priv->waveOutGetNumDevs();

	for(i=0;i<Count;i++)
	{
		if(priv->waveOutGetDevCaps(i, &Caps, sizeof(Caps))==MMSYSERR_NOERROR)
		{
			if(strncmp(Caps.szPname,"WiNRADiO Virtual Sound Card",27)==0)
			{
				OutIndex=i;
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

	Count=priv->waveOutGetNumDevs();

	if(Count==0)
	{
		return -RIG_EIO;
	}

	if(priv->WaveOutDeviceID==-2)
	{
		id=g313_findVSC(priv);
	}
	else
	{
		id=priv->WaveOutDeviceID;
	}


	/* Open Winradio receiver handle */
	priv->hRadio = priv->OpenRadioDevice(device_num);

	if (priv->hRadio == 0)
	{
		return -RIG_EIO;	/* huh! */
	}

	/* Make sure the receiver is switched on */
	priv->SetPower(priv->hRadio, TRUE);

	if(id>-3)
	{
		priv->hWaveOut=priv->StartWaveOut(priv->hRadio,id);

		if(priv->hWaveOut==NULL)
		{
			priv->CloseRadioDevice(priv->hRadio);
			return -RIG_EIO;
		}
	}
	else
	{
		priv->hWaveOut=NULL;
	}


	priv->Opened=1;

	return RIG_OK;
}

int g313_close(RIG *rig)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;


	if(!priv->Opened)
	{
		return RIG_OK;
	}

	priv->Opened=0;

	if(priv->hWaveOut)
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
		return -RIG_EINVAL;

	priv=(struct g313_priv_data *)rig->state.priv;

	/* Clean up the dll access */
	FreeLibrary(priv->dll);
	FreeLibrary(priv->WinMM);
	FreeLibrary(priv->hWRG313WO);


	if (rig->state.priv)
		free(rig->state.priv);
	rig->state.priv = NULL;

	return RIG_OK;
}

int g313_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	int ret;

	ret = priv->G3SetFrequency(priv->hRadio, (DWORD) (freq));
	ret = ret==TRUE ? RIG_OK : -RIG_EIO;

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

	ret = priv->SetPower(priv->hRadio, status==RIG_POWER_ON ? TRUE : FALSE);
	ret = ret==TRUE ? RIG_OK : -RIG_EIO;

	return ret;
}

int g313_get_powerstat(RIG *rig, powerstat_t *status)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	int ret;

	ret = priv->GetPower(priv->hRadio);
	*status = ret==TRUE ? RIG_POWER_ON : RIG_POWER_OFF;

	return RIG_OK;
}

int g313_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	int ret, agc;

	switch(level) {
	case RIG_LEVEL_ATT:
		ret = priv->SetAtten(priv->hRadio, val.i != 0 ? TRUE : FALSE);
		break;

	case RIG_LEVEL_AGC:
		switch (val.i) {
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
		ret = priv->SetIFGain(priv->hRadio, (int)(val.f*100));
		break;

	default:
		return -RIG_EINVAL;
	}

	ret = ret==TRUE ? RIG_OK : -RIG_EIO;
	return ret;
}

int g313_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	int ret;

	ret = RIG_OK;

	switch(level) {
	case RIG_LEVEL_ATT:
		val->i = priv->GetAtten(priv->hRadio)?rig->caps->attenuator[0]:0;
		break;

	case RIG_LEVEL_AGC:
		switch (priv->GetAGC(priv->hRadio)) {
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
		val->i = priv->GetSignalStrengthdBm(priv->hRadio)/10+73;
		break;

	case RIG_LEVEL_RAWSTR:
		val->i = priv->GetRawSignalStrength(priv->hRadio);
		break;

	default:
		return -RIG_EINVAL;
	}

	return ret;
}


static const char* g313_get_info(RIG *rig)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	static RADIO_INFO info;

	info.bLength = sizeof(RADIO_INFO);

	if (priv->G3GetInfo(priv->hRadio,&info) == FALSE)
		return NULL;

	return info.szSerNum;
}


int g313_set_conf(RIG *rig, token_t token, const char *val)
{
 struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
 int id;

	switch(token)
	{
	    case WAVEOUT_SOUNDCARDID:
		    if (val[0] == '0' && val[1] == 'x')
			    id = strtol(val, (char **)NULL, 16);
		    else
			    id = atoi(val);
			if(id<-3 || id>32)
			{
				return -RIG_EINVAL;
			}

			priv->WaveOutDeviceID=id;

			if(priv->Opened)
			{
				if(id==-2)
				{
					id=g313_findVSC(priv);
				}

				if(priv->hWaveOut)
				{
					priv->StopWaveOut(priv->hWaveOut);
				}

				if(id>-3)
				{
					priv->hWaveOut=priv->StartWaveOut(priv->hRadio,id);
				}
				else
				{
					priv->hWaveOut=NULL;
				}
			}

		    break;
	    default:
		    return -RIG_EINVAL;
	}
	return RIG_OK;
}

int g313_get_conf(RIG *rig, token_t token, char *val)
{
 struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

	switch(token)
	{
		case WAVEOUT_SOUNDCARDID:
			sprintf(val,"%d",priv->WaveOutDeviceID);
			break;
	    default:
		    return -RIG_EINVAL;
	}
	return RIG_OK;
}

/* end _WIN32 */
#else

/* linux, maybe other posix */

#include <time.h>
#include "linradio/wrg313api.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define TOK_SHM_AUDIO 0x150901
#define TOK_SHM_IF 0x150902
#define TOK_SHM_SPECTRUM 0x150903

#define FIFO_PATHNAME_SIZE 64

const struct confparams g313_cfg_params[] = {
	{ TOK_SHM_AUDIO, "audio_path", "audio path name",
			"POSIX shared memory path name to the audio ringbuffer",
			"", RIG_CONF_STRING,
	},
	{ TOK_SHM_IF, "if_path", "I/F path name",
			"POSIX shared memory path name to the I/F ringbuffer",
			"", RIG_CONF_STRING,
	},
	{ TOK_SHM_SPECTRUM, "spectrum_path", "spectrum path name",
			"POSIX shared memory path name to the spectrum ringbuffer",
			"", RIG_CONF_STRING,
	},
	{ RIG_CONF_END, NULL, }
};

struct g313_fifo_data {
	int fd;
	char path[FIFO_PATHNAME_SIZE];
};

struct g313_priv_data {
	int hRadio;
	int Opened;
	struct g313_fifo_data if_buf;
	struct g313_fifo_data audio_buf;
	struct g313_fifo_data spectrum_buf;
};

static void g313_audio_callback(short* buffer, int count, void* arg);
static void g313_if_callback(short* buffer, int count, void* arg);
static void  g313_spectrum_callback(float* buffer, int count, void* arg);

int g313_init(RIG *rig)
{
	struct g313_priv_data *priv;

	priv = (struct g313_priv_data*)malloc(sizeof(struct g313_priv_data));

	memset(priv, 0, sizeof(struct g313_priv_data));

	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	if(!InitAPI())
	{
		rig_debug(RIG_DEBUG_ERR, "%s: Unable to initialise G313 api\n", __FUNCTION__);
		free(priv);
		return -RIG_EIO;
	}
	rig_debug(RIG_DEBUG_VERBOSE, "%s: Initialised G313 API\n", __FUNCTION__);

	rig->state.priv = (void*)priv;

	return RIG_OK;
}

int g313_open(RIG *rig)
{
	int ret;
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	RADIO_DESC *List;
	int Count;

	void* audio_callback = g313_audio_callback;
	void* if_callback = g313_if_callback;
	void* spectrum_callback = g313_spectrum_callback;

	if(priv->Opened)
		return RIG_OK;

	ret = GetDeviceList(&List, &Count);
	if(ret<0 || Count==0)
		return -RIG_EIO;

	/* Open Winradio receiver handle (default to first device?) */

	rig_debug(RIG_DEBUG_VERBOSE, "%s: found %d rigs 0 is %s\n", __FUNCTION__, Count, List[0].Path);

	if(rig->state.rigport.pathname[0])
		priv->hRadio = OpenDevice(rig->state.rigport.pathname);
	else
		priv->hRadio = OpenDevice(List[0].Path);

	DestroyDeviceList(List);


	if (priv->hRadio < 0)
	{
		return -RIG_EIO;	/* huh! */
	}
	rig_debug(RIG_DEBUG_VERBOSE, "%s: Openned G313\n", __FUNCTION__);

	/* Make sure the receiver is switched on */
	SetPower(priv->hRadio, 1);

        priv->audio_buf.fd = open(priv->audio_buf.path, O_WRONLY|O_NONBLOCK);
	rig_debug(RIG_DEBUG_VERBOSE, "%s: audio path %s fifo: %d\n", __FUNCTION__, priv->audio_buf.path, priv->audio_buf.fd);
        if(priv->audio_buf.fd == -1)
                audio_callback = NULL;

        priv->if_buf.fd = open(priv->if_buf.path, O_WRONLY|O_NONBLOCK);
	rig_debug(RIG_DEBUG_VERBOSE, "%s: if path %s fifo: %d\n", __FUNCTION__, priv->if_buf.path, priv->if_buf.fd);
        if(priv->if_buf.fd == -1)
                if_callback = NULL;

        priv->spectrum_buf.fd = open(priv->spectrum_buf.path, O_WRONLY|O_NONBLOCK);
	rig_debug(RIG_DEBUG_VERBOSE, "%s: spectrum path %s fifo: %d\n", __FUNCTION__, priv->spectrum_buf.path, priv->spectrum_buf.fd);
        if(priv->spectrum_buf.fd == -1)
                spectrum_callback = NULL;

	ret = StartStreaming(priv->hRadio, audio_callback, if_callback, spectrum_callback, priv);
	if(ret)
	{
		return -RIG_EIO;
	}
	rig_debug(RIG_DEBUG_VERBOSE, "%s: told G313 to start streaming audio: %d, if: %d, spec: %d\n",
					__FUNCTION__,
					audio_callback?1:0, if_callback?1:0, spectrum_callback?1:0);

	priv->Opened=1;

	return RIG_OK;
}

int g313_close(RIG *rig)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

	if(!priv->Opened)
	{
		return RIG_OK;
	}

	priv->Opened=0;

/*
	rig_debug(RIG_DEBUG_VERBOSE, "%s: stop streaming\n", __FUNCTION__);
	StopStreaming(priv->hRadio);

	req.tv_sec=0;
	req.tv_nsec=500000000L;
	nanosleep(&req, NULL);
*/
	rig_debug(RIG_DEBUG_VERBOSE, "%s: Closing G313\n", __FUNCTION__);
	CloseDevice(priv->hRadio);

	return RIG_OK;
}

int g313_cleanup(RIG *rig)
{
	struct g313_priv_data *priv;

	if (!rig)
		return -RIG_EINVAL;

	priv=(struct g313_priv_data *)rig->state.priv;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: close fifos\n", __FUNCTION__);
	if(priv->audio_buf.fd>=0)
		close(priv->audio_buf.fd);
	if(priv->if_buf.fd>=0)
		close(priv->if_buf.fd);
	if(priv->spectrum_buf.fd)
		close(priv->spectrum_buf.fd);

	if (rig->state.priv)
		free(rig->state.priv);
	rig->state.priv = NULL;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: Uninitialising G313 API\n", __FUNCTION__);
	UninitAPI();

	return RIG_OK;
}

int g313_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	int ret;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: %u\n", __FUNCTION__, (unsigned int)freq);
	ret = SetFrequency(priv->hRadio, (unsigned int) (freq));
	ret = ret==0 ? RIG_OK : -RIG_EIO;

	return ret;
}

int g313_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

	unsigned int f;
	int ret = GetFrequency(priv->hRadio, &f);
	rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d f: %u\n", __FUNCTION__, ret, f);
	if(ret)
		return -RIG_EIO;

	*freq = (freq_t)f;
	return RIG_OK;
}

int g313_set_powerstat(RIG *rig, powerstat_t status)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

	int p = status==RIG_POWER_ON ? 1 : 0;
	int ret = SetPower(priv->hRadio, p);
	rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d state: %d\n", __FUNCTION__, ret, p);

	return ret==0 ? RIG_OK : -RIG_EIO;
}

int g313_get_powerstat(RIG *rig, powerstat_t *status)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

	int p;
	int ret = GetPower(priv->hRadio, &p);
	rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d state: %d\n", __FUNCTION__, ret, p);
	if(ret)
		return -RIG_EIO;

	*status = p ? RIG_POWER_ON : RIG_POWER_OFF;

	return RIG_OK;
}

int g313_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	int ret, agc;

	switch(level) {
	case RIG_LEVEL_ATT:
		ret = SetAttenuator(priv->hRadio, val.i != 0 ? 1 : 0);
		rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Attenuator: %d\n", __FUNCTION__, ret, val.i);
		break;

	case RIG_LEVEL_AGC:
		switch (val.i) {
			case RIG_AGC_OFF: agc = 0; break;
			case RIG_AGC_SLOW: agc = 1; break;
			case RIG_AGC_MEDIUM: agc = 2; break;
			case RIG_AGC_FAST: agc = 3; break;
			default:
				return -RIG_EINVAL;
		}
		ret = SetAGC(priv->hRadio, agc);
		rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d AGC: %d\n", __FUNCTION__, ret, val.i);
		break;

	case RIG_LEVEL_RF:
		ret = SetIFGain(priv->hRadio, (int)(val.f*100));
		rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Gain: %f\n", __FUNCTION__, ret, val.f);
		break;

	default:
		return -RIG_EINVAL;
	}

	return ret==0 ? RIG_OK : -RIG_EIO;
}

int g313_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	int ret;
	int value;
	unsigned int uvalue;
	double dbl;
	unsigned char ch;

	switch(level) {
	case RIG_LEVEL_ATT:
		ret = GetAttenuator(priv->hRadio, &value);
		rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Attenuator: %u\n", __FUNCTION__, ret, value);
		if(ret)
			return -RIG_EIO;
		val->i = value?rig->caps->attenuator[0]:0;
		break;

	case RIG_LEVEL_AGC:
		ret = GetAGC(priv->hRadio, &value);
		rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d AGC: %u\n", __FUNCTION__, ret, value);
		if(ret)
			return -RIG_EIO;

		switch (value) {
			case 0: val->i = RIG_AGC_OFF; break;
			case 1: val->i = RIG_AGC_SLOW; break;
			case 2: val->i = RIG_AGC_MEDIUM; break;
			case 3: val->i = RIG_AGC_FAST; break;
			default:
				return -RIG_EINVAL;
		}
		break;

	case RIG_LEVEL_RF:
		ret = GetIFGain(priv->hRadio, &uvalue);
		rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Gain: %u\n", __FUNCTION__, ret, uvalue);
		if(ret)
			return -RIG_EIO;
		val->f = ((float)uvalue)/100.0;
		break;

	case RIG_LEVEL_STRENGTH:
		ret = GetSignalStrength(priv->hRadio, &dbl);
		rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d sigstr: %f\n", __FUNCTION__, ret, dbl);
		if(ret)
			return -RIG_EIO;
		val->i = ((int)dbl/1.0)+73;
		break;

	case RIG_LEVEL_RAWSTR:
		ret = GetRawSignalStrength(priv->hRadio, &ch);
		rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Raw Sigstr: %u\n", __FUNCTION__, ret, (unsigned int)ch);
		if(ret)
			return -RIG_EIO;
		val->i = ch;
		break;

	default:
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

static const char* g313_get_info(RIG *rig)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	static RADIO_INFO info;
	int ret;

	info.Size = sizeof(RADIO_INFO);

	ret = GetRadioInfo(priv->hRadio,&info);

	if(ret)
		return NULL;

	rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d sernum: %s\n", __FUNCTION__, ret, info.SerNum);
	return info.SerNum;
}

int g313_set_conf(RIG *rig, token_t token, const char *val)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

	size_t len = strlen(val);

	switch(token)
	{
	case TOK_SHM_AUDIO:
			if(len>(FIFO_PATHNAME_SIZE-1))
			{
				rig_debug(RIG_DEBUG_WARN, "%s: set audio_path %s is too long\n", __FUNCTION__, val);
				return -RIG_EINVAL;
			}
			memset(priv->audio_buf.path, 0, FIFO_PATHNAME_SIZE);
			strcpy(priv->audio_buf.path, val);
			rig_debug(RIG_DEBUG_VERBOSE, "%s: set audio_path %s\n", __FUNCTION__, priv->audio_buf.path);
			break;
	case TOK_SHM_IF:
			if(len>(FIFO_PATHNAME_SIZE-1))
			{
				rig_debug(RIG_DEBUG_WARN, "%s: set if_path %s is too long\n", __FUNCTION__, val);
				return -RIG_EINVAL;
			}
			memset(priv->if_buf.path, 0, FIFO_PATHNAME_SIZE);
			strcpy(priv->if_buf.path, val);
			rig_debug(RIG_DEBUG_VERBOSE, "%s: set if_path %s\n", __FUNCTION__, priv->if_buf.path);
			break;
	case TOK_SHM_SPECTRUM:
			if(len>(FIFO_PATHNAME_SIZE-1))
			{
				rig_debug(RIG_DEBUG_WARN, "%s: set spectrum_path %s is too long\n", __FUNCTION__, val);
				return -RIG_EINVAL;
			}
			memset(priv->spectrum_buf.path, 0, FIFO_PATHNAME_SIZE);
			strcpy(priv->spectrum_buf.path, val);
			rig_debug(RIG_DEBUG_VERBOSE, "%s: set spectrum_path %s\n", __FUNCTION__, priv->spectrum_buf.path);
	}
	return RIG_OK;
}

int g313_get_conf(RIG *rig, token_t token, char *val)
{
	struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
	switch(token)
	{
	case TOK_SHM_AUDIO:
			strcpy(val, priv->audio_buf.path);
			break;
	case TOK_SHM_IF:
			strcpy(val, priv->if_buf.path);
			break;
	case TOK_SHM_SPECTRUM:
			strcpy(val, priv->spectrum_buf.path);
	}
	return RIG_OK;
}

/* no need to check return from write - if not all can be written, accept overruns */

static void g313_audio_callback(short* buffer, int count, void* arg)
{
	struct g313_priv_data *priv = (struct g313_priv_data*)arg;
	write(priv->audio_buf.fd, buffer, count*sizeof(short));
}

static void g313_if_callback(short* buffer, int count, void* arg)
{
	struct g313_priv_data *priv = (struct g313_priv_data*)arg;
	write(priv->if_buf.fd, buffer, count*sizeof(short));
}

static void  g313_spectrum_callback(float* buffer, int count, void* arg)
{
	struct g313_priv_data *priv = (struct g313_priv_data*)arg;
	write(priv->spectrum_buf.fd, buffer, count*sizeof(float));
}

const struct rig_caps g313_caps =
{
  .rig_model =      RIG_MODEL_G313,
  .model_name =     "WR-G313",
  .mfg_name =       "Winradio",
  .version =        "0.1",
  .copyright = 	    "LGPL",	/* This wrapper, not the G313 shared library or driver */
  .status =         RIG_STATUS_ALPHA,
  .rig_type =       RIG_TYPE_PCRECEIVER,
  .port_type =      RIG_PORT_NONE,
  .targetable_vfo = 	 0,
  .ptt_type =       RIG_PTT_NONE,
  .dcd_type =       RIG_DCD_NONE,
  .has_get_func =   G313_FUNC,
  .has_set_func =   G313_FUNC,
  .has_get_level =  G313_LEVEL,
  .has_set_level =  RIG_LEVEL_SET(G313_LEVEL),
  .has_get_parm = 	 RIG_PARM_NONE,
  .has_set_parm = 	 RIG_PARM_NONE,
  .ctcss_list = 	 NULL,
  .dcs_list =   	 NULL,
  .chan_list = 	 { RIG_CHAN_END },
  .transceive =     RIG_TRN_OFF,
  .max_ifshift = 	 kHz(2),
  .attenuator =     { 20, RIG_DBLST_END, },	/* TBC */
  .rx_range_list1 =  { {.start = kHz(9),.end = MHz(30),.modes = G313_MODES,
		    	.low_power = -1,.high_power = -1,.vfo = RIG_VFO_A},
		    	RIG_FRNG_END, },
  .tx_range_list1 =  { RIG_FRNG_END, },
  .rx_range_list2 =  { {.start = kHz(9),.end = MHz(30),.modes = G313_MODES,
		    	.low_power = -1,.high_power = -1,.vfo = RIG_VFO_A},
		    	RIG_FRNG_END, },
  .tx_range_list2 =  { RIG_FRNG_END, },

  .tuning_steps =  { {G313_MODES,1},
  		  	RIG_TS_END, },

  .filters =       { {G313_MODES, kHz(12)},
                  RIG_FLT_END, },

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
};

#endif	/* not _WIN32 */
