/*
 *  Hamlib Interface - API header
 *  Copyright (c) 2000-2002 by Stephane Fillod and Frank Singleton
 *
 *	$Id: rig.h,v 1.75 2003-03-10 08:40:13 fillods Exp $
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
#include <sys/socket.h>		/* required for struct sockaddr */

#if defined(__CYGWIN__) || defined(_WIN32)
#include <windows.h>		/* HANDLE definition */
#endif

/*! \file rig.h
 *  \ingroup rig
 *  \brief Hamlib rig data structures.
 *
 *  This file contains the data structures and definitions for the Hamlib rig API.
 *  see the rig.c file for more details on the rig API.
 */


/* __BEGIN_DECLS should be used at the beginning of your declarations,
 * so that C++ compilers don't mangle their names.  Use __END_DECLS at
 * the end of C declarations. */
#undef __BEGIN_DECLS
#undef __END_DECLS
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS		/* empty */
# define __END_DECLS		/* empty */
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

/*
 * Error codes that can be returned by the Hamlib functions
 */
enum rig_errcode_e {
	RIG_OK=0,		/*<! No error, operation completed sucessfully */
	RIG_EINVAL,		/*<! invalid parameter */
	RIG_ECONF,		/*<! invalid configuration (serial,..) */
	RIG_ENOMEM,		/*<! memory shortage */
	RIG_ENIMPL,		/*<! function not implemented, but will be */
	RIG_ETIMEOUT,		/*<! communication timed out */
	RIG_EIO,		/*<! IO error, including open failed */
	RIG_EINTERNAL,		/*<! Internal Hamlib error, huh! */
	RIG_EPROTO,		/*<! Protocol error */
	RIG_ERJCTED,		/*<! Command rejected by the rig */
	RIG_ETRUNC,		/*<! Command performed, but arg truncated */
	RIG_ENAVAIL,		/*<! function not available */
	RIG_ENTARGET,		/*<! VFO not targetable */
	RIG_BUSERROR,		/*<! Error talking on the bus */
	RIG_BUSBUSY,		/*<! Collision on the bus */
};

/**
 *\brief Hamlib debug levels
 *
 * REM: Numeric order matters for debug level
 *
 * \sa rig_set_debug
 */
enum rig_debug_level_e {
  RIG_DEBUG_NONE = 0,		/*<! no bug reporting */
  RIG_DEBUG_BUG,		/*<! serious bug */
  RIG_DEBUG_ERR,		/*<! error case (e.g. protocol, memory allocation) */
  RIG_DEBUG_WARN,		/*<! warning */
  RIG_DEBUG_VERBOSE,		/*<! verbose */
  RIG_DEBUG_TRACE		/*<! tracing */
};

/* --------------- Rig capabilities -----------------*/

/* Forward struct references */

struct rig;
struct rig_state;

/*!
 * \brief Rig structure definition (see rig for details).
 */
typedef struct rig RIG;

#define RIGNAMSIZ 30
#define RIGVERSIZ 8
#define FILPATHLEN 100
#define FRQRANGESIZ 30
#define MAXCHANDESC 30		/* describe channel eg: "WWV 5Mhz" */
#define TSLSTSIZ 20		/* max tuning step list size, zero ended */
#define FLTLSTSIZ 16		/* max mode/filter list size, zero ended */
#define MAXDBLSTSIZ 8		/* max preamp/att levels supported, zero ended */
#define CHANLSTSIZ 16		/* max mem_list size, zero ended */


/**
 * \brief CTCSS and DCS type definition.
 *
 * Continuous Tone Controlled Squelch System (CTCSS)
 * sub-audible tone frequency are expressed in \em tenth of Hz.
 * For example, the subaudible tone of 88.5 Hz is represented within
 * Hamlib by 885.
 *
 * Digitally-Coded Squelch codes are simple direct integers.
 */
typedef unsigned int tone_t;

/**
 * \brief Port type
 */
enum rig_port_e {
  RIG_PORT_NONE = 0,		/*!< No port */
  RIG_PORT_SERIAL,		/*!< Serial */
  RIG_PORT_NETWORK,		/*!< Network socket type */
  RIG_PORT_DEVICE,		/*!< Device driver, like the WiNRADiO */
  RIG_PORT_PACKET,		/*!< AX.25 network type, e.g. SV8CS protocol */
  RIG_PORT_DTMF,		/*!< DTMF protocol bridge via another rig, eg. Kenwood Sky Cmd System */
  RIG_PORT_ULTRA,		/*!< IrDA Ultra protocol! */
  RIG_PORT_RPC,			/*!< RPC wrapper */
  RIG_PORT_PARALLEL,		/*!< Parallel port */
};

/**
 * \brief Serial parity
 */
enum serial_parity_e {
  RIG_PARITY_NONE = 0,		/*!< No parity */
  RIG_PARITY_ODD,		/*!< Odd */
  RIG_PARITY_EVEN		/*!< Even */
};

/**
 * \brief Serial handshake
 */
enum serial_handshake_e {
  RIG_HANDSHAKE_NONE = 0,	/*!< No handshake */
  RIG_HANDSHAKE_XONXOFF,	/*!< Software XON/XOFF */
  RIG_HANDSHAKE_HARDWARE	/*!< Hardware CTS/RTS */
};


/**
 * \brief Serial control state
 */
enum serial_control_state_e {
  RIG_SIGNAL_UNSET = 0,	/*!< Unset or tri-state */
  RIG_SIGNAL_ON,	/*!< ON */
  RIG_SIGNAL_OFF	/*!< OFF */
};

#define RIG_FLAG_RECEIVER		(1<<1)
#define RIG_FLAG_TRANSMITTER	(1<<2)
#define RIG_FLAG_SCANNER		(1<<3)

#define RIG_FLAG_MOBILE			(1<<4)
#define RIG_FLAG_HANDHELD		(1<<5)
#define RIG_FLAG_COMPUTER		(1<<6)
#define RIG_FLAG_TRUNKING		(1<<7)
#define RIG_FLAG_APRS			(1<<8)
#define RIG_FLAG_TNC			(1<<9)
#define RIG_FLAG_DXCLUSTER		(1<<10)
#define RIG_FLAG_TUNER			(1<<11)		/* dumb tuner */

#define RIG_FLAG_TRANSCEIVER (RIG_FLAG_RECEIVER|RIG_FLAG_TRANSMITTER)
#define RIG_TYPE_MASK (RIG_FLAG_TRANSCEIVER|RIG_FLAG_SCANNER|RIG_FLAG_MOBILE|RIG_FLAG_HANDHELD|RIG_FLAG_COMPUTER|RIG_FLAG_TRUNKING|RIG_FLAG_TUNER)

#define RIG_TYPE_OTHER		0
#define RIG_TYPE_TRANSCEIVER	RIG_FLAG_TRANSCEIVER
#define RIG_TYPE_HANDHELD	(RIG_FLAG_TRANSCEIVER|RIG_FLAG_HANDHELD)
#define RIG_TYPE_MOBILE		(RIG_FLAG_TRANSCEIVER|RIG_FLAG_MOBILE)
#define RIG_TYPE_RECEIVER	RIG_FLAG_RECEIVER
#define RIG_TYPE_PCRECEIVER	(RIG_FLAG_COMPUTER|RIG_FLAG_RECEIVER)
#define RIG_TYPE_SCANNER	(RIG_FLAG_SCANNER|RIG_FLAG_RECEIVER)
#define RIG_TYPE_TRUNKSCANNER	(RIG_TYPE_SCANNER|RIG_FLAG_TRUNKING)
#define RIG_TYPE_COMPUTER	(RIG_FLAG_TRANSCEIVER|RIG_FLAG_COMPUTER)
#define RIG_TYPE_TUNER		RIG_FLAG_TUNER


