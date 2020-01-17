/****************************************************************************/
/*  Low-level receiver interface code.                                      */
/*  Copyright (C) 2000 WiNRADiO Communications.                             */
/*                                                                          */
/*  This program is free software; you can redistribute it and/or modify    */
/*  it under the terms of the GNU General Public License as published by    */
/*  the Free Software Foundation; Version 2, June 1991.                     */
/*                                                                          */
/*  This program is distributed in the hope that it will be useful, but     */
/*  WITHOUT ANY WARRANTY; without even the implied warranty of              */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       */
/*  General Public License for more details.                                */
/*                                                                          */
/*  You should have received a copy of the GNU General Public License       */
/*  along with this program; if not, write to the Free Software             */
/*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,  */
/*  USA.                                                                    */
/****************************************************************************/

#ifndef _WRAPI_H_
#define _WRAPI_H_

#ifdef __cplusplus
extern "C" {
#endif

/*  WiNRADiO Information Features (capabilities) */

#define RIF_USVERSION   0x00000001  /*  set if hardware is US version */
#define RIF_DSP			0x00000002	/*  set if DSP is present */
#define RIF_LSBUSB		0x00000004	/*  set if receiver as CW/LSB/USB instead of SSB */
#define RIF_CWIFSHIFT	0x00000008	/*  set if receiver uses IFShift in CW (not BFOOffset) */
#define RIF_AGC			0x00000100	/*  set if receiver supports AGC on/off */
#define	RIF_IFGAIN		0x00000200	/*  set if receiver has manual IF gain control */

/*  WiNRADiO Modes */

#define RMD_CW	0
#define RMD_AM	1
#define RMD_FMN	2
#define RMD_FMW	3
#define RMD_LSB	4
#define RMD_USB	5
#define RMD_FMM 6			/*  50kHz FM */
#define RMD_FM6 7			/*  6kHz FMN */

/*  WiNRADiO Hardware Versions */

#define RHV_1000a	0x0100		/*  older WR-1000 series	 */
#define RHV_1000b	0x010a		/*  current WR-1000 series */
#define RHV_1500	0x0132
#define RHV_1550	0x0137		/*  new WR-1550 receiver */
#define RHV_3000	0x0200		/*  Spectrum Monitor series */
#define RHV_3100	0x020a
#define RHV_3150	0x020f		/*  new WR-3150 receiver */
#define RHV_3200	0x0214
#define RHV_3500	0x0232
#define RHV_3700	0x0246
#define RHV_2000	0x0300

/*  frequency x10 multiplier (ie. 2-20 GHz maximum support) */

#define RFQ_X10		0x80000000L

/*  WiNRADiO Hardware Interfaces */

#define RHI_ISA		0
#define RHI_SERIAL	1

#ifndef FALSE
#define FALSE		0
#endif

#ifndef TRUE
#define TRUE		1
#endif

typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned int		UINT;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef FLOAT               *PFLOAT;
typedef BOOL				*PBOOL;
typedef BOOL             *LPBOOL;
typedef BYTE				*PBYTE;
typedef BYTE             *LPBYTE;
typedef int					*PINT;
typedef int              *LPINT;
typedef UINT				*PUINT;
typedef UINT             *LPUINT;
typedef WORD				*PWORD;
typedef WORD             *LPWORD;
typedef long             *LPLONG;
typedef DWORD				*PDWORD;
typedef DWORD            *LPDWORD;
typedef void             *LPVOID;

typedef int *MODELIST;
typedef  MODELIST *LPMODELIST;

typedef struct _RADIOINFO
{
	DWORD       dwSize;				/*  size of structure (must be set before calling GetRadioDeviceInfo) */
	DWORD       dwFeatures;			/*  bit flags for extra features (RIF_XXX) */
	WORD        wAPIVer;			/*  driver version */
	WORD        wHWVer;				/*  hardware version (RHV_XXX) */
	DWORD       dwMinFreq;			/*  minimum frequency receiver can tune to */
	DWORD       dwMaxFreq;			/*  maximum frequency receiver can tune to */
    int         iFreqRes;			/*  resolution of receiver in Hz  */
    int         iNumModes;			/*  number of modes that can be set  */
    int         iMaxVolume;			/*  maximum volume level  */
    int         iMaxBFO;			/*  maximum BFO offset range (+/- in Hz)  */
    int         iMaxFMScanRate;		/*  maximum scan rate for FM scanning/sweeping */
    int         iMaxAMScanRate;		/*  maximum scan rate for AM scanning/sweeping */
    int         iHWInterface;		/*  physical interface radio is connected to (RHI_XXX) */
	int			iDeviceNum;			/*  logical radio device number	 */
    int         iNumSources;        /*  number of selectable audio sources */
	int         iMaxIFShift;        /*  maximum IF shift */
	DWORD       dwWaveFormats;      /*  bit array of supported rec/play formats (RWF_XXX) */
	int			iDSPSources;		/*  number of selectable DSP input sources */
	LPMODELIST  lpSupportedModes;   /*  list of available modes (length specified by iNumModes) */
	DWORD		dwMaxFreqkHz;		/*  same as dwMaxFreq, but in kHz */
	char		szDeviceName[64];	/*  not used in DOSRADIO */
	int			iMaxIFGain;			/*  the maximum manual IF gain level */
	char descr[80]; /* Description (PB) */
} RADIOINFO, *PRADIOINFO,  *LPRADIOINFO;

int OpenRadioDevice(WORD);
BOOL CloseRadioDevice(int);
int GetRadioDeviceInfo(int, LPRADIOINFO);

int GetSignalStrength(int);

BOOL SetFrequency(int, DWORD);
BOOL SetMode(int, int);
BOOL SetVolume(int, int);
BOOL SetAtten(int, BOOL);
BOOL SetMute(int, BOOL);
BOOL SetPower(int, BOOL);
BOOL SetBFOOffset(int, int);
BOOL SetIFShift(int, int);
BOOL SetAGC(int, BOOL);
BOOL SetIFGain(int, int);

DWORD GetFrequency(int);
int GetMode(int);
int GetMaxVolume(int);
int GetVolume(int);
BOOL GetAtten(int);
BOOL GetMute(int);
BOOL GetPower(int);
int GetBFOOffset(int);
int GetIFShift(int);
BOOL GetAGC(int);
int GetMaxIFGain(int);
int GetIFGain(int);
char *GetDescr(int);

#ifdef __cplusplus
}
#endif

#ifdef __KERNEL__
/* Hooks called when rescheduling */
extern void (*yield_hook)();
extern void (*reenter_hook)();
#endif

#endif 
