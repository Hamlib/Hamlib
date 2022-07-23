// ---------------------------------------------------------------------------
//    ADAT Hamlib Backend
// ---------------------------------------------------------------------------
//
//  adat.c
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

#include <hamlib/config.h>

// ---------------------------------------------------------------------------
//    SYSTEM INCLUDES
// ---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
//    HAMLIB INCLUDES
// ---------------------------------------------------------------------------

#include <hamlib/rig.h>
#include "token.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "num_stdio.h"

// ---------------------------------------------------------------------------
//    ADAT INCLUDES
// ---------------------------------------------------------------------------

#include "adat.h"

// ---------------------------------------------------------------------------
//    GLOBAL DEFINITIONS
// ---------------------------------------------------------------------------

#if !defined(NDEDBUG)
#  define ADAT_DEBUG 1
#endif

#undef ADAT_DEBUG // manual override ...

// ---------------------------------------------------------------------------
//    ADAT GLOBAL VARIABLES
// ---------------------------------------------------------------------------

// DEBUG STUFF

static int gFnLevel = 0;

// ADAT MODES

static adat_mode_list_t the_adat_mode_list =
{
    ADAT_NR_MODES,

    {
        {
            ADAT_MODE_STR_CW_R,
            ADAT_MODE_RNR_CW_R,
            ADAT_MODE_ANR_CW_R
        },

        {
            ADAT_MODE_STR_CW,
            ADAT_MODE_RNR_CW,
            ADAT_MODE_ANR_CW
        },

        {
            ADAT_MODE_STR_LSB,
            ADAT_MODE_RNR_LSB,
            ADAT_MODE_ANR_LSB
        },

        {
            ADAT_MODE_STR_USB,
            ADAT_MODE_RNR_USB,
            ADAT_MODE_ANR_USB
        },

        {
            ADAT_MODE_STR_AM,
            ADAT_MODE_RNR_AM,
            ADAT_MODE_ANR_AM
        },

        {
            ADAT_MODE_STR_AM_SL,
            ADAT_MODE_RNR_AM_SL,
            ADAT_MODE_ANR_AM_SL
        },

        {
            ADAT_MODE_STR_AM_SU,
            ADAT_MODE_RNR_AM_SU,
            ADAT_MODE_ANR_AM_SU
        },

        {
            ADAT_MODE_STR_FM,
            ADAT_MODE_RNR_FM,
            ADAT_MODE_ANR_FM
        }
    }
};

// ADAT VFOS

static adat_vfo_list_t the_adat_vfo_list =
{
    ADAT_NR_VFOS,

    {
        {
            ADAT_VFO_STR_A,
            ADAT_VFO_RNR_A,
            ADAT_VFO_ANR_A
        },

        {
            ADAT_VFO_STR_B,
            ADAT_VFO_RNR_B,
            ADAT_VFO_ANR_B
        },

        {
            ADAT_VFO_STR_C,
            ADAT_VFO_RNR_C,
            ADAT_VFO_ANR_C
        }
    }
};

// ---------------------------------------------------------------------------
//    Individual ADAT CAT commands
// ---------------------------------------------------------------------------

// -- NIL --  (Marks the end of a cmd list)
#if 0
static adat_cmd_def_t adat_cmd_nil =
{
    ADAT_CMD_DEF_NIL,
    ADAT_CMD_KIND_WITHOUT_RESULT,
    NULL,

    0,
    {
        NULL
    }
};
#endif
// -- ADAT SPECIAL: DISPLAY OFF --

static adat_cmd_def_t adat_cmd_display_off =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITHOUT_RESULT,
    NULL,

    1,
    {
        ADAT_CMD_DEF_STRING_DISPLAY_OFF
    }
};

// -- ADAT SPECIAL: DISPLAY ON --

static adat_cmd_def_t adat_cmd_display_on =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITHOUT_RESULT,
    NULL,

    1,
    {
        ADAT_CMD_DEF_STRING_DISPLAY_ON
    }
};

// -- ADAT SPECIAL: SELECT VFO --

// -- ADAT SPECIAL: GET SERIAL NR --

static adat_cmd_def_t adat_cmd_get_serial_nr =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITH_RESULT,
    adat_cmd_fn_get_serial_nr,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_SERIAL_NR
    }
};

// -- ADAT SPECIAL: GET FIRMWARE VERSION --

static adat_cmd_def_t adat_cmd_get_fw_version =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITH_RESULT,
    adat_cmd_fn_get_fw_version,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_FW_VERSION
    }
};


// -- ADAT SPECIAL: GET HARDWARE VERSION --

static adat_cmd_def_t adat_cmd_get_hw_version =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITH_RESULT,
    adat_cmd_fn_get_hw_version,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_HW_VERSION
    }
};

// -- ADAT SPECIAL: GET FIRMWARE VERSION --

static adat_cmd_def_t adat_cmd_get_id_code =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITH_RESULT,
    adat_cmd_fn_get_id_code,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_ID_CODE
    }
};

// -- ADAT SPECIAL: GET GUI FIRMWARE VERSION --

static adat_cmd_def_t adat_cmd_get_gui_fw_version =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITH_RESULT,
    adat_cmd_fn_get_gui_fw_version,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_GUI_FW_VERSION
    }
};

// -- ADAT SPECIAL: GET OPTIONS --

static adat_cmd_def_t adat_cmd_get_options =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITH_RESULT,
    adat_cmd_fn_get_options,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_OPTIONS
    }
};

// -- ADAT SPECIAL: GET CALLSIGN --

static adat_cmd_def_t adat_cmd_get_callsign =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITH_RESULT,
    adat_cmd_fn_get_callsign,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_CALLSIGN
    }
};

// -- ADAT SPECIAL: SET CALLSIGN --

static adat_cmd_def_t adat_cmd_set_callsign =
{
    ADAT_CMD_DEF_ADAT_SPECIAL,
    ADAT_CMD_KIND_WITHOUT_RESULT,
    adat_cmd_fn_set_callsign,

    1,
    {
        ADAT_CMD_DEF_STRING_SET_CALLSIGN
    }
};

// -- HAMLIB DEFINED COMMANDS --

// -- GET FREQ --

static adat_cmd_def_t adat_cmd_get_freq =
{
    ADAT_CMD_DEF_GET_FREQ,
    ADAT_CMD_KIND_WITH_RESULT,

    adat_cmd_fn_get_freq,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_FREQ
    }
};

static adat_cmd_list_t adat_cmd_list_get_freq =
{
    2,
    {
        &adat_cmd_display_off,
        &adat_cmd_get_freq
    }
};

// -- SET FREQ --

static adat_cmd_def_t adat_cmd_set_freq =
{
    ADAT_CMD_DEF_SET_FREQ,
    ADAT_CMD_KIND_WITHOUT_RESULT,

    adat_cmd_fn_set_freq,

    1,
    {
        ADAT_CMD_DEF_STRING_SET_FREQ
    }
};

static adat_cmd_list_t adat_cmd_list_set_freq =
{
    3,
    {
        &adat_cmd_display_off,
        &adat_cmd_set_freq,
        &adat_cmd_get_freq,
    }
};
// -- GET VFO --

