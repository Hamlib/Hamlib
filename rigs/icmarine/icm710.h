/*
 *  Hamlib ICOM M710 backend - main header
 *  Copyright (c) 2014-2015 by Stephane Fillod
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

#ifndef _ICM710_H
#define _ICM710_H 1

#include "hamlib/rig.h"
#include "cal.h"
#include "tones.h"

struct icm710_priv_caps {
    unsigned char default_remote_id;  /* the remote default equipment's ID */
};

/* The M710 does not support queries */
/* So we keep a copy of settings in priv to support get functions */
/* The priv settings only reflect what has been previously set */
/* So get's will return 0 until the value has been set */
struct icm710_priv_data {
    unsigned char remote_id;  /* the remote equipment's ID */
    split_t split; /* current split mode */
    freq_t rxfreq, txfreq;
    mode_t mode;
    ptt_t ptt;
    unsigned afgain;
    unsigned rfgain;
    unsigned rfpwr;
    unsigned agc;
};

extern const struct confparams icm710_cfg_params[];

int icm710_init(RIG *rig);
int icm710_cleanup(RIG *rig);
int icm710_open(RIG *rig);
int icm710_close(RIG *rig);
int icm710_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int icm710_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int icm710_set_tx_freq(RIG *rig, vfo_t vfo, freq_t freq);
int icm710_get_tx_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int icm710_set_split_vfo(RIG *rig, vfo_t rx_vfo, split_t split, vfo_t tx_vfo);
int icm710_get_split_vfo(RIG *rig, vfo_t rx_vfo, split_t *split, vfo_t *tx_vfo);
int icm710_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int icm710_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int icm710_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int icm710_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int icm710_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int icm710_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int icm710_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int icm710_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int icm710_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int icm710_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int icm710_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int icm710_set_parm(RIG *rig, setting_t parm, value_t val);
int icm710_get_parm(RIG *rig, setting_t parm, value_t *val);
int icm710_set_conf(RIG *rig, token_t token, const char *val);
int icm710_get_conf(RIG *rig, token_t token, char *val);
int icm710_get_conf2(RIG *rig, token_t token, char *val, int val_len);

extern const struct rig_caps icm700pro_caps;
extern const struct rig_caps icm710_caps;
extern const struct rig_caps icm802_caps;

#endif /* _ICM710_H */
