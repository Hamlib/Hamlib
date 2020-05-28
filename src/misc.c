/*
 *  Hamlib Interface - toolbox
 *  Copyright (c) 2000-2011 by Stephane Fillod
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

/**
 * \addtogroup rig_internal
 * @{
 */

/**
 * \file misc.c
 * \brief Miscellaneous utility routines
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include <unistd.h>

#include <hamlib/rig.h>
#include <hamlib/amplifier.h>

#include "misc.h"


/**
 * \brief Convert from binary to 4-bit BCD digits, little-endian
 * \param bcd_data
 * \param freq
 * \param bcd_len
 * \return bcd_data
 *
 * Convert a long long (e.g. frequency in Hz) to 4-bit BCD digits,
 * packed two digits per octet, in little-endian order
 * (e.g. byte order 90 78 56 34 12 for 1234567890 Hz).
 *
 * bcd_len is the number of BCD digits, usually 10 or 8 in 1-Hz units,
 * and 6 digits in 100-Hz units for Tx offset data.
 *
 * Hope the compiler will do a good job optimizing it (esp. w/the 64bit freq)
 *
 * Returns a pointer to (unsigned char *)bcd_data.
 *
 * \sa to_bcd_be()
 */
unsigned char *HAMLIB_API to_bcd(unsigned char bcd_data[],
                                 unsigned long long freq,
                                 unsigned bcd_len)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* '450'/4-> 5,0;0,4 */
    /* '450'/3-> 5,0;x,4 */

    for (i = 0; i < bcd_len / 2; i++)
    {
        unsigned char a = freq % 10;
        freq /= 10;
        a |= (freq % 10) << 4;
        freq /= 10;
        bcd_data[i] = a;
    }

    if (bcd_len & 1)
    {
        bcd_data[i] &= 0xf0;
        bcd_data[i] |= freq % 10; /* NB: high nibble is left uncleared */
    }

    return bcd_data;
}


/**
 * \brief Convert BCD digits, little-endian, to a long long (e.g. frequency in Hz)
 * \param bcd_data
 * \param bcd_len
 * \return binary result (e.g. frequency)
 *
 * Convert BCD digits, little-endian, (byte order 90 78 56 34 12
 * for 1234567890 Hz) to a long long (e.g. frequency in Hz)
 *
 * bcd_len is the number of BCD digits.
 *
 * Hope the compiler will do a good job optimizing it (esp. w/ the 64bit freq)
 *
 * Returns frequency in Hz an unsigned long long integer.
 *
 * \sa from_bcd_be()
 */
unsigned long long HAMLIB_API from_bcd(const unsigned char bcd_data[],
                                       unsigned bcd_len)
{
    int i;
    freq_t f = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (bcd_len & 1)
    {
        f = bcd_data[bcd_len / 2] & 0x0f;
    }

    for (i = (bcd_len / 2) - 1; i >= 0; i--)
    {
        f *= 10;
        f += bcd_data[i] >> 4;
        f *= 10;
        f += bcd_data[i] & 0x0f;
    }

    return f;
}


/**
 * \brief Convert from binary to 4-bit BCD digits, big-endian
 * \param bcd_data
 * \param freq
 * \param bcd_len
 * \return bcd_data
 *
 * Same as to_bcd, but in big-endian order
 * (e.g. byte order 12 34 56 78 90 for 1234567890 Hz)
 *
 * \sa to_bcd()
 */
unsigned char *HAMLIB_API to_bcd_be(unsigned char bcd_data[],
                                    unsigned long long freq,
                                    unsigned bcd_len)
{
    int i;

    /* '450'/4 -> 0,4;5,0 */
    /* '450'/3 -> 4,5;0,x */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (bcd_len & 1)
    {
        bcd_data[bcd_len / 2] &= 0x0f;
        bcd_data[bcd_len / 2] |= (freq % 10) <<
                                 4; /* NB: low nibble is left uncleared */
        freq /= 10;
    }

    for (i = (bcd_len / 2) - 1; i >= 0; i--)
    {
        unsigned char a = freq % 10;
        freq /= 10;
        a |= (freq % 10) << 4;
        freq /= 10;
        bcd_data[i] = a;
    }

    return bcd_data;
}


