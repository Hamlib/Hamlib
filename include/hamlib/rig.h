/*
 *  Hamlib Interface - API header
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: rig.h,v 1.49 2001-07-25 21:55:59 f4cfe Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _RIG_H
#define _RIG_H 1

#include <hamlib/riglist.h>	/* list in another file to not mess up w/ this one */
#include <stdio.h>		/* required for FILE definition */
#include <sys/time.h>		/* required for struct timeval */

#if defined(__CYGWIN__) || defined(_WIN32)
#include <windows.h>	/* HANDLE definition */
#endif

/* __BEGIN_DECLS should be used at the beginning of your declarations,
 * so that C++ compilers don't mangle their names.  Use __END_DECLS at
 * the end of C declarations. */
#undef __BEGIN_DECLS
#undef __END_DECLS
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS /* empty */
# define __END_DECLS /* empty */
#endif

/* HAMLIB_PARAMS is a macro used to wrap function prototypes, so that compilers
 * that don't understand ANSI C prototypes still work, and ANSI C
 * compilers can issue warnings about type mismatches. */
#undef HAMLIB_PARAMS
#if defined (__STDC__) || defined (_AIX) || (defined (__mips) && defined (_SYSTYPE_SVR4)) || defined(__CYGWIN__) || defined(__cplusplus)
# define HAMLIB_PARAMS(protos) protos
# define rig_ptr_t     void*
#else
# define HAMLIB_PARAMS(protos) ()
# define rig_ptr_t     char*
#endif

#include <hamlib/rig_dll.h>


__BEGIN_DECLS

extern HAMLIB_EXPORT_VAR(const char) hamlib_version[];
extern HAMLIB_EXPORT_VAR(const char) hamlib_copyright[];

typedef unsigned int tone_t;

extern HAMLIB_EXPORT_VAR(const tone_t) full_ctcss_list[];
extern HAMLIB_EXPORT_VAR(const tone_t) common_ctcss_list[];
extern HAMLIB_EXPORT_VAR(const tone_t) full_dcs_list[];
#define CTCSS_LIST_SIZE (sizeof(full_ctcss_list)/sizeof(tone_t)-1)
#define DCS_LIST_SIZE (sizeof(full_dcs_list)/sizeof(tone_t)-1)

/*
 * Error codes that can be returned by the Hamlib functions
 */
#define RIG_OK			0			/* completed sucessfully */
#define RIG_EINVAL		1			/* invalid parameter */
#define RIG_ECONF		2			/* invalid configuration (serial,..) */
#define RIG_ENOMEM		3			/* memory shortage */
#define RIG_ENIMPL		4			/* function not implemented, but will be */
#define RIG_ETIMEOUT	5			/* communication timed out */
#define RIG_EIO			6			/* IO error, including open failed */
#define RIG_EINTERNAL	7			/* Internal Hamlib error, huh! */
#define RIG_EPROTO		8			/* Protocol error */
#define RIG_ERJCTED		9			/* Command rejected by the rig */
#define RIG_ETRUNC 		10			/* Command performed, but arg truncated */
#define RIG_ENAVAIL		11			/* function not available */
#define RIG_ENTARGET	12			/* VFO not targetable */


/* Forward struct references */

struct rig;
struct rig_state;

typedef struct rig RIG;

#define RIGNAMSIZ 30
#define RIGVERSIZ 8
#define FILPATHLEN 100
#define FRQRANGESIZ 30
#define MAXCHANDESC 30		/* describe channel eg: "WWV 5Mhz" */
#define TSLSTSIZ 20			/* max tuning step list size, zero ended */
#define FLTLSTSIZ 16		/* max mode/filter list size, zero ended */
#define MAXDBLSTSIZ 8		/* max preamp/att levels supported, zero ended */
#define CHANLSTSIZ 16		/* max mem_list size, zero ended */


/*
 * REM: Numeric order matters for debug level
 */
enum rig_debug_level_e {
	RIG_DEBUG_NONE = 0,
	RIG_DEBUG_BUG,
	RIG_DEBUG_ERR,
	RIG_DEBUG_WARN,
	RIG_DEBUG_VERBOSE,
	RIG_DEBUG_TRACE
};

/* 
 * Rig capabilities
 *
 * Basic rig type, can store some useful
 * info about different radios. Each lib must
 * be able to populate this structure, so we can make
 * useful enquiries about capablilities.
 */



enum rig_port_e {
	RIG_PORT_NONE = 0,	/* as bizarre as it could be :) */
	RIG_PORT_SERIAL,
	RIG_PORT_NETWORK,
	RIG_PORT_DEVICE,	/* Device driver, like the WiNRADiO */
	RIG_PORT_PACKET,	/* e.g. SV8CS */
	RIG_PORT_DTMF,		/* bridge via another rig, eg. Kenwood Sky Cmd System */
	RIG_PORT_ULTRA		/* IrDA Ultra protocol! */
};

enum serial_parity_e {
	RIG_PARITY_NONE = 0,
	RIG_PARITY_ODD,
	RIG_PARITY_EVEN
};

enum serial_handshake_e {
	RIG_HANDSHAKE_NONE = 0,
	RIG_HANDSHAKE_XONXOFF,
	RIG_HANDSHAKE_HARDWARE
};


enum rig_type_e {
	RIG_TYPE_TRANSCEIVER = 0,	/* aka base station */
	RIG_TYPE_HANDHELD,
	RIG_TYPE_MOBILE,
	RIG_TYPE_RECEIVER,
	RIG_TYPE_PCRECEIVER,
	RIG_TYPE_SCANNER,
	RIG_TYPE_TRUNKSCANNER,
	RIG_TYPE_COMPUTER,		/* eg. Pegasus */
	/* etc. */
	RIG_TYPE_OTHER
};

/*
 * Development status of the backend 
 */
enum rig_status_e {
	RIG_STATUS_ALPHA = 0,
	RIG_STATUS_UNTESTED,	/* written from specs, rig unavailable for test, feedback most wanted! */
	RIG_STATUS_BETA,
	RIG_STATUS_STABLE,
	RIG_STATUS_BUGGY,		/* was stable, but something broke it! */
	RIG_STATUS_NEW
};

enum rptr_shift_e {
	RIG_RPT_SHIFT_NONE = 0,
	RIG_RPT_SHIFT_MINUS,
	RIG_RPT_SHIFT_PLUS
};

typedef enum rptr_shift_e rptr_shift_t;

enum split_e {
	RIG_SPLIT_OFF = 0,
	RIG_SPLIT_ON,
};

