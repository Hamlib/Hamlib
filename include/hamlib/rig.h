/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * rig.h - Copyright (C) 2000,2001 Frank Singleton and Stephane Fillod
 * This program defines the Hamlib API and rig capabilities structures that
 * will be used for obtaining rig capabilities.
 *
 *
 *	$Id: rig.h,v 1.22 2001-03-02 18:46:27 f4cfe Exp $
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#ifndef _RIG_H
#define _RIG_H 1

#include <hamlib/riglist.h>	/* list in another file to not mess up w/ this one */
#include <stdio.h>		/* required for FILE definition */
#include <sys/time.h>		/* required for struct timeval */

extern const char hamlib_version[];

extern const int full_ctcss_list[];
extern const int full_dcs_list[];
#define CTCSS_LIST_SIZE (sizeof(full_ctcss_list)/sizeof(int)-1)
#define DCS_LIST_SIZE (sizeof(full_dcs_list)/sizeof(int)-1)

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



/*
 * REM: Numeric order matters for debug level
 */
enum rig_debug_level_e {
	RIG_DEBUG_NONE = 0,
	RIG_DEBUG_BUG,
	RIG_DEBUG_ERR,
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

#if 1
enum vfo_e {
	RIG_VFO_MAIN = 0,
	RIG_VFO_SUB,
	RIG_VFO_SAT,
	RIG_VFO_A = RIG_VFO_MAIN,
	RIG_VFO_B = RIG_VFO_SUB,
	RIG_VFO_C = RIG_VFO_SAT,
	RIG_VFO_CURR,		/* current VFO */
	RIG_VFO_ALL			/* apply to all VFO (when used as target) */
};

typedef enum vfo_e vfo_t;

#else

#define RIG_VFO_CURR	0	/* current VFO */
#define RIG_VFO_A	(1<<0)
#define RIG_VFO_B	(1<<1)
#define RIG_VFO_C	(1<<2)
#define RIG_VFO_MAIN	RIG_VFO_A
#define RIG_VFO_SUB 	RIG_VFO_B
#define RIG_VFO_SAT 	RIG_VFO_C
/*
 * could RIG_VFO_ALL be useful?
 * i.e. apply to all VFO, when used as target
 */
typedef int vfo_t;
#endif

#if 1
enum passband_width_e {
	RIG_PASSBAND_NORMAL = 0,
	RIG_PASSBAND_NARROW,
	RIG_PASSBAND_WIDE
};
typedef enum passband_width_e pbwidth_t;

#else
#define RIG_PASSBAND_NORMAL Hz(0)
/* 
 * also see rig_passband_normal(rig,mode),
 * 	rig_passband_narrow(rig,mode) and rig_passband_wide(rig,mode)
 */
typedef shortfreq_t pbwidth_t;

#endif


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


enum mem_vfo_op_e {
	RIG_MVOP_VFO_MODE = 0,
	RIG_MVOP_MEM_MODE,
	RIG_MVOP_VFO_CPY,		/* VFO A = VFO B */
	RIG_MVOP_VFO_XCHG,		/* Exchange VFO A/B */
	RIG_MVOP_DUAL_OFF,		/* Dual watch off */
	RIG_MVOP_DUAL_ON,		/* Dual watch on */
	RIG_MVOP_FROM_VFO,		/* VFO->MEM */
	RIG_MVOP_TO_VFO,		/* MEM->VFO */
	RIG_MVOP_MCL			/* Memory clear */
};

typedef enum mem_vfo_op_e mv_op_t;


/*
 * When optional speech synthesizer is installed 
 * what about RIG_ANN_ENG and RIG_ANN_JAPAN? and RIG_ANN_CW?
 */

enum ann_e {
		RIG_ANN_OFF = 0,
		RIG_ANN_FREQ,
		RIG_ANN_RXMODE,
		RIG_ANN_ALL
};
typedef enum ann_e ann_t;

/* Antenna number */
typedef int ant_t;

enum agc_level_e {
		RIG_AGC_OFF = 0,
		RIG_AGC_SUPERFAST,
		RIG_AGC_FAST,
		RIG_AGC_SLOW
};

/*
 * Universal approach for use by set_level/get_level
 */
union value_u {
		signed int i;
		float f;
};
typedef union value_u value_t;

/* free slot:
 * 1<<2 (was RIG_LEVEL_ANT)
 * 1<<20 (was RIG_LEVEL_ANN)
 */
#define RIG_LEVEL_NONE		0
#define RIG_LEVEL_PREAMP	(1<<0)	/* Preamp, arg int (dB) */
#define RIG_LEVEL_ATT		(1<<1)	/* Attenuator, arg int (dB) */
#define RIG_LEVEL_AF		(1<<3)	/* Volume, arg float [0.0..1.0] */
#define RIG_LEVEL_RF		(1<<4)	/* RF gain (not TX power), arg float [0.0..1.0] */
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
#define RIG_PARM_TIME		(1<<5)	/* hh:mm:ss, int in seconds */

#define RIG_PARM_FLOAT_LIST (RIG_PARM_BACKLIGHT)

#define RIG_PARM_IS_FLOAT(l) ((l)&RIG_PARM_FLOAT_LIST)


typedef unsigned long setting_t;	/* 32 bits might not be enough.. */

/*
 * tranceive mode, ie. the rig notify the host of any event,
 * like freq changed, mode changed, etc.
 */
#define	RIG_TRN_OFF 0
#define	RIG_TRN_ON 1
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
#define RIG_FUNC_SBKIN    	(1<<6)		/* Semi Break-in (is it the rigth name?) */
#define RIG_FUNC_FBKIN    	(1<<7)		/* Full Break-in, for CW mode */
#define RIG_FUNC_ANF    	(1<<8)		/* Automatic Notch Filter (DSP) */
#define RIG_FUNC_NR     	(1<<9)		/* Noise Reduction (DSP) */
#define RIG_FUNC_AIP     	(1<<10)		/* AIP (Kenwood) */
#define RIG_FUNC_APF     	(1<<11)		/* Auto Passband Filter */
#define RIG_FUNC_MON     	(1<<12)		/* Monitor? (Icom) */
#define RIG_FUNC_MN     	(1<<13)		/* Manual Notch (Icom) */
#define RIG_FUNC_RNF     	(1<<14)		/* RTTY Filter Notch (Icom) */
#define RIG_FUNC_ARO     	(1<<15)		/* Auto Repeater Offset */


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


#define RIGNAMSIZ 30
#define RIGVERSIZ 8
#define FILPATHLEN 100
#define FRQRANGESIZ 30
#define MAXCHANDESC 30		/* describe channel eg: "WWV 5Mhz" */
#define TSLSTSIZ 20			/* max tuning step list size, zero ended */
#define FLTLSTSIZ 16		/* max mode/filter list size, zero ended */
#define MAXDBLSTSIZ 8		/* max preamp/att levels supported, zero ended */
#define CHANLSTSIZ 16		/* max mem_list size, zero ended */

#define RIG_DBLST_END 0		/* end marker in a preamp/att level list */
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
};
typedef struct freq_range_list freq_range_t;