/**
 * \brief Convert 4-bit BCD digits to binary, big-endian
 * \param bcd_data
 * \param bcd_len
 * \return binary result
 *
 * Same as from_bcd, but in big-endian order
 * (e.g. byte order 12 34 56 78 90 for 1234567890 Hz)
 *
 * \sa from_bcd()
 */
unsigned long long HAMLIB_API from_bcd_be(const unsigned char bcd_data[],
        unsigned bcd_len)
{
    int i;
    freq_t f = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0; i < bcd_len / 2; i++)
    {
        f *= 10;
        f += bcd_data[i] >> 4;
        f *= 10;
        f += bcd_data[i] & 0x0f;
    }

    if (bcd_len & 1)
    {
        f *= 10;
        f += bcd_data[bcd_len / 2] >> 4;
    }

    return f;
}


//! @cond Doxygen_Suppress
#ifndef llabs
#define llabs(a) ((a)<0?-(a):(a))
#endif
//! @endcond


/**
 * \brief Pretty print a frequency
 * \param str for result (may need up to 17 char)
 * \param freq input in Hz
 *
 * rig_freq_snprintf?
 * pretty print frequencies
 * str must be long enough. max can be as long as 17 chars
 */
int HAMLIB_API sprintf_freq(char *str, freq_t freq)
{
    double f;
    char *hz;

    // too verbose
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (llabs(freq) >= GHz(1))
    {
        hz = "GHz";
        f = (double)freq / GHz(1);
    }
    else if (llabs(freq) >= MHz(1))
    {
        hz = "MHz";
        f = (double)freq / MHz(1);
    }
    else if (llabs(freq) >= kHz(1))
    {
        hz = "kHz";
        f = (double)freq / kHz(1);
    }
    else
    {
        hz = "Hz";
        f = (double)freq;
    }

    return sprintf(str, "%g %s", f, hz);
}


/**
 * \brief Convert enum RIG_STATUS_... to printable string
 * \param status RIG_STATUS_??
 * \return string
 */
const char *HAMLIB_API rig_strstatus(enum rig_status_e status)
{
    switch (status)
    {
    case RIG_STATUS_ALPHA:
        return "Alpha";

    case RIG_STATUS_UNTESTED:
        return "Untested";

    case RIG_STATUS_BETA:
        return "Beta";

    case RIG_STATUS_STABLE:
        return "Stable";

    case RIG_STATUS_BUGGY:
        return "Buggy";
    }

    return "";
}


static struct
{
    rmode_t mode;
    const char *str;
} mode_str[] =
{
    { RIG_MODE_AM, "AM" },
    { RIG_MODE_CW, "CW" },
    { RIG_MODE_USB, "USB" },
    { RIG_MODE_LSB, "LSB" },
    { RIG_MODE_RTTY, "RTTY" },
    { RIG_MODE_FM, "FM" },
    { RIG_MODE_WFM, "WFM" },
    { RIG_MODE_CWR, "CWR" },
    { RIG_MODE_RTTYR, "RTTYR" },
    { RIG_MODE_AMS, "AMS" },
    { RIG_MODE_PKTLSB, "PKTLSB" },
    { RIG_MODE_PKTUSB, "PKTUSB" },
    { RIG_MODE_PKTFM, "PKTFM" },
    { RIG_MODE_ECSSUSB, "ECSSUSB" },
    { RIG_MODE_ECSSLSB, "ECSSLSB" },
    { RIG_MODE_FAX, "FAX" },
    { RIG_MODE_SAM, "SAM" },
    { RIG_MODE_SAL, "SAL" },
    { RIG_MODE_SAH, "SAH" },
    { RIG_MODE_DSB, "DSB"},
    { RIG_MODE_FMN, "FMN" },
    { RIG_MODE_PKTAM, "PKTAM"},
    { RIG_MODE_P25, "P25"},
    { RIG_MODE_DSTAR, "D-STAR"},
    { RIG_MODE_DPMR, "DPMR"},
    { RIG_MODE_NXDNVN, "NXDN-VN"},
    { RIG_MODE_NXDN_N, "NXDN-N"},
    { RIG_MODE_DCR, "DCR"},
    { RIG_MODE_AMN, "AMN"},
    { RIG_MODE_PSK, "PSK"},
    { RIG_MODE_PSKR, "PSKR"},
    { RIG_MODE_C4FM, "C4FM"},
    { RIG_MODE_NONE, "" },
};