typedef enum split_e split_t;

/*
 * freq_t: frequency type in Hz, must be >32bits for SHF! 
 * shortfreq_t: frequency on 31bits, suitable for offsets, shifts, etc..
 */
typedef long long freq_t;
typedef signed long shortfreq_t;

#define Hz(f)	((freq_t)(f))
#define kHz(f)	((freq_t)((f)*1000))
#define MHz(f)	((freq_t)((f)*1000000L))
#define GHz(f)	((freq_t)((f)*1000000000LL))

#define RIG_FREQ_NONE Hz(0)


#define RIG_VFO_CURR    0       /* current "tunable channel"/VFO */
#define RIG_VFO_NONE    0       /* used in caps */
#define RIG_VFO_ALL		-1		/* apply to all VFO (when used as target) */

/*
 * Or should it be better designated 
 * as a "tunable channel" (RIG_CTRL_MEM) ? --SF
 */
#define RIG_VFO_MEM		-2		/* means Memory mode, to be used with set_vfo */
#define RIG_VFO_VFO		-3		/* means (any)VFO mode, with set_vfo */

#define RIG_VFO1 (1<<0)
#define RIG_VFO2 (1<<1)
#define RIG_CTRL_MAIN 1
#define RIG_CTRL_SUB 2

/*
 * one byte per "tunable channel":
 *
 *    MSB           LSB
 *     8             1
 *    +-+-+-+-+-+-+-+-+
 *    | |             |
 *    CTL     VFO
 */

/* How to call it? "tunable channel"? Control band? */
#define RIG_CTRL_BAND(band,vfo) ( (0x80<<(8*((band)-1))) | ((vfo)<<(8*((band)-1))) )

				 /* macros */
#define RIG_VFO_A (RIG_CTRL_BAND(RIG_CTRL_MAIN, RIG_VFO1))
#define RIG_VFO_B (RIG_CTRL_BAND(RIG_CTRL_MAIN, RIG_VFO2))
#define RIG_VFO_C (RIG_CTRL_BAND(RIG_CTRL_SUB, RIG_VFO1))
#define RIG_VFO_MAIN (RIG_CTRL_BAND(RIG_CTRL_MAIN, RIG_VFO_CURR))
#define RIG_VFO_SUB  (RIG_CTRL_BAND(RIG_CTRL_SUB, RIG_VFO_CURR))

/*
 * could RIG_VFO_ALL be useful?
 * i.e. apply to all VFO, when used as target
 */
typedef int vfo_t;


#define RIG_PASSBAND_NORMAL Hz(0)
/* 
 * also see rig_passband_normal(rig,mode),
 * 	rig_passband_narrow(rig,mode) and rig_passband_wide(rig,mode)
 */
typedef shortfreq_t pbwidth_t;


enum dcd_e {
	RIG_DCD_OFF = 0,	/* squelch closed */
	RIG_DCD_ON			/* squelch open */
};

typedef enum dcd_e dcd_t;

enum dcd_type_e {
	RIG_DCD_NONE = 0,			/* not available */
	RIG_DCD_RIG,				/* i.e. has get_dcd cap */
	RIG_DCD_SERIAL_DSR,
	RIG_DCD_SERIAL_CTS,
	RIG_DCD_PARALLEL,			/* DCD comes from ?? DATA1? STROBE? */
#ifdef TODO_MORE_DCD
	RIG_DCD_PTT			/* follow ptt_type, i.e. ptt=RTS -> dcd=CTS on same line --SF */
#endif
};

typedef enum dcd_type_e dcd_type_t;


enum ptt_e {
	RIG_PTT_OFF = 0,
	RIG_PTT_ON
};

typedef enum ptt_e ptt_t;

enum ptt_type_e {
	RIG_PTT_NONE = 0,			/* not available */
	RIG_PTT_RIG,				/* legacy PTT */
	RIG_PTT_SERIAL_DTR,
	RIG_PTT_SERIAL_RTS,
	RIG_PTT_PARALLEL			/* PTT accessed through DATA0 */
};

typedef enum ptt_type_e ptt_type_t;

enum powerstat_e {
		RIG_POWER_OFF = 0,
		RIG_POWER_ON,
		RIG_POWER_STANDBY
};
typedef enum powerstat_e powerstat_t;

enum reset_e {
		RIG_RESET_NONE = 0,
		RIG_RESET_SOFT,
		RIG_RESET_VFO,
		RIG_RESET_MCALL,		/* memory clear */
		RIG_RESET_MASTER,
};
typedef enum reset_e reset_t;


/* VFO/MEM mode are set by set_vfo */
#define RIG_OP_NONE	0
#define RIG_OP_CPY		(1<<0)		/* VFO A = VFO B */
#define RIG_OP_XCHG		(1<<1)		/* Exchange VFO A/B */
#define RIG_OP_FROM_VFO	(1<<2)		/* VFO->MEM */
#define RIG_OP_TO_VFO	(1<<3)		/* MEM->VFO */
#define RIG_OP_MCL		(1<<4)		/* Memory clear */
#define RIG_OP_UP		(1<<5)		/* UP */
#define RIG_OP_DOWN		(1<<6)		/* DOWN */
#define RIG_OP_BAND_UP		(1<<7)		/* Band UP */
#define RIG_OP_BAND_DOWN	(1<<8)		/* Band DOWN */

/* 
 * RIG_MVOP_DUAL_ON/RIG_MVOP_DUAL_OFF (Dual watch off/Dual watch on)
 * better be set by set_func IMHO, 
 * or is it the balance (-> set_level) ? --SF
 */

typedef long vfo_op_t;



#define RIG_SCAN_NONE	0L
#define RIG_SCAN_STOP	RIG_SCAN_NONE
#define RIG_SCAN_MEM	(1L<<0)		/* Scan all memory channels */
#define RIG_SCAN_SLCT	(1L<<1)		/* Scan all selected memory channels */
#define RIG_SCAN_PRIO	(1L<<2)		/* Priority watch (mem or call channel) */
#define RIG_SCAN_PROG	(1L<<3)		/* Programmed(edge) scan */
#define RIG_SCAN_DELTA	(1L<<4)		/* delta-f scan */

typedef long scan_t;

/*
 * configuration token
 */
typedef long token_t;

#define RIG_CONF_END 0

#define RIG_TOKEN_BACKEND(t) (t)
#define RIG_TOKEN_FRONTEND(t) ((t)|(1<<30))
#define RIG_IS_TOKEN_FRONTEND(t) ((t)&(1<<30))