/**
 * \brief Development status of the backend
 */
enum rig_status_e {
  RIG_STATUS_ALPHA = 0,		/*!< Alpha quality, i.e. development */
  RIG_STATUS_UNTESTED,		/*!< Written from available specs, rig unavailable for test, feedback wanted! */
  RIG_STATUS_BETA,		/*!< Beta quality */
  RIG_STATUS_STABLE,		/*!< Stable */
  RIG_STATUS_BUGGY,		/*!< Was stable, but something broke it! */
  RIG_STATUS_NEW		/*!< Initial release of code */
};

/**
 * \brief Repeater shift sense
 */
enum rptr_shift_e {
  RIG_RPT_SHIFT_NONE = 0,	/*!< No repeater shift */
  RIG_RPT_SHIFT_MINUS,		/*!< "-" shift */
  RIG_RPT_SHIFT_PLUS		/*!< "+" shift */
};

/** \brief Repeater shift type */
typedef enum rptr_shift_e rptr_shift_t;

/**
 * \brief Split mode
 */
enum split_e {
  RIG_SPLIT_OFF = 0,		/*!< Split mode disabled */
  RIG_SPLIT_ON,			/*!< Split mode enabled */
};

/** \brief Split mode type */
typedef enum split_e split_t;

/**
 * \brief Frequency type,
 * Frequency type in Hz, able to hold SHF frequencies (64bits) */
typedef long long freq_t;
/**
 * \brief Short frequency type
 * Frequency on 31bits, suitable for offsets, shifts, etc..
 */
typedef signed long shortfreq_t;

#define Hz(f)	((freq_t)(f))
#define kHz(f)	((freq_t)((f)*(freq_t)1000))
#define MHz(f)	((freq_t)((f)*(freq_t)1000000))
#define GHz(f)	((freq_t)((f)*(freq_t)1000000000))

#define RIG_FREQ_NONE Hz(0)


/**
 * \brief VFO definition
 *
 * There's several way of using a vfo_t. For most cases, using RIG_VFO_A,
 * RIG_VFO_B, RIG_VFO_CURR, etc., as opaque macros should suffice.
 *
 * Strictly speaking a VFO is Variable Frequency Oscillator.
 * Here, it is referred as a tunable channel, from the radio operator
 * point of view. The channel can be designated individualy by its real 
 * number, or using an alias.
 * Aliases may, or may not be honored by backend, and are defined using
 * high significant bits, like RIG_VFO_MEM, RIG_VFO_MAIN, etc.
 *
 */
typedef int vfo_t;

/** \brief used in caps */
#define RIG_VFO_NONE    0

#define RIG_VFO_TX_FLAG    ((vfo_t)(1<<30))

/** \brief current "tunable channel"/VFO */
#define RIG_VFO_CURR    ((vfo_t)(1<<29))

/** \brief means Memory mode, to be used with set_vfo */
#define RIG_VFO_MEM	((vfo_t)(1<<28))

/** \brief means (last or any)VFO mode, with set_vfo */
#define RIG_VFO_VFO	((vfo_t)(1<<27))

#define RIG_VFO_TX_VFO(v)	((v)|RIG_VFO_TX_FLAG)

/** \brief alias for split tx or uplink, of VFO_CURR  */
#define RIG_VFO_TX	RIG_VFO_TX_VFO(RIG_VFO_CURR)

/** \brief alias for split rx or downlink */
#define RIG_VFO_RX	RIG_VFO_CURR

/** \brief alias for MAIN */
#define RIG_VFO_MAIN	((vfo_t)(1<<26))
/** \brief alias for SUB */
#define RIG_VFO_SUB	((vfo_t)(1<<25))

#define RIG_VFO_N(n) ((vfo_t)(1<<(n)))

/** \brief VFO A */
#define RIG_VFO_A RIG_VFO_N(0)
/** \brief VFO B */
#define RIG_VFO_B RIG_VFO_N(1)
/** \brief VFO C */
#define RIG_VFO_C RIG_VFO_N(2)


#define RIG_TARGETABLE_NONE 0x00
#define RIG_TARGETABLE_FREQ 0x01
#define RIG_TARGETABLE_MODE 0x02
#define RIG_TARGETABLE_ALL  0xffffffffU


#define RIG_PASSBAND_NORMAL Hz(0)
/**
 * \brief Passband width, in Hz
 * \sa rig_passband_normal, rig_passband_narrow, rig_passband_wide
 */
typedef shortfreq_t pbwidth_t;


/**
 * \brief DCD status
 */
enum dcd_e {
  RIG_DCD_OFF = 0,		/*!< Squelch closed */
  RIG_DCD_ON			/*!< Squelch open */
};
/** \brief dcd_t type */
typedef enum dcd_e dcd_t;

/**
 * \brief DCD type
 * \sa rig_get_dcd
 */
enum dcd_type_e {
  RIG_DCD_NONE = 0,		/*!< No DCD available */
  RIG_DCD_RIG,			/*!< Rig has DCD status support, i.e. rig has get_dcd cap */
  RIG_DCD_SERIAL_DSR,		/*!< DCD status from serial DSR signal */
  RIG_DCD_SERIAL_CTS,		/*!< DCD status from serial CTS signal */
  RIG_DCD_PARALLEL,		/*!< DCD status from parallel port pin */
};
/** \brief DCD type */
typedef enum dcd_type_e dcd_type_t;


/**
 * \brief PTT status
 */
enum ptt_e {
  RIG_PTT_OFF = 0,		/*!< PTT activated */
  RIG_PTT_ON			/*!< PTT desactivated */
};
/** \brief PTT status */
typedef enum ptt_e ptt_t;

/**
 * \brief PTT type
 * \sa rig_get_ptt
 */
enum ptt_type_e {
  RIG_PTT_NONE = 0,		/*!< No PTT available */
  RIG_PTT_RIG,			/*!< Legacy PTT */
  RIG_PTT_SERIAL_DTR,		/*!< PTT control through serial DTR signal */
  RIG_PTT_SERIAL_RTS,		/*!< PTT control through serial RTS signal */
  RIG_PTT_PARALLEL		/*!< PTT control through parallel port */
};
/** \brief PTT type */
typedef enum ptt_type_e ptt_type_t;

/**
 * \brief Radio power state
 */
enum powerstat_e {
  RIG_POWER_OFF = 0,		/*!< Power off */
  RIG_POWER_ON,			/*!< Power on */
  RIG_POWER_STANDBY		/*!< Standby */
};
/** \brief Power state */
typedef enum powerstat_e powerstat_t;

/**
 * \brief Reset operation
 */
enum reset_e {
  RIG_RESET_NONE = 0,		/*!< No reset */
  RIG_RESET_SOFT,		/*!< Software reset */
  RIG_RESET_VFO,		/*!< VFO reset */
  RIG_RESET_MCALL,		/*!< Memory clear */
  RIG_RESET_MASTER,		/*!< Master reset */
};
/** \brief Reset operation */
typedef enum reset_e reset_t;


