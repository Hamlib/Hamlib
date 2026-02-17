/*
 *  Hamlib Interface - Amplifier API header
 *  Copyright (c) 2000-2005 by Stephane Fillod
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

#ifndef _AMPLIFIER_H
#define _AMPLIFIER_H 1

#include <hamlib/rig.h>
#include <hamlib/amplist.h>

/**
 * \addtogroup amplifier
 * @{
 */

/**
 * \brief Hamlib amplifier data structures.
 *
 * \file amplifier.h
 *
 * This file contains the data structures and declarations for the Hamlib
 * amplifier Application Programming Interface (API).
 *
 * See the amplifier.c file for details on the amplifier API functions.
 */



__BEGIN_DECLS

/* Forward struct references */

struct amp;
struct amp_state;


/**
 * \brief Main amplifier handle type definition.
 *
 * \typedef typedef struct amp AMP
 *
 * The #AMP handle is returned by amp_init() and is passed as a parameter to
 * every amplifier specific API call.
 *
 * amp_cleanup() must be called when this handle is no longer needed.
 */
typedef struct amp AMP;


/**
 * \brief Type definition for
 * <a href="https://en.wikipedia.org/wiki/Standing_wave_ratio" >SWR (Standing Wave Ratio)</a>.
 *
 * \noop amp_get_swr() is not implemented.
 * \noop The \a swr_t type is used as a parameter for the amp_get_swr() function.
 *
 * The unit of \a swr_t is 1.0 to the maximum value reported by the amplifier's
 * internal antenna system tuner, i.e.
 * <a href="http://www.arrl.org/transmatch-antenna-tuner" >transmatch</a>,
 * representing the ratio of 1.0:1 to Maximum:1.
 */
typedef float swr_t;


/**
 * \brief Type definition for the
 * <a href="http://www.arrl.org/transmatch-antenna-tuner" >transmatch</a>
 * tuning values of
 * <a href="https://en.wikipedia.org/wiki/Capacitance" >capacitance</a>
 * and
 * <a href="https://en.wikipedia.org/wiki/Inductance" >inductance</a>.
 *
 * The \a tune_value_t type is used as a parameter for amp_get_level().
 *
 * The unit of \a tune_value_t is
 * <a href="https://en.wikipedia.org/wiki/Farad" >picoFarads (pF)</a>
 * or
 * <a href="https://en.wikipedia.org/wiki/Henry_(unit)" >nanoHenrys (nH)</a>.
 */
typedef int tune_value_t;


/**
 * \brief The token in the netampctl protocol for returning an error condition code.
 */
#define NETAMPCTL_RET "RPRT "


/** \brief Amplifier reset tokens. */
typedef enum amp_reset_e
{
  AMP_RESET_MEM,    /*!< Erase tuner memory. */
  AMP_RESET_FAULT,  /*!< Reset any fault. */
  AMP_RESET_AMP     /*!< For Elecraft KPA-1500. */
} amp_reset_t;

/** \brief Amplifier type flags. */
typedef enum amp_type_e
{
  AMP_FLAG_1 = (1 << 1),      /*!< TBD */
  AMP_FLAG_2 = (1 << 2)       /*!< TBD */
} amp_type_t;

// TBD AMP_TYPE
///@{
/// Amplifier type bit masks.
#define AMP_TYPE_MASK  (AMP_FLAG_1|AMP_FLAG_2)

#define AMP_TYPE_OTHER 0
#define AMP_TYPE_1     AMP_FLAG_1
#define AMP_TYPE_2     AMP_FLAG_2
#define AMP_TYPE_ALL   (AMP_FLAG_1|AMP_FLAG_2)
///@}


//! @cond Doxygen_Suppress
/**
 * \brief Amplifier Function Settings.
 *
 * Various operating functions supported by an amplifier.
 *
 * \c STRING used in the \c ampctl and \c ampctld utilities.
 *
 * \sa amp_parse_func(), amp_strfunc()
 */