/*
 * strongly inspired from soundmedem. Thanks Thomas!
 */
#define RIG_CONF_STRING      0
#define RIG_CONF_COMBO       1
#define RIG_CONF_NUMERIC     2
#define RIG_CONF_CHECKBUTTON 3

struct confparams {
		token_t token;
		const char *name;	/* try to avoid spaces in the name */
		const char *label;
		const char *tooltip;
        const char *dflt;
		unsigned int type;
		union {
			struct {
				float min;
				float max;
				float step;
			} n;
			struct {
				const char *combostr[8];
			} c;
		} u;
};

/*
 * When optional speech synthesizer is installed 
 * what about RIG_ANN_ENG and RIG_ANN_JAPAN? and RIG_ANN_CW?
 */

#define RIG_ANN_NONE	0
#define RIG_ANN_OFF 	RIG_ANN_NONE
#define RIG_ANN_FREQ	(1<<0)
#define RIG_ANN_RXMODE	(1<<1)
#define RIG_ANN_ALL 	(1<<2)
typedef long ann_t;


/* Antenna number */
typedef int ant_t;

enum agc_level_e {
		RIG_AGC_OFF = 0,
		RIG_AGC_SUPERFAST,
		RIG_AGC_FAST,
		RIG_AGC_SLOW
};

enum meter_level_e {
		RIG_METER_NONE = 0,
		RIG_METER_SWR,
		RIG_METER_COMP,
		RIG_METER_ALC,
		RIG_METER_IC,
		RIG_METER_DB,
};

/*
 * Universal approach for use by set_level/get_level
 */
union value_u {
		signed int i;
		float f;
};
typedef union value_u value_t;

#define RIG_LEVEL_NONE		0ULL
#define RIG_LEVEL_PREAMP	(1<<0)	/* Preamp, arg int (dB) */
#define RIG_LEVEL_ATT		(1<<1)	/* Attenuator, arg int (dB) */
#define RIG_LEVEL_VOX		(1<<2)	/* VOX delay, arg int (tenth of seconds) */
#define RIG_LEVEL_AF		(1<<3)	/* Volume, arg float [0.0..1.0] */
#define RIG_LEVEL_RF		(1<<4)	/* RF gain (not TX power), arg float [0.0..1.0] or in dB ?? -20..20 ?*/
#define RIG_LEVEL_SQL		(1<<5)	/* Squelch, arg float [0.0 .. 1.0] */
#define RIG_LEVEL_IF		(1<<6)	/* IF, arg int (Hz) */
#define RIG_LEVEL_APF		(1<<7)	/* APF, arg float [0.0 .. 1.0] */
#define RIG_LEVEL_NR		(1<<8)	/* Noise Reduction, arg float [0.0 .. 1.0] */
#define RIG_LEVEL_PBT_IN	(1<<9)	/* Twin PBT (inside), arg float [0.0 .. 1.0] */
#define RIG_LEVEL_PBT_OUT	(1<<10)	/* Twin PBT (outside), arg float [0.0 .. 1.0] */
#define RIG_LEVEL_CWPITCH	(1<<11)	/* CW pitch, arg int (Hz) */
#define RIG_LEVEL_RFPOWER	(1<<12)	/* RF Power, arg float [0.0 .. 1.0] */
#define RIG_LEVEL_MICGAIN	(1<<13)	/* MIC Gain, arg float [0.0 .. 1.0] */
#define RIG_LEVEL_KEYSPD	(1<<14)	/* Key Speed, arg int (WPM) */
#define RIG_LEVEL_NOTCHF	(1<<15)	/* Notch Freq., arg int (Hz) */
#define RIG_LEVEL_COMP		(1<<16)	/* Compressor, arg float [0.0 .. 1.0] */
#define RIG_LEVEL_AGC		(1<<17)	/* AGC, arg int (see enum agc_level_e) */
#define RIG_LEVEL_BKINDL	(1<<18)	/* BKin Delay, arg int (tenth of dots) */
#define RIG_LEVEL_BALANCE	(1<<19)	/* Balance (Dual Watch), arg float [0.0 .. 1.0] */
#define RIG_LEVEL_METER		(1<<20)	/* Display meter, arg int (see enum meter_level_e) */

		/* These ones are not settable */
#define RIG_LEVEL_SQLSTAT	(1<<27)	/* SQL status, arg int (open=1/closed=0). Deprecated, use get_dcd instead */
#define RIG_LEVEL_SWR		(1<<28)	/* SWR, arg float */
#define RIG_LEVEL_ALC		(1<<29)	/* ALC, arg float */
#define RIG_LEVEL_STRENGTH	(1<<30)	/* Signal strength, arg int (dB) */

#define RIG_LEVEL_FLOAT_LIST (RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_APF|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_COMP|RIG_LEVEL_BALANCE|RIG_LEVEL_SWR|RIG_LEVEL_ALC)

#define RIG_LEVEL_READONLY_LIST (RIG_LEVEL_SQLSTAT|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_STRENGTH)

#define RIG_LEVEL_IS_FLOAT(l) ((l)&RIG_LEVEL_FLOAT_LIST)
#define RIG_LEVEL_SET(l) ((l)&~RIG_LEVEL_READONLY_LIST)


/*
 * Parameters are settings that are not VFO specific
 */
#define RIG_PARM_NONE		0
#define RIG_PARM_ANN		(1<<0)	/* "Announce" level, see ann_t */
#define RIG_PARM_APO		(1<<1)	/* Auto power off, int in minute */
#define RIG_PARM_BACKLIGHT	(1<<2)	/* LCD light, float [0.0..1.0] */
#define RIG_PARM_BEEP		(1<<4)	/* Beep on keypressed, int (0,1) */
#define RIG_PARM_TIME		(1<<5)	/* hh:mm:ss, int in seconds from 00:00:00 */
#define RIG_PARM_BAT		(1<<6)	/* battery level, float [0.0..1.0] */

#define RIG_PARM_FLOAT_LIST (RIG_PARM_BACKLIGHT|RIG_PARM_BAT)
#define RIG_PARM_READONLY_LIST (RIG_PARM_BAT)

#define RIG_PARM_IS_FLOAT(l) ((l)&RIG_PARM_FLOAT_LIST)
#define RIG_PARM_SET(l) ((l)&~RIG_PARM_READONLY_LIST)

#define RIG_SETTING_MAX 64
typedef unsigned long long setting_t;	/* hope 64 bits will be enough.. */

/*
 * tranceive mode, ie. the rig notify the host of any event,
 * like freq changed, mode changed, etc.
 */