#define RIG_FRNG_END     {0,0,0,0,0}

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

#define RIG_TS_END     {0,0}

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

#define RIG_FLT_END     {0,0}


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
  vfo_t vfo;
  int power;	/* in mW */
  int att;	/* in dB */
  int preamp;	/* in dB */
  int ant;	/* antenna number */
  shortfreq_t tuning_step;	/* */
  shortfreq_t rit;	/* */
  setting_t funcs;
  int ctcss;
  int ctcss_sql;
  int dcs;
  int dcs_sql;
  unsigned char channel_desc[MAXCHANDESC];
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
		RIG_MTYPE_MEMOPAD		/* inaccessible on Icom, what about others? */
};

struct chan_list {
		int start;			/* rig memory channel _number_ */
		int end;
		enum chan_type_e type;	/* among EDGE, MEM, CALL, .. */
		int reserved;			/* don't know yet, maybe smthing like flags */
};

#define RIG_CHAN_END     {0,0,0,0}

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
  rig_model_t rig_model; /* eg. RIG_MODEL_FT847 */
  unsigned char model_name[RIGNAMSIZ]; /* eg "ft847" */
  unsigned char mfg_name[RIGNAMSIZ]; /* eg "Yaesu" */
  char version[RIGVERSIZ]; /* driver version, eg "0.5" */
  const char *copyright; /* author and copyright, eg "(c) 2001 Joe Bar, GPL" */
  enum rig_status_e status; /* among ALPHA, BETA, STABLE, NEW  */

  enum rig_type_e rig_type;
  enum ptt_type_e ptt_type;	/* how we will key the rig */
  enum dcd_type_e dcd_type;	/* how we know squelch status */
  enum rig_port_e port_type;	/* hint about common port type (e.g. serial, device, etc. */

  int serial_rate_min; /* eg 4800 */
  int serial_rate_max; /* eg 9600 */
  int serial_data_bits; /* eg 8 */
  int serial_stop_bits; /* eg 2 */
  enum serial_parity_e serial_parity; /* */
  enum serial_handshake_e serial_handshake; /* */

  int write_delay;		/* delay in ms between each byte sent out */
  int post_write_delay;		/* optional delay after sending last cmd sequence (yaesu) -- FS */
  int timeout;	/* in ms */
  int retry;		/* maximum number of retries, 0 to disable */

  setting_t has_get_func;		/* bitwise OR'ed RIG_FUNC_FAGC, NG, etc. */
  setting_t has_set_func;		/* bitwise OR'ed RIG_FUNC_FAGC, NG, etc. */
  setting_t has_get_level;		/* bitwise OR'ed RIG_LEVEL_* */
  setting_t has_set_level;		/* bitwise OR'ed RIG_LEVEL_* */
  setting_t has_get_parm;		/* bitwise OR'ed RIG_PARM_* */
  setting_t has_set_parm;		/* bitwise OR'ed RIG_PARM_* */

  const int *ctcss_list;    /* points to a 0 terminated array, */
  const int *dcs_list;		/* may be NULL */

  int preamp[MAXDBLSTSIZ];		/* in dB, 0 terminated */
  int attenuator[MAXDBLSTSIZ];	/* in dB, 0 terminated */

  const char *dtmf_digits;	/* ASCIIZ string of supported digits */

  shortfreq_t max_rit;	/* max absolute RIT */
  shortfreq_t max_ifshift;	/* max absolute IF-SHIFT */

  int vfo_list;
  int targetable_vfo;
  int transceive;	/* the rig is able to generate events, to be used by callbacks */

  int chan_qty;		/* number of channels */
  int bank_qty;		/* number of banks */
  int chan_desc_sz;   /* memory channel size, 0 if none */

  chan_t chan_list[CHANLSTSIZ];		/* channel list, zero ended */

  freq_range_t rx_range_list1[FRQRANGESIZ];	/* ITU region 1 */
  freq_range_t tx_range_list1[FRQRANGESIZ];
  freq_range_t rx_range_list2[FRQRANGESIZ];	/* ITU region 2 */
  freq_range_t tx_range_list2[FRQRANGESIZ];
  struct tuning_step_list tuning_steps[TSLSTSIZ];

  struct filter_list filters[FLTLSTSIZ];	/* mode/filter table, at -6dB */

  void *priv;	/* 51 area, useful when having different models in a backend */

  /*
   * Rig Admin API
   *
   */
 
  int (*rig_init)(RIG *rig);	/* setup *priv */
  int (*rig_cleanup)(RIG *rig);
  int (*rig_open)(RIG *rig);	/* called when port just opened */
  int (*rig_close)(RIG *rig);	/* called before port is to be closed */
  int (*rig_probe)(RIG *rig); /* Experimental: may work.. */
  
  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */
  
  int (*set_freq)(RIG *rig, vfo_t vfo, freq_t freq); /* select freq */
  int (*get_freq)(RIG *rig, vfo_t vfo, freq_t *freq); /* get freq */

  int (*set_mode)(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); /* select mode */
  int (*get_mode)(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width); /* get mode */

  int (*set_vfo)(RIG *rig, vfo_t vfo); /* select vfo (A,B, etc.) */
  int (*get_vfo)(RIG *rig, vfo_t *vfo); /* get vfo */

  int (*set_ptt)(RIG *rig, vfo_t vfo, ptt_t ptt); /* ptt on/off */
  int (*get_ptt)(RIG *rig, vfo_t vfo, ptt_t *ptt); /* get ptt status */
  int (*get_dcd)(RIG *rig, vfo_t vfo, dcd_t *dcd); /* get squelch status */

  int (*set_rptr_shift)(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);	/* set repeater shift */
  int (*get_rptr_shift)(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift);	/* get repeater shift */

  int (*set_rptr_offs)(RIG *rig, vfo_t vfo, shortfreq_t offs);/*set duplex offset freq*/
  int (*get_rptr_offs)(RIG *rig, vfo_t vfo, shortfreq_t *offs);/*get duplex offset freq*/

  int (*set_split_freq)(RIG *rig, vfo_t vfo, freq_t rx_freq, freq_t tx_freq);
  int (*get_split_freq)(RIG *rig, vfo_t vfo, freq_t *rx_freq, freq_t *tx_freq);
  int (*set_split)(RIG *rig, vfo_t vfo, split_t split);
  int (*get_split)(RIG *rig, vfo_t vfo, split_t *split);

  int (*set_rit)(RIG *rig, vfo_t vfo, shortfreq_t rit);
  int (*get_rit)(RIG *rig, vfo_t vfo, shortfreq_t *rit);

  int (*set_ts)(RIG *rig, vfo_t vfo, shortfreq_t ts); /* set tuning step */
  int (*get_ts)(RIG *rig, vfo_t vfo, shortfreq_t *ts); /* get tuning step */

  int (*set_dcs)(RIG *rig, vfo_t vfo, unsigned int code);
  int (*get_dcs)(RIG *rig, vfo_t vfo, unsigned int *code);
  int (*set_ctcss)(RIG *rig, vfo_t vfo, unsigned int tone);
  int (*get_ctcss)(RIG *rig, vfo_t vfo, unsigned int *tone);

  int (*set_dcs_sql)(RIG *rig, vfo_t vfo, unsigned int code);
  int (*get_dcs_sql)(RIG *rig, vfo_t vfo, unsigned int *code);
  int (*set_ctcss_sql)(RIG *rig, vfo_t vfo, unsigned int tone);
  int (*get_ctcss_sql)(RIG *rig, vfo_t vfo, unsigned int *tone);

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

  int (*set_ant)(RIG *rig, vfo_t vfo, ant_t ant);	/* antenna */
  int (*get_ant)(RIG *rig, vfo_t vfo, ant_t *ant);

  int (*set_level)(RIG *rig, vfo_t vfo, setting_t level, value_t val);/* set level setting */
  int (*get_level)(RIG *rig, vfo_t vfo, setting_t level, value_t *val);/* get level setting*/

  int (*set_func)(RIG *rig, vfo_t vfo, setting_t func, int status); /* activate the function(s) */
  int (*get_func)(RIG *rig, vfo_t vfo, setting_t func, int *status); /* get the setting from rig */

  int (*set_parm)(RIG *rig, setting_t parm, value_t val);/* set parameter */
  int (*get_parm)(RIG *rig, setting_t parm, value_t *val);/* get parameter */

  int (*send_dtmf)(RIG *rig, vfo_t vfo, const char *digits);
  int (*recv_dtmf)(RIG *rig, vfo_t vfo, char *digits, int *length);

  int (*set_bank)(RIG *rig, vfo_t vfo, int bank);			/* set memory bank number */
  int (*set_mem)(RIG *rig, vfo_t vfo, int ch);			/* set memory channel number */
  int (*get_mem)(RIG *rig, vfo_t vfo, int *ch);		/* get memory channel number */
  int (*mv_ctl)(RIG *rig, vfo_t vfo, mv_op_t op);		/* Mem/VFO operation */

  int (*set_trn)(RIG *rig, vfo_t vfo, int trn);	/* activate transceive mode on radio */
  int (*get_trn)(RIG *rig, vfo_t vfo, int *trn);	/* PCR-1000 can do that, ICR75 too */


  int (*decode_event)(RIG *rig);	/* When transceive on, find out which callback to call, and call it */