#define AMP_FUNC_NONE       0                          /*!< '' -- No Function */
#define AMP_FUNC_TUNER      CONSTANT_64BIT_FLAG (1)   /*!< \c TUNER -- Enable automatic tuner */
#ifndef SWIGLUAHIDE
/* Hide the top 32 bits from the old Lua binding as they can't be represented */
#define AMP_FUNC_BIT63      CONSTANT_64BIT_FLAG (63)   /*!< **Future use**, AMP_FUNC items. */
/* 63 is this highest bit number that can be used */
#endif
//! @endcond

//! @cond Doxygen_Suppress
/**
 * \brief Amplifier Level Settings.
 *
 * Various operating levels supported by an amplifier.
 *
 * \c STRING used in the \c ampctl and \c ampctld utilities.
 *
 * \sa amp_parse_level(), amp_strlevel()
 */
enum amp_level_e
{
  AMP_LEVEL_NONE          = 0,                      /*!< '' -- No Level. */
  AMP_LEVEL_SWR           = CONSTANT_64BIT_FLAG(0), /*!< \c Standing Wave Ratio (SWR) from antenna, 1.0 or greater, type float */
  AMP_LEVEL_NH            = CONSTANT_64BIT_FLAG(1), /*!< \c Tune setting in nanohenries (nH), type int */
  AMP_LEVEL_PF            = CONSTANT_64BIT_FLAG(2), /*!< \c Tune setting in picofarads (pF), type int */
  AMP_LEVEL_PWR_INPUT     = CONSTANT_64BIT_FLAG(3), /*!< \c Input power for amplifier in watts (W), type int */
  AMP_LEVEL_PWR_FWD       = CONSTANT_64BIT_FLAG(4), /*!< \c Output power forward in watts (W), type int */
  AMP_LEVEL_PWR_REFLECTED = CONSTANT_64BIT_FLAG(5), /*!< \c Output power reflected in watts (W), type int */
  AMP_LEVEL_PWR_PEAK      = CONSTANT_64BIT_FLAG(6), /*!< \c Peak output power reading in watts (W), type int */
  AMP_LEVEL_FAULT         = CONSTANT_64BIT_FLAG(7), /*!< \c Fault code as a string message (device-dependent), type string */
  AMP_LEVEL_PWR           = CONSTANT_64BIT_FLAG(8), /*!< \c Output power setting, type float [model-specific range, defaults to 0.0 ... 1.0] - round up to nearest step where 1.0 = max power, if range not specified */
  AMP_LEVEL_WARNING       = CONSTANT_64BIT_FLAG(9), /*!< \c Warning code as a string message (device-dependent), type string */
  AMP_LEVEL_SWR_TUNER     = CONSTANT_64BIT_FLAG(10), /*!< \c Standing Wave Ratio (SWR) reported by tuner, 1.0 or greater, type float */
  AMP_LEVEL_VD_METER      = CONSTANT_64BIT_FLAG(11), /*!< \c Supply voltage in volts (V), type float */
  AMP_LEVEL_ID_METER      = CONSTANT_64BIT_FLAG(12), /*!< \c Current draw in amperes (A), type float */
  AMP_LEVEL_TEMP_METER    = CONSTANT_64BIT_FLAG(13), /*!< \c Temperature in degrees Celsius (C), type float */
  AMP_LEVEL_63            = CONSTANT_64BIT_FLAG(63), /*!< **Future use**, last level. */
};
///@}

//! @cond Doxygen_Suppress
#define AMP_LEVEL_FLOAT_LIST  (AMP_LEVEL_SWR|AMP_LEVEL_PWR|AMP_LEVEL_SWR_TUNER|AMP_LEVEL_VD_METER|AMP_LEVEL_ID_METER|AMP_LEVEL_TEMP_METER)
#define AMP_LEVEL_STRING_LIST  (AMP_LEVEL_FAULT|AMP_LEVEL_WARNING)
#define AMP_LEVEL_IS_FLOAT(l) ((l)&AMP_LEVEL_FLOAT_LIST)
#define AMP_LEVEL_IS_STRING(l) ((l)&AMP_LEVEL_STRING_LIST)