#define	RIG_TRN_OFF 0
#define	RIG_TRN_RIG 1
#define	RIG_TRN_POLL 2


/*
 * These are activated functions.
 */
#define RIG_FUNC_NONE    	0
#define RIG_FUNC_FAGC    	(1<<0)		/* Fast AGC */
#define RIG_FUNC_NB	    	(1<<1)		/* Noise Blanker */
#define RIG_FUNC_COMP    	(1<<2)		/* Compression */
#define RIG_FUNC_VOX    	(1<<3)		/* VOX */
#define RIG_FUNC_TONE    	(1<<4)		/* Tone */
#define RIG_FUNC_TSQL    	(1<<5)		/* may require a tone field */
#define RIG_FUNC_SBKIN    	(1<<6)		/* Semi Break-in */
#define RIG_FUNC_FBKIN    	(1<<7)		/* Full Break-in, for CW mode */
#define RIG_FUNC_ANF    	(1<<8)		/* Automatic Notch Filter (DSP) */
#define RIG_FUNC_NR     	(1<<9)		/* Noise Reduction (DSP) */
#define RIG_FUNC_AIP     	(1<<10)		/* AIP (Kenwood) */
#define RIG_FUNC_APF     	(1<<11)		/* Auto Passband Filter */
#define RIG_FUNC_MON     	(1<<12)		/* Monitor transmitted signal, != rev */
#define RIG_FUNC_MN     	(1<<13)		/* Manual Notch (Icom) */
#define RIG_FUNC_RNF     	(1<<14)		/* RTTY Filter Notch (Icom) */
#define RIG_FUNC_ARO     	(1<<15)		/* Auto Repeater Offset */
#define RIG_FUNC_LOCK     	(1<<16)		/* Lock */
#define RIG_FUNC_MUTE     	(1<<17)		/* Mute, could be emulated by LEVEL_AF*/
#define RIG_FUNC_VSC     	(1<<18)		/* Voice Scan Control */
#define RIG_FUNC_REV     	(1<<19)		/* Reverse tx and rx freqs */
#define RIG_FUNC_SQL     	(1<<20)		/* Turn Squelch Monitor on/off*/
#define RIG_FUNC_ABM     	(1<<21)		/* Auto Band Mode */
#define RIG_FUNC_BC     	(1<<22)		/* Beat Canceller */
#define RIG_FUNC_MBC     	(1<<23)		/* Manual Beat Canceller */


/*
 * power unit macros, converts to mW
 * This is limited to 2MW on 32 bits systems.
 */
#define mW(p)	 ((int)(p))
#define Watts(p) ((int)((p)*1000))
#define W(p)	 Watts(p)
#define kW(p)	 ((int)((p)*1000000L))

typedef unsigned int rmode_t;	/* radio mode  */

/*
 * Do not use an enum since this will be used w/ rig_mode_t bit fields.
 * Also, how should CW reverse sideband and RTTY reverse
 * sideband be handled?
 * */
#define RIG_MODE_NONE  	0
#define RIG_MODE_AM    	(1<<0)
#define RIG_MODE_CW    	(1<<1)
#define RIG_MODE_USB	(1<<2)	 /* select somewhere else the filters ? */
#define RIG_MODE_LSB	(1<<3)
#define RIG_MODE_RTTY	(1<<4)
#define RIG_MODE_FM    	(1<<5)
#define RIG_MODE_WFM   	(1<<6)	/* after all, Wide FM is a mode on its own */

/* macro for backends, no to be used by rig_set_mode et al. */
#define RIG_MODE_SSB  	(RIG_MODE_USB|RIG_MODE_LSB)


#define RIG_DBLST_END 0		/* end marker in a preamp/att level list */
#define RIG_IS_DBLST_END(d) ((d)==0)

/*
 * Put together a bunch of this struct in an array to define 
 * what your rig have access to 
 */ 
struct freq_range_list {
  freq_t start;
  freq_t end;
  rmode_t modes;	/* bitwise OR'ed RIG_MODE_* */
  int low_power;	/* in mW, -1 for no power (ie. rx list) */
  int high_power;	/* in mW, -1 for no power (ie. rx list) */
  vfo_t vfo;		/* VFOs that can access this range */
  ant_t ant;
};
typedef struct freq_range_list freq_range_t;

#define RIG_FRNG_END     {Hz(0),Hz(0),RIG_MODE_NONE,0,0,RIG_VFO_NONE}
#define RIG_IS_FRNG_END(r) ((r).start == Hz(0) && (r).end == Hz(0))

#define RIG_ITU_REGION1 1
#define RIG_ITU_REGION2 2
#define RIG_ITU_REGION3 3

/*
 * Lists the tuning steps available for each mode
 */
struct tuning_step_list {
  rmode_t modes;	/* bitwise OR'ed RIG_MODE_* */
  shortfreq_t ts;		/* tuning step in Hz */
};

#define RIG_TS_END     {RIG_MODE_NONE,0}
#define RIG_IS_TS_END(t)	((t).modes == RIG_MODE_NONE && (t).ts == 0)

/*
 * Lists the filters available for each mode
 * If more than one filter is available for a given mode,
 *  the first entry in the array will be the default
 *  filter to use for this mode (cf rig_set_mode).
 */
struct filter_list {
  rmode_t modes;	/* bitwise OR'ed RIG_MODE_* */
  shortfreq_t width;		/* passband width in Hz */
};

#define RIG_FLT_END     {RIG_MODE_NONE,0}
#define RIG_IS_FLT_END(f)	((f).modes == RIG_MODE_NONE)


/* 
 * Convenience struct, describes a freq/vfo/mode combo 
 * Also useful for memory handling -- FS
 *
 * TODO: skip flag, split, shift, etc.
 */

struct channel {
  int channel_num;
  freq_t freq;
  rmode_t mode;
  pbwidth_t width;
  freq_t tx_freq;
  rmode_t tx_mode;
  pbwidth_t tx_width;
  split_t split;
  rptr_shift_t rptr_shift;
  shortfreq_t rptr_offs;
  vfo_t vfo;

  int ant;	/* antenna number */
  shortfreq_t tuning_step;
  shortfreq_t rit;
  shortfreq_t xit;
  setting_t funcs;
  value_t levels[RIG_SETTING_MAX];
  tone_t ctcss_tone;
  tone_t ctcss_sql;
  tone_t dcs_code;
  tone_t dcs_sql;
  char channel_desc[MAXCHANDESC];
};

typedef struct channel channel_t;