/*
 * Convenience Functions 
 */

  int (*set_channel)(RIG *rig, const channel_t *chan);
  int (*get_channel)(RIG *rig, channel_t *chan);

  /* get firmware info, etc. */
  unsigned char* (*get_info)(RIG *rig);

  /* more to come... */
};

/*
 * example of 3.5MHz -> 3.8MHz, 100W transmit range
 *   tx_range_list = {{3500000,3800000,RIG_MODE_AM|RIG_MODE_CW,100000},0}
 *
 */


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
  enum rig_port_e port_type;	/* serial, network, etc. */

  int serial_rate;
  int serial_data_bits; /* eg 8 */
  int serial_stop_bits; /* eg 2 */
  enum serial_parity_e serial_parity; /* */
  enum serial_handshake_e serial_handshake; /* */

  int write_delay;        /* delay in ms between each byte sent out */
  int post_write_delay;		/* for some yaesu rigs */
  struct timeval post_write_date;		/* hamlib internal use */
  int timeout;	/* in ms */
  int retry;		/* maximum number of retries, 0 to disable */

  enum ptt_type_e ptt_type;	/* how we will key the rig */
  enum dcd_type_e dcd_type;

  char ptt_path[FILPATHLEN];	/* path to the keying device (serial,//) */
  char rig_path[FILPATHLEN]; /* serial port/network path(host:port) */
  double vfo_comp;				/* VFO compensation in PPM, 0.0 to disable */

  int fd;	/* serial port/socket file handle */
  int ptt_fd;	/* ptt port file handle */
  FILE *stream;	/* serial port/socket (buffered) stream handle */

  int transceive;	/* whether the transceive mode is on */
  int hold_decode;/* set to 1 to hold the event decoder (async) otherwise 0 */
  vfo_t current_vfo;

  int itu_region;
  freq_range_t rx_range_list[FRQRANGESIZ];	/* these ones can be updated */
  freq_range_t tx_range_list[FRQRANGESIZ];
  struct tuning_step_list tuning_steps[TSLSTSIZ];

  struct filter_list filters[FLTLSTSIZ];	/* mode/filter table, at -6dB */

  shortfreq_t max_rit;	/* max absolute RIT */
  shortfreq_t max_ifshift;	/* max absolute IF-SHIFT */

  int vfo_list;

  int preamp[MAXDBLSTSIZ];			/* in dB, 0 terminated */
  int attenuator[MAXDBLSTSIZ];		/* in dB, 0 terminated */
	   
  setting_t has_get_func;
  setting_t has_set_func;		/* updatable, e.g. for optional DSP, etc. */
  setting_t has_get_level;
  setting_t has_set_level;
  setting_t has_get_parm;
  setting_t has_set_parm;

  /*
   * Pointer to private data
   * tuff like CI_V_address for Icom goes in this *priv 51 area
   */
  void *priv;
  
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

