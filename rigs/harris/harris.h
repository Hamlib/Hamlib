#ifndef _HARRIS_H
#define _HARRIS_H

#include <hamlib/rig.h>

struct harris_priv_data {
    freq_t current_freq;
    rmode_t current_mode;
};

/* Prototypes for prc138.c */
int harris_init(RIG *rig);
int harris_cleanup(RIG *rig);
int harris_open(RIG *rig);
int harris_close(RIG *rig);
int harris_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int harris_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int harris_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int harris_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int harris_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int harris_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int harris_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int harris_get_bw(RIG *rig, vfo_t vfo, mode_t mode, pbwidth_t *width);
int harris_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int harris_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);

#endif