static adat_cmd_list_t adat_cmd_list_get_vfo =
{
    2,
    {
        &adat_cmd_display_off,
        &adat_cmd_get_freq,
    }
};

// -- GET MODE --

static adat_cmd_def_t adat_cmd_get_mode =
{
    ADAT_CMD_DEF_GET_MODE,
    ADAT_CMD_KIND_WITH_RESULT,

    adat_cmd_fn_get_mode,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_MODE
    }
};

static adat_cmd_list_t adat_cmd_list_get_mode =
{
    2,
    {
        &adat_cmd_display_off,
        &adat_cmd_get_mode
    }
};

// -- SET VFO --

static adat_cmd_def_t adat_cmd_set_vfo =
{
    ADAT_CMD_DEF_SET_VFO,
    ADAT_CMD_KIND_WITHOUT_RESULT,

    adat_cmd_fn_set_vfo,

    2,
    {
        ADAT_CMD_DEF_STRING_SWITCH_ON_VFO,
        ADAT_CMD_DEF_STRING_SET_VFO_AS_MAIN_VFO
    }
};

static adat_cmd_list_t adat_cmd_list_set_vfo =
{
    2,
    {
        &adat_cmd_display_off,
        &adat_cmd_set_vfo
    }
};


// -- SET MODE --

static adat_cmd_def_t adat_cmd_set_mode =
{
    ADAT_CMD_DEF_SET_MODE,
    ADAT_CMD_KIND_WITHOUT_RESULT,

    adat_cmd_fn_set_mode,

    1,
    {
        ADAT_CMD_DEF_STRING_SET_MODE
    }
};

static adat_cmd_list_t adat_cmd_list_set_mode =
{
    3,
    {
        &adat_cmd_display_off,
        &adat_cmd_set_vfo,
        &adat_cmd_set_mode,
    }
};

// -- SET PTT --

static adat_cmd_def_t adat_cmd_set_ptt =
{
    ADAT_CMD_DEF_SET_PTT,
    ADAT_CMD_KIND_WITHOUT_RESULT,

    adat_cmd_fn_set_ptt,

    1,
    {
        ADAT_CMD_DEF_STRING_SET_PTT
    }
};

static adat_cmd_list_t adat_cmd_list_set_ptt =
{
    2,
    {
        &adat_cmd_set_ptt,
        &adat_cmd_display_off
    }
};

// -- GET PTT --

static adat_cmd_def_t adat_cmd_get_ptt =
{
    ADAT_CMD_DEF_GET_PTT,
    ADAT_CMD_KIND_WITH_RESULT,

    adat_cmd_fn_get_ptt,

    1,
    {
        ADAT_CMD_DEF_STRING_GET_PTT
    }
};

static adat_cmd_list_t adat_cmd_list_get_ptt =
{
    2,
    {
        &adat_cmd_display_off,
        &adat_cmd_get_ptt
    }
};

// -- GET POWER STATUS --

static adat_cmd_list_t adat_cmd_list_get_powerstatus =
{
    1,
    {
        &adat_cmd_get_id_code
    }
};

// -- GET INFO --

static adat_cmd_list_t adat_cmd_list_get_info =
{
    7,
    {
        &adat_cmd_get_serial_nr,
        &adat_cmd_get_id_code,
        &adat_cmd_get_fw_version,
        &adat_cmd_get_gui_fw_version,
        &adat_cmd_get_hw_version,
        &adat_cmd_get_options,
        &adat_cmd_get_callsign
    }
};

// -- OPEN ADAT --

static adat_cmd_list_t adat_cmd_list_open_adat =
{
    8,
    {
        &adat_cmd_display_off,
        &adat_cmd_get_serial_nr,
        &adat_cmd_get_id_code,
        &adat_cmd_get_fw_version,
        &adat_cmd_get_gui_fw_version,
        &adat_cmd_get_hw_version,
        &adat_cmd_get_options,
        &adat_cmd_set_callsign
    }
};

// -- CLOSE ADAT --

static adat_cmd_list_t adat_cmd_list_close_adat =
{
    1,
    {
        &adat_cmd_display_on
    }
};


// -- ADAT SPECIAL: RECOVER FROM ERROR --

static adat_cmd_list_t adat_cmd_list_recover_from_error =
{
    1,
    {
        &adat_cmd_display_on
    }
};


// ---------------------------------------------------------------------------
//    IMPLEMENTATION
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// trimwhitespace - taken from Stackoverflow
// http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
// ---------------------------------------------------------------------------
// Status: RELEASED
size_t trimwhitespace(char *out, size_t len, const char *str)
{
    char    *end      = NULL;
    size_t   out_size = 0;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. In -> '%s', %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, str, (int)len);

    if (len == 0)
    {
        gFnLevel--;
        return 0;
    }

    // Trim leading space
    while (isspace((int)*str))
    {
        str++;
    }

    if (*str == 0)   // All spaces?
    {
        out = NULL;
        gFnLevel--;
        return 1;
    }

    // Trim trailing space
    end = (char *)(str + strlen(str) - 1);

    while (end > str && isspace((int)*end))
    {
        *end = '\0';
        end--;
    }

    // Set output size to minimum of trimmed string length and buffer size minus 1
    //out_size = (end - str) < len-1 ? (end - str) : len - 1;
    out_size = strlen(str);

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = 0;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Out -> \"%s\", %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, out, (int)out_size);
    gFnLevel--;

    return out_size;
}


// ---------------------------------------------------------------------------
// adat_print_cmd
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_print_cmd(adat_cmd_def_ptr pCmd)
{
    int nRC = RIG_OK;

    int nI  = 0;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %s (%s:%d): ENTRY.\n",
              __func__, __FILE__, __LINE__);

    rig_debug(RIG_DEBUG_TRACE,
              "*** -> Command ID = %u\n", (unsigned int)(pCmd->nCmdId));

    rig_debug(RIG_DEBUG_TRACE,
              "*** -> Command kind = %d\n",
              pCmd->nCmdKind);

    while (nI < pCmd->nNrCmdStrs)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "*** -> Command String %d = \"%s\"\n",
                  nI, pCmd->pacCmdStrs[ nI ]);
        nI++;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %s (%s:%d): EXIT. Return Code = %d\n",
              __func__, __FILE__, __LINE__, nRC);

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_parse_freq
// ---------------------------------------------------------------------------
// Status: RELEASED