extern RIG *rig_init(rig_model_t rig_model);
extern int rig_open(RIG *rig);

  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */

extern int rig_set_freq(RIG *rig, vfo_t vfo, freq_t freq); /* select freq */
extern int rig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq); /* get freq */

extern int rig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); /* select mode */
extern int rig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width); /* get mode */

extern int rig_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
extern int rig_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

extern int rig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt); /* ptt on/off */
extern int rig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt); /* get ptt status */

extern int rig_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd); /* get dcd status */

extern int rig_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift); /* set repeater shift */
extern int rig_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift); /* get repeater shift */
extern int rig_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs); /* set repeater offset */
extern int rig_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs); /* get repeater offset */

extern int rig_set_ctcss(RIG *rig, vfo_t vfo, unsigned int tone);
extern int rig_get_ctcss(RIG *rig, vfo_t vfo, unsigned int *tone);
extern int rig_set_dcs(RIG *rig, vfo_t vfo, unsigned int code);
extern int rig_get_dcs(RIG *rig, vfo_t vfo, unsigned int *code);

extern int rig_set_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int tone);
extern int rig_get_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int *tone);
extern int rig_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code);
extern int rig_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code);

extern int rig_set_split_freq(RIG *rig, vfo_t vfo, freq_t rx_freq, freq_t tx_freq);
extern int rig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *rx_freq, freq_t *tx_freq);
extern int rig_set_split(RIG *rig, vfo_t vfo, split_t split);
extern int rig_get_split(RIG *rig, vfo_t vfo, split_t *split);