/**
 * \brief VFO operation
 * A VFO operation is an action on a VFO.
 * The difference with a function is that an action has no on/off
 * status, it is performed once.
 *
 * Note: the vfo argument for some vfo operation may be irrelevant,
 * and thus will be ignored.
 */
typedef long vfo_op_t;

/* VFO/MEM mode are set by set_vfo */
#define RIG_OP_NONE	0
#define RIG_OP_CPY		(1<<0)	/* VFO A = VFO B */
#define RIG_OP_XCHG		(1<<1)	/* Exchange VFO A/B */
#define RIG_OP_FROM_VFO	(1<<2)	/* VFO->MEM */
#define RIG_OP_TO_VFO	(1<<3)	/* MEM->VFO */
#define RIG_OP_MCL		(1<<4)	/* Memory clear */
#define RIG_OP_UP		(1<<5)	/* UP */
#define RIG_OP_DOWN		(1<<6)	/* DOWN */
#define RIG_OP_BAND_UP		(1<<7)	/* Band UP */
#define RIG_OP_BAND_DOWN	(1<<8)	/* Band DOWN */
#define RIG_OP_LEFT		(1<<9)	/* LEFT */
#define RIG_OP_RIGHT	(1<<10)	/* RIGHT */


#define RIG_SCAN_NONE	0L
#define RIG_SCAN_STOP	RIG_SCAN_NONE
#define RIG_SCAN_MEM	(1L<<0)		/* Scan all memory channels */
#define RIG_SCAN_SLCT	(1L<<1)		/* Scan all selected memory channels */
#define RIG_SCAN_PRIO	(1L<<2)		/* Priority watch (mem or call channel) */
#define RIG_SCAN_PROG	(1L<<3)		/* Programmed(edge) scan */
#define RIG_SCAN_DELTA	(1L<<4)		/* delta-f scan */
#define RIG_SCAN_VFO	(1L<<5)		/* most basic scan */

/** \brief Scan type */
typedef long scan_t;

/**
 * configuration token
 */
typedef long token_t;

#define RIG_CONF_END 0

/*
 * strongly inspired from soundmedem. Thanks Thomas!
 */
#define RIG_CONF_STRING      0
#define RIG_CONF_COMBO       1
#define RIG_CONF_NUMERIC     2
#define RIG_CONF_CHECKBUTTON 3

#define RIG_COMBO_MAX	8

/**
 * \brief Configuration parameter structure.
 */
struct confparams {
  token_t token;		/*!< Conf param token ID */
  const char *name;		/*!< Param name, no spaces allowed */
  const char *label;		/*!< Human readable label */
  const char *tooltip;		/*!< Hint on the parameter */
  const char *dflt;		/*!< Default value */
  unsigned int type;		/*!< Type of the parameter */
  union {			/*!< */
	struct {		/*!< */
		float min;	/*!< Minimum value */
		float max;	/*!< Maximum value */
		float step;	/*!< Step */
	} n;			/*!< Numeric type */
	struct {		/*!< */
		const char *combostr[RIG_COMBO_MAX];	/*!< Combo list */
	} c;			/*!< Combo type */
  } u;				/*!< Type union */
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

/** \brief Announce */
typedef long ann_t;


/* Antenna number */
typedef int ant_t;

#define RIG_ANT_NONE	0
#define RIG_ANT_1	(1<<0)
#define RIG_ANT_2	(1<<1)
#define RIG_ANT_3	(1<<2)
#define RIG_ANT_4	(1<<3)


/* TODO: kill me, and replace by real AGC delay */
enum agc_level_e {
	RIG_AGC_OFF = 0,
	RIG_AGC_SUPERFAST,
	RIG_AGC_FAST,
	RIG_AGC_SLOW,
	RIG_AGC_USER		/* some rig's are selectable :) --kd7eni */
};

/**
 * \brief Level display meters
 */
enum meter_level_e {
  RIG_METER_NONE = 0,		/*< No display meter */
  RIG_METER_SWR,		/*< Stationary Wave Ratio */
  RIG_METER_COMP,		/*< Compression level */
  RIG_METER_ALC,		/*< ALC */
  RIG_METER_IC,			/*< IC */
  RIG_METER_DB,			/*< DB */
};

/**
 * \brief Universal approach for passing values
 * \sa rig_set_level, rig_get_level, rig_set_parm, rig_get_parm
 */
union value_u {
  signed int i;			/*!< Signed integer */
  float f;			/*!< Single precision float */
  char *s;			/*!< Pointer to char string */
  const char *cs;		/*!< Pointer to constant char string */
};
/** \brief Value */
typedef union value_u value_t;

#define RIG_LEVEL_NONE		0ULL
#define RIG_LEVEL_PREAMP	(1<<0)	/* Preamp, arg int (dB) */
#define RIG_LEVEL_ATT		(1<<1)	/* Attenuator, arg int (dB) */
#define RIG_LEVEL_VOX		(1<<2)	/* VOX delay, arg int (tenth of seconds) */
#define RIG_LEVEL_AF		(1<<3)	/* Volume, arg float [0.0..1.0] */
#define RIG_LEVEL_RF		(1<<4)	/* RF gain (not TX power), arg float [0.0..1.0] or in dB ?? -20..20 ? */
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

#define RIG_LEVEL_VOXGAIN   (1<<21)	/* VOX gain level, arg float [0.0 .. 1.0] */
#define RIG_LEVEL_VOXDELAY  RIG_LEVEL_VOX	/* VOX delay, arg int (tenth of seconds) */
#define RIG_LEVEL_ANTIVOX   (1<<22)	/* anti-VOX level, arg float [0.0 .. 1.0] */