// Can be used to parse strings with VFO nr and without VFO nr in it:
// "1 123.456kHz" => nMode = ADAT_FREQ_PARSE_MODE_WITH_VFO
// "800Hz"        => nMode = ADAT_FREQ_PARSE_MODE_WITHOUT_VFO
int adat_parse_freq(char                    *pcStr,
                    adat_freq_parse_mode_t   nMode,
                    int                     *nVFO,
                    freq_t                  *nFreq)
{
    int nRC = RIG_OK;

    gFnLevel++;
    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pcStr = \"%s\"\n",
              gFnLevel, __func__, __FILE__, __LINE__, pcStr);

    if (pcStr != NULL)
    {
        int    _nVFO  = 0;

        char   *pcEnd = NULL;

        if (nMode == ADAT_FREQ_PARSE_MODE_WITH_VFO)
        {
            // Get VFO from response string

            _nVFO = strtol(pcStr, &pcEnd, 10);

            // Save VFO

            *nVFO  = _nVFO;
        }
        else
        {
            pcEnd = pcStr;
        }

        if ((_nVFO != 0)   // VFO = 0 -> Current VFO not active.
                || (nMode == ADAT_FREQ_PARSE_MODE_WITHOUT_VFO))
        {
            char   acValueBuf[ ADAT_BUFSZ + 1 ];
            char   acUnitBuf[ ADAT_BUFSZ + 1 ];
            int    nI       = 0;
            double dTmpFreq = 0.0;
            freq_t _nFreq;

            memset(acValueBuf, 0, ADAT_BUFSZ + 1);
            memset(acUnitBuf, 0, ADAT_BUFSZ + 1);

            // Get Freq Value from response string

            while ((isalpha((int)*pcEnd) == 0)
                    || (*pcEnd == '.'))
            {
                acValueBuf[ nI++ ] = *pcEnd;
                pcEnd += sizeof(char);
            }

            dTmpFreq = strtod(acValueBuf, (char **) NULL);

            rig_debug(RIG_DEBUG_TRACE,
                      "*** ADAT: %d acValueBuf = \"%s\", dTmpFreq = %f, *pcEnd = %c\n",
                      gFnLevel, acValueBuf, dTmpFreq, *pcEnd);

            // Get Freq Unit from response string

            nI = 0;

            while (isalpha((int)*pcEnd) != 0)
            {
                acUnitBuf[ nI++ ] = *pcEnd;
                pcEnd += sizeof(char);
            }

            rig_debug(RIG_DEBUG_TRACE,
                      "*** ADAT: %d acUnitBuf = \"%s\"\n",
                      gFnLevel, acUnitBuf);

            // Normalize to Hz

            if (!strncmp(acUnitBuf,
                         ADAT_FREQ_UNIT_HZ,
                         ADAT_FREQ_UNIT_HZ_LEN))
            {
                _nFreq = Hz(dTmpFreq);
            }
            else
            {
                if (!strncmp(acUnitBuf,
                             ADAT_FREQ_UNIT_KHZ,
                             ADAT_FREQ_UNIT_KHZ_LEN))
                {
                    _nFreq = kHz(dTmpFreq);
                }
                else
                {
                    if (!strncmp(acUnitBuf,
                                 ADAT_FREQ_UNIT_MHZ,
                                 ADAT_FREQ_UNIT_MHZ_LEN))
                    {
                        _nFreq = MHz(dTmpFreq);
                    }
                    else
                    {
                        if (!strncmp(acUnitBuf,
                                     ADAT_FREQ_UNIT_GHZ,
                                     ADAT_FREQ_UNIT_GHZ_LEN))
                        {
                            _nFreq = GHz(dTmpFreq);
                        }
                        else
                        {
                            _nFreq = 0;
                            nRC = -RIG_EINVAL;
                        }
                    }
                }

            }

            // Save Freq

            *nFreq = _nFreq;
        }
    }
    else
    {
        // If input string is NULL set Freq and VFO also to NULL

        *nFreq = 0;
        *nVFO  = 0;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, nVFO = %d, nFreq = %f\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, *nVFO, *nFreq);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_parse_mode
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_parse_mode(char     *pcStr,
                    rmode_t  *nRIGMode,
                    char     *pcADATMode)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pcStr = \"%s\"\n",
              gFnLevel, __func__, __FILE__, __LINE__, pcStr);

    if (pcStr != NULL)
    {
        int nI    = 0;
        int nFini = 0;

        while ((nI < the_adat_mode_list.nNrModes) && (nFini == 0))
        {
            if (!strcmp(pcStr,
                        the_adat_mode_list.adat_modes[ nI ].pcADATModeStr))
            {
                *nRIGMode = the_adat_mode_list.adat_modes[ nI ].nRIGMode;
                nFini     = 1; // Done.
            }
            else
            {
                nI++;
            }
        }
    }
    else
    {
        // If input string is NULL ...

        *nRIGMode  = RIG_MODE_NONE;
        *pcADATMode = 0;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, Mode = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, (int)*nRIGMode);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_mode_rnr2anr
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_mode_rnr2anr(rmode_t  nRIGMode,
                      int     *nADATMode)
{
    int nRC   = RIG_OK;
    int nI    = 0;
    int nFini = 0;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: nRIGMode = %u\n",
              gFnLevel, __func__, __FILE__, __LINE__, (unsigned int)nRIGMode);

    while ((nI < the_adat_mode_list.nNrModes) && (nFini == 0))
    {
        if (the_adat_mode_list.adat_modes[ nI ].nRIGMode == nRIGMode)
        {
            *nADATMode = the_adat_mode_list.adat_modes[ nI ].nADATMode;
            nFini      = 1; // Done.
        }
        else
        {
            nI++;
        }
    }

    if (nFini == 0)
    {
        // No valid Mode given

        nRC = -RIG_EINVAL;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, ADAT Mode = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, *nADATMode);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_mode_anr2rnr
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_mode_anr2rnr(int      nADATMode,
                      rmode_t  *nRIGMode)
{
    int nRC        = RIG_OK;
    int nI    = 0;
    int nFini = 0;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: nRIGMode = %u\n",
              gFnLevel, __func__, __FILE__, __LINE__, (unsigned int)*nRIGMode);

    while ((nI < the_adat_mode_list.nNrModes) && (nFini == 0))
    {
        if (the_adat_mode_list.adat_modes[ nI ].nADATMode == nADATMode)
        {
            *nRIGMode = the_adat_mode_list.adat_modes[ nI ].nRIGMode;
            nFini     = 1; // Done.
        }
        else
        {
            nI++;
        }
    }

    if (nFini == 0)
    {
        // No valid Mode given

        nRC = -RIG_EINVAL;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, RIG Mode = %u\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, (unsigned int)*nRIGMode);
    gFnLevel--;

    return nRC;
}


#ifdef XXREMOVEDXX
// this function wasn't referenced anywhere
// ---------------------------------------------------------------------------
// adat_parse_vfo
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_parse_vfo(char   *pcStr,
                   vfo_t  *nRIGVFONr,
                   int    *nADATVFONr)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pcStr = \"%s\"\n",
              gFnLevel, __func__, __FILE__, __LINE__, pcStr);

    if (pcStr != NULL)
    {
        int nI    = 0;
        int nFini = 0;

        while ((nI < the_adat_vfo_list.nNrVFOs) && (nFini == 0))
        {
            if (!strcmp(pcStr,
                        the_adat_vfo_list.adat_vfos[ nI ].pcADATVFOStr))
            {
                *nRIGVFONr  = the_adat_vfo_list.adat_vfos[ nI ].nRIGVFONr;
                *nADATVFONr = the_adat_vfo_list.adat_vfos[ nI ].nADATVFONr;
                nFini       = 1; // Done.
            }
            else
            {
                nI++;
            }
        }

        if (nFini == 0)
        {
            nRC = -RIG_EINVAL;
        }
    }
    else
    {
        // If input string is NULL ...

        *nRIGVFONr  = RIG_VFO_NONE;
        *nADATVFONr = 0;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, RIG VFO Nr = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, *nRIGVFONr);
    gFnLevel--;

    return nRC;
}
#endif