/* 
 * chan_t is used to describe what memory your rig is equipped with
 * cf. chan_list field in caps
 * Example for the Ic706MkIIG (99 memory channels, 2 scan edges, 2 call chans):
 * 	chan_t chan_list[] = {
 * 		{ 1, 99, RIG_MTYPE_MEM, 0 },
 * 		{ 100, 103, RIG_MTYPE_EDGE, 0 },
 * 		{ 104, 105, RIG_MTYPE_CALL, 0 },
 * 		RIG_CHAN_END
 * 	}
 */
enum chan_type_e {
		RIG_MTYPE_NONE=0,
		RIG_MTYPE_MEM,			/* regular */
		RIG_MTYPE_EDGE,			/* scan edge */
		RIG_MTYPE_CALL,			/* call channel */
		RIG_MTYPE_MEMOPAD,		/* inaccessible on Icom, what about others? */
		RIG_MTYPE_SAT			/* satellite */
};

struct chan_list {
		int start;			/* rig memory channel _number_ */
		int end;
		enum chan_type_e type;	/* among EDGE, MEM, CALL, .. */
		int reserved;			/* don't know yet, maybe smthing like flags */
};

#define RIG_CHAN_END     {0,0,RIG_MTYPE_NONE,0}
#define RIG_IS_CHAN_END(c)	((c).type == RIG_MTYPE_NONE)

typedef struct chan_list chan_t;


/* Basic rig type, can store some useful
* info about different radios. Each lib must
* be able to populate this structure, so we can make
* useful enquiries about capablilities.
*/

/* 
 * The main idea of this struct is that it will be defined by the backend
 * rig driver, and will remain readonly for the application.
 * Fields that need to be modifiable by the application are
 * copied into the struct rig_state, which is a kind of private
 * of the RIG instance.
 * This way, you can have several rigs running within the same application,
 * sharing the struct rig_caps of the backend, while keeping their own
 * customized data.
 * NB: don't move fields around, as backend depends on it when initializing
 * 		their caps.
 */
struct rig_caps {
  rig_model_t rig_model;
  const char *model_name;
  const char *mfg_name;
  const char *version;
  const char *copyright;
  enum rig_status_e status;

  enum rig_type_e rig_type;
  enum ptt_type_e ptt_type;
  enum dcd_type_e dcd_type;
  enum rig_port_e port_type;

  int serial_rate_min;
  int serial_rate_max;
  int serial_data_bits;
  int serial_stop_bits;
  enum serial_parity_e serial_parity;
  enum serial_handshake_e serial_handshake;

  int write_delay;
  int post_write_delay;
  int timeout;
  int retry;

  setting_t has_get_func;
  setting_t has_set_func;
  setting_t has_get_level;
  setting_t has_set_level;
  setting_t has_get_parm;
  setting_t has_set_parm;

  int level_gran[RIG_SETTING_MAX];
  int parm_gran[RIG_SETTING_MAX];

  const tone_t *ctcss_list;
  const tone_t *dcs_list;

  int preamp[MAXDBLSTSIZ];
  int attenuator[MAXDBLSTSIZ];

  shortfreq_t max_rit;
  shortfreq_t max_xit;
  shortfreq_t max_ifshift;

  ann_t announces;
  vfo_op_t vfo_ops;
  scan_t scan_ops;
  int targetable_vfo;
  int transceive;

  int bank_qty;
  int chan_desc_sz;

  chan_t chan_list[CHANLSTSIZ];

  freq_range_t rx_range_list1[FRQRANGESIZ];	/* ITU region 1 */
  freq_range_t tx_range_list1[FRQRANGESIZ];
  freq_range_t rx_range_list2[FRQRANGESIZ];	/* ITU region 2 */
  freq_range_t tx_range_list2[FRQRANGESIZ];
  struct tuning_step_list tuning_steps[TSLSTSIZ];

  struct filter_list filters[FLTLSTSIZ];	/* mode/filter table, at -6dB */

  const struct confparams *cfgparams;
  const rig_ptr_t priv;

  /*
   * Rig Admin API
   *
   */
 
  int (*rig_init)(RIG *rig);
  int (*rig_cleanup)(RIG *rig);
  int (*rig_open)(RIG *rig);
  int (*rig_close)(RIG *rig);
  
  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */
  
  int (*set_freq)(RIG *rig, vfo_t vfo, freq_t freq);
  int (*get_freq)(RIG *rig, vfo_t vfo, freq_t *freq);