/**
 * \brief Convert alpha string to enum RIG_MODE
 * \param s input alpha string
 * \return enum RIG_MODE_??
 *
 * \sa rmode_t
 */
rmode_t HAMLIB_API rig_parse_mode(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; mode_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, mode_str[i].str))
        {
            return mode_str[i].mode;
        }
    }

    return RIG_MODE_NONE;
}


/**
 * \brief Convert enum RIG_MODE to alpha string
 * \param mode RIG_MODE_...
 * \return alpha string
 *
 * \sa rmode_t
 */
const char *HAMLIB_API rig_strrmode(rmode_t mode)
{
    int i;

    // only enable if needed for debugging -- too verbose otherwise
    //rig_debug(RIG_DEBUG_TRACE, "%s called mode=0x%"PRXll"\n", __func__, mode);

    if (mode == RIG_MODE_NONE)
    {
        return "";
    }

    for (i = 0 ; mode_str[i].str[0] != '\0'; i++)
    {
        if (mode == mode_str[i].mode)
        {
            return mode_str[i].str;
        }
    }

    return "";
}

/**
 * \brief Convert RIG_MODE or'd value to alpha string of all modes
 * \param modes RIG_MODE or'd value
 * \param buf char* of result buffer
 * \param buflen length of buffer
 * \return rig status -- RIG_ETRUNC if buffer not big enough
 *
 * \sa rmode_t
 */
int HAMLIB_API rig_strrmodes(rmode_t modes, char *buf, int buflen)
{
    int i;

    // only enable if needed for debugging -- too verbose otherwise
    //rig_debug(RIG_DEBUG_TRACE, "%s called mode=0x%"PRXll"\n", __func__, mode);

    if (modes == RIG_MODE_NONE)
    {
        snprintf(buf, buflen, "NONE");
        return RIG_OK;
    }

    for (i = 0 ; mode_str[i].str[0] != '\0'; i++)
    {
        if (modes & mode_str[i].mode)
        {
            char modebuf[16];

            if (strlen(buf) == 0) { snprintf(modebuf, sizeof(modebuf), "%s", mode_str[i].str); }
            else { snprintf(modebuf, sizeof(modebuf), " %s", mode_str[i].str); }

            strncat(buf, modebuf, buflen - strlen(buf) - 1);

            if (strlen(buf) > buflen - 10) { return -RIG_ETRUNC; }
        }
    }

    return RIG_OK;
}


static struct
{
    vfo_t vfo;
    const char *str;
} vfo_str[] =
{
    { RIG_VFO_A, "VFOA" },
    { RIG_VFO_B, "VFOB" },
    { RIG_VFO_C, "VFOC" },
    { RIG_VFO_CURR, "currVFO" },
    { RIG_VFO_MEM, "MEM" },
    { RIG_VFO_VFO, "VFO" },
    { RIG_VFO_TX, "TX" },
    { RIG_VFO_RX, "RX" },
    { RIG_VFO_MAIN, "Main" },
    { RIG_VFO_MAIN_A, "MainA" },
    { RIG_VFO_MAIN_B, "MainB" },
    { RIG_VFO_SUB, "Sub" },
    { RIG_VFO_SUB_A, "SubA" },
    { RIG_VFO_SUB_B, "SubB" },
    { RIG_VFO_NONE, "None" },
    { 0xffffff, "" },
};


/**
 * \brief Convert alpha string to enum RIG_VFO_...
 * \param s input alpha string
 * \return RIG_VFO_...
 *
 * \sa RIG_VFO_A RIG_VFO_B RIG_VFO_C RIG_VFO_MAIN RIG_VFO_MAIN_A RIG_VFO_MAIN_B RIG_VFO_SUB RIG_VFO_SUB_A RIG_VFO_SUB_B RIG_VFO_VFO RIG_VFO_CURR RIG_VFO_MEM RIG_VFO_TX RIG_VFO_RX RIG_VFO_NONE
 */
vfo_t HAMLIB_API rig_parse_vfo(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; vfo_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, vfo_str[i].str))
        {
            return vfo_str[i].vfo;
        }
    }

    return RIG_VFO_NONE;
}


/**
 * \brief Convert enum RIG_VFO_... to alpha string
 * \param vfo RIG_VFO_...
 * \return alpha string
 *
 * \sa RIG_VFO_A RIG_VFO_B RIG_VFO_C RIG_VFO_MAIN RIG_VFO_SUB RIG_VFO_VFO RIG_VFO_CURR RIG_VFO_MEM RIG_VFO_TX RIG_VFO_RX RIG_VFO_NONE
 */
