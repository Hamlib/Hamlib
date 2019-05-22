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
 * \addtogroup rig_internal
 * @{
 */

/**
 * \file debug.c
 * \brief control hamlib debugging functions
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>  /* Standard input/output definitions */
#include <string.h> /* String function definitions */
#include <unistd.h> /* UNIX standard function definitions */
#include <fcntl.h>  /* File control definitions */
#include <errno.h>  /* Error number definitions */
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#ifdef ANDROID
#  include <android/log.h>
#endif

#include <hamlib/rig.h>
#include "misc.h"

#define DUMP_HEX_WIDTH 16


static int rig_debug_level = RIG_DEBUG_TRACE;
static int rig_debug_time_stamp = 0;
static FILE *rig_debug_stream;
static vprintf_cb_t rig_vprintf_cb;
static rig_ptr_t rig_vprintf_arg;


/**
 * \param ptr Pointer to memory area
 * \param size Number of chars to words to dump
 * \brief Do a hex dump of the unsigned char array.
 */
void dump_hex(const unsigned char ptr[], size_t size)
{
  /* example
   * 0000  4b 30 30 31 34 35 30 30 30 30 30 30 30 35 30 32  K001450000000502
   * 0010  30 30 0d 0a                                      00..
   */
  char line[4 + 4 + 3 * DUMP_HEX_WIDTH + 4 + DUMP_HEX_WIDTH + 1];
  unsigned char c;
  int i;

  if (!rig_need_debug(RIG_DEBUG_TRACE))
  {
    return;
  }

  line[sizeof(line) - 1] = '\0';

  for (i = 0; i < size; ++i)
  {
    if (i % DUMP_HEX_WIDTH == 0)
    {
      /* new line */
      sprintf(line + 0, "%04x", i);
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


/**
 * \param debug_level
 * \brief Change the current debug level
 */
void HAMLIB_API rig_set_debug(enum rig_debug_level_e debug_level)
{
  rig_debug_level = debug_level;
}


/**
 * \param debug_level
 * \brief Useful for dump_hex, etc.
 */
int HAMLIB_API rig_need_debug(enum rig_debug_level_e debug_level)
{
  return (debug_level <= rig_debug_level);
}

/**
 * \param debug_time_stamp
 * \brief Enbable/disable time stamp on debug output
 */
void HAMLIB_API rig_set_debug_time_stamp(int flag)
{
  rig_debug_time_stamp = flag;
}


char *date_strget(char *buf, int buflen)
{
  time_t mytime;
  struct tm *mytm;
  struct timeval tv;
  mytime = time(NULL);
  mytm = gmtime(&mytime);
  gettimeofday(&tv, NULL);
  strftime(buf, buflen, "%Y-%m-%d:%H:%M:%S.", mytm);
  char tmp[16];
  sprintf(tmp, "%06ld", (long)tv.tv_usec);
  strcat(buf, tmp);
  return buf;
}

/**
 * \param debug_level
 * \param fmt
 * \brief Default is debugging messages are done through stderr
 */
void HAMLIB_API rig_debug(enum rig_debug_level_e debug_level,
                          const char *fmt, ...)
{
  va_list ap;

  if (!rig_need_debug(debug_level))
  {
    return;
  }


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
      fprintf(rig_debug_stream, "%s: ", date_strget(buf, sizeof(buf)));
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
}


/**
 * \brief set callback to handle debug messages
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for \a rig_debug messages.
\code
int
rig_message_cb(enum rig_debug_level_e debug_level,
               rig_ptr_t user_data,
               const char *fmt,
               va_list ap)
{
    char buf[1024];

    sprintf (buf, "Message(%s) ", (char*)user_data);
    syslog (LOG_USER, buf);
    vsprintf (buf, fmt, ap);
    syslog (LOG_USER, buf);

    return RIG_OK;
}

    . . .

    char *cookie = "Foo";
    rig_set_debug_callback (rig_message_cb, (rig_ptr_t)cookie);
\endcode
 *
 * \return RIG_OK if the operation has been sucessful, otherwise
 * a negative value if an error occured (in which case, cause
 * is set appropriately).
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
 * \brief change stderr to some different output
 * \param stream The stream to set output to
 */
FILE *HAMLIB_API rig_set_debug_file(FILE *stream)
{
  FILE *prev_stream = rig_debug_stream;

  rig_debug_stream = stream;

  return prev_stream;
}

/** @} */