#define AMP_LEVEL_READONLY_LIST (AMP_LEVEL_SWR|AMP_LEVEL_SWR_TUNER|AMP_LEVEL_VD_METER|AMP_LEVEL_ID_METER|AMP_LEVEL_TEMP_METER|AMP_LEVEL_PWR_INPUT|AMP_LEVEL_PWR_FWD|AMP_LEVEL_PWR_REFLECTED|AMP_LEVEL_PWR_PEAK|AMP_LEVEL_FAULT|AMP_LEVEL_WARNING)
#define AMP_LEVEL_SET(l) ((l)&~AMP_LEVEL_READONLY_LIST)
//! @endcond

//! @cond Doxygen_Suppress
/**
 * \brief Amplifier Parameters
 *
 * Parameters are settings that are not related to the RF output of the amplifier.\n
 * \c STRING used in ampctl
 *
 * \sa amp_parse_parm(), amp_strparm()
 */
enum amp_parm_e {
    AMP_PARM_NONE =         0,          /*!< '' -- No Parm */
    AMP_PARM_BACKLIGHT =    (1 << 1),   /*!< \c BACKLIGHT -- LCD light, float [0.0 ... 1.0] */
    AMP_PARM_BEEP =         (1 << 2),   /*!< \c BEEP -- Beep on keypressed, int (0,1) */
};
//! @endcond

//! @cond Doxygen_Suppress
#define AMP_PARM_FLOAT_LIST  (AMP_PARM_BACKLIGHT)
#define AMP_PARM_STRING_LIST  (AMP_PARM_NONE)
#define AMP_PARM_IS_FLOAT(l) ((l)&AMP_PARM_FLOAT_LIST)
#define AMP_PARM_IS_STRING(l) ((l)&AMP_PARM_STRING_LIST)

#define AMP_PARM_READONLY_LIST (0)
#define AMP_PARM_SET(l) ((l)&~AMP_PARM_READONLY_LIST)
//! @endcond

/* Basic amp type, can store some useful info about different amplifiers. Each
 * lib must be able to populate this structure, so we can make useful
 * enquiries about capabilities.
 */

/**
 * Convenience macro to map the `amp_model` number and `macro_name` string from amplist.h.
 *
 * Used when populating a backend amp_caps structure.
 */
#define AMP_MODEL(arg) .amp_model=arg,.macro_name=#arg

/**
 * \brief AMP operation
 *
 * A AMP operation is an action on a the amplifier.
 * The difference with a function is that an action has no on/off
 * status, it is performed at once.
 *
 * \c STRING used in ampctl
 *
 * \sa amp_parse_amp_op(), amp_strampop()
 */
typedef enum {
    AMP_OP_NONE =       0,          /*!< '' No AMP_OP */
    AMP_OP_TUNE =       (1 << 0),   /*!< \c TUNE -- Start tuning */
    AMP_OP_BAND_UP =    (1 << 1),   /*!< \c BAND_UP -- Band UP */
    AMP_OP_BAND_DOWN =  (1 << 2),   /*!< \c BAND_DOWN -- Band DOWN */
    AMP_OP_L_NH_UP =    (1 << 3),   /*!< \c L_NH_UP -- Tune manually L (nH) UP */
    AMP_OP_L_NH_DOWN =  (1 << 4),   /*!< \c L_NH_DOWN -- Tune manually L (nH) DOWN */
    AMP_OP_C_PF_UP =    (1 << 5),   /*!< \c C_PF_UP -- Tune manually C (pF) UP */
    AMP_OP_C_PF_DOWN =  (1 << 6),   /*!< \c C_PF_DOWN -- Tune manually C (pF) DOWN */
} amp_op_t;


/**
 * \brief Amplifier status flags - common faults, warnings and other status indicators for amplifier.
 *
 * Faults prevent the amplifier from working and usually trigger transition from OPERATE to STANDBY state.
 * Warnings indicate a possible issue, but the conditions still allow amplifier to operate.
 */
