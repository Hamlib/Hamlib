/*
 *  Hamlib Interface - debug
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

/**
 * \addtogroup rig
 * @{
 */

/**
 * \file debug.c
 *
 * \brief Control Hamlib debugging functions.
 */

#include <hamlib/config.h>

#include <stdarg.h>
#include <stdio.h>  /* Standard input/output definitions */
#include <string.h> /* String function definitions */
#include <fcntl.h>  /* File control definitions */
#include <errno.h>  /* Error number definitions */
#include <sys/types.h>

#ifdef ANDROID
#  include <android/log.h>
#endif

#include <hamlib/rig.h>
#include <hamlib/rig_dll.h>
#include "misc.h"

/*! @} */


/**
 * \addtogroup rig_internal
 * @{
 */


/** \brief Sets the number of hexadecimal pairs to print per line. */
#define DUMP_HEX_WIDTH 16


static int rig_debug_level = RIG_DEBUG_TRACE;
static int rig_debug_time_stamp = 0;
FILE *rig_debug_stream;
static vprintf_cb_t rig_vprintf_cb;
static rig_ptr_t rig_vprintf_arg;

extern HAMLIB_EXPORT(void) dump_hex(const unsigned char ptr[], size_t size);

/**
 * \brief Do a hex dump of the unsigned char array.
 *
 * \param ptr Pointer to a character array.
 * \param size Number of chars to words to dump.
 *
 * Prints the hex dump to `stderr` via rig_debug():
 *
 * ```
 * 0000  4b 30 30 31 34 35 30 30 30 30 30 30 30 35 30 32  K001450000000502
 * 0010  30 30 0d 0a                                      00..
 * ```
 */
void dump_hex(const unsigned char ptr[], size_t size)
{
    /* example
     * 0000  4b 30 30 31 34 35 30 30 30 30 30 30 30 35 30 32  K001450000000502
     * 0010  30 30 0d 0a                                      00..
     */
    char line[4 + 4 + 3 * DUMP_HEX_WIDTH + 4 + DUMP_HEX_WIDTH + 1];
    int i;

    if (!rig_need_debug(RIG_DEBUG_TRACE))
    {
        return;
    }

    line[sizeof(line) - 1] = '\0';

    for (i = 0; i < size; ++i)
    {
        unsigned char c;

        if (i % DUMP_HEX_WIDTH == 0)
        {
            /* new line */
            SNPRINTF(line + 0, sizeof(line), "%04x", i);
            memset(line + 4, ' ', sizeof(line) - 4 - 1);
        }

        c = ptr[i];

        /* hex print */
        sprintf(line + 8 + 3 * (i % DUMP_HEX_WIDTH), "%02x", c);
        line[8 + 3 * (i % DUMP_HEX_WIDTH) + 2] = ' '; /* no \0 */

        /* ascii print */
        line[8 + 3 * DUMP_HEX_WIDTH + 4 + (i % DUMP_HEX_WIDTH)] = (c >= ' '
                && c < 0x7f) ? c : '.';

        /* actually print the line */
        if (i + 1 == size || (i && i % DUMP_HEX_WIDTH == DUMP_HEX_WIDTH - 1))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s\n", line);
        }
    }
}

/*! @} */


/**
 * \addtogroup rig
 * @{
 */

/**
 * \brief Change the current debug level.
 *
 * \param debug_level Equivalent to the `-v` option of the utilities.
 *
 * Allows for dynamically changing the debugging output without reinitializing
 * the library.
 *
 * Useful for programs that want to enable and disable debugging
 * output without restarting.
 */
void HAMLIB_API rig_set_debug(enum rig_debug_level_e debug_level)
{
    rig_debug_level = debug_level;
}


/**
 * \brief Test if a given debug level is active.
 *
 * \param debug_level The level to test.
 *
 * May be used to determine if an action such as opening a dialog should
 * happen only if a desired debug level is active.
 *
 * Also useful for dump_hex(), etc.
 */
int HAMLIB_API rig_need_debug(enum rig_debug_level_e debug_level)
{
    return (debug_level <= rig_debug_level);
}


