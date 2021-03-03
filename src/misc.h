/*
 *  Hamlib Interface - toolbox header
 *  Copyright (c) 2000-2004 by Stephane Fillod
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

#ifndef _MISC_H
#define _MISC_H 1

#include <hamlib/rig.h>


/*
 * Careful!! These macros are NOT reentrant!
 * ie. they may not be executed atomically,
 * thus not ensure mutual exclusion.
 * Fix it when making Hamlib reentrant!  --SF
 */
#define Hold_Decode(rig) {(rig)->state.hold_decode = 1;}
#define Unhold_Decode(rig) {(rig)->state.hold_decode = 0;}

__BEGIN_DECLS

/*
 * Do a hex dump of the unsigned char array.
 */

void dump_hex(const unsigned char ptr[], size_t size);

/*
 * BCD conversion routines.
 *
 * to_bcd() converts a long long int to a little endian BCD array,
 *  and return a pointer to this array.
 *
 * from_bcd() converts a little endian BCD array to long long int
 *  representation, and return it.
 *
 * bcd_len is the number of digits in the BCD array.
 */
extern HAMLIB_EXPORT(unsigned char *) to_bcd(unsigned char bcd_data[],
                                             unsigned long long freq,
                                             unsigned bcd_len);

extern HAMLIB_EXPORT(unsigned long long) from_bcd(const unsigned char
                                                  bcd_data[],
                                                  unsigned bcd_len);

/*
 * same as to_bcd() and from_bcd(), but in Big Endian mode
 */
extern HAMLIB_EXPORT(unsigned char *) to_bcd_be(unsigned char bcd_data[],
                                                unsigned long long freq,
                                                unsigned bcd_len);

extern HAMLIB_EXPORT(unsigned long long) from_bcd_be(const unsigned char
                                                     bcd_data[],
                                                     unsigned bcd_len);

extern HAMLIB_EXPORT(double) morse_code_dot_to_millis(int wpm);
extern HAMLIB_EXPORT(int) dot10ths_to_millis(int dot10ths, int wpm);
extern HAMLIB_EXPORT(int) millis_to_dot10ths(int millis, int wpm);

extern HAMLIB_EXPORT(int) sprintf_freq(char *str, int len, freq_t);

/* flag that determines if AI mode should be restored on exit on
   applicable rigs - See rig_no_restore_ai() */
extern int no_restore_ai;

/* check if it's any of CR or LF */
#define isreturn(c) ((c) == 10 || (c) == 13)


/* needs config.h included beforehand in .c file */
#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

extern HAMLIB_EXPORT(int) rig_check_cache_timeout(const struct timeval *tv,
                                                  int timeout);

extern HAMLIB_EXPORT(void) rig_force_cache_timeout(struct timeval *tv);

extern HAMLIB_EXPORT(setting_t) rig_idx2setting(int i);

extern HAMLIB_EXPORT(int) hl_usleep(rig_useconds_t usec);

extern HAMLIB_EXPORT(double) elapsed_ms(struct timespec *start, int start_flag);

extern HAMLIB_EXPORT(vfo_t) vfo_fixup(RIG *rig, vfo_t vfo);

extern HAMLIB_EXPORT(int) parse_hoststr(char *host, char hoststr[256], char port[6]);

#ifdef PRId64
/** \brief printf(3) format to be used for long long (64bits) type */
#  define PRIll PRId64
#  define PRXll PRIx64
#else
#  ifdef FBSD4
#    define PRIll "qd"
#    define PRXll "qx"
#  else
#    define PRIll "lld"
#    define PRXll "lld"
#  endif
#endif

#ifdef SCNd64
/** \brief scanf(3) format to be used for long long (64bits) type */
#  define SCNll SCNd64
#  define SCNXll SCNx64
#else
#  ifdef FBSD4
#    define SCNll "qd"
#    define SCNXll "qx"
#  else
#    define SCNll "lld"
#    define SCNXll "llx"
#  endif
#endif

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
void errmsg(int err, char *s, const char *func, const char *file, int line);
#define ERRMSG(err, s) errmsg(err,  s, __func__, __FILENAME__, __LINE__)
#define ENTERFUNC rig_debug(RIG_DEBUG_VERBOSE, "%s(%d):%s entered\n", __FILENAME__, __LINE__, __func__)
#define RETURNFUNC(rc) do { \
                        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d):%s return\n", __FILENAME__, __LINE__, __func__); \
                        return rc; \
                       } while(0)

#if 0 // 5.0
    elapsed_ms(&rig->state.cache.time_freqMainC, HAMLIB_ELAPSED_INVALIDATE);
#endif
#define CACHE_RESET {\
    elapsed_ms(&rig->state.cache.time_freq, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_freqCurr, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_freqMainA, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_freqMainB, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_freqSubA, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_freqSubB, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_vfo, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_mode, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_INVALIDATE);\
    elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_INVALIDATE);\
     }

__END_DECLS

#endif /* _MISC_H */