extern int rig_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
extern int rig_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);

extern int rig_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts); /* set tuning step */
extern int rig_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts); /* get tuning step */

extern int rig_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode);
extern int rig_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode);

extern shortfreq_t rig_get_resolution(RIG *rig, rmode_t mode);

extern int rig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
extern int rig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

#define rig_get_strength(r,v,s) rig_get_level((r),(v),RIG_LEVEL_STRENGTH, (value_t*)(s))

extern int rig_set_parm(RIG *rig, setting_t parm, value_t val);
extern int rig_get_parm(RIG *rig, setting_t parm, value_t *val);

extern int rig_set_powerstat(RIG *rig, powerstat_t status);
extern int rig_get_powerstat(RIG *rig, powerstat_t *status);

/* more to come -- FS */

extern int rig_close(RIG *rig);
extern int rig_cleanup(RIG *rig);

extern RIG *rig_probe(const char *rig_path);

extern int rig_set_ant(RIG *rig, vfo_t vfo, ant_t ant);	/* antenna */
extern int rig_get_ant(RIG *rig, vfo_t vfo, ant_t *ant);

extern setting_t rig_has_get_level(RIG *rig, setting_t level);
extern setting_t rig_has_set_level(RIG *rig, setting_t level);

extern setting_t rig_has_get_parm(RIG *rig, setting_t parm);
extern setting_t rig_has_set_parm(RIG *rig, setting_t parm);