/**
 * \brief Enable or disable the time stamp on debugging output.
 *
 * \param flag `TRUE` or `FALSE`.
 *
 * Sets or unsets the flag which controls whether debugging output includes a
 * time stamp.
 */
void HAMLIB_API rig_set_debug_time_stamp(int flag)
{
    rig_debug_time_stamp = flag;
}

//! @endcond


/**
 * \brief Print debugging messages through `stderr` by default.
 *
 * \param debug_level Debug level from none to most output.
 * \param fmt Formatted character string to print.
 *
 * The formatted character string is passed to the `frprintf`(3) C library
 * call and follows its format specification.
 */
#undef rig_debug
void HAMLIB_API rig_debug(enum rig_debug_level_e debug_level,
                          const char *fmt, ...)
{
    static pthread_mutex_t client_debug_lock = PTHREAD_MUTEX_INITIALIZER;
    va_list ap;

    if (!rig_need_debug(debug_level))
    {
        return;
    }

    pthread_mutex_lock(&client_debug_lock);
    va_start(ap, fmt);

    if (rig_vprintf_cb)
    {
        rig_vprintf_cb(debug_level, rig_vprintf_arg, fmt, ap);
    }
    else
    {
        if (!rig_debug_stream)
        {
            rig_debug_stream = stderr;
        }

        if (rig_debug_time_stamp)
        {
            char buf[256];
            fprintf(rig_debug_stream, "%s: ", date_strget(buf, sizeof(buf), 1));
        }

        vfprintf(rig_debug_stream, fmt, ap);
        fflush(rig_debug_stream);
    }

    va_end(ap);
#ifdef ANDROID
    int a;
    va_start(ap, fmt);

    switch (debug_level)
    {
//        case RIG_DEBUG_NONE:
    case RIG_DEBUG_BUG:
        a = ANDROID_LOG_FATAL;
        break;

    case RIG_DEBUG_ERR:
        a = ANDROID_LOG_ERROR;
        break;

    case RIG_DEBUG_WARN:
        a = ANDROID_LOG_WARN;
        break;

    case RIG_DEBUG_VERBOSE:
        a = ANDROID_LOG_VERBOSE;
        break;

    case RIG_DEBUG_TRACE:
        a = ANDROID_LOG_VERBOSE;
        break;

    default:
        a = ANDROID_LOG_DEBUG;
        break;
    }

    __android_log_vprint(a, PACKAGE_NAME, fmt, ap);

    va_end(ap);
#endif
    pthread_mutex_unlock(&client_debug_lock);
}


/**
 * \brief Set callback to handle debugging messages.
 *
 * \param cb The callback function to install.
 * \param arg A Pointer to some private data to pass later on to the callback.
 *
 *  Install a callback for rig_debug() messages.
 * \code
 * int
 * rig_message_cb(enum rig_debug_level_e debug_level,
 *                rig_ptr_t user_data,
 *                const char *fmt,
 *                va_list ap)
 * {
 *     char buf[1024];
 *
 *     sprintf (buf, "Message(%s) ", (char*)user_data);
 *     syslog (LOG_USER, buf);
 *     vsprintf (buf, fmt, ap);
 *     syslog (LOG_USER, buf);
 *
 *     return RIG_OK;
 * }
 *
 * . . .
 *
 * char *cookie = "Foo";
 * rig_set_debug_callback (rig_message_cb, (rig_ptr_t)cookie);
 * \endcode
 *
 * \return A pointer to the previous callback that was set, if any.
 *
 * \sa rig_debug()
 */
vprintf_cb_t HAMLIB_API rig_set_debug_callback(vprintf_cb_t cb, rig_ptr_t arg)
{
    vprintf_cb_t prev_cb = rig_vprintf_cb;

    rig_vprintf_cb = cb;
    rig_vprintf_arg = arg;

    return prev_cb;
}


/**
 * \brief Change the output stream from `stderr` a different stream.
 *
 * \param stream The stream to direct debugging output.
 *
 * \sa `FILE`(3)
 */
FILE *HAMLIB_API rig_set_debug_file(FILE *stream)
{
    FILE *prev_stream = rig_debug_stream;

    rig_debug_stream = stream;

    return prev_stream;
}

/** @} */
