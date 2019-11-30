#if (!defined(_WIN32) || !defined(__CYGWIN__))

#include <stdio.h>

#include "config.h"

#ifdef  HAVE_DLFCN_H
#  include <dlfcn.h>
#endif

#include "wrg313api.h"


OPEN_DEVICE OpenDevice = 0;
CLOSE_DEVICE CloseDevice = 0;
SET_POWER SetPower = 0;
GET_POWER GetPower = 0;
SET_FREQUENCY SetFrequency = 0;
GET_FREQUENCY GetFrequency = 0;
GET_RADIO_INFO GetRadioInfo = 0;
GET_RSSI GetRSSI = 0;
GET_AGC GetAGC = 0;
SET_AGC SetAGC = 0;
SET_IF_GAIN SetIFGain = 0;
GET_IF_GAIN GetIFGain = 0;
SET_SOFT_AGC SetSoftAGC = 0;
GET_SOFT_AGC GetSoftAGC = 0;
SET_VOLUME SetVolume = 0;
GET_VOLUME GetVolume = 0;
SET_MODE SetMode = 0;
GET_MODE GetMode = 0;
GET_DEVICE_LIST GetDeviceList = 0;
DESTROY_DEVICE_LIST DestroyDeviceList = 0;
SET_ATTENUATOR SetAttenuator = 0;
GET_ATTENUATOR GetAttenuator = 0;
SET_IF_SHIFT SetIFShift = 0;
GET_IF_SHIFT GetIFShift = 0;
SET_IF_BANDWIDTH SetIFBandwidth = 0;
GET_IF_BANDWIDTH GetIFBandwidth = 0;
START_STREAMING StartStreaming = 0;
STOP_STREAMING StopStreaming = 0;
GET_RAW_SIGNAL_STRENGTH GetRawSignalStrength = 0;
GET_SIGNAL_STRENGTH GetSignalStrength = 0;
IS_DEVICE_CONNECTED IsDeviceConnected = 0;
GET_INTERFACE GetInterface = 0;
SET_CW_TONE SetCWTone = 0;
GET_CW_TONE GetCWTone = 0;
SET_FM_AF_SQUELCH_LEVEL SetFMAFSquelchLevel = 0;
GET_FM_AF_SQUELCH_LEVEL GetFMAFSquelchLevel = 0;
SET_NOTCH_FILTER SetNotchFilter = 0;
GET_NOTCH_FILTER GetNotchFilter = 0;
SET_NOISE_BLANKER SetNoiseBlanker = 0;
GET_NOISE_BLANKER GetNoiseBlanker = 0;
SET_ISB_AUDIO_CHANNEL SetISBAudioChannel = 0;
GET_ISB_AUDIO_CHANNEL GetISBAudioChannel = 0;
LOAD_CALIBRATION_FILE LoadCalibrationFile = 0;
RESET_CALIBRATION ResetCalibration = 0;
GET_API_VERSION GetAPIVersion = 0;

