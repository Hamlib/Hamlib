/*
 *  Hamlib ICOM Marine backend - main header
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

#ifndef _ICMARINE_H
#define _ICMARINE_H 1

#include "hamlib/rig.h"
#include "cal.h"
#include "tones.h"

#define BACKEND_VER "20181007"

struct icmarine_priv_caps {
    unsigned char default_remote_id;  /* the remote default equipment's ID */
};

struct icmarine_priv_data {
    unsigned char remote_id;  /* the remote equipment's ID */
    split_t split; /* current split mode */
};

extern const struct confparams icmarine_cfg_params[];

int icmarine_transaction(RIG *rig, const char *cmd, const char *param, char *response);
int icmarine_init(RIG *rig);
int icmarine_cleanup(RIG *rig);
int icmarine_open(RIG *rig);
int icmarine_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int icmarine_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int icmarine_set_tx_freq(RIG *rig, vfo_t vfo, freq_t freq);
int icmarine_get_tx_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int icmarine_set_split_vfo(RIG *rig, vfo_t rx_vfo, split_t split, vfo_t tx_vfo);
int icmarine_get_split_vfo(RIG *rig, vfo_t rx_vfo, split_t *split, vfo_t *tx_vfo);
int icmarine_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int icmarine_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int icmarine_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int icmarine_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int icmarine_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int icmarine_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int icmarine_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int icmarine_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int icmarine_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int icmarine_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int icmarine_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int icmarine_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int icmarine_set_parm(RIG *rig, setting_t parm, value_t val);
int icmarine_get_parm(RIG *rig, setting_t parm, value_t *val);
int icmarine_set_conf(RIG *rig, token_t token, const char *val);
int icmarine_get_conf(RIG *rig, token_t token, char *val);
int icmarine_get_conf2(RIG *rig, token_t token, char *val, int val_len);

extern const struct rig_caps icm700pro_caps;
extern const struct rig_caps icm710_caps;
extern const struct rig_caps icm802_caps;
extern const struct rig_caps icm803_caps;

#endif /* _ICMARINE_H */