extern setting_t rig_has_get_func(RIG *rig, setting_t func);
extern setting_t rig_has_set_func(RIG *rig, setting_t func);

extern int rig_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);	/* activate the function(s) */
extern int rig_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status); /* get the setting from rig */

extern int rig_send_dtmf(RIG *rig, vfo_t vfo, const char *digits);
extern int rig_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length);

extern int rig_set_bank(RIG *rig, vfo_t vfo, int bank);	/* set memory bank number */
extern int rig_set_mem(RIG *rig, vfo_t vfo, int ch);		/* set memory channel number */
extern int rig_get_mem(RIG *rig, vfo_t vfo, int *ch);		/* get memory channel number */
extern int rig_mv_ctl(RIG *rig, vfo_t vfo, mv_op_t op);	/* Mem/VFO operation */

extern int rig_set_channel(RIG *rig, const channel_t *chan);
extern int rig_get_channel(RIG *rig, channel_t *chan);

extern int rig_set_trn(RIG *rig, vfo_t vfo, int trn); /* activate the transceive mode */
extern int rig_get_trn(RIG *rig, vfo_t vfo, int *trn);


extern unsigned char *rig_get_info(RIG *rig);

extern const struct rig_caps *rig_get_caps(rig_model_t rig_model);
const freq_range_t *rig_get_range(const freq_range_t range_list[], freq_t freq, rmode_t mode);

extern const char *rigerror(int errnum);

/*
 * Even if these functions are prefixed with "rig_", they are not rig specific
 * Maybe "hamlib_" would have been better. Let me know. --SF
 */
void rig_set_debug(enum rig_debug_level_e debug_level);
int rig_need_debug(enum rig_debug_level_e debug_level);
void rig_debug(enum rig_debug_level_e debug_level, const char *fmt, ...);

int rig_register(const struct rig_caps *caps);
int rig_unregister(rig_model_t rig_model);
int rig_list_foreach(int (*cfunc)(const struct rig_caps*,void*),void *data);
int rig_load_backend(const char *be_name);

#endif /* _RIG_H */

