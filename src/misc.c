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

#include <hamlib/config.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include <math.h>

#include <hamlib/rig.h>
#include <hamlib/rotator.h>
#include <hamlib/amplifier.h>

#include "misc.h"
#include "serial.h"
#include "network.h"

#if defined(_WIN32)
#  include <time.h>
#  ifndef localtime_r
#    define localtime_r(T,Tm) (localtime_s(Tm,T) ? NULL : Tm)
#  endif
#endif

#ifdef __APPLE__

#include <time.h>

#if !defined(CLOCK_REALTIME) && !defined(CLOCK_MONOTONIC)
//
// MacOS < 10.12 does not have clock_gettime
//
// Contribution from github user "ra1nb0w"
//

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 6
typedef int clockid_t;

#include <sys/time.h>
#include <mach/mach_time.h>

static int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    if (clock_id == CLOCK_REALTIME)
    {
        struct timeval t;

        if (gettimeofday(&t, NULL) != 0)
        {
            return -1;
        }

        tp->tv_sec = t.tv_sec;
        tp->tv_nsec = t.tv_usec * 1000;
    }
    else if (clock_id == CLOCK_MONOTONIC)
    {
        static mach_timebase_info_data_t info = { 0, 0 };

        if (info.denom == 0)
        {
            mach_timebase_info(&info);
        }

        uint64_t t = mach_absolute_time() * info.numer / info.denom;
        tp->tv_sec = t / 1000000000;
        tp->tv_nsec = t % 1000000000;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

#endif // !HAVE_CLOCK_GETTIME

#endif // __APPLE__

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

size_t HAMLIB_API to_hex(size_t source_length, const unsigned char *source_data,
                         size_t dest_length, char *dest_data)
{
    size_t i;
    size_t length = source_length;
    const unsigned char *source = source_data;
    char *dest = dest_data;

    if (source_length == 0 || dest_length == 0)
    {
        return 0;
    }

    if (source_length * 2 > dest_length)
    {
        length = dest_length / 2 - 1;
    }

    for (i = 0; i < length; i++)
    {
        SNPRINTF(dest, dest_length - 2 * i, "%02X", source[0]);
        source++;
        dest += 2;
    }

    return length;
}

/**
 * \brief Convert duration of one morse code dot (element) to milliseconds at the given speed.
 * \param wpm morse code speed in words per minute
 * \return double duration in milliseconds
 *
 * The morse code speed is calculated using the standard based on word PARIS.
 *
 * "If you send PARIS 5 times in a minute (5WPM) you have sent 250 elements (using correct spacing).
 * 250 elements into 60 seconds per minute = 240 milliseconds per element."
 *
 * Source: http://kent-engineers.com/codespeed.htm
 */
double morse_code_dot_to_millis(int wpm)
{
    return 240.0 * (5.0 / (double) wpm);
}

/**
 * \brief Convert duration of tenths of morse code dots to milliseconds at the given speed.
 * \param tenths_of_dots number of 1/10ths of dots
 * \param wpm morse code speed in words per minute
 * \return int duration in milliseconds
 *
 * The morse code speed is calculated using the standard based on word PARIS.
 */
int dot10ths_to_millis(int dot10ths, int wpm)
{
    return ceil(morse_code_dot_to_millis(wpm) * (double) dot10ths / 10.0);
}

/**
 * \brief Convert duration in milliseconds to tenths of morse code dots at the given speed.
 * \param millis duration in milliseconds
 * \param wpm morse code speed in words per minute
 * \return int number of 1/10ths of dots
 *
 * The morse code speed is calculated using the standard based on word PARIS.
 */
int millis_to_dot10ths(int millis, int wpm)
{
    return ceil(millis / morse_code_dot_to_millis(wpm) * 10.0);
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
int HAMLIB_API sprintf_freq(char *str, int str_len, freq_t freq)
{
    double f;
    char *hz;
    int decplaces = 10;
    int retval;

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
        decplaces = 7;
    }
    else if (llabs(freq) >= kHz(1))
    {
        hz = "kHz";
        f = (double)freq / kHz(1);
        decplaces = 4;
    }
    else
    {
        hz = "Hz";
        f = (double)freq;
        decplaces = 1;
    }

    SNPRINTF(str, str_len, "%.*f %s", decplaces, f, hz);
    retval = strlen(str);
    return retval;
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


static const struct
{
    rmode_t mode;
    const char *str;
} mode_str[] =
{
    { RIG_MODE_AM, "AM" },
    { RIG_MODE_PKTAM, "AM-D" },
    { RIG_MODE_CW, "CW" },
    { RIG_MODE_USB, "USB" },
    { RIG_MODE_LSB, "LSB" },
    { RIG_MODE_RTTY, "RTTY" },
    { RIG_MODE_FM, "FM" },
    { RIG_MODE_PKTFM, "FM-D" },
    { RIG_MODE_WFM, "WFM" },
    { RIG_MODE_CWR, "CWR" },
    { RIG_MODE_CWR, "CW-R" },
    { RIG_MODE_RTTYR, "RTTYR" },
    { RIG_MODE_RTTYR, "RTTY-R" },
    { RIG_MODE_AMS, "AMS" },
    { RIG_MODE_PKTLSB, "PKTLSB" },
    { RIG_MODE_PKTUSB, "PKTUSB" },
    { RIG_MODE_PKTLSB, "LSB-D" },
    { RIG_MODE_PKTUSB, "USB-D" },
    { RIG_MODE_PKTFM, "PKTFM" },
    { RIG_MODE_PKTFMN, "PKTFMN" },
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
    { RIG_MODE_SPEC, "SPEC"},
    { RIG_MODE_CWN, "CWN"},
    { RIG_MODE_IQ, "IQ"},
    { RIG_MODE_ISBUSB, "ISBUSB"},
    { RIG_MODE_ISBLSB, "ISBLSB"},
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

    rig_debug(RIG_DEBUG_WARN, "%s: mode '%s' not found\n", __func__, s);
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
        SNPRINTF(buf, buflen, "NONE");
        return RIG_OK;
    }

    for (i = 0 ; mode_str[i].str[0] != '\0'; i++)
    {
        if (modes & mode_str[i].mode)
        {
            char modebuf[16];

            if (strlen(buf) == 0) { SNPRINTF(modebuf, sizeof(modebuf), "%s", mode_str[i].str); }
            else { SNPRINTF(modebuf, sizeof(modebuf), " %s", mode_str[i].str); }

            strncat(buf, modebuf, buflen - strlen(buf) - 1);

            if (strlen(buf) > buflen - 10) { return -RIG_ETRUNC; }
        }
    }

    return RIG_OK;
}


static const struct
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
    { RIG_VFO_MAIN_C, "MainC" },
    { RIG_VFO_SUB, "Sub" },
    { RIG_VFO_SUB_A, "SubA" },
    { RIG_VFO_SUB_B, "SubB" },
    { RIG_VFO_SUB_C, "SubC" },
    { RIG_VFO_NONE, "None" },
    { RIG_VFO_OTHER, "otherVFO" },
    { RIG_VFO_ALL, "AllVFOs" },
    { 0xffffffff, "" },
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
            rig_debug(RIG_DEBUG_CACHE, "%s: str='%s' vfo='%s'\n", __func__, vfo_str[i].str,
                      rig_strvfo(vfo_str[i].vfo));
            return vfo_str[i].vfo;
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: '%s' not found so vfo='%s'\n", __func__, s,
              rig_strvfo(RIG_VFO_NONE));
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