typedef enum {
    AMP_STATUS_NONE =                       0,        /*!< '' -- No status. */
    AMP_STATUS_PTT =                        (1 << 0), /*!< PTT is active, amplifier is transmitting. */
    AMP_STATUS_FAULT_SWR =                  (1 << 1), /*!< SWR exceeds limits. */
    AMP_STATUS_FAULT_INPUT_POWER =          (1 << 2), /*!< Input power too high. */
    AMP_STATUS_FAULT_TEMPERATURE =          (1 << 3), /*!< Temperature too high. */
    AMP_STATUS_FAULT_PWR_FWD =              (1 << 4), /*!< Forward power too high. */
    AMP_STATUS_FAULT_PWR_REFLECTED =        (1 << 5), /*!< Reflected power too high. */
    AMP_STATUS_FAULT_VOLTAGE =              (1 << 6), /*!< Voltage too high or too low. */
    AMP_STATUS_FAULT_FREQUENCY =            (1 << 7), /*!< Frequency/band not supported by the amplifier. */
    AMP_STATUS_FAULT_TUNER_NO_MATCH =       (1 << 8), /*!< Tuner did not find a match. */
    AMP_STATUS_FAULT_OTHER =                (1 << 9), /*!< Other fault. Get model-specific fault with \c AMP_LEVEL_FAULT */
    AMP_STATUS_WARNING_SWR_HIGH =           (1 << 10), /*!< SWR is high. */
    AMP_STATUS_WARNING_POWER_LIMIT =        (1 << 11), /*!< Power limit exceeded. */
    AMP_STATUS_WARNING_TEMPERATURE =        (1 << 12), /*!< Temperature high. */
    AMP_STATUS_WARNING_FREQUENCY =          (1 << 13), /*!< Frequency/band not supported by the amplifier. */
    AMP_STATUS_WARNING_TUNER_NO_INPUT =     (1 << 14), /*!< Tuning with no input power. */
    AMP_STATUS_WARNING_OTHER  =             (1 << 15), /*!< Other warning. Get model-specific warning with \c AMP_LEVEL_WARNING */
} amp_status_t;

//! @cond Doxygen_Suppress
/* So far only used in tests/sprintflst.c. */
#define AMP_STATUS_N(n)        (1u<<(n))
//! @endcond


/**
 * \brief Amplifier frequency range
 *
 * Put together a group of this struct in an array to define
 * what frequencies the amplifier has access to.
 */
typedef struct amp_freq_range_list {
    freq_t startf;      /*!< Start frequency */
    freq_t endf;        /*!< End frequency */
    rmode_t modes;      /*!< Bit field of RIG_MODE's supported for this range, 0 means any mode */
    int low_power;      /*!< Lower RF power in mW, -1 for no power (ie. rx list) */
    int high_power;     /*!< Higher RF power in mW, -1 for no power (ie. rx list) */
    ant_t input;        /*!< Input list for this range, 0 means all (or no inputs). If a particular frequency range uses a specific input always, specify it here. */
    ant_t ant;          /*!< Antenna list equipped with this range, 0 means all, RIG_ANT_CURR means dedicated to certain bands and automatically switches, no set_ant command */
    char *label;        /*!< Label for this range that explains why.  e.g. Icom rigs USA, EUR, ITR, TPE, KOR */
} amp_freq_range_t;


/**
 * \brief Amplifier capabilities.
 *
 * The main idea of this struct is that it will be defined by the backend
 * amplifier driver and will remain read-only for the application.  Fields
 * that need to be modifiable by the application are copied into the
 * amp_state structure, which is the private memory area of the #AMP instance.
 *
 * This way you can have several amplifiers running within the same
 * application, sharing the amp_caps structure of the backend, while keeping
 * their own customized data.
 *
 * \b Note: Don't move fields around and only add new fields at the end of the
 * amp_caps structure.  Shared libraries and DLLs depend on a constant
 * structure to maintain compatibility.
 */
struct amp_caps
{
  amp_model_t amp_model;                      /*!< Amplifier model as defined in amplist.h. */
  const char *model_name;                     /*!< Model name, e.g. MM-5k. */
  const char *mfg_name;                       /*!< Manufacturer, e.g. Moonbeam. */
  const char *version;                        /*!< Driver version, typically in YYYYMMDD.x format. */
  const char *copyright;                      /*!< Copyright info (should be LGPL). */
  enum rig_status_e status;                   /*!< Driver status. */