		/* These ones are not settable */
#define RIG_LEVEL_SQLSTAT	(1<<27)	/* SQL status, arg int (open=1/closed=0). Deprecated, use get_dcd instead */
#define RIG_LEVEL_SWR		(1<<28)	/* SWR, arg float */
#define RIG_LEVEL_ALC		(1<<29)	/* ALC, arg float */
#define RIG_LEVEL_STRENGTH	(1<<30)	/* Signal strength, arg int (dB) */

#define RIG_LEVEL_FLOAT_LIST (RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_APF|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_COMP|RIG_LEVEL_BALANCE|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX)

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
#define RIG_PARM_KEYLIGHT	(1<<7)  /* Button backlight, on/off */

#define RIG_PARM_FLOAT_LIST (RIG_PARM_BACKLIGHT|RIG_PARM_BAT)
#define RIG_PARM_READONLY_LIST (RIG_PARM_BAT)

#define RIG_PARM_IS_FLOAT(l) ((l)&RIG_PARM_FLOAT_LIST)
#define RIG_PARM_SET(l) ((l)&~RIG_PARM_READONLY_LIST)

#define RIG_SETTING_MAX 64
/**
 * \brief Setting
 *
 * This can be a func, a level or a parm.
 * Each bit designate one of them, let's hope 64 bits will be enough.
 */
typedef unsigned long long setting_t;

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
#define RIG_FUNC_FAGC    	(1<<0)	/* Fast AGC */
#define RIG_FUNC_NB	    	(1<<1)	/* Noise Blanker */
#define RIG_FUNC_COMP    	(1<<2)	/* Compression */
#define RIG_FUNC_VOX    	(1<<3)	/* VOX */
#define RIG_FUNC_TONE    	(1<<4)	/* Tone */
#define RIG_FUNC_TSQL    	(1<<5)	/* may require a tone field */
#define RIG_FUNC_SBKIN    	(1<<6)	/* Semi Break-in */
#define RIG_FUNC_FBKIN    	(1<<7)	/* Full Break-in, for CW mode */
#define RIG_FUNC_ANF    	(1<<8)	/* Automatic Notch Filter (DSP) */
#define RIG_FUNC_NR     	(1<<9)	/* Noise Reduction (DSP) */
#define RIG_FUNC_AIP     	(1<<10)	/* AIP (Kenwood) */
#define RIG_FUNC_APF     	(1<<11)	/* Auto Passband Filter */
#define RIG_FUNC_MON     	(1<<12)	/* Monitor transmitted signal, != rev */
#define RIG_FUNC_MN     	(1<<13)	/* Manual Notch (Icom) */
#define RIG_FUNC_RNF     	(1<<14)	/* RTTY Filter Notch (Icom) */
#define RIG_FUNC_ARO     	(1<<15)	/* Auto Repeater Offset */
#define RIG_FUNC_LOCK     	(1<<16)	/* Lock */
#define RIG_FUNC_MUTE     	(1<<17)	/* Mute, could be emulated by LEVEL_AF */
#define RIG_FUNC_VSC     	(1<<18)	/* Voice Scan Control */
#define RIG_FUNC_REV     	(1<<19)	/* Reverse tx and rx freqs */
#define RIG_FUNC_SQL     	(1<<20)	/* Turn Squelch Monitor on/off */
#define RIG_FUNC_ABM     	(1<<21)	/* Auto Band Mode */
#define RIG_FUNC_BC     	(1<<22)	/* Beat Canceller */
#define RIG_FUNC_MBC     	(1<<23)	/* Manual Beat Canceller */
#define RIG_FUNC_LMP    	(1<<24)	/* LCD lamp ON/OFF */
#define RIG_FUNC_AFC    	(1<<25)	/* Auto Frequency Control ON/OFF */
#define RIG_FUNC_SATMODE	(1<<26)	/* Satellite mode ON/OFF (IC-910H) */
#define RIG_FUNC_SCOPE  	(1<<27)	/* Simple bandscope ON/OFF (IC-910H) */
#define RIG_FUNC_RESUME		(1<<28)	/* Scan resume */
#define RIG_FUNC_TBURST		(1<<29)	/* 1750 Hz tone burst */


/*
 * power unit macros, converts to mW
 * This is limited to 2MW on 32 bits systems.
 */
#define mW(p)	 ((int)(p))
#define Watts(p) ((int)((p)*1000))
#define W(p)	 Watts(p)
#define kW(p)	 ((int)((p)*1000000L))

/** \brief Radio mode  */
typedef unsigned int rmode_t;

/*
 * Do not use an enum since this will be used w/ rig_mode_t bit fields.
 */
#define RIG_MODE_NONE  	0
#define RIG_MODE_AM    	(1<<0)
#define RIG_MODE_CW    	(1<<1)
#define RIG_MODE_USB	(1<<2)
#define RIG_MODE_LSB	(1<<3)
#define RIG_MODE_RTTY	(1<<4)
#define RIG_MODE_FM    	(1<<5)
#define RIG_MODE_WFM   	(1<<6)	/* broadcast wide FM */
#define RIG_MODE_CWR   	(1<<7)	/* CW reverse sideband */
#define RIG_MODE_RTTYR	(1<<8)

/* macro for backends, no to be used by rig_set_mode et al. */
#define RIG_MODE_SSB  	(RIG_MODE_USB|RIG_MODE_LSB)


#define RIG_DBLST_END 0		/* end marker in a preamp/att level list */
#define RIG_IS_DBLST_END(d) ((d)==0)

/**
 * \brief Frequency range
 *
 * Put together a bunch of this struct in an array to define
 * what frequencies your rig has access to.
 */
struct freq_range_list {
  freq_t start;		/*!< Start frequency */
  freq_t end;		/*!< End frequency */
  rmode_t modes;	/*!< Bit field of RIG_MODE's */
  int low_power;	/*!< Lower RF power in mW, -1 for no power (ie. rx list) */
  int high_power;	/*!< Higher RF power in mW, -1 for no power (ie. rx list) */
  vfo_t vfo;		/*!< VFO list equipped with this range */
  ant_t ant;		/*!< Antenna list equipped with this range, 0 means all */
};
/** \brief Freq range */
typedef struct freq_range_list freq_range_t;

#define RIG_FRNG_END     {Hz(0),Hz(0),RIG_MODE_NONE,0,0,RIG_VFO_NONE}
#define RIG_IS_FRNG_END(r) ((r).start == Hz(0) && (r).end == Hz(0))

#define RIG_ITU_REGION1 1
#define RIG_ITU_REGION2 2
#define RIG_ITU_REGION3 3

/**
 * \brief Tuning step definition
 *
 * Lists the tuning steps available for each mode.
 *
 * If in the list a ts field has RIG_TS_ANY value,
 * this means the rig allows its tuning step to be
 * set to any value ranging from the lowest to the
 * highest (if any) value in the list for that mode.
 * The tuning step must be sorted in the ascending
 * order, and the RIG_TS_ANY value, if present, must
 * be the last one in the list.
 *
 * Note also that the minimum frequency resolution
 * of the rig is determined by the lowest value
 * in the Tuning step list.
 *
 * \sa rig_set_ts, rig_get_resolution
 */
struct tuning_step_list {
  rmode_t modes;	/*!< Bit field of RIG_MODE's */
  shortfreq_t ts;	/*!< Tuning step in Hz */
};

#define RIG_TS_ANY     0
#define RIG_TS_END     {RIG_MODE_NONE,0}
#define RIG_IS_TS_END(t)	((t).modes == RIG_MODE_NONE && (t).ts == 0)

/**
 * \brief Filter definition
 *
 * Lists the filters available for each mode.
 *
 * If more than one filter is available for a given mode,
 * the first entry in the array will be the default
 * filter to use for the normal passband of this mode.
 * The first entry in the array below the default normal passband
 * is the default narrow passband and the first entry in the array 
 * above the default normal passband is the default wide passband.
 * Note: if there's no lower width or upper width, then narrow or
 * respectively wide passband is equal to the default normal passband.
 *
 * If in the list a width field has RIG_FLT_ANY value,
 * this means the rig allows its passband width to be
 * set to any value ranging from the lowest to the
 * highest value (if any) in the list for that mode.
 * The RIG_TS_ANY value, if present, must
 * be the last one in the list.
 *
 * The width field is the narrowest passband in a transmit/receive chain
 * with regard to different IF.
 * 
 * \sa rig_set_mode, rig_passband_normal, rig_passband_narrow, rig_passband_wide
 */
struct filter_list {
  rmode_t modes;	/*!< Bit field of RIG_MODE's */
  pbwidth_t width;	/*!< Passband width in Hz */
};

#define RIG_FLT_ANY     0
#define RIG_FLT_END     {RIG_MODE_NONE,0}
#define RIG_IS_FLT_END(f)	((f).modes == RIG_MODE_NONE)


/*
 * Used in the channel.flags field
 */
#define RIG_CHFLAG_NONE	0
#define RIG_CHFLAG_SKIP	(1<<0)

/**
 * \brief Extension attribute definition
 *
 */
struct ext_list {
  token_t token;	/*!< Token ID */
  value_t val;		/*!< Value */
};

#define RIG_EXT_END     {0, {.i=0}}
#define RIG_IS_EXT_END(x)	((x).token == 0)

/**
 * Channel
 * \struct channel
 * \brief Channel definition
 *
 * The channel struct stores all the attributes peculiar to a VFO.
 *
 * \sa rig_set_channel
 */
struct channel {
  int channel_num;		/*!< Channel number */
  int bank_num;			/*!< Bank number */
  vfo_t vfo;			/*!< VFO */
  int ant;			/*!< Selected antenna */
  freq_t freq;			/*!< Receive frequency */
  rmode_t mode;			/*!< Receive mode */
  pbwidth_t width;		/*!< Receive passband width associated with mode */

#ifndef NO_SPLIT_FIELD 
  freq_t tx_freq;		/*!< Transmit frequency */
  rmode_t tx_mode;		/*!< Transmit mode */
  pbwidth_t tx_width;		/*!< Transmit passband width associated with mode */
#endif