// ---------------------------------------------------------------------------
// adat_vfo_rnr2anr
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_vfo_rnr2anr(vfo_t  nRIGVFONr,
                     int   *nADATVFONr)
{
    int nRC   = RIG_OK;
    int nI    = 0;
    int nFini = 0;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: nRIGVFONr = %u\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRIGVFONr);

    while ((nI < the_adat_vfo_list.nNrVFOs) && (nFini == 0))
    {
        if (the_adat_vfo_list.adat_vfos[ nI ].nRIGVFONr == nRIGVFONr)
        {
            *nADATVFONr = the_adat_vfo_list.adat_vfos[ nI ].nADATVFONr;
            nFini       = 1; // Done.
        }
        else
        {
            nI++;
        }
    }

    if (nFini == 0)
    {
        // No valid Mode given

        nRC = -RIG_EINVAL;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, ADAT VFO Nr = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, *nADATVFONr);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_vfo_anr2rnr
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_vfo_anr2rnr(int   nADATVFONr,
                     vfo_t *nRIGVFONr)
{
    int nRC   = RIG_OK;
    int nI    = 0;
    int nFini = 0;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: nADATVFONr = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nADATVFONr);

    while ((nI < the_adat_vfo_list.nNrVFOs) && (nFini == 0))
    {
        if (the_adat_vfo_list.adat_vfos[ nI ].nADATVFONr == nADATVFONr)
        {
            *nRIGVFONr = the_adat_vfo_list.adat_vfos[ nI ].nRIGVFONr;
            nFini      = 1; // Done.
        }
        else
        {
            nI++;
        }
    }

    if (nFini == 0)
    {
        // No valid ADAT VFO Nr given

        nRC = -RIG_EINVAL;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, RIG VFO Nr = %u\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, *nRIGVFONr);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_parse_ptt
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_parse_ptt(char *pcStr,
                   int  *nADATPTTStatus)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pcStr = \"%s\"\n",
              gFnLevel, __func__, __FILE__, __LINE__, pcStr);

    if ((pcStr != NULL) && (strlen(pcStr) > 0))
    {
        *nADATPTTStatus = strtol(pcStr, NULL, 10);
    }
    else
    {
        // If input string is NULL ...

        *nADATPTTStatus = ADAT_PTT_STATUS_ANR_OFF;
        nRC             = -RIG_EINVAL;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


#ifdef XXREMOVEDXX
// this function wasn't referenced anywhere
// ---------------------------------------------------------------------------
// adat_ptt_rnr2anr
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_ptt_rnr2anr(ptt_t  nRIGPTTStatus,
                     int   *nADATPTTStatus)
{
    int nRC   = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: nRIGPTTStatus = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRIGPTTStatus);

    switch (nRIGPTTStatus)
    {
    case ADAT_PTT_STATUS_RNR_ON:
        *nADATPTTStatus = ADAT_PTT_STATUS_ANR_ON;
        break;

    case ADAT_PTT_STATUS_RNR_OFF:
        *nADATPTTStatus = ADAT_PTT_STATUS_ANR_OFF;
        break;

    default:
        nRC = -RIG_EINVAL;
        break;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, ADAT PTT Status = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, *nADATPTTStatus);
    gFnLevel--;

    return nRC;
}
#endif


