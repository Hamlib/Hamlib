// ---------------------------------------------------------------------------
//    ADAT
// ---------------------------------------------------------------------------
//
//  adat.h
//
//  Created by Frank Goenninger DG1SBG.
//  Copyright © 2011, 2012 Frank Goenninger.
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public
//   License along with this library; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA


#if !defined( __ADAT_INCLUDED__ )
#define __ADAT_INCLUDED__

// ---------------------------------------------------------------------------
// SYSTEM INCLUDES
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
//    HAMLIB INCLUDES
// ---------------------------------------------------------------------------

#include <hamlib/rig.h>

// ---------------------------------------------------------------------------
//    GLOBAL DEFINITIONS
// ---------------------------------------------------------------------------

#define BACKEND_VER "20191206"

#define ADAT_BUFSZ                 256
#define ADAT_RESPSZ                256

#define ADAT_CR                    "\x0d"
#define ADAT_EOL                   "\x0a"
#define ADAT_BOM                   "$"       // Begin of message
#define ADAT_EOM                   ADAT_CR   // End of message

#define ADAT_ON                    1
#define ADAT_OFF                   0

#define ADAT_TOGGLE_ON             ADAT_ON
#define ADAT_TOGGLE_OFF            ADAT_OFF

#define ADAT_FREQ_UNIT_HZ          "Hz"
#define ADAT_FREQ_UNIT_HZ_LEN      2

#define ADAT_FREQ_UNIT_KHZ         "kHz"
#define ADAT_FREQ_UNIT_KHZ_LEN     3

#define ADAT_FREQ_UNIT_MHZ         "MHz"
#define ADAT_FREQ_UNIT_MHZ_LEN     3

#define ADAT_FREQ_UNIT_GHZ         "GHz"
#define ADAT_FREQ_UNIT_GHZ_LEN     3

#define TOKEN_ADAT_PRODUCT_NAME    TOKEN_BACKEND(1)

#define ADAT_SLEEP_MICROSECONDS_BETWEEN_CMDS (11*1000) // = 11 ms
#define ADAT_SLEEP_AFTER_RIG_CLOSE  2 // unit: seconds
#define ADAT_SLEEP_AFTER_RIG_OPEN   2 // unit: seconds

// ADAT VFO SET/GET DEFINITIONS

#define ADAT_NR_VFOS               3

// Each mode is defined by three values:
// ADAT_VFO_STR_... -> The string as given back by TRX when asked by
//                     $VFO?
// ADAT_VFO_RNR_... -> The Hamlib number of the mode: RIG_VFO_...
// ADAT_VFO_ANR_... -> The ADAT Nr representing the VFO when setting it

#define ADAT_VFO_STR_A             "A"
#define ADAT_VFO_RNR_A             RIG_VFO_A
#define ADAT_VFO_ANR_A             1

#define ADAT_VFO_STR_B             "B"
#define ADAT_VFO_RNR_B             RIG_VFO_B
#define ADAT_VFO_ANR_B             2

#define ADAT_VFO_STR_C             "C"
#define ADAT_VFO_RNR_C             RIG_VFO_C
#define ADAT_VFO_ANR_C             3

// ADAT MODE DEFINITIONS

#define ADAT_MODE_LENGTH           5
#define ADAT_NR_MODES              8

// Each mode is defined by three values:
// ADAT_MODE_STR_... -> The string as given back by TRX when asked by
//                      $MOD?
// ADAT_MODE_RNR_... -> The Hamlib number of the mode: RIG_MODE_...
// ADAT_MODE_ANR_... -> The ADAT Nr representing the mode when setting it

#define ADAT_MODE_STR_CW_R         "CW-R"
#define ADAT_MODE_RNR_CW_R         RIG_MODE_CWR
#define ADAT_MODE_ANR_CW_R         0

#define ADAT_MODE_STR_CW           "CW"
#define ADAT_MODE_RNR_CW           RIG_MODE_CW
#define ADAT_MODE_ANR_CW           1

#define ADAT_MODE_STR_LSB          "LSB"
#define ADAT_MODE_RNR_LSB          RIG_MODE_LSB
#define ADAT_MODE_ANR_LSB          2

#define ADAT_MODE_STR_USB          "USB"
#define ADAT_MODE_RNR_USB          RIG_MODE_USB
#define ADAT_MODE_ANR_USB          3