  split_t split;		/*!< Split mode */
  rptr_shift_t rptr_shift;	/*!< Repeater shift */
  shortfreq_t rptr_offs;	/*!< Repeater offset */
  shortfreq_t tuning_step;	/*!< Tuning step */
  shortfreq_t rit;		/*!< RIT */
  shortfreq_t xit;		/*!< XIT */
  setting_t funcs;		/*!< Function status */
  value_t levels[RIG_SETTING_MAX];	/*!< Level values */
  tone_t ctcss_tone;		/*!< CTCSS tone */
  tone_t ctcss_sql;		/*!< CTCSS squelch tone */
  tone_t dcs_code;		/*!< DCS code */
  tone_t dcs_sql;		/*!< DCS squelch code */
  int scan_group;		/*!< Scan group */
  int flags;			/*!< Channel flags, see RIG_CHFLAG's */
  char channel_desc[MAXCHANDESC];	/*!< Name */
  struct ext_list *ext_levels;	/*!< Extension level value list, NULL ended. ext_levels can be NULL */
};

/** \brief Channel */
typedef struct channel channel_t;

/**
 * Channel capabilities
 * \struct channel_cap
 * \brief Channel capability definition
 *
 * Definition of the attribute that can be stored/retrieved in/from memory
 */
struct channel_cap {
  unsigned bank_num:1;		/*!< Bank number */
  unsigned vfo:1;		/*!< VFO */
  unsigned ant:1;		/*!< Selected antenna */
  unsigned freq:1;		/*!< Receive frequency */
  unsigned mode:1;		/*!< Receive mode */
  unsigned width:1;		/*!< Receive passband width associated with mode */
#ifndef NO_SPLIT_FIELD 
  unsigned tx_freq:1;		/*!< Transmit frequency */
  unsigned tx_mode:1;		/*!< Transmit mode */
  unsigned tx_width:1;		/*!< Transmit passband width associated with mode */
#endif
  unsigned split:1;		/*!< Split mode */
  unsigned rptr_shift:1;	/*!< Repeater shift */
  unsigned rptr_offs:1;		/*!< Repeater offset */
  unsigned tuning_step:1;	/*!< Tuning step */
  unsigned rit:1;		/*!< RIT */
  unsigned xit:1;		/*!< XIT */
  setting_t funcs;		/*!< Function status */
  setting_t levels;		/*!< Level values */
  unsigned ctcss_tone:1;	/*!< CTCSS tone */
  unsigned ctcss_sql:1;		/*!< CTCSS squelch tone */
  unsigned dcs_code:1;		/*!< DCS code */
  unsigned dcs_sql:1;		/*!< DCS squelch code */
  unsigned scan_group:1;	/*!< Scan group */
  unsigned flags:1;		/*!< Channel flags */
  unsigned channel_desc:1;	/*!< Name */
  unsigned ext_levels:1;	/*!< Extension level value list */
};

/** \brief Channel cap */
typedef struct channel_cap channel_cap_t;


/**
 * \brief Memory channel type definition
 *
 * Definition of memory types. Depending on the type, the content
 * of the memory channel has to be interpreted accordingly.
 * For instance, a RIG_MTYPE_EDGE channel_t will hold only a start 
 * or stop frequency.
 *
 * \sa chan_list
 */
enum chan_type_e {
  RIG_MTYPE_NONE=0,		/*!< None */
  RIG_MTYPE_MEM,		/*!< Regular */
  RIG_MTYPE_EDGE,		/*!< Scan edge */
  RIG_MTYPE_CALL,		/*!< Call channel */
  RIG_MTYPE_MEMOPAD,		/*!< Memory pad */
  RIG_MTYPE_SAT			/*!< Satellite */
};

/**
 * \brief Memory channel list definition
 *
 * Example for the Ic706MkIIG (99 memory channels, 2 scan edges, 2 call chans):
\code
	chan_t chan_list[] = {
		{ 1, 99, RIG_MTYPE_MEM  },
		{ 100, 103, RIG_MTYPE_EDGE },
		{ 104, 105, RIG_MTYPE_CALL },
		RIG_CHAN_END
	}
\endcode
 */
struct chan_list {
  int start;			/*!< Starting memory channel \b number */
  int end;			/*!< Ending memory channel \b number */
  enum chan_type_e type;	/*!< Memory type. see chan_type_e */
  channel_cap_t mem_caps;	/*!< Definition of attributes that can be stored/retrieved */
};

#define RIG_CHAN_END     {0,0,RIG_MTYPE_NONE}
#define RIG_IS_CHAN_END(c)	((c).type == RIG_MTYPE_NONE)

/** \brief chan_t type */
typedef struct chan_list chan_t;

/** \brief gran_t type */
typedef	float gran_t;


/**
 * \brief Rig data structure.
 *
 * Basic rig type, can store some useful * info about different radios.
 * Each lib must be able to populate this structure, so we can make
 * useful enquiries about capablilities.
 *
 * The main idea of this struct is that it will be defined by the backend
 * rig driver, and will remain readonly for the application.
 * Fields that need to be modifiable by the application are
 * copied into the struct rig_state, which is a kind of private
 * of the RIG instance.
 * This way, you can have several rigs running within the same application,
 * sharing the struct rig_caps of the backend, while keeping their own
 * customized data.
 * NB: don't move fields around, as backend depends on it when initializing
 *     their caps.
 */
struct rig_caps {
  rig_model_t rig_model;	/*!< Rig model. */
  const char *model_name;	/*!< Model name. */
  const char *mfg_name;		/*!< Manufacturer. */
  const char *version;		/*!< Driver version. */
  const char *copyright;	/*!< Copyright info. */
  enum rig_status_e status;	/*!< Driver status. */

  int rig_type;			/*!< Rotator type. */
  enum ptt_type_e ptt_type;	/*!< Type of the PTT port. */
  enum dcd_type_e dcd_type;	/*!< Type of the DCD port. */
  enum rig_port_e port_type;	/*!< Type of communication port. */

  int serial_rate_min;		/*!< Minimal serial speed. */
  int serial_rate_max;		/*!< Maximal serial speed. */
  int serial_data_bits;		/*!< Number of data bits. */
  int serial_stop_bits;		/*!< Number of stop bits. */
  enum serial_parity_e serial_parity;		/*!< Parity. */
  enum serial_handshake_e serial_handshake;	/*!< Handshake. */