// ---------------------------------------------------------------------------
// adat_ptt_anr2rnr
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_ptt_anr2rnr(int   nADATPTTStatus,
                     ptt_t *nRIGPTTStatus)
{
    int nRC        = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: nADATPTTStatus = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nADATPTTStatus);

    switch (nADATPTTStatus)
    {
    case ADAT_PTT_STATUS_ANR_ON:
        *nRIGPTTStatus = ADAT_PTT_STATUS_RNR_ON;
        break;

    case ADAT_PTT_STATUS_ANR_OFF:
        *nRIGPTTStatus = ADAT_PTT_STATUS_RNR_OFF;
        break;

    default:
        nRC = -RIG_EINVAL;
        break;
    }

    // Done

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d, RIG PTT Status = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, *nRIGPTTStatus);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_send
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_send(RIG  *pRig,
              char *pcData)
{
    int               nRC       = RIG_OK;
    struct rig_state *pRigState = &pRig->state;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p, pcData = %s\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig, pcData);

    rig_flush(&pRigState->rigport);

    nRC = write_block(&pRigState->rigport, (unsigned char *) pcData,
                      strlen(pcData));

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_receive
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_receive(RIG  *pRig,
                 char *pcData)
{
    int               nRC       = RIG_OK;
    struct rig_state *pRigState = &pRig->state;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    nRC = read_string(&pRigState->rigport, (unsigned char *) pcData, ADAT_RESPSZ,
                      ADAT_EOL, 1, 0, 1);

    if (nRC > 0)
    {
        nRC = RIG_OK;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_priv_set_cmd
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_priv_set_cmd(RIG *pRig, char *pcCmd, int nCmdKind)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p, pcCmd = \"%s\"\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig, pcCmd);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        if (pPriv->pcCmd != NULL)
        {
            free(pPriv->pcCmd);
        }

        pPriv->pcCmd    = strdup(pcCmd);
        pPriv->nCmdKind = nCmdKind;
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_priv_set_result
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_priv_set_result(RIG *pRig, char *pcResult)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p, pcResult = \"%s\"\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig, pcResult);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        if (pPriv->pcResult != NULL)
        {
            free(pPriv->pcResult);
        }

        pPriv->pcResult = strdup(pcResult);

        rig_debug(RIG_DEBUG_TRACE,
                  "*** ADAT: %d pPriv->pcResult = \"%s\"\n",
                  gFnLevel, pPriv->pcResult);
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_priv_clear_result
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_priv_clear_result(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        if (pPriv->pcResult != NULL)
        {
            free(pPriv->pcResult);
        }

        pPriv->pcResult = NULL;
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_get_single_cmd_result
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_get_single_cmd_result(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr   pPriv     = (adat_priv_data_ptr) pRig->state.priv;
        struct rig_state    *pRigState = &pRig->state;

        nRC = adat_send(pRig, pPriv->pcCmd);

        if ((nRC == RIG_OK)
                && (pPriv->nCmdKind == ADAT_CMD_KIND_WITH_RESULT))
        {

            char  acBuf[ ADAT_RESPSZ + 1 ];
            char  acBuf2[ ADAT_RESPSZ + 1 ];
            char *pcBufEnd    = NULL;
            char *pcPos       = NULL;
            char *pcResult    = NULL;

            memset(acBuf, 0, ADAT_RESPSZ + 1);
            memset(acBuf2, 0, ADAT_RESPSZ + 1);

            nRC = adat_receive(pRig, acBuf);

            rig_debug(RIG_DEBUG_TRACE,
                      "*** ADAT: %d acBuf ........ = %p\n",
                      gFnLevel, acBuf);

            pcPos      = acBuf;

            if (nRC == RIG_OK)
            {
                int   nBufLength  = 0;

                if (*pcPos == '\0') // Adjust for 00 byte at beginning ...
                {
                    pcPos++;        // No, please don't ask me why this happens ... ;-)
                }

                nBufLength = strlen(pcPos);
                pcBufEnd   = pcPos + nBufLength - 1;

                pcResult = pcPos;    // Save position

                if (pcPos < pcBufEnd)
                {
                    int nLength = strlen(pcPos);

                    if (nLength > 0)
                    {
                        char *pcPos2 = strchr(pcPos, (char) 0x0d);

                        if (pcPos2 != NULL)
                        {
                            *pcPos2 = '\0';    // Truncate \0d\0a
                        }

                        pcPos = strchr(pcPos, ' ');

                        if ((pcPos != NULL) && (pcPos < pcBufEnd))
                        {
                            pcPos += sizeof(char);

                            rig_debug(RIG_DEBUG_TRACE,
                                      "*** ADAT: %d pcPos ........ = %p\n",
                                      gFnLevel, pcPos);

                            rig_debug(RIG_DEBUG_TRACE,
                                      "*** ADAT: %d pcBufEnd ..... = %p\n",
                                      gFnLevel, pcBufEnd);

                            rig_debug(RIG_DEBUG_TRACE,
                                      "*** ADAT: %d nBufLength ... = %d\n",
                                      gFnLevel, nBufLength);

                            rig_debug(RIG_DEBUG_TRACE,
                                      "*** ADAT: %d pcPos2 ....... = %p\n",
                                      gFnLevel, pcPos2);

                            trimwhitespace(acBuf2, strlen(pcPos), pcPos);
                            pcResult = acBuf2;
                        }
                    }
                    else
                    {
                        nRC = -RIG_EINVAL;
                    }
                }
                else
                {
                    nRC = -RIG_EINVAL;
                }

                if (nRC == RIG_OK)
                {
                    adat_priv_set_result(pRig, pcResult);
                }
                else
                {
                    adat_priv_clear_result(pRig);
                }
            }
        }

        rig_flush(&pRigState->rigport);

        pPriv->nRC = nRC;
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_recover_from_error
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_recover_from_error(RIG *pRig, int nError)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        // Recover from communication error

        if ((nError == RIG_ETIMEOUT)
                || (nError == RIG_EPROTO)
                || (nError == RIG_EIO))
        {

            rig_close(pRig);
            sleep(ADAT_SLEEP_AFTER_RIG_CLOSE);

            rig_open(pRig);
        }

        // Reset critical Priv values

        pPriv->nRC = RIG_OK;

        // Execute recovery commands

        (void) adat_transaction(pRig, &adat_cmd_list_recover_from_error);
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_callsign
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_callsign(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_CALLSIGN,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                pPriv->pcCallsign = strdup(pPriv->pcResult);

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d pPriv->pcCallsign = \"%s\"\n",
                          gFnLevel, pPriv->pcCallsign);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d  %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_set_callsign
// ---------------------------------------------------------------------------
// Status: IN WORK
int adat_cmd_fn_set_callsign(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        char acBuf[ ADAT_BUFSZ + 1 ];

        memset(acBuf, 0, ADAT_BUFSZ + 1);

        strcpy(acBuf, ADAT_CMD_DEF_STRING_SET_CALLSIGN);
        strcat(acBuf, "DG1SBG"ADAT_CR);

        nRC = adat_priv_set_cmd(pRig, acBuf,
                                ADAT_CMD_KIND_WITHOUT_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_serial_nr
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_serial_nr(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_SERIAL_NR,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                pPriv->pcSerialNr = strdup(pPriv->pcResult);

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d pPriv->pcSerialNr = \"%s\"\n",
                          gFnLevel, pPriv->pcSerialNr);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_fw_version
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_fw_version(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_FW_VERSION,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                pPriv->pcFWVersion = strdup(pPriv->pcResult);

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d pPriv->pcFWVersion = \"%s\"\n",
                          gFnLevel, pPriv->pcFWVersion);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_hw_version
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_hw_version(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_HW_VERSION,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                pPriv->pcHWVersion = strdup(pPriv->pcResult);

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d pPriv->pcHWVersion = \"%s\"\n",
                          gFnLevel, pPriv->pcHWVersion);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_gui_fw_version
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_gui_fw_version(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_GUI_FW_VERSION,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                pPriv->pcGUIFWVersion = strdup(pPriv->pcResult);

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d pPriv->pcGUIFWVersion = \"%s\"\n",
                          gFnLevel, pPriv->pcGUIFWVersion);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_id_code
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_id_code(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_ID_CODE,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                pPriv->pcIDCode = strdup(pPriv->pcResult);

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d pPriv->pcIDCode = \"%s\"\n",
                          gFnLevel, pPriv->pcIDCode);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_options
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_options(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_OPTIONS,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                pPriv->pcOptions = strdup(pPriv->pcResult);

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d pPriv->pcOptions = \"%s\"\n",
                          gFnLevel, pPriv->pcOptions);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_mode
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_mode(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_MODE,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                nRC = adat_parse_mode(pPriv->pcResult,
                                      &(pPriv->nRIGMode),
                                      pPriv->acADATMode);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_set_mode
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_set_mode(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        // Translate Mode from RIG Mode Nr to ADAT Mode Nr

        nRC = adat_mode_rnr2anr(pPriv->nRIGMode, &(pPriv->nADATMode));

        if (nRC == RIG_OK)
        {
            // Prepare Command

            char acBuf[ ADAT_BUFSZ + 1 ];

            memset(acBuf, 0, ADAT_BUFSZ + 1);

            SNPRINTF(acBuf, sizeof(acBuf), "%s%02d%s",
                     ADAT_CMD_DEF_STRING_SET_MODE,
                     (int) pPriv->nADATMode,
                     ADAT_EOM);

            nRC = adat_priv_set_cmd(pRig, acBuf, ADAT_CMD_KIND_WITHOUT_RESULT);

            // Execute Command

            if (nRC == RIG_OK)
            {
                nRC = adat_get_single_cmd_result(pRig);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);

    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_freq
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_freq(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_FREQ,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                nRC = adat_parse_freq(pPriv->pcResult,
                                      ADAT_FREQ_PARSE_MODE_WITH_VFO,
                                      &(pPriv->nCurrentVFO),
                                      &(pPriv->nFreq));

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d pPriv->nCurrentVFO = %d, Freq [Hz] = %f\n",
                          gFnLevel, pPriv->nCurrentVFO, pPriv->nFreq);

                if (nRC == RIG_OK)
                {
                    nRC = adat_vfo_anr2rnr(pPriv->nCurrentVFO,
                                           &(pPriv->nRIGVFONr));
                }
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_set_freq
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_set_freq(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;
        char               acBuf[ ADAT_BUFSZ + 1 ];

        // Get frequency of selected VFO

        memset(acBuf, 0, ADAT_BUFSZ + 1);

        SNPRINTF(acBuf, sizeof(acBuf), "%s%d%s",
                 ADAT_CMD_DEF_STRING_SET_FREQ,
                 (int) pPriv->nFreq,
                 ADAT_EOM);

        nRC = adat_priv_set_cmd(pRig, acBuf, ADAT_CMD_KIND_WITHOUT_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_set_vfo
// ---------------------------------------------------------------------------
// Status: RELEASED

// Setting a VFO on an ADAT is actually two steps:
// 1. Switching on that VFO
// 2. Setting this VFO as the main VFO
int adat_cmd_fn_set_vfo(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;
        char               acBuf[ ADAT_BUFSZ + 1 ];

        // Switch on VFO

        memset(acBuf, 0, ADAT_BUFSZ + 1);

        SNPRINTF(acBuf, ADAT_BUFSZ, ADAT_CMD_DEF_STRING_SWITCH_ON_VFO,
                 (int) pPriv->nCurrentVFO,
                 ADAT_EOM);

        nRC = adat_priv_set_cmd(pRig, acBuf, ADAT_CMD_KIND_WITHOUT_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                memset(acBuf, 0, ADAT_BUFSZ + 1);
                SNPRINTF(acBuf, ADAT_BUFSZ,
                         ADAT_CMD_DEF_STRING_SET_VFO_AS_MAIN_VFO,
                         (int) pPriv->nCurrentVFO,
                         ADAT_EOM);

                nRC = adat_priv_set_cmd(pRig, acBuf,
                                        ADAT_CMD_KIND_WITHOUT_RESULT);

                if (nRC == RIG_OK)
                {
                    nRC = adat_get_single_cmd_result(pRig);
                }
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_get_ptt
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_get_ptt(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_priv_set_cmd(pRig,
                                ADAT_CMD_DEF_STRING_GET_PTT,
                                ADAT_CMD_KIND_WITH_RESULT);

        if (nRC == RIG_OK)
        {
            nRC = adat_get_single_cmd_result(pRig);

            if (nRC == RIG_OK)
            {
                nRC = adat_parse_ptt(pPriv->pcResult,
                                     &(pPriv->nADATPTTStatus));

                if (nRC == RIG_OK)
                {
                    nRC = adat_ptt_anr2rnr(pPriv->nADATPTTStatus,
                                           &(pPriv->nRIGPTTStatus));
                }
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_cmd_fn_set_ptt
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cmd_fn_set_ptt(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr  pPriv    = (adat_priv_data_ptr) pRig->state.priv;
        char                acBuf[ ADAT_BUFSZ + 1 ];
        char               *pcPTTStr = NULL;

        memset(acBuf, 0, ADAT_BUFSZ + 1);

        // Switch PTT

        switch (pPriv->nOpCode)
        {
        case ADAT_OPCODE_PTT_SWITCH_ON:

            pPriv->nADATPTTStatus = ADAT_PTT_STATUS_ANR_ON;
            nRC = adat_ptt_anr2rnr(ADAT_PTT_STATUS_ANR_ON,
                                   &(pPriv->nRIGPTTStatus));
            pcPTTStr = ADAT_CMD_PTT_STR_ON;
            break;

        case ADAT_OPCODE_PTT_SWITCH_OFF:
            pPriv->nADATPTTStatus = ADAT_PTT_STATUS_ANR_OFF;
            nRC = adat_ptt_anr2rnr(ADAT_PTT_STATUS_ANR_OFF,
                                   &(pPriv->nRIGPTTStatus));
            pcPTTStr = ADAT_CMD_PTT_STR_OFF;
            break;

        default:
            nRC = -RIG_EINVAL;
            break;
        }

        if (nRC == RIG_OK)
        {
            SNPRINTF(acBuf, ADAT_BUFSZ, ADAT_CMD_DEF_STRING_SET_PTT,
                     pcPTTStr,
                     ADAT_EOM);

            nRC = adat_priv_set_cmd(pRig, acBuf, ADAT_CMD_KIND_WITHOUT_RESULT);

            if (nRC == RIG_OK)
            {
                nRC = adat_get_single_cmd_result(pRig);
            }
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_transaction
// ---------------------------------------------------------------------------
// Status: RELEASED

// adat_transaction is a generalized command processor able to execute
// commands of type adat_cmd_def_t .
int adat_transaction(RIG                *pRig,
                     adat_cmd_list_ptr   pCmdList)
{
    int nRC   = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        int                nI    = 0;
        int nFini = 0;  // = 1 -> Stop executing commands
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        rig_debug(RIG_DEBUG_TRACE,
                  "*** ADAT: %d %s (%s:%d): Nr of commands = %d\n",
                  gFnLevel, __func__, __FILE__, __LINE__, pCmdList->nNrCmds);

        while ((nRC == RIG_OK) && (nFini == 0) && (nI < pCmdList->nNrCmds))
        {
            adat_cmd_def_ptr pCmd = NULL;

            pCmd = pCmdList->adat_cmds[ nI ];

            if ((pCmd != NULL) && (pCmd->nCmdId != ADAT_CMD_DEF_NIL))
            {

                rig_debug(RIG_DEBUG_TRACE,
                          "*** ADAT: %d About to execute ADAT Command ... \n",
                          gFnLevel);
                adat_print_cmd(pCmd);

                // Execute Command

                if (pCmd->pfCmdFn != NULL)
                {
                    rig_debug(RIG_DEBUG_TRACE,
                              "*** ADAT: %d Calling function via fn ptr ... \n",
                              gFnLevel);
                    nRC = pCmd->pfCmdFn(pRig);
                }
                else
                {
                    rig_debug(RIG_DEBUG_TRACE,
                              "*** ADAT: %d Sending command string ... \n",
                              gFnLevel);
// TODO: Quell clang warning of conditional always evaluating to true.
//                    if( pCmd->pacCmdStrs != NULL )
//                    {

                    if (pCmd->nNrCmdStrs > 0)
                    {
                        int  nJ       = 0;
                        rig_debug(RIG_DEBUG_TRACE,
                                  "*** ADAT: %d pacCmdStrs[%d] = %s\n",
                                  gFnLevel, nJ, pCmd->pacCmdStrs[ nJ ]);

                        while ((nJ < pCmd->nNrCmdStrs)
                                && (nRC == RIG_OK)
                                && (pCmd->pacCmdStrs[ nJ ] != NULL))
                        {

                            nRC = adat_send(pRig, pCmd->pacCmdStrs[ nJ ]);

                            if (nRC == RIG_OK)
                            {
                                if (pCmd->nCmdKind == ADAT_CMD_KIND_WITH_RESULT)
                                {
                                    char acBuf[ ADAT_RESPSZ + 1 ];

                                    memset(acBuf, 0, ADAT_RESPSZ + 1);

                                    nRC = adat_receive(pRig, acBuf);

                                    while ((nRC == RIG_OK)
                                            && (strncmp(acBuf, ADAT_BOM,
                                                        strlen(ADAT_BOM)) != 0))
                                    {

                                        nRC = adat_receive(pRig, acBuf);
                                    }

                                    if (pPriv->pcResult != NULL)
                                    {
                                        free(pPriv->pcResult);
                                    }

                                    pPriv->pcResult = strdup(acBuf);
                                }
                            }

                            nJ++;
                        }
                    }

//                    }
                }

                if (nRC != RIG_OK)
                {
                    (void) adat_cmd_recover_from_error(pRig, nRC);
                }

                nI++;
            }
            else
            {
                nFini = 1;
            }

            // sleep between cmds - ADAT needs time to act upoon cmds

            hl_usleep(ADAT_SLEEP_MICROSECONDS_BETWEEN_CMDS);
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// adat_new_priv_data
// ---------------------------------------------------------------------------
// Status: RELEASED
adat_priv_data_ptr adat_new_priv_data(RIG *pRig)
{
    int                 nRC   = 0;
    adat_priv_data_ptr  pPriv = NULL;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig != NULL)
    {
        // Init Priv Data

        pPriv = pRig->state.priv = (adat_priv_data_ptr) calloc(sizeof(adat_priv_data_t),
                                   1);

        if (pRig->state.priv != NULL)
        {
            char acBuf[ ADAT_BUFSZ + 1 ];
            memset(acBuf, 0, ADAT_BUFSZ + 1);

            // FIXME: pointless code at init time
#if 0
            nRC = adat_get_conf(pRig, TOKEN_ADAT_PRODUCT_NAME, acBuf);

            if (nRC == 0)
            {
                pPriv->pcProductName = strdup(acBuf);

                pRig->state.priv = (void *) pPriv;
            }

#endif
        }
        else
        {
            nRC = -RIG_ENOMEM;
        }
    }
    else
    {
        nRC = -RIG_EARG;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. RC = %d, pPriv = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC, pPriv);
    gFnLevel--;

    return pPriv;
}


// ---------------------------------------------------------------------------
// adat_del_priv_data
// ---------------------------------------------------------------------------
// Status: RELEASED
void adat_del_priv_data(adat_priv_data_t **ppPriv)
{
    int nRC = 0;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: ppPrivData = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, ppPriv);

    if ((ppPriv != NULL) && (*ppPriv != NULL))
    {
        // Delete / Free Priv Data

        if ((*ppPriv)->pcProductName != NULL)
        {
            free((*ppPriv)->pcProductName);
        }

        if ((*ppPriv)->pcSerialNr != NULL)
        {
            free((*ppPriv)->pcSerialNr);
        }

        if ((*ppPriv)->pcHWVersion != NULL)
        {
            free((*ppPriv)->pcHWVersion);
        }

        if ((*ppPriv)->pcFWVersion != NULL)
        {
            free((*ppPriv)->pcFWVersion);
        }

        if ((*ppPriv)->pcGUIFWVersion != NULL)
        {
            free((*ppPriv)->pcGUIFWVersion);
        }

        if ((*ppPriv)->pcOptions != NULL)
        {
            free((*ppPriv)->pcOptions);
        }

        if ((*ppPriv)->pcIDCode != NULL)
        {
            free((*ppPriv)->pcIDCode);
        }

        if ((*ppPriv)->pcCallsign != NULL)
        {
            free((*ppPriv)->pcCallsign);
        }

        // Free priv struct itself

        free((*ppPriv));
        *ppPriv = NULL;
    }
    else
    {
        nRC = -RIG_EARG;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. RC = %d.\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return;
}


// ---------------------------------------------------------------------------
// Function adat_init
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_init(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig != NULL)
    {
        adat_priv_data_ptr pPriv = NULL;

        // Get new Priv Data

        pPriv = adat_new_priv_data(pRig);

        if (pPriv == NULL)
        {
            nRC = -RIG_ENOMEM;
        }
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_cleanup
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_cleanup(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        if (pRig->state.priv != NULL)
        {
            adat_del_priv_data((adat_priv_data_t **) & (pRig->state.priv));
            pRig->state.priv = NULL;
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_open
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_open(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        // grace period for the radio to be there

        sleep(ADAT_SLEEP_AFTER_RIG_OPEN);

        // Now get basic info from ADAT TRX

        nRC = adat_transaction(pRig, &adat_cmd_list_open_adat);
    }

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_close
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_close(RIG *pRig)
{
    int nRC = RIG_OK;
    adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

    if (pPriv->pcCmd != NULL) { free(pPriv->pcCmd); }

    if (pPriv->pcResult != NULL) { free(pPriv->pcResult); }

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Now switch to interactive mode

    nRC = adat_transaction(pRig, &adat_cmd_list_close_adat);

    // Done !

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_get_info
// ---------------------------------------------------------------------------
// Status: RELEASED
const char *adat_get_info(RIG *pRig)
{
    static char acBuf[ 512 ];

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    memset(acBuf, 0, 512);

    if (pRig != NULL)
    {
        int nRC = adat_transaction(pRig, &adat_cmd_list_get_info);

        if (nRC == RIG_OK)
        {
            adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

            SNPRINTF(acBuf, sizeof(acBuf),
                     "ADAT ADT-200A, Callsign: %s, S/N: %s, ID Code: %s, Options: %s, FW: %s, GUI FW: %s, HW: %s",
                     pPriv->pcCallsign,
                     pPriv->pcSerialNr,
                     pPriv->pcIDCode,
                     pPriv->pcOptions,
                     pPriv->pcFWVersion,
                     pPriv->pcGUIFWVersion,
                     pPriv->pcHWVersion);
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Value ='%s'\n",
              gFnLevel, __func__, __FILE__, __LINE__, acBuf);
    gFnLevel--;

    return acBuf;
}


// ---------------------------------------------------------------------------
// Function adat_set_freq
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_set_freq(RIG *pRig, vfo_t vfo, freq_t freq)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        pPriv->nFreq = freq;

        nRC = adat_transaction(pRig, &adat_cmd_list_set_freq);
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_get_freq
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_get_freq(RIG *pRig, vfo_t vfo, freq_t *freq)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_transaction(pRig, &adat_cmd_list_get_freq);

        *freq = pPriv->nFreq;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_set_level
// ---------------------------------------------------------------------------
// Status: IN WORK
int adat_set_level(RIG *pRig, vfo_t vfo, setting_t level, value_t val)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        //adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_get_level
// ---------------------------------------------------------------------------
// Status: IN WORK
int adat_get_level(RIG *pRig, vfo_t vfo, setting_t level, value_t *val)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        //adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_set_mode
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_set_mode(RIG *pRig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int nRC;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        pPriv->nRIGMode    = mode;
        adat_vfo_rnr2anr(vfo, &(pPriv->nCurrentVFO));

        if (width != RIG_PASSBAND_NOCHANGE)
        {
            if (width == RIG_PASSBAND_NORMAL)
            {
                width = rig_passband_normal(pRig, mode);
            }

            pPriv->nWidth = width;
        }

        nRC = adat_transaction(pRig, &adat_cmd_list_set_mode);
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_get_mode
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_get_mode(RIG *pRig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC =  adat_transaction(pRig, &adat_cmd_list_get_mode);

        if (nRC == RIG_OK)
        {
            *mode = pPriv->nRIGMode;
            *width = pPriv->nWidth;
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_get_vfo
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_get_vfo(RIG *pRig, vfo_t *vfo)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_transaction(pRig, &adat_cmd_list_get_vfo);

        *vfo = pPriv->nRIGVFONr;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_set_vfo
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_set_vfo(RIG *pRig, vfo_t vfo)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_vfo_rnr2anr(vfo, &(pPriv->nCurrentVFO));

        if (nRC == RIG_OK)
        {
            nRC = adat_transaction(pRig, &adat_cmd_list_set_vfo);
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_get_ptt
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_get_ptt(RIG *pRig, vfo_t vfo, ptt_t *ptt)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_transaction(pRig, &adat_cmd_list_get_ptt);

        *ptt = pPriv->nRIGPTTStatus;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_set_ptt
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_set_ptt(RIG *pRig, vfo_t vfo, ptt_t ptt)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        switch (ptt)
        {
        case RIG_PTT_ON:
            pPriv->nOpCode = ADAT_OPCODE_PTT_SWITCH_ON;
            break;

        case RIG_PTT_OFF:
            pPriv->nOpCode = ADAT_OPCODE_PTT_SWITCH_OFF;
            break;

        default:
            nRC = -RIG_EINVAL;
            break;
        }

        if (nRC == RIG_OK)
        {
            nRC = adat_transaction(pRig, &adat_cmd_list_set_ptt);
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_power2mW
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_power2mW(RIG *pRig,
                  unsigned int *mwpower,
                  float power,
                  freq_t freq,
                  rmode_t mode)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if ((pRig == NULL) || (mwpower == NULL))
    {
        nRC = -RIG_EARG;
    }
    else
    {
        *mwpower = power * ADAT_MAX_POWER_IN_mW;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_mW2power
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_mW2power(RIG *pRig,
                  float *power,
                  unsigned int mwpower,
                  freq_t freq,
                  rmode_t mode)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if ((pRig == NULL) || (power == NULL))
    {
        nRC = -RIG_EARG;
    }
    else
    {
        *power = mwpower / ((float)ADAT_MAX_POWER_IN_mW);
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_get_powerstat
// ---------------------------------------------------------------------------
// Status: RELEASED
int adat_get_powerstat(RIG *pRig, powerstat_t *status)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        //adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        nRC = adat_transaction(pRig, &adat_cmd_list_get_powerstatus);

        // nRC < 0 -> Power is off.

        if (nRC == RIG_OK)
        {
            *status = RIG_POWER_ON;
        }
        else
        {
            *status = RIG_POWER_OFF;
            nRC = RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_set_conf
// ---------------------------------------------------------------------------
// Status: IN WORK
int adat_set_conf(RIG *pRig, token_t token, const char *val)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        switch (token)
        {
        case TOKEN_ADAT_PRODUCT_NAME:
            if (pPriv->pcProductName != NULL) { free(pPriv->pcProductName); }

            pPriv->pcProductName = strdup(val);
            break;

        default:
            nRC = -RIG_EINVAL;
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_get_conf
// ---------------------------------------------------------------------------
// Status: IN WORK
int adat_get_conf(RIG *pRig, token_t token, char *val)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

        switch (token)
        {
        case TOKEN_ADAT_PRODUCT_NAME:
            strcpy(val, pPriv->pcProductName != NULL ? pPriv->pcProductName :
                   "Unknown product");
            break;

        default:
            nRC = -RIG_EINVAL;
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_reset
// ---------------------------------------------------------------------------
// Status: IN WORK
int adat_reset(RIG *pRig, reset_t reset)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        //adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;

    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// Function adat_handle_event
// ---------------------------------------------------------------------------
// Status: IN WORK
int adat_handle_event(RIG *pRig)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY. Params: pRig = %p\n",
              gFnLevel, __func__, __FILE__, __LINE__, pRig);

    // Check Params

    if (pRig == NULL)
    {
        nRC = -RIG_EARG;
    }
    else
    {
        //adat_priv_data_ptr pPriv = (adat_priv_data_ptr) pRig->state.priv;
        char               acBuf[ ADAT_RESPSZ + 1 ];

        memset(acBuf, 0, ADAT_RESPSZ + 1);
        adat_receive(pRig, acBuf);

        rig_debug(RIG_DEBUG_TRACE,
                  "*** ADAT: %d Event data = \"%s\"\n",
                  gFnLevel, acBuf);
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// initrigs_adat is called by rig_backend_load
// ---------------------------------------------------------------------------
// Status: RELEASED
DECLARE_INITRIG_BACKEND(adat)
{
    int nRC = RIG_OK;

    gFnLevel++;

#if 0
    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY.\n",
              gFnLevel, __func__, __FILE__, __LINE__);
#endif
    rig_register(&adt_200a_caps);

#if 0
    rig_debug(RIG_DEBUG_VERBOSE, "ADAT: Rig ADT-200A registered.\n");

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
#endif
    gFnLevel--;

    return nRC;
}


// ---------------------------------------------------------------------------
// proberig_adat
// ---------------------------------------------------------------------------
DECLARE_PROBERIG_BACKEND(adat)
{
    int nRC = RIG_OK;

    gFnLevel++;

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): ENTRY.\n",
              gFnLevel, __func__, __FILE__, __LINE__);

    if (!port)
    {
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
    }

    port->write_delay = port->post_write_delay = 10;
    port->parm.serial.stop_bits = 2;
    port->retry = 1;


    nRC = serial_open(port);

    if (nRC != RIG_OK)
    {
        nRC = RIG_MODEL_NONE;
    }
    else
    {
        char acBuf[ ADAT_RESPSZ + 1 ];
        int  nRead = 0;

        memset(acBuf, 0, ADAT_RESPSZ + 1);

        nRC = write_block(port,
                          (unsigned char *)ADAT_CMD_DEF_STRING_GET_ID_CODE,
                          strlen(ADAT_CMD_DEF_STRING_GET_ID_CODE));
        nRead = read_string(port, (unsigned char *) acBuf, ADAT_RESPSZ,
                            ADAT_EOM, 1, 0, 1);
        close(port->fd);

        if ((nRC != RIG_OK || nRead < 0))
        {
            nRC = RIG_MODEL_NONE;
        }
        else
        {
            rig_debug(RIG_DEBUG_VERBOSE, "ADAT: %d Received ID = %s.",
                      gFnLevel, acBuf);

            nRC = RIG_MODEL_ADT_200A;
        }
    }

    rig_debug(RIG_DEBUG_TRACE,
              "*** ADAT: %d %s (%s:%d): EXIT. Return Code = %d\n",
              gFnLevel, __func__, __FILE__, __LINE__, nRC);
    gFnLevel--;

    return nRC;
}

// ---------------------------------------------------------------------------
// END OF FILE
// ---------------------------------------------------------------------------
