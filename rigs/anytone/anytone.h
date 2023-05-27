#ifndef _ANYTONE_H
#define _ANYTONE_H 1

#include "hamlib/rig.h"

#define BACKEND_VER "20230527"

#define ANYTONE_RESPSZ 64

extern const struct rig_caps anytone_d578_caps;

typedef struct _anytone_priv_data
{
    int           ptt;
    vfo_t         vfo_curr;
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

#endif /* _ANYTONE_H */