#define ADAT_MODE_STR_AM           "AM"
#define ADAT_MODE_RNR_AM           RIG_MODE_AM
#define ADAT_MODE_ANR_AM           5

#define ADAT_MODE_STR_AM_SL        "AM-SL"
#define ADAT_MODE_RNR_AM_SL        RIG_MODE_SAL
#define ADAT_MODE_ANR_AM_SL        6

#define ADAT_MODE_STR_AM_SU        "AM-SU"
#define ADAT_MODE_RNR_AM_SU        RIG_MODE_SAH
#define ADAT_MODE_ANR_AM_SU        7

#define ADAT_MODE_STR_FM           "FM"
#define ADAT_MODE_RNR_FM           RIG_MODE_FM
#define ADAT_MODE_ANR_FM           8

// ADAT PTT DEFINITIONS

#define ADAT_PTT_STATUS_ANR_ON     ADAT_ON
#define ADAT_PTT_STATUS_RNR_ON     RIG_PTT_ON

#define ADAT_PTT_STATUS_ANR_OFF    ADAT_OFF
#define ADAT_PTT_STATUS_RNR_OFF    RIG_PTT_OFF

// ADAT POWER LEVEL DEFINITIONS

#define ADAT_PWR_LVL_ANR_00              0
#define ADAT_PWR_LVL_RNR_00            100 // 100 mW

#define ADAT_PWR_LVL_ANR_01              1
#define ADAT_PWR_LVL_RNR_01            300 // 300 mW

#define ADAT_PWR_LVL_ANR_02              2
#define ADAT_PWR_LVL_RNR_02           1000 // ...

#define ADAT_PWR_LVL_ANR_03              3
#define ADAT_PWR_LVL_RNR_03           2000

#define ADAT_PWR_LVL_ANR_04              4
#define ADAT_PWR_LVL_RNR_04           3000

#define ADAT_PWR_LVL_ANR_05              5
#define ADAT_PWR_LVL_RNR_05           5000

#define ADAT_PWR_LVL_ANR_06              6
#define ADAT_PWR_LVL_RNR_06           7000

#define ADAT_PWR_LVL_ANR_07              7
#define ADAT_PWR_LVL_RNR_07          10000

#define ADAT_PWR_LVL_ANR_08              8
#define ADAT_PWR_LVL_RNR_08          15000

#define ADAT_PWR_LVL_ANR_09              9
#define ADAT_PWR_LVL_RNR_09          20000

#define ADAT_PWR_LVL_ANR_10             10
#define ADAT_PWR_LVL_RNR_10          25000

#define ADAT_PWR_LVL_ANR_11             11
#define ADAT_PWR_LVL_RNR_11          30000

#define ADAT_PWR_LVL_ANR_12             12
#define ADAT_PWR_LVL_RNR_12          35000

#define ADAT_PWR_LVL_ANR_13             13
#define ADAT_PWR_LVL_RNR_13          40000

#define ADAT_PWR_LVL_ANR_14             14 // Default value after reset
#define ADAT_PWR_LVL_RNR_14          45000

#define ADAT_PWR_LVL_ANR_15             15
#define ADAT_PWR_LVL_RNR_15          50000 // 50 W

#define ADAT_MAX_POWER_IN_mW         ADAT_PWR_LVL_RNR_15

// ADAT CHANNEL CAPS

#define ADAT_MEM_CAPS \
    { \
        .vfo          = 1, \
        .ant          = 1, \
        .freq         = 1, \
        .mode         = 1, \
        .width        = 1, \
        .tx_freq      = 1, \
        .tx_mode      = 1, \
        .tx_width     = 1, \
        .tx_vfo       = 1, \
        .rit          = 1, \
        .xit          = 1, \
        .tuning_step  = 1, \
        .channel_desc = 1  \
    }

#define ADAT_MEM_DESC_SIZE         64

// ADAT OPCODES - Kind of an internal command within ADAT Hamlib Backend

#define ADAT_OPCODE_BASE_PTT        110000

#define ADAT_OPCODE_PTT_SWITCH_ON   (ADAT_OPCODE_BASE_PTT + 1)
#define ADAT_OPCODE_PTT_SWITCH_OFF  (ADAT_OPCODE_BASE_PTT + 2)