  int amp_type;                               /*!< Amplifier type. */
  enum rig_port_e port_type;                  /*!< Type of communication port (serial, ethernet, etc.). */

  int serial_rate_min;                        /*!< Minimal serial speed. */
  int serial_rate_max;                        /*!< Maximal serial speed. */
  int serial_data_bits;                       /*!< Number of data bits. */
  int serial_stop_bits;                       /*!< Number of stop bits. */
  enum serial_parity_e serial_parity;         /*!< Parity. */
  enum serial_handshake_e serial_handshake;   /*!< Handshake. */

  int write_delay;                            /*!< Write delay. */
  int post_write_delay;                       /*!< Post-write delay. */
  int timeout;                                /*!< Timeout. */
  int retry;                                  /*!< Number of retries if a command fails. */

  const struct confparams *cfgparams;         /*!< Configuration parameters. */
  const rig_ptr_t priv;                       /*!< Private data. */
  const char *amp_model_macro_name;           /*!< Model macro name. */

  setting_t has_get_level;                    /*!< List of get levels. */
  setting_t has_set_level;                    /*!< List of set levels.  */

  gran_t level_gran[RIG_SETTING_MAX];         /*!< Level granularity. */
  gran_t parm_gran[RIG_SETTING_MAX];          /*!< Parameter granularity. */

  /*
   * Amp Admin API
   *
   */

  int (*amp_init)(AMP *amp);    /*!< Pointer to backend implementation of amp_init(). */
  int (*amp_cleanup)(AMP *amp); /*!< Pointer to backend implementation of amp_cleanup(). */
  int (*amp_open)(AMP *amp);    /*!< Pointer to backend implementation of amp_open(). */
  int (*amp_close)(AMP *amp);   /*!< Pointer to backend implementation of amp_close(). */

  int (*set_freq)(AMP *amp, freq_t val);  /*!< Pointer to backend implementation of amp_set_freq(). */
  int (*get_freq)(AMP *amp, freq_t *val); /*!< Pointer to backend implementation of amp_get_freq(). */

  int (*set_conf)(AMP *amp, hamlib_token_t token, const char *val); /*!< Pointer to backend implementation of amp_set_conf(). */
  int (*get_conf2)(AMP *amp, hamlib_token_t token, char *val, int val_len);       /*!< Pointer to backend implementation of amp_get_conf2(). */
  int (*get_conf)(AMP *amp, hamlib_token_t token, char *val);       /*!< Pointer to backend implementation of amp_get_conf(). */

  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */

  int (*reset)(AMP *amp, amp_reset_t reset);                   /*!< Pointer to backend implementation of amp_reset(). */
  int (*get_level)(AMP *amp, setting_t level, value_t *val);   /*!< Pointer to backend implementation of amp_get_level(). */
  int (*set_level)(AMP *amp, setting_t level, value_t val);    /*!< Pointer to backend implementation of amp_get_level(). */
  int (*get_ext_level)(AMP *amp, hamlib_token_t level, value_t *val); /*!< Pointer to backend implementation of amp_get_ext_level(). */
  int (*set_ext_level)(AMP *amp, hamlib_token_t level, value_t val);  /*!< Pointer to backend implementation of amp_set_ext_level(). */
  int (*set_powerstat)(AMP *amp, powerstat_t status);          /*!< Pointer to backend implementation of amp_set_powerstat(). */
  int (*get_powerstat)(AMP *amp, powerstat_t *status);         /*!< Pointer to backend implementation of amp_get_powerstat(). */

  /* get firmware info, etc. */
  const char *(*get_info)(AMP *amp); /*!< Pointer to backend implementation of amp_get_info(). */

//! @cond Doxygen_Suppress
  setting_t levels;         /*!< Levels bit mask */
  unsigned ext_levels;      /*!< Extension levels value. */
//! @endcond
  const struct confparams *extlevels;         /*!< Extension levels list.  \sa amp_ext.c */
  const struct confparams *extparms;          /*!< Extension parameters list.  \sa amp_ext.c */

  const char *macro_name;                     /*!< Amplifier model macro name. */