  int (*set_mode)(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
  int (*get_mode)(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

  int (*set_vfo)(RIG *rig, vfo_t vfo);
  int (*get_vfo)(RIG *rig, vfo_t *vfo);

  int (*set_ptt)(RIG *rig, vfo_t vfo, ptt_t ptt);
  int (*get_ptt)(RIG *rig, vfo_t vfo, ptt_t *ptt);
  int (*get_dcd)(RIG *rig, vfo_t vfo, dcd_t *dcd);

  int (*set_rptr_shift)(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
  int (*get_rptr_shift)(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift);

  int (*set_rptr_offs)(RIG *rig, vfo_t vfo, shortfreq_t offs);
  int (*get_rptr_offs)(RIG *rig, vfo_t vfo, shortfreq_t *offs);

  int (*set_split_freq)(RIG *rig, vfo_t vfo, freq_t tx_freq);
  int (*get_split_freq)(RIG *rig, vfo_t vfo, freq_t *tx_freq);
  int (*set_split_mode)(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width);
  int (*get_split_mode)(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width);
  int (*set_split)(RIG *rig, vfo_t vfo, split_t split);
  int (*get_split)(RIG *rig, vfo_t vfo, split_t *split);

  int (*set_rit)(RIG *rig, vfo_t vfo, shortfreq_t rit);
  int (*get_rit)(RIG *rig, vfo_t vfo, shortfreq_t *rit);
  int (*set_xit)(RIG *rig, vfo_t vfo, shortfreq_t xit);
  int (*get_xit)(RIG *rig, vfo_t vfo, shortfreq_t *xit);

  int (*set_ts)(RIG *rig, vfo_t vfo, shortfreq_t ts);
  int (*get_ts)(RIG *rig, vfo_t vfo, shortfreq_t *ts);

  int (*set_dcs_code)(RIG *rig, vfo_t vfo, tone_t code);
  int (*get_dcs_code)(RIG *rig, vfo_t vfo, tone_t *code);
  int (*set_ctcss_tone)(RIG *rig, vfo_t vfo, tone_t tone);
  int (*get_ctcss_tone)(RIG *rig, vfo_t vfo, tone_t *tone);

  int (*set_dcs_sql)(RIG *rig, vfo_t vfo, tone_t code);
  int (*get_dcs_sql)(RIG *rig, vfo_t vfo, tone_t *code);
  int (*set_ctcss_sql)(RIG *rig, vfo_t vfo, tone_t tone);
  int (*get_ctcss_sql)(RIG *rig, vfo_t vfo, tone_t *tone);

  /*
   * It'd be nice to have a power2mW and mW2power functions
   * that could tell at what power (watts) the rig is running.
   * Unfortunately, on most rigs, the formula is not the same
   * on all bands/modes. Have to work this out.. --SF
   */
  int (*power2mW)(RIG *rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode);
  int (*mW2power)(RIG *rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode);

  int (*set_powerstat)(RIG *rig, powerstat_t status);
  int (*get_powerstat)(RIG *rig, powerstat_t *status);
  int (*reset)(RIG *rig, reset_t reset);

  int (*set_ant)(RIG *rig, vfo_t vfo, ant_t ant);
  int (*get_ant)(RIG *rig, vfo_t vfo, ant_t *ant);

  int (*set_level)(RIG *rig, vfo_t vfo, setting_t level, value_t val);
  int (*get_level)(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

  int (*set_func)(RIG *rig, vfo_t vfo, setting_t func, int status);
  int (*get_func)(RIG *rig, vfo_t vfo, setting_t func, int *status);

  int (*set_parm)(RIG *rig, setting_t parm, value_t val);
  int (*get_parm)(RIG *rig, setting_t parm, value_t *val);

  int (*set_conf)(RIG *rig, token_t token, const char *val);
  int (*get_conf)(RIG *rig, token_t token, char *val);

  int (*send_dtmf)(RIG *rig, vfo_t vfo, const char *digits);
  int (*recv_dtmf)(RIG *rig, vfo_t vfo, char *digits, int *length);
  int (*send_morse)(RIG *rig, vfo_t vfo, const char *msg);

  int (*set_bank)(RIG *rig, vfo_t vfo, int bank);
  int (*set_mem)(RIG *rig, vfo_t vfo, int ch);
  int (*get_mem)(RIG *rig, vfo_t vfo, int *ch);
  int (*vfo_op)(RIG *rig, vfo_t vfo, vfo_op_t op);
  int (*scan)(RIG *rig, vfo_t vfo, scan_t scan, int ch);

  int (*set_trn)(RIG *rig, vfo_t vfo, int trn);
  int (*get_trn)(RIG *rig, vfo_t vfo, int *trn);


  int (*decode_event)(RIG *rig);

/*
 * Convenience Functions 
 */

  int (*set_channel)(RIG *rig, const channel_t *chan);
  int (*get_channel)(RIG *rig, channel_t *chan);

  /* get firmware info, etc. */
  const char* (*get_info)(RIG *rig);

  /* more to come... */
};

/*
 * yeah, looks like OO painstakingly programmed in C, sigh
 */
typedef struct {
  union {
  	enum rig_port_e rig;	/* serial, network, etc. */
  	enum ptt_type_e ptt;
  	enum dcd_type_e dcd;
  } type;
  int fd;
  FILE *stream;
#if defined(__CYGWIN__) || defined(_WIN32)
  HANDLE handle;		/* for serial special handling (PTT,DCD,..) */
#endif

  int write_delay;        /* delay in ms between each byte sent out */
  int post_write_delay;		/* for some yaesu rigs */
  struct timeval post_write_date;		/* hamlib internal use */
  int timeout;	/* in ms */
  int retry;		/* maximum number of retries, 0 to disable */

  char pathname[FILPATHLEN];
  union {
		  struct {
    		int rate;
    		int data_bits;
    		int stop_bits;
    		enum serial_parity_e parity;
    		enum serial_handshake_e handshake;
		  } serial;
		  struct {
			int pin;
		  } parallel;
		  struct {
				  /* place holder */
		  } device;
#ifdef NET
		  struct {
			struct sockaddr saddr;
		  } network;
#endif
  } parm;
} port_t;

/* 
 * Rig state
 *
 * This struct contains live data, as well as a copy of capability fields
 * that may be updated (ie. customized)
 *
 * It is fine to move fields around, as this kind of struct should
 * not be initialized like caps are.
 */
struct rig_state {
  port_t rigport;
  port_t pttport;
  port_t dcdport;

  double vfo_comp;				/* VFO compensation in PPM, 0.0 to disable */

  int transceive;	/* whether the transceive mode is on */
  int hold_decode;/* set to 1 to hold the event decoder (async) otherwise 0 */
  vfo_t current_vfo;

  int itu_region;
  freq_range_t rx_range_list[FRQRANGESIZ];	/* these ones can be updated */
  freq_range_t tx_range_list[FRQRANGESIZ];
  struct tuning_step_list tuning_steps[TSLSTSIZ];

  struct filter_list filters[FLTLSTSIZ];	/* mode/filter table, at -6dB */

  chan_t chan_list[CHANLSTSIZ];		/* channel list, zero ended */

  shortfreq_t max_rit;	/* max absolute RIT */
  shortfreq_t max_xit;	/* max absolute XIT */
  shortfreq_t max_ifshift;	/* max absolute IF-SHIFT */

  ann_t announces;
  int vfo_list;

  int preamp[MAXDBLSTSIZ];			/* in dB, 0 terminated */
  int attenuator[MAXDBLSTSIZ];		/* in dB, 0 terminated */
	   
  setting_t has_get_func;
  setting_t has_set_func;		/* updatable, e.g. for optional DSP, etc. */
  setting_t has_get_level;
  setting_t has_set_level;
  setting_t has_get_parm;
  setting_t has_set_parm;

  int level_gran[RIG_SETTING_MAX];		/* level granularity */

  /*
   * Pointer to private data
   * tuff like CI_V_address for Icom goes in this *priv 51 area
   */
  rig_ptr_t priv;
  
  /*
   * internal use by hamlib++ for event handling
   */
  rig_ptr_t obj;

  /* etc... */
};

/*
 * Rig callbacks
 * ie., the rig notify the host computer someone changed 
 *	the freq/mode from the panel, depressed a button, etc.
 *	In order to achieve this, the hamlib would have to run
 *	an internal thread listening the rig with a select(),
 *	or poll regularly...
 * 
 * Event based programming, really appropriate in a GUI.
 * So far, Icoms are able to do that in Transceive mode, and PCR-1000 too.
 */
struct rig_callbacks {
	int (*freq_event)(RIG *rig, vfo_t vfo, freq_t freq); 
	int (*mode_event)(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); 
	int (*vfo_event)(RIG *rig, vfo_t vfo); 
	int (*ptt_event)(RIG *rig, vfo_t vfo, ptt_t mode); 
	int (*dcd_event)(RIG *rig, vfo_t vfo, dcd_t mode); 
	/* etc.. */
};

/* 
 * struct rig is the master data structure, 
 * acting as a handle for the controlled rig.
 */
struct rig {
	const struct rig_caps *caps;
	struct rig_state state;
	struct rig_callbacks callbacks;
};



/* --------------- API function prototypes -----------------*/

extern HAMLIB_EXPORT(RIG *) rig_init HAMLIB_PARAMS((rig_model_t rig_model));
extern HAMLIB_EXPORT(int) rig_open HAMLIB_PARAMS((RIG *rig));

  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */

extern HAMLIB_EXPORT(int) rig_set_freq HAMLIB_PARAMS((RIG *rig, vfo_t vfo, freq_t freq));
extern HAMLIB_EXPORT(int) rig_get_freq HAMLIB_PARAMS((RIG *rig, vfo_t vfo, freq_t *freq));

extern HAMLIB_EXPORT(int) rig_set_mode HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width));
extern HAMLIB_EXPORT(int) rig_get_mode HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width));