const char *HAMLIB_API rig_strvfo(vfo_t vfo)
{
    int i;

    //a bit too verbose
    //rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    for (i = 0 ; vfo_str[i].str[0] != '\0'; i++)
    {
        if (vfo == vfo_str[i].vfo)
        {
            //rig_debug(RIG_DEBUG_TRACE, "%s returning %s\n", __func__, vfo_str[i].str);
            return vfo_str[i].str;
        }
    }

    return "";
}


static struct
{
    setting_t func;
    const char *str;
} func_str[] =
{
    { RIG_FUNC_FAGC, "FAGC" },
    { RIG_FUNC_NB, "NB" },
    { RIG_FUNC_COMP, "COMP" },
    { RIG_FUNC_VOX, "VOX" },
    { RIG_FUNC_TONE, "TONE" },
    { RIG_FUNC_TSQL, "TSQL" },
    { RIG_FUNC_SBKIN, "SBKIN" },
    { RIG_FUNC_FBKIN, "FBKIN" },
    { RIG_FUNC_ANF, "ANF" },
    { RIG_FUNC_NR, "NR" },
    { RIG_FUNC_AIP, "AIP" },
    { RIG_FUNC_APF, "APF" },
    { RIG_FUNC_MON, "MON" },
    { RIG_FUNC_MN, "MN" },
    { RIG_FUNC_RF, "RF" },
    { RIG_FUNC_ARO, "ARO" },
    { RIG_FUNC_LOCK, "LOCK" },
    { RIG_FUNC_MUTE, "MUTE" },
    { RIG_FUNC_VSC, "VSC" },
    { RIG_FUNC_REV, "REV" },
    { RIG_FUNC_SQL, "SQL" },
    { RIG_FUNC_ABM, "ABM" },
    { RIG_FUNC_BC, "BC" },
    { RIG_FUNC_MBC, "MBC" },
    { RIG_FUNC_RIT, "RIT" },
    { RIG_FUNC_AFC, "AFC" },
    { RIG_FUNC_SATMODE, "SATMODE" },
    { RIG_FUNC_SCOPE, "SCOPE" },
    { RIG_FUNC_RESUME, "RESUME" },
    { RIG_FUNC_TBURST, "TBURST" },
    { RIG_FUNC_TUNER, "TUNER" },
    { RIG_FUNC_XIT, "XIT" },
    { RIG_FUNC_NB2, "NB2" },
    { RIG_FUNC_DSQL, "DSQL" },
    { RIG_FUNC_AFLT, "AFLT" },
    { RIG_FUNC_ANL, "ANL" },
    { RIG_FUNC_BC2, "BC2" },
    { RIG_FUNC_DUAL_WATCH, "DUAL_WATCH"},
    { RIG_FUNC_DIVERSITY, "DIVERSITY"},
    { RIG_FUNC_CSQL, "CSQL" },
    { RIG_FUNC_SCEN, "SCEN" },
    { RIG_FUNC_NONE, "" },
};

/**
 * utility function to convert index to bit value
 *
 */
// cppcheck-suppress *
uint64_t rig_idx2setting(int i)
{
    return ((uint64_t)1) << i;
}

/**
 * \brief Convert alpha string to enum RIG_FUNC_...
 * \param s input alpha string
 * \return RIG_FUNC_...
 *
 * \sa rig_func_e()
 */
setting_t HAMLIB_API rig_parse_func(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; func_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, func_str[i].str))
        {
            return func_str[i].func;
        }
    }

    return RIG_FUNC_NONE;
}


/**
 * \brief Convert enum RIG_FUNC_... to alpha string
 * \param func RIG_FUNC_...
 * \return alpha string
 *
 * \sa rig_func_e()
 */
const char *HAMLIB_API rig_strfunc(setting_t func)
{
    int i;

    // too verbose to keep on unless debugging this in particular
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (func == RIG_FUNC_NONE)
    {
        return "";
    }

    for (i = 0; func_str[i].str[0] != '\0'; i++)
    {
        if (func == func_str[i].func)
        {
            return func_str[i].str;
        }
    }

    return "";
}