// ---------------------------------------------------------------------------
//    Individual ADAT CAT commands
// ---------------------------------------------------------------------------

#define ADAT_CMD_DEF_ADAT_SPECIAL (1<<30)

// -- NIL --  (Marks the end of a cmd list)

#define ADAT_CMD_DEF_NIL 0

// -- ADAT SPECIAL: DISPLAY OFF --

#define ADAT_CMD_DEF_STRING_DISPLAY_OFF             "$VRU>"ADAT_CR

// -- ADAT SPECIAL: DISPLAY ON --

#define ADAT_CMD_DEF_STRING_DISPLAY_ON              "$VRU<"ADAT_CR

// -- ADAT SPECIAL: GET SERIAL NR --

#define ADAT_CMD_DEF_STRING_GET_SERIAL_NR           "$CIS?"ADAT_CR

// -- ADAT SPECIAL: GET FIRMWARE VERSION --

#define ADAT_CMD_DEF_STRING_GET_FW_VERSION          "$CIF?"ADAT_CR

// -- ADAT SPECIAL: GET HARDWARE VERSION --

#define ADAT_CMD_DEF_STRING_GET_HW_VERSION          "$CIH?"ADAT_CR

// -- ADAT SPECIAL: GET FIRMWARE VERSION --

#define ADAT_CMD_DEF_STRING_GET_ID_CODE             "$CID?"ADAT_CR

// -- ADAT SPECIAL: GET GUI FIRMWARE VERSION --

#define ADAT_CMD_DEF_STRING_GET_GUI_FW_VERSION      "$CIG?"ADAT_CR

// -- ADAT SPECIAL: GET OPTIONS --

#define ADAT_CMD_DEF_STRING_GET_OPTIONS             "$CIO?"ADAT_CR

// -- ADAT SPECIAL: GET CALLSIGN --

#define ADAT_CMD_DEF_STRING_GET_CALLSIGN            "$CAL?"ADAT_CR

// -- ADAT SPECIAL: SET CALLSIGN --

#define ADAT_CMD_DEF_STRING_SET_CALLSIGN            "$CAL:"

// -- HAMLIB DEFINED COMMANDS --

// -- GET FREQ --

#define ADAT_CMD_DEF_GET_FREQ                             (1<<0)
#define ADAT_CMD_DEF_STRING_GET_FREQ                      "$FRA?"ADAT_CR

// -- SET FREQ --

#define ADAT_CMD_DEF_SET_FREQ                             (1<<1)
#define ADAT_CMD_DEF_STRING_SET_FREQ                      "$FR1:"

// -- GET VFO --

// -- GET MODE --

#define ADAT_CMD_DEF_GET_MODE                             (1<<2)
#define ADAT_CMD_DEF_STRING_GET_MODE                      "$MOD?"ADAT_CR

// -- SET VFO --

#define ADAT_CMD_DEF_SET_VFO                              (1<<3)
#define ADAT_CMD_DEF_STRING_SWITCH_ON_VFO                 "$VO%1d>%s"
#define ADAT_CMD_DEF_STRING_SET_VFO_AS_MAIN_VFO           "$VO%1d%%%s"

// -- SET MODE --

#define ADAT_CMD_DEF_SET_MODE                             (1<<4)
#define ADAT_CMD_DEF_STRING_SET_MODE                      "$MOD:"

// -- SET PTT --

#define ADAT_CMD_DEF_SET_PTT                              (1<<5)
#define ADAT_CMD_DEF_STRING_SET_PTT                       "$MOX%s%s"
#define ADAT_CMD_PTT_STR_OFF                               "<"
#define ADAT_CMD_PTT_STR_ON                                ">"

// -- GET PTT --

#define ADAT_CMD_DEF_GET_PTT                              (1<<6)
#define ADAT_CMD_DEF_STRING_GET_PTT                       "$MTR?"ADAT_CR

// -- GET POWER STATUS --

// -- GET INFO --

// -- OPEN ADAT --

// -- ADAT SPECIAL: RECOVER FROM ERROR --

// ---------------------------------------------------------------------------
//    ADAT PRIVATE DATA
// ---------------------------------------------------------------------------

