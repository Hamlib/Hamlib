#ifndef _PRC138_H
#define _PRC138_H

#include <hamlib/rig.h>

#define PRC138_MODES (RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_CW | RIG_MODE_AM)
#define PRC138_VFO   (RIG_VFO_A)

extern struct rig_caps prc138_caps;

#endif