static const struct
{
    setting_t func;
    const char *str;
} rig_func_str[] =
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
    { RIG_FUNC_TRANSCEIVE, "TRANSCEIVE" },
    { RIG_FUNC_SPECTRUM, "SPECTRUM" },
    { RIG_FUNC_SPECTRUM_HOLD, "SPECTRUM_HOLD" },
    { RIG_FUNC_SEND_MORSE, "SEND_MORSE" },
    { RIG_FUNC_SEND_VOICE_MEM, "SEND_VOICE_MEM" },
    { RIG_FUNC_OVF_STATUS, "OVF_STATUS" },
    { RIG_FUNC_NONE, "" },
};


static const struct
{
    setting_t func;
    const char *str;
} rot_func_str[] =
{
    { ROT_FUNC_NONE, "" },
};


/**
 * utility function to convert index to bit value
 *
 */
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

    for (i = 0 ; rig_func_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, rig_func_str[i].str))
        {
            return rig_func_str[i].func;
        }
    }

    return RIG_FUNC_NONE;
}


/**
 * \brief Convert alpha string to enum ROT_FUNC_...
 * \param s input alpha string
 * \return ROT_FUNC_...
 *
 * \sa rot_func_e()
 */
setting_t HAMLIB_API rot_parse_func(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; rot_func_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, rot_func_str[i].str))
        {
            return rot_func_str[i].func;
        }
    }

    return ROT_FUNC_NONE;
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

    for (i = 0; rig_func_str[i].str[0] != '\0'; i++)
    {
        if (func == rig_func_str[i].func)
        {
            return rig_func_str[i].str;
        }
    }

    return "";
}


/**
 * \brief Convert enum ROT_FUNC_... to alpha string
 * \param func ROT_FUNC_...
 * \return alpha string
 *
 * \sa rot_func_e()
 */
const char *HAMLIB_API rot_strfunc(setting_t func)
{
    int i;

    // too verbose to keep on unless debugging this in particular
    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (func == ROT_FUNC_NONE)
    {
        return "";
    }

    for (i = 0; rot_func_str[i].str[0] != '\0'; i++)
    {
        if (func == rot_func_str[i].func)
        {
            return rot_func_str[i].str;
        }
    }

    return "";
}


static const struct
{
    setting_t level;
    const char *str;
} rig_level_str[] =
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
    { RIG_LEVEL_RFPOWER_METER_WATTS, "RFPOWER_METER_WATTS" },
    { RIG_LEVEL_SPECTRUM_MODE, "SPECTRUM_MODE" },
    { RIG_LEVEL_SPECTRUM_SPAN, "SPECTRUM_SPAN" },
    { RIG_LEVEL_SPECTRUM_EDGE_LOW, "SPECTRUM_EDGE_LOW" },
    { RIG_LEVEL_SPECTRUM_EDGE_HIGH, "SPECTRUM_EDGE_HIGH" },
    { RIG_LEVEL_SPECTRUM_SPEED, "SPECTRUM_SPEED" },
    { RIG_LEVEL_SPECTRUM_REF, "SPECTRUM_REF" },
    { RIG_LEVEL_SPECTRUM_AVG, "SPECTRUM_AVG" },
    { RIG_LEVEL_SPECTRUM_ATT, "SPECTRUM_ATT" },
    { RIG_LEVEL_TEMP_METER, "TEMP_METER" },
    { RIG_LEVEL_BAND_SELECT, "BAND_SELECT" },
    { RIG_LEVEL_USB_AF, "USB_AF" },
    { RIG_LEVEL_AGC_TIME, "AGC_TIME" },
    { RIG_LEVEL_NONE, "" },
};


static const struct
{
    setting_t level;
    const char *str;
} rot_level_str[] =
{
    { ROT_LEVEL_SPEED, "SPEED" },
    { ROT_LEVEL_NONE, "" },
};