static struct
{
    setting_t level;
    const char *str;
} level_str[] =
{
    { RIG_LEVEL_PREAMP, "PREAMP" },
    { RIG_LEVEL_ATT, "ATT" },
    { RIG_LEVEL_VOXDELAY, "VOXDELAY" },
    { RIG_LEVEL_AF, "AF" },
    { RIG_LEVEL_RF, "RF" },
    { RIG_LEVEL_SQL, "SQL" },
    { RIG_LEVEL_IF, "IF" },
    { RIG_LEVEL_APF, "APF" },
    { RIG_LEVEL_NR, "NR" },
    { RIG_LEVEL_PBT_IN, "PBT_IN" },
    { RIG_LEVEL_PBT_OUT, "PBT_OUT" },
    { RIG_LEVEL_CWPITCH, "CWPITCH" },
    { RIG_LEVEL_RFPOWER, "RFPOWER" },
    { RIG_LEVEL_MICGAIN, "MICGAIN" },
    { RIG_LEVEL_KEYSPD, "KEYSPD" },
    { RIG_LEVEL_NOTCHF, "NOTCHF" },
    { RIG_LEVEL_COMP, "COMP" },
    { RIG_LEVEL_AGC, "AGC" },
    { RIG_LEVEL_BKINDL, "BKINDL" },
    { RIG_LEVEL_BALANCE, "BAL" },
    { RIG_LEVEL_METER, "METER" },
    { RIG_LEVEL_VOXGAIN, "VOXGAIN" },
    { RIG_LEVEL_ANTIVOX, "ANTIVOX" },
    { RIG_LEVEL_SLOPE_LOW, "SLOPE_LOW" },
    { RIG_LEVEL_SLOPE_HIGH, "SLOPE_HIGH" },
    { RIG_LEVEL_BKIN_DLYMS, "BKIN_DLYMS" },
    { RIG_LEVEL_RAWSTR, "RAWSTR" },
    { RIG_LEVEL_SQLSTAT, "SQLSTAT" },
    { RIG_LEVEL_SWR, "SWR" },
    { RIG_LEVEL_ALC, "ALC" },
    { RIG_LEVEL_STRENGTH, "STRENGTH" },
    { RIG_LEVEL_RFPOWER_METER, "RFPOWER_METER" },
    { RIG_LEVEL_COMP_METER, "COMP_METER" },
    { RIG_LEVEL_VD_METER, "VD_METER" },
    { RIG_LEVEL_ID_METER, "ID_METER" },
    { RIG_LEVEL_NOTCHF_RAW, "NOTCHF_RAW" },
    { RIG_LEVEL_MONITOR_GAIN, "MONITOR_GAIN" },
    { RIG_LEVEL_NB, "NB" },
    { RIG_LEVEL_NONE, "" },
};

static struct
{
    setting_t level;
    const char *str;
} levelamp_str[] =
{
    { AMP_LEVEL_SWR, "SWR" },
    { AMP_LEVEL_NH, "NH" },
    { AMP_LEVEL_PF, "PF" },
    { AMP_LEVEL_PWR_INPUT, "PWRINPUT" },
    { AMP_LEVEL_PWR_FWD, "PWRFORWARD" },
    { AMP_LEVEL_PWR_REFLECTED, "PWRREFLECTED" },
    { AMP_LEVEL_PWR_PEAK, "PWRPEAK" },
    { AMP_LEVEL_FAULT, "FAULT" },
    { AMP_LEVEL_NONE, "" },
};


/**
 * \brief Convert alpha string to enum RIG_LEVEL_...
 * \param s input alpha string
 * \return RIG_LEVEL_...
 *
 * \sa rig_level_e()
 */
setting_t HAMLIB_API rig_parse_level(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; level_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, level_str[i].str))
        {
            return level_str[i].level;
        }
    }

    return RIG_LEVEL_NONE;
}

/**
 * \brief Convert alpha string to enum AMP_LEVEL_...
 * \param s input alpha string
 * \return AMP_LEVEL_...
 *
 * \sa amp_level_e()
 */
setting_t HAMLIB_API amp_parse_level(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called level=%s\n", __func__, s);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called str=%s\n", __func__,
              levelamp_str[0].str);

    for (i = 0 ; levelamp_str[i].str[0] != '\0'; i++)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s called checking=%s\n", __func__,
                  levelamp_str[i].str);

        if (!strcmp(s, levelamp_str[i].str))
        {
            return levelamp_str[i].level;
        }
    }

    return RIG_LEVEL_NONE;
}