typedef struct _adat_priv_data
{
    int           nOpCode;

    char         *pcProductName; // Future use (USB direct I/O)

    // ADAT device info

    char         *pcSerialNr;
    char         *pcIDCode;
    char         *pcOptions;
    char         *pcFWVersion;
    char         *pcHWVersion;
    char         *pcGUIFWVersion;

    char         *pcCallsign;

    // ADAT Operational Settings: will change during TRX use

    int           nCurrentVFO;
    vfo_t         nRIGVFONr;

    freq_t        nFreq;
    char          acRXFreq[ ADAT_BUFSZ ];
    char          acTXFreq[ ADAT_BUFSZ ];

    rmode_t       nRIGMode;
    char          acADATMode[ ADAT_MODE_LENGTH + 1 ];
    int           nADATMode;

    pbwidth_t     nWidth;

    int           nADATPTTStatus;
    ptt_t         nRIGPTTStatus;


    value_t       mNB1;
    value_t       mNB2;

    value_t       mAGC;
    value_t       mRFGain;
    value_t       mIFShift;
    value_t       mRawStr;

    // ADAT Command-related Values

    char         *pcCmd;
    int           nCmdKind;

    char         *pcResult;
    int           nRC;

} adat_priv_data_t,
* adat_priv_data_ptr;

// ---------------------------------------------------------------------------
//    ADAT CAT COMMAND DATA TYPE DECLARATIONS
// ---------------------------------------------------------------------------

typedef unsigned long long adat_cmd_id_t; // Bit mask for commands. Each command
                                          // is represented by 1 bit.

// adat_cmd_def : ADAT COMMAND DEFINITION.
// Basic idea: Each command can be made of several strings to be sent
// to the ADAT device. Therefore it is possible to build aggregated
// commands which will be executed as a set of individual commands
// executed by adat_transaction(). The last value as returned by the
// commands will be set as overall command result.

typedef enum
{
    ADAT_CMD_KIND_WITH_RESULT    = 0,  // After sending a command to the ADAT,
                                       // a result has to be read.
    ADAT_CMD_KIND_WITHOUT_RESULT = 1

} adat_cmd_kind_t;

typedef struct _adat_cmd_def_t
{
    adat_cmd_id_t    nCmdId;        // Bit indicating this cmd
    adat_cmd_kind_t  nCmdKind;      // Defines if result expected

    int (*pfCmdFn)(RIG *pRig);      // Fn to be called to execute this cmd

    int              nNrCmdStrs;    // Oh my, C as a language ... I'd love to
                                    // switch to Common Lisp ... What a hack here ...
    char            *pacCmdStrs[];  // Commands to be executed if no CmdFn given

} adat_cmd_def_t,
* adat_cmd_def_ptr;

typedef struct _adat_cmd_table_t
{
    int              nNrCmds;
    adat_cmd_def_ptr adat_cmds[];

} adat_cmd_table_t,
* adat_cmd_table_ptr;

typedef struct _adat_cmd_list_t
{
    int              nNrCmds;
    adat_cmd_def_ptr adat_cmds[];

} adat_cmd_list_t,
* adat_cmd_list_ptr;

// ---------------------------------------------------------------------------
//    OTHER ADAT DATA TYPES
// ---------------------------------------------------------------------------

typedef enum
{
    ADAT_FREQ_PARSE_MODE_WITH_VFO     = 0,
    ADAT_FREQ_PARSE_MODE_WITHOUT_VFO  = 1

} adat_freq_parse_mode_t;

// ADAT MODE DEFINITION

typedef struct _adat_mode_def
{
    char     *pcADATModeStr;
    rmode_t   nRIGMode;
    int       nADATMode;

} adat_mode_def_t,
* adat_mode_def_ptr;

typedef struct _adat_mode_list
{
    int nNrModes;

    adat_mode_def_t adat_modes[ ADAT_NR_MODES ];

} adat_mode_list_t,
* adat_mode_list_ptr;

// ADAT VFO DEFINITION

typedef struct _adat_vfo_def
{
    char     *pcADATVFOStr;
    vfo_t     nRIGVFONr;
    int       nADATVFONr;

} adat_vfo_def_t,
* adat_vfo_def_ptr;

typedef struct _adat_vfo_list
{
    int nNrVFOs;

    adat_vfo_def_t adat_vfos[ ADAT_NR_VFOS ];

} adat_vfo_list_t,
* adat_vfo_list_ptr;