static const struct
{
    setting_t level;
    const char *str;
} amp_level_str[] =
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

    for (i = 0 ; rig_level_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, rig_level_str[i].str))
        {
            return rig_level_str[i].level;
        }
    }

    return RIG_LEVEL_NONE;
}


/**
 * \brief Convert alpha string to enum ROT_LEVEL_...
 * \param s input alpha string
 * \return ROT_LEVEL_...
 *
 * \sa rot_level_e()
 */
setting_t HAMLIB_API rot_parse_level(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; rot_level_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, rot_level_str[i].str))
        {
            return rot_level_str[i].level;
        }
    }

    return ROT_LEVEL_NONE;
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
              amp_level_str[0].str);

    for (i = 0 ; amp_level_str[i].str[0] != '\0'; i++)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s called checking=%s\n", __func__,
                  amp_level_str[i].str);

        if (!strcmp(s, amp_level_str[i].str))
        {
            return amp_level_str[i].level;
        }
    }

    return AMP_LEVEL_NONE;
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

    for (i = 0; rig_level_str[i].str[0] != '\0'; i++)
    {
        if (level == rig_level_str[i].level)
        {
            return rig_level_str[i].str;
        }
    }

    return "";
}


/**
 * \brief Convert enum ROT_LEVEL_... to alpha string
 * \param level ROT_LEVEL_...
 * \return alpha string
 *
 * \sa rot_level_e()
 */
const char *HAMLIB_API rot_strlevel(setting_t level)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (level == ROT_LEVEL_NONE)
    {
        return "";
    }

    for (i = 0; rot_level_str[i].str[0] != '\0'; i++)
    {
        if (level == rot_level_str[i].level)
        {
            return rot_level_str[i].str;
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

    for (i = 0; amp_level_str[i].str[0] != '\0'; i++)
    {
        if (level == amp_level_str[i].level)
        {
            return amp_level_str[i].str;
        }
    }

    return "";
}


static const struct
{
    setting_t parm;
    const char *str;
} rig_parm_str[] =
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


static const struct
{
    setting_t parm;
    const char *str;
} rot_parm_str[] =
{
    { ROT_PARM_NONE, "" },
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

    for (i = 0 ; rig_parm_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, rig_parm_str[i].str))
        {
            return rig_parm_str[i].parm;
        }
    }

    return RIG_PARM_NONE;
}


/**
 * \brief Convert alpha string to ROT_PARM_...
 * \param s input alpha string
 * \return ROT_PARM_...
 *
 * \sa rot_parm_e()
 */
setting_t HAMLIB_API rot_parse_parm(const char *s)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0 ; rot_parm_str[i].str[0] != '\0'; i++)
    {
        if (!strcmp(s, rot_parm_str[i].str))
        {
            return rot_parm_str[i].parm;
        }
    }

    return ROT_PARM_NONE;
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

    for (i = 0; rig_parm_str[i].str[0] != '\0'; i++)
    {
        if (parm == rig_parm_str[i].parm)
        {
            return rig_parm_str[i].str;
        }
    }

    return "";
}


/**
 * \brief Convert enum ROT_PARM_... to alpha string
 * \param parm ROT_PARM_...
 * \return alpha string
 *
 * \sa rot_parm_e()
 */
const char *HAMLIB_API rot_strparm(setting_t parm)
{
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (parm == ROT_PARM_NONE)
    {
        return "";
    }

    for (i = 0; rot_parm_str[i].str[0] != '\0'; i++)
    {
        if (parm == rot_parm_str[i].parm)
        {
            return rot_parm_str[i].str;
        }
    }

    return "";
}

static const struct
{
    enum agc_level_e level;
    const char *str;
} rig_agc_level_str[] =
{
    { RIG_AGC_OFF, "OFF" },
    { RIG_AGC_SUPERFAST, "SUPERFAST" },
    { RIG_AGC_FAST, "FAST" },
    { RIG_AGC_SLOW, "SLOW" },
    { RIG_AGC_USER, "USER" },
    { RIG_AGC_MEDIUM, "MEDIUM" },
    { RIG_AGC_AUTO, "AUTO" },
    { RIG_AGC_LONG, "LONG" },
    { RIG_AGC_ON, "ON" },
    { RIG_AGC_NONE, "NONE" },
    { -1, "" },
};

/**
 * \brief Convert enum RIG_AGC_... to alpha string
 * \param mode RIG_AGC_...
 * \return alpha string
 */
const char *HAMLIB_API rig_stragclevel(enum agc_level_e level)
{
    int i;

    if (level < 0)
    {
        return "";
    }

    for (i = 0; rig_agc_level_str[i].str[0] != '\0'; i++)
    {
        if (level == rig_agc_level_str[i].level)
        {
            return rig_agc_level_str[i].str;
        }
    }

    return "";
}

/**
 * \brief Convert a enum agc_level_e to value
 * \param integer...
 * \return agc_level_e value
 */
value_t rig_valueagclevel(enum agc_level_e agcLevel)
{
    value_t value;

    if (agcLevel == RIG_AGC_OFF) { value.i = 0; }
    else if (agcLevel == RIG_AGC_SUPERFAST) { value.i = 1; }
    else if (agcLevel == RIG_AGC_FAST) { value.i = 2; }
    else if (agcLevel == RIG_AGC_SLOW) { value.i = 3; }
    else if (agcLevel == RIG_AGC_USER) { value.i = 4; }
    else if (agcLevel == RIG_AGC_MEDIUM) { value.i = 5; }
    else { value.i = 6; } //RIG_AGC_AUTO

    return value;
}

/**
 * \brief Convert a value to agc_level_e -- constrains the range
 * \param integer...
 * \return agc_level_e
 */
enum agc_level_e rig_levelagcvalue(int agcValue)
{
    enum agc_level_e agcLevel;