/**
 * \brief Convert enum RIG_LEVEL_... to alpha string
 * \param level RIG_LEVEL_...
 * \return alpha string
 *
 * \sa rig_level_e()
 */
const char *HAMLIB_API rig_strlevel(setting_t level)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (level == RIG_LEVEL_NONE)
    {
        return "";
    }

    for (i = 0; level_str[i].str[0] != '\0'; i++)
    {
        if (level == level_str[i].level)
        {
            return level_str[i].str;
        }
    }

    return "";
}

/**
 * \brief Convert enum AMP_LEVEL_... to alpha string
 * \param level AMP_LEVEL_...
 * \return alpha string
 *
 * \sa amp_level_e()
 */
const char *HAMLIB_API amp_strlevel(setting_t level)
{
    int i;

    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (level == AMP_LEVEL_NONE)
    {
        return "";
    }

    for (i = 0; levelamp_str[i].str[0] != '\0'; i++)
    {
        if (level == levelamp_str[i].level)
        {
            return levelamp_str[i].str;
        }
    }

    return "";
}


static struct
{
    setting_t parm;
    const char *str;
} parm_str[] =
{
    { RIG_PARM_ANN, "ANN" },
    { RIG_PARM_APO, "APO" },
    { RIG_PARM_BACKLIGHT, "BACKLIGHT" },
    { RIG_PARM_BEEP, "BEEP" },
    { RIG_PARM_TIME, "TIME" },
    { RIG_PARM_BAT, "BAT" },
    { RIG_PARM_KEYLIGHT, "KEYLIGHT"},
    { RIG_PARM_SCREENSAVER, "SCREENSAVER"},
    { RIG_PARM_NONE, "" },
};


/**
 * \brief Convert alpha string to RIG_PARM_...
 * \param s input alpha string
 * \return RIG_PARM_...
 *
 * \sa rig_parm_e()
 */
setting_t HAMLIB_API rig_parse_parm(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; parm_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, parm_str[i].str))
        {
            return parm_str[i].parm;
        }
    }

    return RIG_PARM_NONE;
}


/**
 * \brief Convert enum RIG_PARM_... to alpha string
 * \param parm RIG_PARM_...
 * \return alpha string
 *
 * \sa rig_parm_e()
 */
const char *HAMLIB_API rig_strparm(setting_t parm)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (parm == RIG_PARM_NONE)
    {
        return "";
    }

    for (i = 0; parm_str[i].str[0] != '\0'; i++)
    {
        if (parm == parm_str[i].parm)
        {
            return parm_str[i].str;
        }
    }

    return "";
}


static struct
{
    vfo_op_t vfo_op;
    const char *str;
} vfo_op_str[] =
{
    { RIG_OP_CPY, "CPY" },
    { RIG_OP_XCHG, "XCHG" },
    { RIG_OP_FROM_VFO, "FROM_VFO" },
    { RIG_OP_TO_VFO, "TO_VFO" },
    { RIG_OP_MCL, "MCL" },
    { RIG_OP_UP, "UP" },
    { RIG_OP_DOWN, "DOWN" },
    { RIG_OP_BAND_UP, "BAND_UP" },
    { RIG_OP_BAND_DOWN, "BAND_DOWN" },
    { RIG_OP_LEFT, "LEFT" },
    { RIG_OP_RIGHT, "RIGHT" },
    { RIG_OP_TUNE, "TUNE" },
    { RIG_OP_TOGGLE, "TOGGLE" },
    { RIG_OP_NONE, "" },
};


/**
 * \brief Convert alpha string to enum RIG_OP_...
 * \param s alpha string
 * \return RIG_OP_...
 *
 * \sa vfo_op_t()
 */
vfo_op_t HAMLIB_API rig_parse_vfo_op(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; vfo_op_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, vfo_op_str[i].str))
        {
            return vfo_op_str[i].vfo_op;
        }
    }

    return RIG_OP_NONE;
}


/**
 * \brief Convert enum RIG_OP_... to alpha string
 * \param op RIG_OP_...
 * \return alpha string
 *
 * \sa vfo_op_t()
 */
