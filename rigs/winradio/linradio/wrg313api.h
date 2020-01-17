#ifndef __WRG313_API_H__
#define __WRG313_API_H__

#include <linux/types.h>

#define WRG3APINAME "wrg313api"

#define RADIOMODE_AM    0
#define RADIOMODE_SAM   1
#define RADIOMODE_LSB   2
#define RADIOMODE_USB   3
#define RADIOMODE_DSB   4
#define RADIOMODE_ISB   5
#define RADIOMODE_CW    6
#define RADIOMODE_FMN   7

#define AGC_NONE        0
#define AGC_SLOW        1
#define AGC_MEDIUM      2
#define AGC_FAST        3

#define INTERFACE_PCI 0
#define INTERFACE_USB 1

#pragma pack(push,1)

typedef struct
{
  __u32     Size;
  char      SerNum[9];
  char      Model[9];
  __u32     MinFreq;
  __u32     MaxFreq;
  __u8      NumBands;
  __u32     BandFreq[16];
  __u32     LOFreq;
  __u8      NumVcos;
  __u32     VcoFreq[8];
  __u16     VcoDiv[8];
  __u8      VcoBits[8];
  __u32     RefClk1;
  __u32     RefClk2;
  __u8      IF1DAC[8];
  __s32     AGCstart;
  __s32     AGCmid;
  __s32     AGCend;
  __s32     DropLevel;
  __s32     RSSItop;
  __s32     RSSIref;
  __s32     RxGain;
} RADIO_INFO;


typedef struct
{
    char Path[64];
  __u8      Interface;
    RADIO_INFO Info;
} RADIO_DESC;

#pragma pack(pop)

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*IF_CALLBACK)(__s16 *Buffer,int Count,void *Context);
typedef void (*SPECTRUM_CALLBACK)(float *Spectrum,int Count,void *Context);
typedef void (*AUDIO_CALLBACK)(__s16 *AudioBuffer,int Count,void *UserData);


typedef int (*OPEN_DEVICE)(const char *DeviceName);
typedef void (*CLOSE_DEVICE)(int hRadio);
typedef int (*SET_POWER)(int hRadio,int Power);
typedef int (*GET_POWER)(int hRadio,int *Power);
typedef int (*GET_FREQUENCY)(int hRadio,unsigned int *Frequency);
typedef int (*SET_FREQUENCY)(int hRadio,unsigned int Frequency);
typedef int (*GET_RADIO_INFO)(int hRadio,RADIO_INFO *Info);
typedef int (*GET_RSSI)(int hRadio,int *RSSI);
typedef int (*GET_AGC)(int hRadio,int *AGC);
typedef int (*SET_AGC)(int hRadio,int AGC);
typedef int (*SET_IF_GAIN)(int hRadio,unsigned int Gain);
typedef int (*GET_IF_GAIN)(int hRadio,unsigned int *Gain);
typedef int (*SET_SOFT_AGC)(int hRadio,int AGC);
typedef int (*GET_SOFT_AGC)(int hRadio,int *AGC);
typedef int (*SET_VOLUME)(int hRadio,unsigned int Volume);
typedef int (*GET_VOLUME)(int hRadio,unsigned int *Volume);
typedef int (*SET_MODE)(int hRadio,int Mode);
typedef int (*GET_MODE)(int hRadio,int *Mode);
typedef int (*SET_IF_SHIFT)(int hRadio,int IFShift);
typedef int (*GET_IF_SHIFT)(int hRadio,int *IFShift);
typedef int (*SET_IF_BANDWIDTH)(int hRadio,unsigned int IFBandwidth);
typedef int (*GET_IF_BANDWIDTH)(int hRadio,unsigned int *IFBandwidth);
typedef int (*GET_DEVICE_LIST)(RADIO_DESC **List,int *Count);
typedef void (*DESTROY_DEVICE_LIST)(RADIO_DESC *List);
typedef int (*SET_ATTENUATOR)(int hRadio,int Attennutator);
typedef int (*GET_ATTENUATOR)(int hRadio,int *Attenuator);
typedef int (*START_STREAMING)(int hRadio,AUDIO_CALLBACK AudioCallback,IF_CALLBACK IFCallback,SPECTRUM_CALLBACK SpectrumCallback,void *Context);
typedef int (*STOP_STREAMING)(int hRadio);
typedef int (*GET_RAW_SIGNAL_STRENGTH)(int hRadio,unsigned char *Raw);
typedef int (*GET_SIGNAL_STRENGTH)(int hRadio,double *SignalStrength);
typedef int (*IS_DEVICE_CONNECTED)(int hRadio);
typedef int (*GET_INTERFACE)(int hRadio,int *Interface);
typedef int (*SET_CW_TONE)(int hRadio,unsigned int Frequency);
typedef int (*GET_CW_TONE)(int hRadio,unsigned int *Frequency);
typedef int (*SET_FM_AF_SQUELCH_LEVEL)(int hRadio,unsigned int Level);
typedef int (*GET_FM_AF_SQUELCH_LEVEL)(int hRadio,unsigned int *Level);
typedef int (*SET_NOTCH_FILTER)(int hRadio,int Active,int Frequency,unsigned int Bandwidth);
typedef int (*GET_NOTCH_FILTER)(int hRadio,int *Active,int *Frequency,unsigned int *Bandwidth);
typedef int (*SET_NOISE_BLANKER)(int hRadio,int Active,unsigned int Threshold);
typedef int (*GET_NOISE_BLANKER)(int hRadio,int *Active,unsigned int *Threshold);
typedef int (*SET_ISB_AUDIO_CHANNEL)(int hRadio,unsigned int Channel);
typedef int (*GET_ISB_AUDIO_CHANNEL)(int hRadio,unsigned int *Channel);
typedef int (*LOAD_CALIBRATION_FILE)(int hRadio,const char *Path);
typedef int (*RESET_CALIBRATION)(int hRadio);
typedef unsigned int (*GET_API_VERSION)(void);