  amp_status_t has_status;                    /*!< Supported status flags. */

  amp_op_t amp_ops;                           /*!< AMP op bit field list */

  setting_t has_get_func;                     /*!< List of get functions. */
  setting_t has_set_func;                     /*!< List of set functions. */
  setting_t has_get_parm;                     /*!< List of get parameters. */
  setting_t has_set_parm;                     /*!< List of set parameters. */

  const struct confparams *extfuncs;          /*!< Extension func list.  \sa amp_ext.c */
  int *ext_tokens;                            /*!< Extension token list. */

  int (*get_status)(AMP *amp, amp_status_t *status);        /*!< Pointer to backend implementation of ::amp_get_status(). */

  int (*amp_op)(AMP *amp, amp_op_t op);                     /*!< Pointer to backend implementation of ::amp_op(). */

  int (*set_input)(AMP *amp, ant_t input);                  /*!< Pointer to backend implementation of ::amp_set_input(). */
  int (*get_input)(AMP *amp, ant_t *input);                 /*!< Pointer to backend implementation of ::amp_get_input(). */

  int (*set_ant)(AMP *amp, ant_t ant);                      /*!< Pointer to backend implementation of ::amp_set_ant(). */
  int (*get_ant)(AMP *amp, ant_t *ant);                     /*!< Pointer to backend implementation of ::amp_get_ant(). */

  int (*set_func)(AMP *amp, setting_t func, int status);     /*!< Pointer to backend implementation of ::amp_set_func(). */
  int (*get_func)(AMP *amp, setting_t func, int *status);    /*!< Pointer to backend implementation of ::amp_get_func(). */

  int (*set_parm)(AMP *amp, setting_t parm, value_t val);    /*!< Pointer to backend implementation of ::amp_set_parm(). */
  int (*get_parm)(AMP *amp, setting_t parm, value_t *val);   /*!< Pointer to backend implementation of ::amp_get_parm(). */

  int (*set_ext_func)(AMP *amp, hamlib_token_t token, int status);  /*!< Pointer to backend implementation of ::amp_set_ext_func(). */
  int (*get_ext_func)(AMP *amp, hamlib_token_t token, int *status); /*!< Pointer to backend implementation of ::amp_get_ext_func(). */

  int (*set_ext_parm)(AMP *amp, hamlib_token_t token, value_t val);     /*!< Pointer to backend implementation of ::amp_set_ext_parm(). */
  int (*get_ext_parm)(AMP *amp, hamlib_token_t token, value_t *val);    /*!< Pointer to backend implementation of ::amp_get_ext_parm(). */

  amp_freq_range_t range_list1[HAMLIB_FRQRANGESIZ];              /*!< Amplifier frequency range list #1 */
  amp_freq_range_t range_list2[HAMLIB_FRQRANGESIZ];              /*!< Amplifier frequency range list #2 */
  amp_freq_range_t range_list3[HAMLIB_FRQRANGESIZ];              /*!< Amplifier frequency range list #3 */
  amp_freq_range_t range_list4[HAMLIB_FRQRANGESIZ];              /*!< Amplifier frequency range list #4 */
  amp_freq_range_t range_list5[HAMLIB_FRQRANGESIZ];              /*!< Amplifier frequency range list #5 */
};

//---Start cut here---
// Amp state structure definition moved to include/hamlib/amp_state.h
// Temporary include until 5.0
#ifndef NO_OLD_INCLUDES

__END_DECLS

#include <hamlib/amp_state.h>

__BEGIN_DECLS

#endif

//---End cut here---
/**
 * \brief Master amplifier structure.
 *
 * Master amplifier data structure acting as the #AMP handle for the
 * controlled amplifier.  A pointer to this structure is returned by the
 * amp_init() API function and is passed as a parameter to every amplifier
 * specific API call.
 *
 * \sa amp_init(), amp_caps, amp_state
 */
struct amp
{
  struct amp_caps *caps;      /*!< Amplifier caps. */
  struct amp_state state;     /*!< Amplifier state. */
};


//! @cond Doxygen_Suppress
/* --------------- API function prototypes -----------------*/