  int write_delay;		/*!< Delay between each byte sent out, in ms */
  int post_write_delay;		/*!< Delay between each commands send out, in ms */
  int timeout;			/*!< Timeout, in ms */
  int retry;			/*!< Maximum number of retries if command fails, 0 to disable */

  setting_t has_get_func;	/*!< List of get functions */
  setting_t has_set_func;	/*!< List of set functions */
  setting_t has_get_level;	/*!< List of get level */
  setting_t has_set_level;	/*!< List of set level */
  setting_t has_get_parm;	/*!< List of get parm */
  setting_t has_set_parm;	/*!< List of set parm */

  gran_t level_gran[RIG_SETTING_MAX];	/*!< level granularity (i.e. steps) */
  gran_t parm_gran[RIG_SETTING_MAX];	/*!< parm granularity (i.e. steps) */

  const struct confparams *extparms;	/*!< Extension parm list */
  const struct confparams *extlevels;	/*!< Extension level list */

  const tone_t *ctcss_list;	/*!< CTCSS tones list, zero ended */
  const tone_t *dcs_list;	/*!< DCS code list, zero ended */

  int preamp[MAXDBLSTSIZ];	/*!< Preamp list in dB, 0 terminated */
  int attenuator[MAXDBLSTSIZ];	/*!< Preamp list in dB, 0 terminated */
  shortfreq_t max_rit;		/*!< max absolute RIT */
  shortfreq_t max_xit;		/*!< max absolute XIT */
  shortfreq_t max_ifshift;	/*!< max absolute IF-SHIFT */

  ann_t announces;	/*!< Announces bit field list */

  vfo_op_t vfo_ops;	/*!< VFO op bit field list */
  scan_t scan_ops;	/*!< Scan bit field list */
  int targetable_vfo;	/*!< Bit field list of direct VFO access commands */
  int transceive;	/*!< Supported transceive mode */

  int bank_qty;		/*!< Number of banks */
  int chan_desc_sz;	/*!< Max lenght of memory channel name */

  chan_t chan_list[CHANLSTSIZ];	/*!< Channel list, zero ended */

  freq_range_t rx_range_list1[FRQRANGESIZ];	/*!< Receive frequency range list for ITU region 1 */
  freq_range_t tx_range_list1[FRQRANGESIZ];	/*!< Transmit frequency range list for ITU region 1 */
  freq_range_t rx_range_list2[FRQRANGESIZ];	/*!< Receive frequency range list for ITU region 2 */
  freq_range_t tx_range_list2[FRQRANGESIZ];	/*!< Transmit frequency range list for ITU region 2 */

  struct tuning_step_list tuning_steps[TSLSTSIZ];	/*!< Tuning step list */
  struct filter_list filters[FLTLSTSIZ];		/*!< mode/filter table, at -6dB */

  const struct confparams *cfgparams;            /*!< Configuration parametres. */
  const rig_ptr_t priv;                          /*!< Private data. */

	/*
	 * Rig API
	 *
	 */

  int (*rig_init) (RIG * rig);
  int (*rig_cleanup) (RIG * rig);
  int (*rig_open) (RIG * rig);
  int (*rig_close) (RIG * rig);

	/*
	 *  General API commands, from most primitive to least.. :()
	 *  List Set/Get functions pairs
	 */

  int (*set_freq) (RIG * rig, vfo_t vfo, freq_t freq);
  int (*get_freq) (RIG * rig, vfo_t vfo, freq_t * freq);

  int (*set_mode) (RIG * rig, vfo_t vfo, rmode_t mode,
			 pbwidth_t width);
  int (*get_mode) (RIG * rig, vfo_t vfo, rmode_t * mode,
			 pbwidth_t * width);

  int (*set_vfo) (RIG * rig, vfo_t vfo);
  int (*get_vfo) (RIG * rig, vfo_t * vfo);

  int (*set_ptt) (RIG * rig, vfo_t vfo, ptt_t ptt);
  int (*get_ptt) (RIG * rig, vfo_t vfo, ptt_t * ptt);
  int (*get_dcd) (RIG * rig, vfo_t vfo, dcd_t * dcd);

  int (*set_rptr_shift) (RIG * rig, vfo_t vfo,
			       rptr_shift_t rptr_shift);
  int (*get_rptr_shift) (RIG * rig, vfo_t vfo,
			       rptr_shift_t * rptr_shift);

  int (*set_rptr_offs) (RIG * rig, vfo_t vfo, shortfreq_t offs);
  int (*get_rptr_offs) (RIG * rig, vfo_t vfo, shortfreq_t * offs);

#ifndef NO_SPLIT_FIELD 
  int (*set_split_freq) (RIG * rig, vfo_t vfo, freq_t tx_freq);
  int (*get_split_freq) (RIG * rig, vfo_t vfo, freq_t * tx_freq);
  int (*set_split_mode) (RIG * rig, vfo_t vfo, rmode_t tx_mode,
			       pbwidth_t tx_width);
  int (*get_split_mode) (RIG * rig, vfo_t vfo, rmode_t * tx_mode,
			       pbwidth_t * tx_width);
#endif
  int (*set_split) (RIG * rig, vfo_t vfo, split_t split);
  int (*get_split) (RIG * rig, vfo_t vfo, split_t * split);

  int (*set_rit) (RIG * rig, vfo_t vfo, shortfreq_t rit);
  int (*get_rit) (RIG * rig, vfo_t vfo, shortfreq_t * rit);
  int (*set_xit) (RIG * rig, vfo_t vfo, shortfreq_t xit);
  int (*get_xit) (RIG * rig, vfo_t vfo, shortfreq_t * xit);

  int (*set_ts) (RIG * rig, vfo_t vfo, shortfreq_t ts);
  int (*get_ts) (RIG * rig, vfo_t vfo, shortfreq_t * ts);

  int (*set_dcs_code) (RIG * rig, vfo_t vfo, tone_t code);
  int (*get_dcs_code) (RIG * rig, vfo_t vfo, tone_t * code);
  int (*set_tone) (RIG * rig, vfo_t vfo, tone_t tone);
  int (*get_tone) (RIG * rig, vfo_t vfo, tone_t * tone);
  int (*set_ctcss_tone) (RIG * rig, vfo_t vfo, tone_t tone);
  int (*get_ctcss_tone) (RIG * rig, vfo_t vfo, tone_t * tone);

  int (*set_dcs_sql) (RIG * rig, vfo_t vfo, tone_t code);
  int (*get_dcs_sql) (RIG * rig, vfo_t vfo, tone_t * code);
  int (*set_tone_sql) (RIG * rig, vfo_t vfo, tone_t tone);
  int (*get_tone_sql) (RIG * rig, vfo_t vfo, tone_t * tone);
  int (*set_ctcss_sql) (RIG * rig, vfo_t vfo, tone_t tone);
  int (*get_ctcss_sql) (RIG * rig, vfo_t vfo, tone_t * tone);

  int (*power2mW) (RIG * rig, unsigned int *mwpower, float power,
			 freq_t freq, rmode_t mode);
  int (*mW2power) (RIG * rig, float *power, unsigned int mwpower,
			 freq_t freq, rmode_t mode);

  int (*set_powerstat) (RIG * rig, powerstat_t status);
  int (*get_powerstat) (RIG * rig, powerstat_t * status);
  int (*reset) (RIG * rig, reset_t reset);