    switch (agcValue)
    {
    case 0: agcLevel = RIG_AGC_OFF; break;

    case 1: agcLevel = RIG_AGC_SUPERFAST; break;

    case 2: agcLevel = RIG_AGC_FAST; break;

    case 3: agcLevel = RIG_AGC_SLOW; break;

    case 4: agcLevel = RIG_AGC_USER; break;

    case 5: agcLevel = RIG_AGC_MEDIUM; break;

    case 6: agcLevel = RIG_AGC_AUTO; break;

    default: agcLevel = RIG_AGC_AUTO; break;
    }

    return agcLevel;
}

/**
 * \brief Convert AGC string... to agc_level_e
 * \param mode AGC string...
 * \return agc_level_e
 */
enum agc_level_e rig_levelagcstr(char *agcString)
{
    enum agc_level_e agcLevel;

    if (strcmp(agcString, "OFF") == 0) { agcLevel = RIG_AGC_OFF; }
    else if (strcmp(agcString, "SUPERFAST") == 0) { agcLevel = RIG_AGC_SUPERFAST; }
    else if (strcmp(agcString, "FAST") == 0) { agcLevel = RIG_AGC_FAST; }
    else if (strcmp(agcString, "SLOW") == 0) { agcLevel = RIG_AGC_SLOW; }
    else if (strcmp(agcString, "USER") == 0) { agcLevel = RIG_AGC_USER; }
    else if (strcmp(agcString, "MEDIUM") == 0) { agcLevel = RIG_AGC_MEDIUM; }
    else { agcLevel = RIG_AGC_AUTO; }

    return agcLevel;
}


static const struct
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


static const struct
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


static const struct
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

static const struct
{
    enum rig_spectrum_mode_e mode;
    const char *str;
} rig_spectrum_mode_str[] =
{
    { RIG_SPECTRUM_MODE_CENTER, "CENTER" },
    { RIG_SPECTRUM_MODE_FIXED, "FIXED" },
    { RIG_SPECTRUM_MODE_CENTER_SCROLL, "CENTER_SCROLL" },
    { RIG_SPECTRUM_MODE_FIXED_SCROLL, "FIXED_SCROLL" },
    { RIG_SPECTRUM_MODE_NONE, "" },
};

/**
 * \brief Convert enum RIG_SPECTRUM_MODE_... to alpha string
 * \param mode RIG_SPECTRUM_MODE_...
 * \return alpha string
 */
const char *HAMLIB_API rig_strspectrummode(enum rig_spectrum_mode_e mode)
{
    int i;

    if (mode == RIG_SPECTRUM_MODE_NONE)
    {
        return "";
    }

    for (i = 0; rig_spectrum_mode_str[i].str[0] != '\0'; i++)
    {
        if (mode == rig_spectrum_mode_str[i].mode)
        {
            return rig_spectrum_mode_str[i].str;
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

    if (option == HAMLIB_ELAPSED_SET)
    {
        start->tv_sec = start->tv_nsec = 0;
    }

    stop = *start; // just to suppress some compiler warnings

    //rig_debug(RIG_DEBUG_TRACE, "%s: start = %ld,%ld\n", __func__,
    //          (long)start->tv_sec, (long)start->tv_nsec);


    switch (option)
    {
    case HAMLIB_ELAPSED_GET:
        if (start->tv_nsec == 0)   // if we haven't done SET yet
        {
            clock_gettime(CLOCK_REALTIME, start);
            return 1000 * 1000;
        }

        clock_gettime(CLOCK_REALTIME, &stop);
        break;

    case HAMLIB_ELAPSED_SET:
        clock_gettime(CLOCK_REALTIME, start);
        //rig_debug(RIG_DEBUG_TRACE, "%s: after gettime, start = %ld,%ld\n", __func__,
        //          (long)start->tv_sec, (long)start->tv_nsec);
        return 999 * 1000; // so we can tell the difference in debug where we came from
        break;

    case HAMLIB_ELAPSED_INVALIDATE:
        clock_gettime(CLOCK_REALTIME, start);
        stop = *start;
        start->tv_sec -= 10; // ten seconds should be more than enough
        break;
    }

    elapsed_msec = ((stop.tv_sec - start->tv_sec) + (stop.tv_nsec / 1e9 -
                    start->tv_nsec / 1e9)) * 1e3;

    //rig_debug(RIG_DEBUG_TRACE, "%s: elapsed_msecs=%.0f\n", __func__, elapsed_msec);

    if (elapsed_msec < 0 || option == HAMLIB_ELAPSED_INVALIDATE) { return 1000000; }

    return elapsed_msec;
}

int HAMLIB_API rig_get_cache_timeout_ms(RIG *rig, hamlib_cache_t selection)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called selection=%d\n", __func__, selection);
    return rig->state.cache.timeout_ms;
}

int HAMLIB_API rig_set_cache_timeout_ms(RIG *rig, hamlib_cache_t selection,
                                        int ms)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called selection=%d, ms=%d\n", __func__,
              selection, ms);
    rig->state.cache.timeout_ms = ms;
    return RIG_OK;
}

static char *funcname = "Unknown";
static int linenum = 0;

#undef vfo_fixup
vfo_t HAMLIB_API vfo_fixup2a(RIG *rig, vfo_t vfo, split_t split,
                             const char *func, int line)
{
    funcname = (char *)func;
    linenum = (int)line;
    return vfo_fixup(rig, vfo, split);
}