extern HAMLIB_EXPORT(AMP *)
amp_init(amp_model_t amp_model);

extern HAMLIB_EXPORT(int)
amp_open(AMP *amp);

extern HAMLIB_EXPORT(int)
amp_close(AMP *amp);

extern HAMLIB_EXPORT(int)
amp_cleanup(AMP *amp);

extern HAMLIB_EXPORT(int)
amp_set_conf(AMP *amp,
             hamlib_token_t token,
             const char *val);
HL_DEPRECATED extern HAMLIB_EXPORT(int)
amp_get_conf(AMP *amp,
             hamlib_token_t token,
             char *val);
extern HAMLIB_EXPORT(int)
amp_get_conf2(AMP *amp,
              hamlib_token_t token,
              char *val,
              int val_len);
extern HAMLIB_EXPORT(int)
amp_set_powerstat(AMP *amp,
                  powerstat_t status);
extern HAMLIB_EXPORT(int)
amp_get_powerstat(AMP *amp,
                  powerstat_t *status);


/*
 *  General API commands, from most primitive to least.. )
 *  List Set/Get functions pairs
 */
extern HAMLIB_EXPORT(int)
amp_get_freq(AMP *amp,
             freq_t *freq);
extern HAMLIB_EXPORT(int)
amp_set_freq(AMP *amp,
             freq_t freq);

extern HAMLIB_EXPORT(int)
amp_reset(AMP *amp,
          amp_reset_t reset);

extern HAMLIB_EXPORT(const char *)
amp_get_info(AMP *amp);

extern HAMLIB_EXPORT(int)
amp_get_status(AMP *amp,
        amp_status_t *status);

extern HAMLIB_EXPORT(int)
amp_set_input(AMP *amp,
                             ant_t input);
extern HAMLIB_EXPORT(int)
amp_get_input(AMP *amp,
                             ant_t *input);

extern HAMLIB_EXPORT(int)
amp_set_ant(AMP *amp,
                           ant_t ant);
extern HAMLIB_EXPORT(int)
amp_get_ant(AMP *amp,
                           ant_t *ant);

extern HAMLIB_EXPORT(int)
amp_set_func(AMP *amp,
        setting_t func,
        int status);
extern HAMLIB_EXPORT(int)
amp_get_func(AMP *amp,
        setting_t func,
        int *status);

extern HAMLIB_EXPORT(int)
amp_get_level(AMP *amp, setting_t level, value_t *val);

extern HAMLIB_EXPORT(int)
amp_set_level(AMP *amp, setting_t level, value_t val);

extern HAMLIB_EXPORT(int)
amp_get_parm(AMP *amp, setting_t parm, value_t *val);

extern HAMLIB_EXPORT(int)
amp_set_parm(AMP *amp, setting_t parm, value_t val);

extern HAMLIB_EXPORT(int)
amp_register(const struct amp_caps *caps);

extern HAMLIB_EXPORT(int)
amp_unregister(amp_model_t amp_model);

extern HAMLIB_EXPORT(int)
amp_list_foreach(int (*cfunc)(const struct amp_caps *, rig_ptr_t),
                 rig_ptr_t data);

extern HAMLIB_EXPORT(int)
amp_load_backend(const char *be_name);

extern HAMLIB_EXPORT(int)
amp_check_backend(amp_model_t amp_model);

extern HAMLIB_EXPORT(int)
amp_load_all_backends(void);

extern HAMLIB_EXPORT(amp_model_t)
amp_probe_all(hamlib_port_t *p);

extern HAMLIB_EXPORT(int)
amp_token_foreach(AMP *amp,
                  int (*cfunc)(const struct confparams *, rig_ptr_t),
                  rig_ptr_t data);

extern HAMLIB_EXPORT(const struct confparams *)
amp_confparam_lookup(AMP *amp,
                     const char *name);

extern HAMLIB_EXPORT(hamlib_token_t)
amp_token_lookup(AMP *amp,
                 const char *name);

extern HAMLIB_EXPORT(const struct amp_caps *)
amp_get_caps(amp_model_t amp_model);