  int (*set_ant) (RIG * rig, vfo_t vfo, ant_t ant);
  int (*get_ant) (RIG * rig, vfo_t vfo, ant_t * ant);

  int (*set_level) (RIG * rig, vfo_t vfo, setting_t level,
			  value_t val);
  int (*get_level) (RIG * rig, vfo_t vfo, setting_t level,
			  value_t * val);

  int (*set_func) (RIG * rig, vfo_t vfo, setting_t func, int status);
  int (*get_func) (RIG * rig, vfo_t vfo, setting_t func,
			 int *status);

  int (*set_parm) (RIG * rig, setting_t parm, value_t val);
  int (*get_parm) (RIG * rig, setting_t parm, value_t * val);

  int (*set_ext_level)(RIG *rig, vfo_t vfo, token_t token, value_t val);
  int (*get_ext_level)(RIG *rig, vfo_t vfo, token_t token, value_t *val);

  int (*set_ext_parm)(RIG *rig, token_t token, value_t val);
  int (*get_ext_parm)(RIG *rig, token_t token, value_t *val);

  int (*set_conf) (RIG * rig, token_t token, const char *val);
  int (*get_conf) (RIG * rig, token_t token, char *val);

  int (*send_dtmf) (RIG * rig, vfo_t vfo, const char *digits);
  int (*recv_dtmf) (RIG * rig, vfo_t vfo, char *digits, int *length);
  int (*send_morse) (RIG * rig, vfo_t vfo, const char *msg);

  int (*set_bank) (RIG * rig, vfo_t vfo, int bank);
  int (*set_mem) (RIG * rig, vfo_t vfo, int ch);
  int (*get_mem) (RIG * rig, vfo_t vfo, int *ch);
  int (*vfo_op) (RIG * rig, vfo_t vfo, vfo_op_t op);
  int (*scan) (RIG * rig, vfo_t vfo, scan_t scan, int ch);

  int (*set_trn) (RIG * rig, int trn);
  int (*get_trn) (RIG * rig, int *trn);

  int (*decode_event) (RIG * rig);

  int (*set_channel) (RIG * rig, const channel_t * chan);
  int (*get_channel) (RIG * rig, channel_t * chan);

  const char *(*get_info) (RIG * rig);

};

/**
 * \brief Port definition
 *
 * Of course, looks like OO painstakingly programmed in C, sigh.
 */
typedef struct {
  union {
	enum rig_port_e rig;	/*!< Communication port type */
	enum ptt_type_e ptt;	/*!< PTT port type */
	enum dcd_type_e dcd;	/*!< DCD port type */
  } type;
  int fd;			/*!< File descriptor */
  FILE *stream;			/*!< FILE stream handle */
#if defined(__CYGWIN__) || defined(_WIN32)
  HANDLE handle;		/*!< Win32 handle for serial special handling (PTT,DCD,..) */
#endif

  int write_delay;		/*!< Delay between each byte sent out, in ms */
  int post_write_delay;		/*!< Delay between each commands send out, in ms */
  struct timeval post_write_date;	/*!< hamlib internal use */
  int timeout;			/*!< Timeout, in ms */
  int retry;			/*!< Maximum number of retries, 0 to disable */

  char pathname[FILPATHLEN];	/*!< Port pathname */
  union {
	struct {
		int rate;	/*!< Serial baud rate */
		int data_bits;	/*!< Number of data bits */
		int stop_bits;	/*!< Number of stop bits */
		enum serial_parity_e parity;		/*!< Serial parity */
		enum serial_handshake_e handshake;	/*!< Serial handshake */
		enum serial_control_state_e rts_state;	/*!< RTS set state */
		enum serial_control_state_e dtr_state;	/*!< DTR set state */
	} serial;		/*!< serial attributes */
	struct {
		int pin;	/*!< Parrallel port pin number */
	} parallel;		/*!< parallel attributes */
	struct {
		struct sockaddr saddr;	/*!< Dest socket address */
	} network;		/*!< Network attributes */
  } parm;			/*!< Port parameter union */
} port_t;


/** 
 * \brief Rig state containing live data and customized fields.
 *
 * This struct contains live data, as well as a copy of capability fields
 * that may be updated (ie. customized)
 *
 * It is fine to move fields around, as this kind of struct should
 * not be initialized like caps are.
 */
struct rig_state {
	/*
	 * overridable fields
	 */
  port_t rigport;	/*!< Rig port (internal use). */
  port_t pttport;	/*!< PTT port (internal use). */
  port_t dcdport;	/*!< DCD port (internal use). */

  double vfo_comp;	/*!< VFO compensation in PPM, 0.0 to disable */

  int itu_region;	/*!< ITU region to select among freq_range_t */
  freq_range_t rx_range_list[FRQRANGESIZ];	/*!< Receive frequency range list */
  freq_range_t tx_range_list[FRQRANGESIZ];	/*!< Transmit frequency range list */

  struct tuning_step_list tuning_steps[TSLSTSIZ];	/*!< Tuning step list */

  struct filter_list filters[FLTLSTSIZ];	/*!< Mode/filter table, at -6dB */

  chan_t chan_list[CHANLSTSIZ];	/*!< Channel list, zero ended */

  shortfreq_t max_rit;		/*!< max absolute RIT */
  shortfreq_t max_xit;		/*!< max absolute XIT */
  shortfreq_t max_ifshift;	/*!< max absolute IF-SHIFT */

  ann_t announces;		/*!< Announces bit field list */

  int preamp[MAXDBLSTSIZ];	/*!< Preamp list in dB, 0 terminated */
  int attenuator[MAXDBLSTSIZ];	/*!< Preamp list in dB, 0 terminated */

  setting_t has_get_func;	/*!< List of get functions */
  setting_t has_set_func;	/*!< List of set functions */
  setting_t has_get_level;	/*!< List of get level */
  setting_t has_set_level;	/*!< List of set level */
  setting_t has_get_parm;	/*!< List of get parm */
  setting_t has_set_parm;	/*!< List of set parm */

  gran_t level_gran[RIG_SETTING_MAX];	/*!< level granularity */
  gran_t parm_gran[RIG_SETTING_MAX];	/*!< parm granularity */


	/* 
	 * non overridable fields, internal use
	 */