// ---------------------------------------------------------------------------
//    ADAT INTERNAL FUNCTION DECLARATIONS
// ---------------------------------------------------------------------------

// Helper functions

size_t trimwhitespace(char *, size_t, const char *);
int adat_print_cmd(adat_cmd_def_ptr);

int adat_parse_freq(char *, adat_freq_parse_mode_t, int *, freq_t *);

int adat_parse_mode(char *, rmode_t *, char *);
int adat_mode_rnr2anr(rmode_t, int *);
#ifdef XXREMOVEDXX
// this function wasn't referenced anywhere
int adat_mode_anr2rnr(int, rmode_t *);
#endif

#ifdef XXREMOVEDXX
// this function wasn't referenced anywhere
int adat_parse_vfo(char *, vfo_t *, int *);
#endif
int adat_vfo_rnr2anr(vfo_t, int *);
int adat_vfo_anr2rnr(int, vfo_t *);

int adat_parse_ptt(char *, int *);
int adat_ptt_rnr2anr(ptt_t, int *);
int adat_ptt_anr2rnr(int, ptt_t *);

int adat_send(RIG *, char *);
int adat_receive(RIG *, char *);

int adat_priv_set_cmd(RIG *, char *, int);
int adat_priv_set_result(RIG *, char *);
int adat_priv_clear_result(RIG *);

int adat_get_single_cmd_result(RIG *);

int adat_cmd_recover_from_error(RIG *, int);

int adat_transaction(RIG *, adat_cmd_list_ptr);

adat_priv_data_ptr adat_new_priv_data(RIG *);
void adat_del_priv_data(adat_priv_data_t **);

// Command implementation

int adat_cmd_fn_get_serial_nr(RIG *);
int adat_cmd_fn_get_fw_version(RIG *);
int adat_cmd_fn_get_hw_version(RIG *);
int adat_cmd_fn_get_gui_fw_version(RIG *);
int adat_cmd_fn_get_id_code(RIG *);
int adat_cmd_fn_get_options(RIG *);

int adat_cmd_fn_set_callsign(RIG *);
int adat_cmd_fn_get_callsign(RIG *);

int adat_cmd_fn_set_freq(RIG *);
int adat_cmd_fn_get_freq(RIG *);

int adat_cmd_fn_get_mode(RIG *);
int adat_cmd_fn_set_mode(RIG *);

int adat_cmd_fn_get_vfo(RIG *);
int adat_cmd_fn_set_vfo(RIG *);

int adat_cmd_fn_get_ptt(RIG *);
int adat_cmd_fn_set_ptt(RIG *);

// ---------------------------------------------------------------------------
//    ADAT FUNCTION DECLARATIONS
// ---------------------------------------------------------------------------

int adat_init(RIG *);
int adat_cleanup(RIG *);
int adat_reset(RIG *, reset_t);
int adat_open(RIG *);
int adat_close(RIG *);

int adat_set_conf(RIG *, token_t, const char *val);
int adat_get_conf(RIG *, token_t, char *val);

int adat_set_freq(RIG *, vfo_t, freq_t);
int adat_get_freq(RIG *, vfo_t, freq_t *);

int adat_set_vfo(RIG *, vfo_t);
int adat_get_vfo(RIG *, vfo_t *);

int adat_set_ptt(RIG *, vfo_t, ptt_t);
int adat_get_ptt(RIG *, vfo_t, ptt_t *);

int adat_set_mode(RIG *, vfo_t, rmode_t, pbwidth_t);
int adat_get_mode(RIG *, vfo_t, rmode_t *, pbwidth_t *);

int adat_set_func(RIG *, vfo_t, setting_t func, int);
int adat_get_func(RIG *, vfo_t, setting_t func, int *);

int adat_set_level(RIG *, vfo_t, setting_t level, value_t);
int adat_get_level(RIG *, vfo_t, setting_t level, value_t *);

int adat_handle_event(RIG *);

const char *adat_get_info(RIG *);

int adat_mW2power(RIG *, float *, unsigned int, freq_t, rmode_t);
int adat_power2mW(RIG *, unsigned int *, float, freq_t, rmode_t);

int adat_get_powerstat(RIG *, powerstat_t *);

extern const struct rig_caps adt_200a_caps;

// ---------------------------------------------------------------------------
//    END OF FILE
// ---------------------------------------------------------------------------

#endif