const char *HAMLIB_API rig_strvfop(vfo_op_t op)
{
    int i;

// too verbose
//    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0; vfo_op_str[i].str[0] != '\0'; i++)
    {
        if (op == vfo_op_str[i].vfo_op)
        {
            return vfo_op_str[i].str;
        }
    }

    return "";
}


static struct
{
    scan_t rscan;
    const char *str;
} scan_str[] =
{
    { RIG_SCAN_STOP, "STOP" },
    { RIG_SCAN_MEM, "MEM" },
    { RIG_SCAN_SLCT, "SLCT" },
    { RIG_SCAN_PRIO, "PRIO" },
    { RIG_SCAN_PROG, "PROG" },
    { RIG_SCAN_DELTA, "DELTA" },
    { RIG_SCAN_VFO, "VFO" },
    { RIG_SCAN_PLT, "PLT" },
    { RIG_SCAN_NONE, "" },
    { -1, NULL }
};


/**
 * \brief Convert alpha string to enum RIG_SCAN_...
 * \param s alpha string
 * \return RIG_SCAN_...
 *
 * \sa scan_t()
 */
scan_t HAMLIB_API rig_parse_scan(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; scan_str[i].str[0] != '\0'; i++)
    {
        if (strcmp(s, scan_str[i].str) == 0)
        {
            return scan_str[i].rscan;
        }
    }

    return RIG_SCAN_NONE;
}


/**
 * \brief Convert enum RIG_SCAN_... to alpha string
 * \param rscan RIG_SCAN_...
 * \return alpha string
 *
 * \sa scan_t()
 */
const char *HAMLIB_API rig_strscan(scan_t rscan)
{
    int i;

// too verbose
//    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (rscan == RIG_SCAN_NONE)
    {
        return "";
    }

    for (i = 0; scan_str[i].str[0] != '\0'; i++)
    {
        if (rscan == scan_str[i].rscan)
        {
            return scan_str[i].str;
        }
    }

    return "";
}


/**
 * \brief convert enum RIG_RPT_SHIFT_... to printable character
 * \param shift RIG_RPT_SHIFT_??
 * \return alpha character
 */
const char *HAMLIB_API rig_strptrshift(rptr_shift_t shift)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (shift)
    {
    case RIG_RPT_SHIFT_MINUS:
        return "-";

    case RIG_RPT_SHIFT_PLUS:
        return "+";

    case RIG_RPT_SHIFT_NONE:
        return "None";
    }

    return NULL;
}


/**
 * \brief Convert alpha char to enum RIG_RPT_SHIFT_...
 * \param s alpha char
 * \return RIG_RPT_SHIFT_...
 */
rptr_shift_t HAMLIB_API rig_parse_rptr_shift(const char *s)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (strcmp(s, "+") == 0)
    {
        return RIG_RPT_SHIFT_PLUS;
    }
    else if (strcmp(s, "-") == 0)
    {
        return RIG_RPT_SHIFT_MINUS;
    }
    else
    {
        return RIG_RPT_SHIFT_NONE;
    }
}


static struct
{
    chan_type_t mtype;
    const char *str;
} mtype_str[] =
{
    { RIG_MTYPE_MEM, "MEM" },
    { RIG_MTYPE_EDGE, "EDGE" },
    { RIG_MTYPE_CALL, "CALL" },
    { RIG_MTYPE_MEMOPAD, "MEMOPAD" },
    { RIG_MTYPE_SAT, "SAT" },
    { RIG_MTYPE_BAND, "BAND" },
    { RIG_MTYPE_PRIO, "PRIO" },
    { RIG_MTYPE_NONE, "" },
};


/**
 * \brief Convert alpha string to enum RIG_MTYPE_...
 * \param s alpha string
 * \return RIG_MTYPE_...
 *
 * \sa chan_type_t()
 */
chan_type_t HAMLIB_API rig_parse_mtype(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; mtype_str[i].str[0] != '\0'; i++)
    {
        if (strcmp(s, mtype_str[i].str) == 0)
        {
            return mtype_str[i].mtype;
        }
    }

    return RIG_MTYPE_NONE;
}


/**
 * \brief Convert enum RIG_MTYPE_... to alpha string
 * \param mtype RIG_MTYPE_...
 * \return alpha string
 *
 * \sa chan_type_t()
 */