int InitAPI(void *hWRAPI)
{
    if (hWRAPI == NULL)
    {
        return 0;
    }

    GetAPIVersion = (GET_API_VERSION)dlsym(hWRAPI, "GetAPIVersion");

    OpenDevice = (OPEN_DEVICE)dlsym(hWRAPI, "OpenDevice");
    CloseDevice = (CLOSE_DEVICE)dlsym(hWRAPI, "CloseDevice");
    SetPower = (SET_POWER)dlsym(hWRAPI, "SetPower");
    GetPower = (GET_POWER)dlsym(hWRAPI, "GetPower");
    SetFrequency = (SET_FREQUENCY)dlsym(hWRAPI, "SetFrequency");
    GetFrequency = (GET_FREQUENCY)dlsym(hWRAPI, "GetFrequency");
    GetRadioInfo = (GET_RADIO_INFO)dlsym(hWRAPI, "GetRadioInfo");

    GetRSSI = (GET_RSSI)dlsym(hWRAPI, "GetRSSI");
    GetAGC = (GET_AGC)dlsym(hWRAPI, "GetAGC");
    SetAGC = (SET_AGC)dlsym(hWRAPI, "SetAGC");
    SetIFGain = (SET_IF_GAIN)dlsym(hWRAPI, "SetIFGain");
    GetIFGain = (GET_IF_GAIN)dlsym(hWRAPI, "GetIFGain");

    GetDeviceList = (GET_DEVICE_LIST)dlsym(hWRAPI, "GetDeviceList");
    DestroyDeviceList = (DESTROY_DEVICE_LIST)dlsym(hWRAPI, "DestroyDeviceList");

    SetSoftAGC = (SET_SOFT_AGC)dlsym(hWRAPI, "SetSoftAGC");
    GetSoftAGC = (GET_SOFT_AGC)dlsym(hWRAPI, "GetSoftAGC");
    GetVolume = (GET_VOLUME)dlsym(hWRAPI, "GetVolume");
    SetVolume = (SET_VOLUME)dlsym(hWRAPI, "SetVolume");
    SetMode = (SET_MODE)dlsym(hWRAPI, "SetMode");
    GetMode = (GET_MODE)dlsym(hWRAPI, "GetMode");
    SetIFShift = (SET_IF_SHIFT)dlsym(hWRAPI, "SetIFShift");
    GetIFShift = (GET_IF_SHIFT)dlsym(hWRAPI, "GetIFShift");
    SetIFBandwidth = (SET_IF_BANDWIDTH)dlsym(hWRAPI, "SetIFBandwidth");
    GetIFBandwidth = (GET_IF_BANDWIDTH)dlsym(hWRAPI, "GetIFBandwidth");

    StartStreaming = (START_STREAMING)dlsym(hWRAPI, "StartStreaming");
    StopStreaming = (STOP_STREAMING)dlsym(hWRAPI, "StopStreaming");

    SetAttenuator = (SET_ATTENUATOR)dlsym(hWRAPI, "SetAttenuator");
    GetAttenuator = (GET_ATTENUATOR)dlsym(hWRAPI, "GetAttenuator");

    IsDeviceConnected = (IS_DEVICE_CONNECTED)dlsym(hWRAPI, "IsDeviceConnected");

    GetInterface = (GET_INTERFACE)dlsym(hWRAPI, "GetInterface");

    GetRawSignalStrength = (GET_RAW_SIGNAL_STRENGTH)dlsym(hWRAPI,
                           "GetRawSignalStrength");
    GetSignalStrength = (GET_SIGNAL_STRENGTH)dlsym(hWRAPI, "GetSignalStrength");

    SetCWTone = (SET_CW_TONE)dlsym(hWRAPI, "SetCWTone");
    GetCWTone = (GET_CW_TONE)dlsym(hWRAPI, "GetCWTone");

    SetFMAFSquelchLevel = (SET_FM_AF_SQUELCH_LEVEL)dlsym(hWRAPI,
                          "SetFMAFSquelchLevel");
    GetFMAFSquelchLevel = (GET_FM_AF_SQUELCH_LEVEL)dlsym(hWRAPI,
                          "GetFMAFSquelchLevel");

    SetNotchFilter = (SET_NOTCH_FILTER)dlsym(hWRAPI, "SetNotchFilter");
    GetNotchFilter = (GET_NOTCH_FILTER)dlsym(hWRAPI, "GetNotchFilter");

    SetNoiseBlanker = (SET_NOISE_BLANKER)dlsym(hWRAPI, "SetNoiseBlanker");
    GetNoiseBlanker = (GET_NOISE_BLANKER)dlsym(hWRAPI, "GetNoiseBlanker");

    SetISBAudioChannel = (SET_ISB_AUDIO_CHANNEL)dlsym(hWRAPI, "SetISBAudioChannel");
    GetISBAudioChannel = (GET_ISB_AUDIO_CHANNEL)dlsym(hWRAPI, "GetISBAudioChannel");

    LoadCalibrationFile = (LOAD_CALIBRATION_FILE)dlsym(hWRAPI,
                          "LoadCalibrationFile");
    ResetCalibration = (RESET_CALIBRATION)dlsym(hWRAPI, "ResetCalibration");

    if (!GetAPIVersion || !OpenDevice || !CloseDevice || !SetPower || !GetPower
            || !GetFrequency || !SetFrequency ||
            !GetRadioInfo || !GetRSSI || !GetAGC || !SetAGC ||
            !GetIFGain || !SetIFGain || !SetSoftAGC || !GetSoftAGC || !SetVolume
            || !GetVolume || !GetMode ||
            !SetMode || !GetDeviceList || !DestroyDeviceList || !ResetCalibration ||
            !StartStreaming || !StopStreaming || !LoadCalibrationFile ||
            !SetAttenuator || !GetAttenuator || !GetSignalStrength ||
            !SetIFShift || !SetIFBandwidth || !GetIFBandwidth || !GetIFShift ||
            !GetRawSignalStrength || !IsDeviceConnected || !GetInterface ||
            !SetCWTone || !GetCWTone || !SetFMAFSquelchLevel || !GetFMAFSquelchLevel ||
            !SetNotchFilter || !GetNotchFilter || !SetNoiseBlanker || !GetNoiseBlanker ||
            !SetISBAudioChannel || !GetISBAudioChannel)
    {
        return 0;
    }

    return 1;
}

#endif  /* not _WIN32 or __CYGWIN__ */