extern HAMLIB_EXPORT(int) rig_set_vfo HAMLIB_PARAMS((RIG *rig, vfo_t vfo));
extern HAMLIB_EXPORT(int) rig_get_vfo HAMLIB_PARAMS((RIG *rig, vfo_t *vfo));

extern HAMLIB_EXPORT(int) rig_set_ptt HAMLIB_PARAMS((RIG *rig, vfo_t vfo, ptt_t ptt));
extern HAMLIB_EXPORT(int) rig_get_ptt HAMLIB_PARAMS((RIG *rig, vfo_t vfo, ptt_t *ptt));

extern HAMLIB_EXPORT(int) rig_get_dcd HAMLIB_PARAMS((RIG *rig, vfo_t vfo, dcd_t *dcd));

extern HAMLIB_EXPORT(int) rig_set_rptr_shift HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift));
extern HAMLIB_EXPORT(int) rig_get_rptr_shift HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift));
extern HAMLIB_EXPORT(int) rig_set_rptr_offs HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t rptr_offs));
extern HAMLIB_EXPORT(int) rig_get_rptr_offs HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs));

extern HAMLIB_EXPORT(int) rig_set_ctcss_tone HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t tone));
extern HAMLIB_EXPORT(int) rig_get_ctcss_tone HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t *tone));
extern HAMLIB_EXPORT(int) rig_set_dcs_code HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t code));
extern HAMLIB_EXPORT(int) rig_get_dcs_code HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t *code));

extern HAMLIB_EXPORT(int) rig_set_ctcss_sql HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t tone));
extern HAMLIB_EXPORT(int) rig_get_ctcss_sql HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t *tone));
extern HAMLIB_EXPORT(int) rig_set_dcs_sql HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t code));
extern HAMLIB_EXPORT(int) rig_get_dcs_sql HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t *code));

extern HAMLIB_EXPORT(int) rig_set_split_freq HAMLIB_PARAMS((RIG *rig, vfo_t vfo, freq_t tx_freq));
extern HAMLIB_EXPORT(int) rig_get_split_freq HAMLIB_PARAMS((RIG *rig, vfo_t vfo, freq_t *tx_freq));
extern HAMLIB_EXPORT(int) rig_set_split_mode HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width));
extern HAMLIB_EXPORT(int) rig_get_split_mode HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width));
extern HAMLIB_EXPORT(int) rig_set_split HAMLIB_PARAMS((RIG *rig, vfo_t vfo, split_t split));
extern HAMLIB_EXPORT(int) rig_get_split HAMLIB_PARAMS((RIG *rig, vfo_t vfo, split_t *split));

extern HAMLIB_EXPORT(int) rig_set_rit HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t rit));
extern HAMLIB_EXPORT(int) rig_get_rit HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t *rit));
extern HAMLIB_EXPORT(int) rig_set_xit HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t xit));
extern HAMLIB_EXPORT(int) rig_get_xit HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t *xit));

extern HAMLIB_EXPORT(int) rig_set_ts HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t ts));
extern HAMLIB_EXPORT(int) rig_get_ts HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t *ts));

extern HAMLIB_EXPORT(int) rig_power2mW HAMLIB_PARAMS((RIG *rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode));
extern HAMLIB_EXPORT(int) rig_mW2power HAMLIB_PARAMS((RIG *rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode));

extern HAMLIB_EXPORT(shortfreq_t) rig_get_resolution HAMLIB_PARAMS((RIG *rig, rmode_t mode));

extern HAMLIB_EXPORT(int) rig_set_level HAMLIB_PARAMS((RIG *rig, vfo_t vfo, setting_t level, value_t val));
extern HAMLIB_EXPORT(int) rig_get_level HAMLIB_PARAMS((RIG *rig, vfo_t vfo, setting_t level, value_t *val));

#define rig_get_strength(r,v,s) rig_get_level((r),(v),RIG_LEVEL_STRENGTH, (value_t*)(s))

extern HAMLIB_EXPORT(int) rig_set_parm HAMLIB_PARAMS((RIG *rig, setting_t parm, value_t val));
extern HAMLIB_EXPORT(int) rig_get_parm HAMLIB_PARAMS((RIG *rig, setting_t parm, value_t *val));

extern HAMLIB_EXPORT(int) rig_set_conf HAMLIB_PARAMS((RIG *rig, token_t token, const char *val));
extern HAMLIB_EXPORT(int) rig_get_conf HAMLIB_PARAMS((RIG *rig, token_t token, char *val));

extern HAMLIB_EXPORT(int) rig_set_powerstat HAMLIB_PARAMS((RIG *rig, powerstat_t status));
extern HAMLIB_EXPORT(int) rig_get_powerstat HAMLIB_PARAMS((RIG *rig, powerstat_t *status));

extern HAMLIB_EXPORT(int) rig_reset HAMLIB_PARAMS((RIG *rig, reset_t reset));	/* dangerous! */


extern HAMLIB_EXPORT(const struct confparams*) rig_confparam_lookup HAMLIB_PARAMS((RIG *rig, const char *name));
extern HAMLIB_EXPORT(token_t) rig_token_lookup HAMLIB_PARAMS((RIG *rig, const char *name));