// we're mapping our VFO here to work with either VFO A/B rigs or Main/Sub
// Hamlib uses VFO_A  and VFO_B as TX/RX as of 2021-04-13
// So we map these to Main/Sub as required
vfo_t HAMLIB_API vfo_fixup(RIG *rig, vfo_t vfo, split_t split)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:(from %s:%d) vfo=%s, vfo_curr=%s, split=%d\n",
              __func__, funcname, linenum,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo), split);

    if (vfo == RIG_VFO_NONE) { vfo = RIG_VFO_A; }

    if (vfo == RIG_VFO_CURR || vfo == RIG_VFO_VFO)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Leaving currVFO alone\n", __func__);
        return vfo;  // don't modify vfo for RIG_VFO_CURR
    }

    if (vfo == RIG_VFO_OTHER)
    {
        switch (rig->state.current_vfo)
        {
        case RIG_VFO_A:
            return RIG_VFO_B;

        case RIG_VFO_MAIN:
            return RIG_VFO_SUB;

        case RIG_VFO_B:
            return RIG_VFO_A;

        case RIG_VFO_SUB:
            return RIG_VFO_MAIN;

        case RIG_VFO_SUB_A:
            return RIG_VFO_SUB_B;

        case RIG_VFO_SUB_B:
            return RIG_VFO_SUB_A;
        }
    }

    if (vfo == RIG_VFO_RX)
    {
        vfo = rig->state.rx_vfo;
    }
    else if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN)
    {
        vfo = RIG_VFO_A; // default to mapping VFO_MAIN to VFO_A

        if (VFO_HAS_MAIN_SUB_ONLY) { vfo = RIG_VFO_MAIN; }

        if (VFO_HAS_MAIN_SUB_A_B_ONLY) { vfo = RIG_VFO_MAIN; }
    }
    else if (vfo == RIG_VFO_TX)
    {
#if 0
        int retval;
        split_t split = 0;
        // get split if we can -- it will default to off otherwise
        // maybe split/satmode/vfo/freq/mode can be cached for rigs
        // that don't have read capability or get_vfo like Icom?
        // Icom's lack of get_vfo is problematic in this respect
        // If we cache vfo or others than twiddling the rig may cause problems
        retval = rig_get_split(rig, vfo, &split);

        if (retval != RIG_OK)
        {
            split = rig->state.cache.split;
        }

#endif

        int satmode = rig->state.cache.satmode;

        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): split=%d, vfo==%s tx_vfo=%s\n", __func__,
                  __LINE__, split, rig_strvfo(vfo), rig_strvfo(rig->state.tx_vfo));

        //if (vfo == RIG_VFO_TX) { vfo = rig->state.tx_vfo; RETURNFUNC(RIG_OK); }

        if (VFO_HAS_MAIN_SUB_ONLY && !split && !satmode && vfo != RIG_VFO_B) { vfo = RIG_VFO_MAIN; }

        else if (VFO_HAS_MAIN_SUB_ONLY && (split || satmode || vfo == RIG_VFO_B)) { vfo = RIG_VFO_SUB; }

        else if (VFO_HAS_MAIN_SUB_A_B_ONLY && split) { vfo = RIG_VFO_B; }

        else if (VFO_HAS_MAIN_SUB_A_B_ONLY && satmode) { vfo = RIG_VFO_SUB; }

        else if (VFO_HAS_A_B_ONLY) { vfo = split ? RIG_VFO_B : RIG_VFO_A; }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: RIG_VFO_TX changed to %s, split=%d, satmode=%d\n", __func__,
                  rig_strvfo(vfo), split, satmode);
    }
    else if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
    {
        vfo = RIG_VFO_B;  // default to VFO_B

        if (VFO_HAS_MAIN_SUB_ONLY) { vfo = RIG_VFO_SUB; }

        if (VFO_HAS_MAIN_SUB_A_B_ONLY) { vfo = RIG_VFO_SUB; }

        rig_debug(RIG_DEBUG_TRACE, "%s: final vfo=%s\n", __func__, rig_strvfo(vfo));
    }

    return vfo;
}

int HAMLIB_API parse_hoststr(char *hoststr, int hoststr_len, char host[256],
                             char port[6])
{
    unsigned int net1, net2, net3, net4, net5, net6, net7, net8;
    char dummy[6], link[32], *p;
    host[0] = 0;
    port[0] = 0;
    dummy[0] = 0;

    // Exclude any names that aren't a host:port format
    // Handle device names 1st
    if (strstr(hoststr, "/dev")) { return -1; }

    if (strstr(hoststr, "/")) { return -1; } // probably a path so not a hoststr

    if (strncasecmp(hoststr, "com", 3) == 0) { return -1; }

    // escaped COM port like \\.\COM3 or \.\COM3
    if (strstr(hoststr, "\\.\\")) { return -1; }

    // Now let's try and parse a host:port thing
    // bracketed IPV6 with optional port
    int n = sscanf(hoststr, "[%255[^]]]:%5s", host, port);

    if (n >= 1)
    {
        return RIG_OK;
    }

    // non-bracketed full IPV6 with optional link addr
    n = sscanf(hoststr, "%x:%x:%x:%x:%x:%x:%x:%x%%%31[^:]:%5s", &net1, &net2, &net3,
               &net4, &net5, &net6, &net7, &net8, link, port);

    if (n == 8 || n == 9)
    {
        strcpy(host, hoststr);
        return RIG_OK;
    }
    else if (n == 10)
    {
        strcpy(host, hoststr);
        p = strrchr(host, ':'); // remove port from host
        *p = 0;
        return RIG_OK;
    }

    // non-bracketed IPV6 with optional link addr and optional port
    n = sscanf(hoststr, "%x::%x:%x:%x:%x%%%31[^:]:%5s", &net1, &net2, &net3,
               &net4, &net5, link, port);

    if (strchr(hoststr, '%') && (n == 5 || n == 6))
    {
        strcpy(host, hoststr);
        return RIG_OK;
    }
    else if (n == 7)
    {
        strcpy(host, hoststr);
        p = strrchr(host, ':'); // remove port from host
        *p = 0;
        return RIG_OK;
    }

    // non-bracketed IPV6 short form with optional port
    n = sscanf(hoststr, "%x::%x:%x:%x:%x:%5[0-9]%1s", &net1, &net2, &net3, &net4,
               &net5, port, dummy);

    if (n == 5)
    {
        strcpy(host, hoststr);
        return RIG_OK;
    }
    else if (n == 6)
    {
        strcpy(host, hoststr);
        p = strrchr(host, ':');
        *p = 0;
        return RIG_OK;
    }
    else if (n == 7)
    {
        return -RIG_EINVAL;
    }

    // bracketed localhost
    if (strstr(hoststr, "::1"))
    {
        n = sscanf(hoststr, "::1%5s", dummy);
        strcpy(host, hoststr);

        if (n == 1)
        {
            p = strrchr(host, ':');
            *p = 0;
            strcpy(port, p + 1);
        }

        return RIG_OK;
    }

    if (sscanf(hoststr, ":%5[0-9]%1s", port,
               dummy) == 1) // just a port if you please
    {
        SNPRINTF(hoststr, hoststr_len, "%s:%s\n", "localhost", port);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: hoststr=%s\n", __func__, hoststr);
        return RIG_OK;
    }

    // if we're here then we must have a hostname
    n = sscanf(hoststr, "%255[^:]:%5[0-9]%1s", host, port, dummy);

    if (n >= 1 && strlen(dummy) == 0) { return RIG_OK; }

    printf("Unhandled host=%s\n", hoststr);

    return -1;
}