#ifdef __cplusplus
};
#endif

extern OPEN_DEVICE OpenDevice;
extern CLOSE_DEVICE CloseDevice;
extern SET_POWER SetPower;
extern GET_POWER GetPower;
extern SET_FREQUENCY SetFrequency;
extern GET_FREQUENCY GetFrequency;
extern GET_RADIO_INFO GetRadioInfo;
extern GET_RSSI GetRSSI;
extern GET_AGC GetAGC;
extern SET_AGC SetAGC;
extern SET_IF_GAIN SetIFGain;
extern GET_IF_GAIN GetIFGain;
extern SET_SOFT_AGC SetSoftAGC;
extern GET_SOFT_AGC GetSoftAGC;
extern SET_VOLUME SetVolume;
extern GET_VOLUME GetVolume;
extern SET_MODE SetMode;
extern GET_MODE GetMode;
extern SET_IF_SHIFT SetIFShift;
extern GET_IF_SHIFT GetIFShift;
extern SET_IF_BANDWIDTH SetIFBandwidth;
extern GET_IF_BANDWIDTH GetIFBandwidth;
extern GET_DEVICE_LIST GetDeviceList;
extern DESTROY_DEVICE_LIST DestroyDeviceList;
extern SET_ATTENUATOR SetAttenuator;
extern GET_ATTENUATOR GetAttenuator;
extern START_STREAMING StartStreaming;
extern STOP_STREAMING StopStreaming;
extern GET_RAW_SIGNAL_STRENGTH GetRawSignalStrength;
extern GET_SIGNAL_STRENGTH GetSignalStrength;
extern IS_DEVICE_CONNECTED IsDeviceConnected;
extern GET_INTERFACE GetInterface;
extern SET_CW_TONE SetCWTone;
extern GET_CW_TONE GetCWTone;
extern SET_FM_AF_SQUELCH_LEVEL SetFMAFSquelchLevel;
extern GET_FM_AF_SQUELCH_LEVEL GetFMAFSquelchLevel;
extern SET_NOTCH_FILTER SetNotchFilter;
extern GET_NOTCH_FILTER GetNotchFilter;
extern SET_NOISE_BLANKER SetNoiseBlanker;
extern GET_NOISE_BLANKER GetNoiseBlanker;
extern SET_ISB_AUDIO_CHANNEL SetISBAudioChannel;
extern GET_ISB_AUDIO_CHANNEL GetISBAudioChannel;
extern LOAD_CALIBRATION_FILE LoadCalibrationFile;
extern RESET_CALIBRATION ResetCalibration;
extern GET_API_VERSION GetAPIVersion;

int InitAPI(void *hWRAPI);
void UninitAPI(void);

#endif