const char *HAMLIB_API rig_strmtype(chan_type_t mtype)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (mtype == RIG_MTYPE_NONE)
    {
        return "";
    }

    for (i = 0; mtype_str[i].str[0] != '\0'; i++)
    {
        if (mtype == mtype_str[i].mtype)
        {
            return mtype_str[i].str;
        }
    }

    return "";
}


static long timediff(const struct timeval *tv1, const struct timeval *tv2)
{
    struct timeval tv;

    tv.tv_usec = tv1->tv_usec - tv2->tv_usec;
    tv.tv_sec  = tv1->tv_sec  - tv2->tv_sec;

    return ((tv.tv_sec * 1000L) + (tv.tv_usec / 1000L));
}


/**
 * \brief Helper for checking cache timeout
 * \param tv       pointer to timeval, date of cache
 * \param timeout  duration of cache validity, in millisec
 * \return 1 when timed out, 0 when cache shall be used
 */
int HAMLIB_API rig_check_cache_timeout(const struct timeval *tv, int timeout)
{
    struct timeval curr;
    long t;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (tv->tv_sec == 0 && tv->tv_usec == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: forced cache timeout\n",
                  __func__);
        return 1;
    }

    gettimeofday(&curr, NULL);

    t = timediff(&curr, tv);

    if (t < timeout)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: using cache (%ld ms)\n",
                  __func__,
                  t);
        return 0;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: cache timed out (%ld ms)\n",
                  __func__,
                  t);
        return 1;
    }
}


/**
 * \brief Helper for forcing cache timeout next call
 *
 * This function is typically to be called in backend_set_* functions,
 * so that a sequence:
 *
\code
    rig_get_freq();
    rig_set_freq();
    rig_get_freq();
\endcode
 *
 * doesn't return a bogus (cached) value in the last rig_get_freq().
 *
 * \param tv       pointer to timeval to be reset
 */
void HAMLIB_API rig_force_cache_timeout(struct timeval *tv)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    tv->tv_sec = 0;
    tv->tv_usec = 0;
}


//! @cond Doxygen_Suppress
int no_restore_ai;
//! @endcond


//! @cond Doxygen_Suppress
void HAMLIB_API rig_no_restore_ai()
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    no_restore_ai = -1;
}

//! @cond Doxygen_Suppress
double HAMLIB_API elapsed_ms(struct timespec *start, int option)
{
    // If option then we are starting the timing, else we get elapsed
    struct timespec stop;
    double elapsed_msec;

    if (option == ELAPSED_SET)
    {
        start->tv_sec = start->tv_nsec = 0;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: start = %ld,%ld\n", __func__,
              (long)start->tv_sec, (long)start->tv_nsec);


    switch (option)
    {
    case ELAPSED_GET:
        if (start->tv_nsec == 0)   // if we haven't done SET yet
        {
            clock_gettime(CLOCK_REALTIME, start);
            return 1000 * 1000;
        }

        clock_gettime(CLOCK_REALTIME, &stop);
        break;

    case ELAPSED_SET:
        clock_gettime(CLOCK_REALTIME, start);
        rig_debug(RIG_DEBUG_TRACE, "%s: after gettime, start = %ld,%ld\n", __func__,
                  (long)start->tv_sec, (long)start->tv_nsec);
        return 999 * 1000; // so we can tell the difference in debug where we came from
        break;

    case ELAPSED_INVALIDATE:
        clock_gettime(CLOCK_REALTIME, start);
        start->tv_sec -= 3600;
        break;
    }

    elapsed_msec = ((stop.tv_sec - start->tv_sec) + (stop.tv_nsec / 1e9 -
                    start->tv_nsec / 1e9)) * 1e3;

    rig_debug(RIG_DEBUG_TRACE, "%s: elapsed_msecs=%g\n", __func__, elapsed_msec);

    if (elapsed_msec < 0 || option == ELAPSED_INVALIDATE) { return 1000000; }

    return elapsed_msec;
}

int HAMLIB_API rig_get_cache_timeout_ms(RIG *rig, cache_t selection)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called selection=%d\n", __func__, selection);
    return rig->state.cache.timeout_ms;
}

int HAMLIB_API rig_set_cache_timeout_ms(RIG *rig, cache_t selection, int ms)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called selection=%d, ms=%d\n", __func__,
              selection, ms);
    rig->state.cache.timeout_ms = ms;
    return RIG_OK;
}

//! @endcond

/** @} */