  int hold_decode;	/*!< set to 1 to hold the event decoder (async) otherwise 0 */
  vfo_t current_vfo;	/*!< VFO currently set */
  int transceive;	/*!< Whether the transceive mode is on */
  int vfo_list;		/*!< Complete list of VFO for this rig */
  int comm_state;	/*!< Comm port state, opened/closed. */
  rig_ptr_t priv;	/*!< Pointer to private rig state data. */
  rig_ptr_t obj;	/*!< Internal use by hamlib++ for event handling. */

};


typedef int (*freq_cb_t) (RIG *, vfo_t, freq_t, rig_ptr_t);
typedef int (*mode_cb_t) (RIG *, vfo_t, rmode_t, pbwidth_t, rig_ptr_t);
typedef int (*vfo_cb_t) (RIG *, vfo_t, rig_ptr_t);
typedef int (*ptt_cb_t) (RIG *, vfo_t, ptt_t, rig_ptr_t);
typedef int (*dcd_cb_t) (RIG *, vfo_t, dcd_t, rig_ptr_t);

/**
 * Rig callbacks
 * \struct rig_callbacks
 * \brief Callback functions and args for rig event.
 *
 * Some rigs are able to notify the host computer the operator changed 
 * the freq/mode from the front panel, depressed a button, etc.
 *
 * Event from the rig are received through async io,
 * so callback functions will be called from the SIGIO sighandler context.
 *
 * Don't set these fields directly, use instead rig_set_freq_callback et al.
 *
 * Callbacks suit very well event based programming, 
 * really appropriate in a GUI.
 *
 * \sa rig_set_freq_callback, rig_set_mode_callback, rig_set_vfo_callback,
 *	 rig_set_ptt_callback, rig_set_dcd_callback
 */
struct rig_callbacks {
  freq_cb_t freq_event;	/*!< Frequency change event */
  rig_ptr_t freq_arg;	/*!< Frequency change argument */
  mode_cb_t mode_event;	/*!< Mode change event */
  rig_ptr_t mode_arg;	/*!< Mode change argument */
  vfo_cb_t vfo_event;	/*!< VFO change event */
  rig_ptr_t vfo_arg;	/*!< VFO change argument */
  ptt_cb_t ptt_event;	/*!< PTT change event */
  rig_ptr_t ptt_arg;	/*!< PTT change argument */
  dcd_cb_t dcd_event;	/*!< DCD change event */
  rig_ptr_t dcd_arg;	/*!< DCD change argument */
  /* etc.. */
};

/**
 * Rig structure
 * \struct rig
 * \brief This is the master data structure, 
 * acting as a handle for the controlled rig.
 *
 * This is the master data structure, acting as a handle for the controlled
 * rig. A pointer to this structure is returned by the rig_init() API
 * function and is passed as a parameter to every rig specific API call.
 *
 * \sa rig_init(), rig_caps, rig_state
 */
struct rig {
  struct rig_caps *caps;		/*!< Pointer to rig capabilities */
  struct rig_state state;		/*!< Rig state */
  struct rig_callbacks callbacks;	/*!< registered event callbacks */
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

#ifndef NO_SPLIT_FIELD 
extern HAMLIB_EXPORT(int) rig_set_split_freq HAMLIB_PARAMS((RIG *rig, vfo_t vfo, freq_t tx_freq));
extern HAMLIB_EXPORT(int) rig_get_split_freq HAMLIB_PARAMS((RIG *rig, vfo_t vfo, freq_t *tx_freq));
extern HAMLIB_EXPORT(int) rig_set_split_mode HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width));
extern HAMLIB_EXPORT(int) rig_get_split_mode HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width));
#endif
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

extern HAMLIB_EXPORT(int) rig_set_ext_level HAMLIB_PARAMS((RIG *rig, vfo_t vfo,
			token_t token, value_t val));
extern HAMLIB_EXPORT(int) rig_get_ext_level HAMLIB_PARAMS((RIG *rig, vfo_t vfo,
			token_t token, value_t *val));

extern HAMLIB_EXPORT(int) rig_set_ext_parm HAMLIB_PARAMS((RIG *rig, token_t token, value_t val));
extern HAMLIB_EXPORT(int) rig_get_ext_parm HAMLIB_PARAMS((RIG *rig, token_t token, value_t *val));

extern HAMLIB_EXPORT(int) rig_ext_level_foreach HAMLIB_PARAMS((RIG *rig, int (*cfunc)(RIG*, const struct confparams *, rig_ptr_t), rig_ptr_t data));
extern HAMLIB_EXPORT(int) rig_ext_parm_foreach HAMLIB_PARAMS((RIG *rig, int (*cfunc)(RIG*, const struct confparams *, rig_ptr_t), rig_ptr_t data));
extern HAMLIB_EXPORT(const struct confparams*) rig_ext_lookup HAMLIB_PARAMS((RIG *rig, const char *name));
extern HAMLIB_EXPORT(token_t) rig_ext_token_lookup HAMLIB_PARAMS((RIG *rig, const char *name));


extern HAMLIB_EXPORT(int) rig_token_foreach HAMLIB_PARAMS((RIG *rig, int (*cfunc)(const struct confparams *, rig_ptr_t), rig_ptr_t data));
extern HAMLIB_EXPORT(const struct confparams*) rig_confparam_lookup HAMLIB_PARAMS((RIG *rig, const char *name));
extern HAMLIB_EXPORT(token_t) rig_token_lookup HAMLIB_PARAMS((RIG *rig, const char *name));

extern HAMLIB_EXPORT(int) rig_close HAMLIB_PARAMS((RIG *rig));
extern HAMLIB_EXPORT(int) rig_cleanup HAMLIB_PARAMS((RIG *rig));

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

extern HAMLIB_EXPORT(int) rig_set_channel HAMLIB_PARAMS((RIG *rig, const channel_t *chan));	/* mem */
extern HAMLIB_EXPORT(int) rig_get_channel HAMLIB_PARAMS((RIG *rig, channel_t *chan));

extern HAMLIB_EXPORT(int) rig_set_trn HAMLIB_PARAMS((RIG *rig, int trn));
extern HAMLIB_EXPORT(int) rig_get_trn HAMLIB_PARAMS((RIG *rig, int *trn));
extern HAMLIB_EXPORT(int) rig_set_freq_callback HAMLIB_PARAMS((RIG *, freq_cb_t, rig_ptr_t));
extern HAMLIB_EXPORT(int) rig_set_mode_callback HAMLIB_PARAMS((RIG *, mode_cb_t, rig_ptr_t));
extern HAMLIB_EXPORT(int) rig_set_vfo_callback HAMLIB_PARAMS((RIG *, vfo_cb_t, rig_ptr_t));
extern HAMLIB_EXPORT(int) rig_set_ptt_callback HAMLIB_PARAMS((RIG *, ptt_cb_t, rig_ptr_t));
extern HAMLIB_EXPORT(int) rig_set_dcd_callback HAMLIB_PARAMS((RIG *, dcd_cb_t, rig_ptr_t));

extern HAMLIB_EXPORT(const char *) rig_get_info HAMLIB_PARAMS((RIG *rig));

extern HAMLIB_EXPORT(const struct rig_caps *) rig_get_caps HAMLIB_PARAMS((rig_model_t rig_model));
extern HAMLIB_EXPORT(const freq_range_t *) rig_get_range HAMLIB_PARAMS((const freq_range_t range_list[], freq_t freq, rmode_t mode));

extern HAMLIB_EXPORT(pbwidth_t) rig_passband_normal HAMLIB_PARAMS((RIG *rig, rmode_t mode));
extern HAMLIB_EXPORT(pbwidth_t) rig_passband_narrow HAMLIB_PARAMS((RIG *rig, rmode_t mode));
extern HAMLIB_EXPORT(pbwidth_t) rig_passband_wide HAMLIB_PARAMS((RIG *rig, rmode_t mode));

extern HAMLIB_EXPORT(const char *) rigerror HAMLIB_PARAMS((int errnum));

extern HAMLIB_EXPORT(int) rig_setting2idx HAMLIB_PARAMS((setting_t s));
#define rig_idx2setting(i) (1ULL<<(i))

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

typedef int (*rig_probe_func_t)(const port_t *, rig_model_t, rig_ptr_t);
extern HAMLIB_EXPORT(int) rig_probe_all HAMLIB_PARAMS((port_t *p, rig_probe_func_t, rig_ptr_t));
extern HAMLIB_EXPORT(rig_model_t) rig_probe HAMLIB_PARAMS((port_t *p));


__END_DECLS

#endif /* _RIG_H */
