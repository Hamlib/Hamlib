#ifndef _ANYTONE_H
#define _ANYTONE_H 1

#include "hamlib/rig.h"

#define BACKEND_VER "20230530"

#define ANYTONE_RESPSZ 64

extern const struct rig_caps anytone_d578_caps;

#ifdef PTHREAD
#include <pthread.h>
#define MUTEX(var) static pthread_mutex_t var = PTHREAD_MUTEX_INITIALIZER
#define MUTEX_LOCK(var) pthread_mutex_lock(var)
#define MUTEX_UNLOCK(var)  pthread_mutex_unlock(var)
#else
#define MUTEX(var)
#define MUTEX_LOCK(var)
#define MUTEX_UNLOCK(var)
#endif

typedef struct _anytone_priv_data
{
    ptt_t         ptt;
    vfo_t         vfo_curr;
    int           runflag; // thread control
    char          buf[64];
    pthread_mutex_t mutex;
} anytone_priv_data_t,
* anytone_priv_data_ptr;


extern int anytone_init(RIG *rig);
extern int anytone_cleanup(RIG *rig);
extern int anytone_open(RIG *rig);
extern int anytone_close(RIG *rig);

extern int anytone_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
extern int anytone_get_ptt(RIG *rig, vfo_t vfo,ptt_t *ptt);

extern int anytone_set_vfo(RIG *rig, vfo_t vfo);
extern int anytone_get_vfo(RIG *rig, vfo_t *vfo);

extern int anytone_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
extern int anytone_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

#endif /* _ANYTONE_H */