extern HAMLIB_EXPORT(setting_t)
amp_has_get_level(AMP *amp,
                  setting_t level);

extern HAMLIB_EXPORT(setting_t)
amp_has_set_level(AMP *amp,
                  setting_t level);

extern HAMLIB_EXPORT(setting_t)
amp_has_get_parm(AMP *amp,
                                setting_t parm);
extern HAMLIB_EXPORT(setting_t)
amp_has_set_parm(AMP *amp,
                                setting_t parm);

extern HAMLIB_EXPORT(setting_t)
amp_has_get_func(AMP *amp,
                                setting_t func);
extern HAMLIB_EXPORT(setting_t)
amp_has_set_func(AMP *amp,
                                setting_t func);

extern HAMLIB_EXPORT(const struct confparams *)
amp_ext_lookup(AMP *amp,
               const char *name);

extern HAMLIB_EXPORT(const struct confparams *)
amp_ext_lookup_tok(AMP *amp,
                                  hamlib_token_t token);

extern HAMLIB_EXPORT(hamlib_token_t)
amp_ext_token_lookup(AMP *amp,
                                    const char *name);

extern HAMLIB_EXPORT(int)
amp_get_ext_level(AMP *amp,
                  hamlib_token_t token,
                  value_t *val);

extern HAMLIB_EXPORT(int)
amp_set_ext_level(AMP *amp,
                  hamlib_token_t token,
                  value_t val);

extern HAMLIB_EXPORT(int)
amp_set_ext_func(AMP *amp,
                                 hamlib_token_t token,
                                 int status);
extern HAMLIB_EXPORT(int)
amp_get_ext_func(AMP *amp,
                                 hamlib_token_t token,
                                 int *status);

extern HAMLIB_EXPORT(int)
amp_set_ext_parm(AMP *amp,
                                hamlib_token_t token,
                                value_t val);
extern HAMLIB_EXPORT(int)
amp_get_ext_parm(AMP *amp,
                                hamlib_token_t token,
                                value_t *val);

extern HAMLIB_EXPORT(int)
amp_ext_func_foreach(AMP *amp,
                                     int (*cfunc)(AMP *,
                                                  const struct confparams *,
                                                  rig_ptr_t),
                                     rig_ptr_t data);
extern HAMLIB_EXPORT(int)
amp_ext_level_foreach(AMP *amp,
                                     int (*cfunc)(AMP *,
                                                  const struct confparams *,
                                                  rig_ptr_t),
                                     rig_ptr_t data);
extern HAMLIB_EXPORT(int)
amp_ext_parm_foreach(AMP *amp,
                                    int (*cfunc)(AMP *,
                                                 const struct confparams *,
                                                 rig_ptr_t),
                                    rig_ptr_t data);


extern HAMLIB_EXPORT(int)
amp_op(AMP *amp, amp_op_t op);

extern HAMLIB_EXPORT(amp_op_t)
amp_has_op(AMP *amp,
                          amp_op_t op);

extern HAMLIB_EXPORT(amp_op_t) amp_parse_amp_op(const char *s);
extern HAMLIB_EXPORT(const char *) amp_strampop(amp_op_t op);

extern HAMLIB_EXPORT(const char *) amp_strstatus(amp_status_t);

extern HAMLIB_EXPORT(setting_t) amp_parse_func(const char *s);
extern HAMLIB_EXPORT(setting_t) amp_parse_level(const char *s);
extern HAMLIB_EXPORT(setting_t) amp_parse_parm(const char *s);
extern HAMLIB_EXPORT(const char *) amp_strfunc(setting_t);
extern HAMLIB_EXPORT(const char *) amp_strlevel(setting_t);
extern HAMLIB_EXPORT(const char *) amp_strparm(setting_t);

extern HAMLIB_EXPORT(void *) amp_data_pointer(AMP *amp, rig_ptrx_t idx);

//! @endcond


/**
 * \brief Convenience macro for generating debugging messages.
 *
 * This is an alias of the rig_debug() function call and is used in the same
 * manner.
 */
#define amp_debug rig_debug

__END_DECLS

#endif /* _AMPLIFIER_H */

/** @} */