extern HAMLIB_EXPORT(int) rig_close HAMLIB_PARAMS((RIG *rig));
extern HAMLIB_EXPORT(int) rig_cleanup HAMLIB_PARAMS((RIG *rig));

extern HAMLIB_EXPORT(rig_model_t) rig_probe HAMLIB_PARAMS((port_t *p));

extern HAMLIB_EXPORT(int) rig_set_ant HAMLIB_PARAMS((RIG *rig, vfo_t vfo, ant_t ant));	/* antenna */
extern HAMLIB_EXPORT(int) rig_get_ant HAMLIB_PARAMS((RIG *rig, vfo_t vfo, ant_t *ant));

extern HAMLIB_EXPORT(setting_t) rig_has_get_level HAMLIB_PARAMS((RIG *rig, setting_t level));
extern HAMLIB_EXPORT(setting_t) rig_has_set_level HAMLIB_PARAMS((RIG *rig, setting_t level));

extern HAMLIB_EXPORT(setting_t) rig_has_get_parm HAMLIB_PARAMS((RIG *rig, setting_t parm));
extern HAMLIB_EXPORT(setting_t) rig_has_set_parm HAMLIB_PARAMS((RIG *rig, setting_t parm));

extern HAMLIB_EXPORT(setting_t) rig_has_get_func HAMLIB_PARAMS((RIG *rig, setting_t func));
extern HAMLIB_EXPORT(setting_t) rig_has_set_func HAMLIB_PARAMS((RIG *rig, setting_t func));

extern HAMLIB_EXPORT(int) rig_set_func HAMLIB_PARAMS((RIG *rig, vfo_t vfo, setting_t func, int status));
extern HAMLIB_EXPORT(int) rig_get_func HAMLIB_PARAMS((RIG *rig, vfo_t vfo, setting_t func, int *status));

extern HAMLIB_EXPORT(int) rig_send_dtmf HAMLIB_PARAMS((RIG *rig, vfo_t vfo, const char *digits));
extern HAMLIB_EXPORT(int) rig_recv_dtmf HAMLIB_PARAMS((RIG *rig, vfo_t vfo, char *digits, int *length));
extern HAMLIB_EXPORT(int) rig_send_morse HAMLIB_PARAMS((RIG *rig, vfo_t vfo, const char *msg));

extern HAMLIB_EXPORT(int) rig_set_bank HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int bank));
extern HAMLIB_EXPORT(int) rig_set_mem HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int ch));
extern HAMLIB_EXPORT(int) rig_get_mem HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int *ch));
extern HAMLIB_EXPORT(int) rig_vfo_op HAMLIB_PARAMS((RIG *rig, vfo_t vfo, vfo_op_t op));
extern HAMLIB_EXPORT(vfo_op_t) rig_has_vfo_op HAMLIB_PARAMS((RIG *rig, vfo_op_t op));
extern HAMLIB_EXPORT(int) rig_scan HAMLIB_PARAMS((RIG *rig, vfo_t vfo, scan_t scan, int ch));
extern HAMLIB_EXPORT(scan_t) rig_has_scan HAMLIB_PARAMS((RIG *rig, scan_t scan));

extern HAMLIB_EXPORT(int) rig_restore_channel HAMLIB_PARAMS((RIG *rig, const channel_t *chan)); /* curr VFO */
extern HAMLIB_EXPORT(int) rig_save_channel HAMLIB_PARAMS((RIG *rig, channel_t *chan));
extern HAMLIB_EXPORT(int) rig_set_channel HAMLIB_PARAMS((RIG *rig, const channel_t *chan));	/* mem */
extern HAMLIB_EXPORT(int) rig_get_channel HAMLIB_PARAMS((RIG *rig, channel_t *chan));

extern HAMLIB_EXPORT(int) rig_set_trn HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int trn));
extern HAMLIB_EXPORT(int) rig_get_trn HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int *trn));


extern HAMLIB_EXPORT(const char *) rig_get_info HAMLIB_PARAMS((RIG *rig));

extern HAMLIB_EXPORT(const struct rig_caps *) rig_get_caps HAMLIB_PARAMS((rig_model_t rig_model));
extern HAMLIB_EXPORT(const freq_range_t *) rig_get_range HAMLIB_PARAMS((const freq_range_t range_list[], freq_t freq, rmode_t mode));

extern HAMLIB_EXPORT(pbwidth_t) rig_passband_normal HAMLIB_PARAMS((RIG *rig, rmode_t mode));
extern HAMLIB_EXPORT(pbwidth_t) rig_passband_narrow HAMLIB_PARAMS((RIG *rig, rmode_t mode));
extern HAMLIB_EXPORT(pbwidth_t) rig_passband_wide HAMLIB_PARAMS((RIG *rig, rmode_t mode));

extern HAMLIB_EXPORT(const char *) rigerror HAMLIB_PARAMS((int errnum));

extern HAMLIB_EXPORT(int) rig_setting2idx HAMLIB_PARAMS((setting_t s));
#define rig_idx2setting(i) (1<<(i))

/*
 * Even if these functions are prefixed with "rig_", they are not rig specific
 * Maybe "hamlib_" would have been better. Let me know. --SF
 */
extern HAMLIB_EXPORT(void) rig_set_debug HAMLIB_PARAMS((enum rig_debug_level_e debug_level));
extern HAMLIB_EXPORT(int) rig_need_debug HAMLIB_PARAMS((enum rig_debug_level_e debug_level));
extern HAMLIB_EXPORT(void) rig_debug HAMLIB_PARAMS((enum rig_debug_level_e debug_level, const char *fmt, ...));

extern HAMLIB_EXPORT(int) rig_register HAMLIB_PARAMS((const struct rig_caps *caps));
extern HAMLIB_EXPORT(int) rig_unregister HAMLIB_PARAMS((rig_model_t rig_model));
extern HAMLIB_EXPORT(int) rig_list_foreach HAMLIB_PARAMS((int (*cfunc)(const struct rig_caps*, rig_ptr_t), rig_ptr_t data));
extern HAMLIB_EXPORT(int) rig_load_backend HAMLIB_PARAMS((const char *be_name));
extern HAMLIB_EXPORT(int) rig_check_backend HAMLIB_PARAMS((rig_model_t rig_model));
extern HAMLIB_EXPORT(int) rig_load_all_backends HAMLIB_PARAMS(());
extern HAMLIB_EXPORT(rig_model_t) rig_probe_all HAMLIB_PARAMS((port_t *p));


__END_DECLS

#endif /* _RIG_H */