//K3 was showing stacked command replies so re-enabling this
//#define RIG_FLUSH_REMOVE
int HAMLIB_API rig_flush(hamlib_port_t *port)
{
#ifndef RIG_FLUSH_REMOVE
    rig_debug(RIG_DEBUG_TRACE, "%s: called for %s device\n", __func__,
              port->type.rig == RIG_PORT_SERIAL ? "serial" : "network");

    if (port->type.rig == RIG_PORT_NONE)
    {
        return RIG_OK;
    }

    if (port->type.rig == RIG_PORT_NETWORK
            || port->type.rig == RIG_PORT_UDP_NETWORK)
    {
        network_flush(port);
        return RIG_OK;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: Expected serial port type!!\nWhat is this rig?\n", __func__);
    }

    return serial_flush(port); // we must be on serial port
#else
    return RIG_OK;
#endif
}


static const struct
{
    rot_status_t status;
    const char *str;
} rot_status_str[] =
{
    { ROT_STATUS_BUSY, "BUSY" },
    { ROT_STATUS_MOVING, "MOVING" },
    { ROT_STATUS_MOVING_AZ, "MOVING_AZ" },
    { ROT_STATUS_MOVING_LEFT, "MOVING_LEFT" },
    { ROT_STATUS_MOVING_RIGHT, "MOVING_RIGHT" },
    { ROT_STATUS_MOVING_EL, "MOVING_EL" },
    { ROT_STATUS_MOVING_UP, "MOVING_UP" },
    { ROT_STATUS_MOVING_DOWN, "MOVING_DOWN" },
    { ROT_STATUS_LIMIT_UP, "LIMIT_UP" },
    { ROT_STATUS_LIMIT_DOWN, "LIMIT_DOWN" },
    { ROT_STATUS_LIMIT_LEFT, "LIMIT_LEFT" },
    { ROT_STATUS_LIMIT_RIGHT, "LIMIT_RIGHT" },
    { ROT_STATUS_OVERLAP_UP, "OVERLAP_UP" },
    { ROT_STATUS_OVERLAP_DOWN, "OVERLAP_DOWN" },
    { ROT_STATUS_OVERLAP_LEFT, "OVERLAP_LEFT" },
    { ROT_STATUS_OVERLAP_RIGHT, "OVERLAP_RIGHT" },
    { 0xffffff, "" },
};


/**
 * \brief Convert enum ROT_STATUS_... to a string
 * \param status ROT_STATUS_...
 * \return the corresponding string value
 */
const char *HAMLIB_API rot_strstatus(rot_status_t status)
{
    int i;

    for (i = 0 ; rot_status_str[i].str[0] != '\0'; i++)
    {
        if (status == rot_status_str[i].status)
        {
            return rot_status_str[i].str;
        }
    }

    return "";
}

/**
 * \brief Get pointer to rig function instead of using rig->caps
 * \param RIG* and rig_function_e
 * \return the corresponding function pointer
 */
void *HAMLIB_API rig_get_function_ptr(rig_model_t rig_model,
                                      enum rig_function_e rig_function)
{
    const struct rig_caps *caps = rig_get_caps(rig_model);

