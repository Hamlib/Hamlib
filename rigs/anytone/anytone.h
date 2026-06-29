// ---------------------------------------------------------------------------
//    AnyTone D578 Hamlib Backend
// ---------------------------------------------------------------------------
//
//  anytone.h
//
//  Created by Michael Black W9MDB
//  Copyright © 2023 Michael Black W9MDB.
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

#ifndef _ANYTONE_H
#define _ANYTONE_H 1

#include "hamlib/rig.h"
#include "token.h"

#define BACKEND_VER "20260629"

#define ANYTONE_RESPSZ 64

#define TOK_COMMODE TOKEN_BACKEND(1)

extern struct rig_caps anytone_d578_caps;
extern const struct confparams anytone_cfg_params[];

#include <pthread.h>
#define MUTEX(var) static pthread_mutex_t var = PTHREAD_MUTEX_INITIALIZER
#define MUTEX_LOCK(var) pthread_mutex_lock(var)
#define MUTEX_UNLOCK(var)  pthread_mutex_unlock(var)

// Button key codes from the direct serial / BT-01 mic protocol
#define ANYTONE_KEY_0     0x01
#define ANYTONE_KEY_1     0x02
#define ANYTONE_KEY_2     0x03
#define ANYTONE_KEY_3     0x04
#define ANYTONE_KEY_4     0x05
#define ANYTONE_KEY_5     0x06
#define ANYTONE_KEY_6     0x07
#define ANYTONE_KEY_7     0x08
#define ANYTONE_KEY_8     0x09
#define ANYTONE_KEY_9     0x0A
#define ANYTONE_KEY_STAR  0x0B
#define ANYTONE_KEY_HASH  0x0C
#define ANYTONE_KEY_SUBAB 0x0D
#define ANYTONE_KEY_UP    0x10
#define ANYTONE_KEY_DOWN  0x11

typedef struct _anytone_priv_data
{
    ptt_t         ptt;
    vfo_t         vfo_curr;
    volatile int  runflag;
    int           commode;
    char          buf[64];
    pthread_mutex_t mutex;
    pthread_t     thread_id;
} anytone_priv_data_t,
* anytone_priv_data_ptr;


extern int anytone_init(RIG *rig);
extern int anytone_cleanup(RIG *rig);
extern int anytone_open(RIG *rig);
extern int anytone_close(RIG *rig);

extern int anytone_set_conf(RIG *rig, hamlib_token_t token, const char *val);
extern int anytone_get_conf(RIG *rig, hamlib_token_t token, char *val);

extern int anytone_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
extern int anytone_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);

extern int anytone_set_vfo(RIG *rig, vfo_t vfo);
extern int anytone_get_vfo(RIG *rig, vfo_t *vfo);

extern int anytone_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
extern int anytone_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

extern int anytone_set_clock(RIG *rig, int year, int month, int day,
                             int hour, int min, int sec, double msec,
                             int utc_offset);
extern int anytone_get_clock(RIG *rig, int *year, int *month, int *day,
                             int *hour, int *min, int *sec, double *msec,
                             int *utc_offset);

#endif /* _ANYTONE_H */