    switch (rig_function)
    {
    case RIG_FUNCTION_INIT:
        return caps->rig_init;

    case RIG_FUNCTION_CLEANUP:
        return caps->rig_cleanup;

    case RIG_FUNCTION_OPEN:
        return caps->rig_open;

    case RIG_FUNCTION_CLOSE:
        return caps->rig_close;

    case RIG_FUNCTION_SET_FREQ:
        return caps->set_freq;

    case RIG_FUNCTION_GET_FREQ:
        return caps->get_freq;

    case RIG_FUNCTION_SET_MODE:
        return caps->set_mode;

    case RIG_FUNCTION_GET_MODE:
        return caps->get_mode;

    case RIG_FUNCTION_SET_VFO:
        return caps->set_vfo;

    case RIG_FUNCTION_GET_VFO:
        return caps->get_vfo;

    case RIG_FUNCTION_SET_PTT:
        return caps->set_ptt;

    case RIG_FUNCTION_GET_PTT:
        return caps->get_ptt;

    case RIG_FUNCTION_GET_DCD:
        return caps->get_dcd;

    case RIG_FUNCTION_SET_RPTR_SHIFT:
        return caps->set_rptr_shift;

    case RIG_FUNCTION_GET_RPTR_SHIFT:
        return caps->get_rptr_shift;

    case RIG_FUNCTION_SET_RPTR_OFFS:
        return caps->set_rptr_offs;

    case RIG_FUNCTION_GET_RPTR_OFFS:
        return caps->get_rptr_offs;

    case RIG_FUNCTION_SET_SPLIT_FREQ:
        return caps->set_split_freq;

    case RIG_FUNCTION_GET_SPLIT_FREQ:
        return caps->get_split_freq;

    case RIG_FUNCTION_SET_SPLIT_MODE:
        return caps->set_split_mode;

    case RIG_FUNCTION_SET_SPLIT_FREQ_MODE:
        return caps->set_split_freq_mode;

    case RIG_FUNCTION_GET_SPLIT_FREQ_MODE:
        return caps->get_split_freq_mode;

    case RIG_FUNCTION_SET_SPLIT_VFO:
        return caps->set_split_vfo;

    case RIG_FUNCTION_GET_SPLIT_VFO:
        return caps->get_split_vfo;

    case RIG_FUNCTION_SET_RIT:
        return caps->set_rit;

    case RIG_FUNCTION_GET_RIT:
        return caps->get_rit;

    case RIG_FUNCTION_SET_XIT:
        return caps->set_xit;

    case RIG_FUNCTION_GET_XIT:
        return caps->get_xit;

    case RIG_FUNCTION_SET_TS:
        return caps->set_ts;

    case RIG_FUNCTION_GET_TS:
        return caps->get_ts;

    case RIG_FUNCTION_SET_DCS_CODE:
        return caps->set_dcs_code;

    case RIG_FUNCTION_GET_DCS_CODE:
        return caps->get_dcs_code;

    case RIG_FUNCTION_SET_TONE:
        return caps->set_tone;

    case RIG_FUNCTION_GET_TONE:
        return caps->get_tone;

    case RIG_FUNCTION_SET_CTCSS_TONE:
        return caps->set_ctcss_tone;

    case RIG_FUNCTION_GET_CTCSS_TONE:
        return caps->get_ctcss_tone;

    case RIG_FUNCTION_SET_DCS_SQL:
        return caps->set_dcs_sql;

    case RIG_FUNCTION_GET_DCS_SQL:
        return caps->get_dcs_sql;

    case RIG_FUNCTION_SET_TONE_SQL:
        return caps->set_tone_sql;

    case RIG_FUNCTION_GET_TONE_SQL:
        return caps->get_tone_sql;

    case RIG_FUNCTION_SET_CTCSS_SQL:
        return caps->set_ctcss_sql;

    case RIG_FUNCTION_GET_CTCSS_SQL:
        return caps->get_ctcss_sql;

    case RIG_FUNCTION_POWER2MW:
        return caps->power2mW;

    case RIG_FUNCTION_MW2POWER:
        return caps->mW2power;

    case RIG_FUNCTION_SET_POWERSTAT:
        return caps->set_powerstat;

    case RIG_FUNCTION_GET_POWERSTAT:
        return caps->get_powerstat;

    case RIG_FUNCTION_RESET:
        return caps->reset;

    case RIG_FUNCTION_SET_ANT:
        return caps->set_ant;

    case RIG_FUNCTION_GET_ANT:
        return caps->get_ant;

    case RIG_FUNCTION_SET_LEVEL:
        return caps->set_level;

    case RIG_FUNCTION_GET_LEVEL:
        return caps->get_level;

    case RIG_FUNCTION_SET_FUNC:
        return caps->set_func;

    case RIG_FUNCTION_GET_FUNC:
        return caps->get_func;

    case RIG_FUNCTION_SET_PARM:
        return caps->set_parm;

    case RIG_FUNCTION_GET_PARM:
        return caps->get_parm;

    case RIG_FUNCTION_SET_EXT_LEVEL:
        return caps->set_ext_level;

    case RIG_FUNCTION_GET_EXT_LEVEL:
        return caps->get_ext_level;

    case RIG_FUNCTION_SET_EXT_FUNC:
        return caps->set_ext_func;

    case RIG_FUNCTION_GET_EXT_FUNC:
        return caps->get_ext_func;

    case RIG_FUNCTION_SET_EXT_PARM:
        return caps->set_ext_parm;

    case RIG_FUNCTION_GET_EXT_PARM:
        return caps->get_ext_parm;

    case RIG_FUNCTION_SET_CONF:
        return caps->set_conf;

    case RIG_FUNCTION_GET_CONF:
        return caps->get_conf;

    case RIG_FUNCTION_GET_CONF2:
        return caps->get_conf2;

    case RIG_FUNCTION_SEND_DTMF:
        return caps->send_dtmf;

    case RIG_FUNCTION_SEND_MORSE:
        return caps->send_morse;

    case RIG_FUNCTION_STOP_MORSE:
        return caps->stop_morse;

    case RIG_FUNCTION_WAIT_MORSE:
        return caps->wait_morse;

    case RIG_FUNCTION_SEND_VOICE_MEM:
        return caps->send_voice_mem;

    case RIG_FUNCTION_SET_BANK:
        return caps->set_bank;

    case RIG_FUNCTION_SET_MEM:
        return caps->set_mem;

    case RIG_FUNCTION_GET_MEM:
        return caps->get_mem;

    case RIG_FUNCTION_VFO_OP:
        return caps->vfo_op;

    case RIG_FUNCTION_SCAN:
        return caps->scan;

    case RIG_FUNCTION_SET_TRN:
        return caps->set_trn;

    case RIG_FUNCTION_GET_TRN:
        return caps->get_trn;

    case RIG_FUNCTION_DECODE_EVENT:
        return caps->decode_event;

    case RIG_FUNCTION_SET_CHANNEL:
        return caps->set_channel;

    case RIG_FUNCTION_GET_CHANNEL:
        return caps->get_channel;

    case RIG_FUNCTION_GET_INFO:
        return caps->get_info;

    case RIG_FUNCTION_SET_CHAN_ALL_CB:
        return caps->set_chan_all_cb;

    case RIG_FUNCTION_GET_CHAN_ALL_CB:
        return caps->get_chan_all_cb;

    case RIG_FUNCTION_SET_MEM_ALL_CB:
        return caps->set_mem_all_cb;

    case RIG_FUNCTION_GET_MEM_ALL_CB:
        return caps->get_mem_all_cb;

    case RIG_FUNCTION_SET_VFO_OPT:
        return caps->set_vfo_opt;

    case RIG_FUNCTION_READ_FRAME_DIRECT:
        return caps->read_frame_direct;

    case RIG_FUNCTION_IS_ASYNC_FRAME:
        return caps->is_async_frame;

    case RIG_FUNCTION_PROCESS_ASYNC_FRAME:
        return caps->process_async_frame;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unknown function?? function=%d\n", rig_function);
    }

    return NULL;
}

// negative return indicates error
/**
 * \brief Get integer/long instead of using rig->caps
 *  watch out for integer values that may be negative -- if needed must change hamlib
 * \param RIG* and rig_caps_int_e
 * \return the corresponding long value -- -RIG_EINVAL is the only error possible
 */
long long HAMLIB_API rig_get_caps_int(rig_model_t rig_model,
                                      enum rig_caps_int_e rig_caps)
{
    const struct rig_caps *caps = rig_get_caps(rig_model);
    //rig_debug(RIG_DEBUG_TRACE, "%s: getting rig_caps=%u\n", __func__, rig_caps);

    switch (rig_caps)
    {
    case RIG_CAPS_TARGETABLE_VFO:
        return caps->targetable_vfo;

    case RIG_CAPS_RIG_MODEL:
        return caps->rig_model;

    case RIG_CAPS_PTT_TYPE:
        //rig_debug(RIG_DEBUG_TRACE, "%s: return %u\n", __func__, caps->ptt_type);
        return caps->ptt_type;

    case RIG_CAPS_PORT_TYPE:
        return caps->port_type;

    case RIG_CAPS_HAS_GET_LEVEL:
        return caps->has_get_level;

    default:
        //rig_debug(RIG_DEBUG_ERR, "%s: Unknown rig_caps value=%lld\n", __func__, rig_caps);
        return (-RIG_EINVAL);
    }
}

const char *HAMLIB_API rig_get_caps_cptr(rig_model_t rig_model,
        enum rig_caps_cptr_e rig_caps)
{
    const struct rig_caps *caps = rig_get_caps(rig_model);

    switch (rig_caps)
    {
    case RIG_CAPS_VERSION_CPTR:
        return caps->version;

    case RIG_CAPS_MFG_NAME_CPTR:
        return caps->mfg_name;

    case RIG_CAPS_MODEL_NAME_CPTR:
        return caps->model_name;

    case RIG_CAPS_STATUS_CPTR:
        return rig_strstatus(caps->status);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown requested rig_caps value=%d\n", __func__,
                  rig_caps);
        return "Unknown caps value";
    }
}

void errmsg(int err, char *s, const char *func, const char *file, int line)
{
    rig_debug(RIG_DEBUG_ERR, "%s(%s:%d): %s: %s\b", __func__, file, line, s,
              rigerror(err));
}

uint32_t CRC32_function(uint8_t *buf, uint32_t len)
{

    uint32_t crc;
    uint8_t i;

    crc = 0xFFFFFFFF;

    while (len--)
    {
        uint32_t val;
        val = (crc^*buf++) & 0xFF;

        for (i = 0; i < 8; i++)
        {
            val = val & 1 ? (val >> 1) ^ 0xEDB88320 : val >> 1;
        }

        crc = val ^ crc >> 8;
    }

    return crc ^ 0xFFFFFFFF;
}

#if defined(_WIN32)
// gmtime_r can be defined by mingw
#ifndef gmtime_r
static struct tm *gmtime_r(const time_t *t, struct tm *r)
{
    // gmtime is threadsafe in windows because it uses TLS
    struct tm *theTm = gmtime(t);

    if (theTm)
    {
        *r = *theTm;
        return r;
    }
    else
    {
        return 0;
    }
}
#endif // gmtime_r
#endif // _WIN32

//! @cond Doxygen_Suppress
char *date_strget(char *buf, int buflen, int localtime)
{
    char tmpbuf[64];
    struct tm *mytm;
    time_t t;
    struct timeval tv;
    struct tm result;
    int mytimezone;

    t = time(NULL);

    if (localtime)
    {
        mytm = localtime_r(&t, &result);
        mytimezone = timezone;
    }
    else
    {
        mytm = gmtime_r(&t, &result);
        mytimezone = 0;
    }

    strftime(buf, buflen, "%Y-%m-%dT%H:%M:%S.", mytm);
    gettimeofday(&tv, NULL);
    SNPRINTF(tmpbuf, sizeof(tmpbuf), "%06ld", (long)tv.tv_usec);
    strcat(buf, tmpbuf);
    SNPRINTF(tmpbuf, sizeof(tmpbuf), "%s%04d", mytimezone >= 0 ? "-" : "+",
             ((int)abs(mytimezone) / 3600) * 100);
    strcat(buf, tmpbuf);
    return buf;
}

const char *spaces()
{
    static char *s = "                     ";
    return s;
}



//! @endcond

/** @} */
