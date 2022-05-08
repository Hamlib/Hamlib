/*
 *  Hamlib PRM80 backend - main header
 *  Copyright (c) 2010,2021 by Stephane Fillod
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

#ifndef _PRM80_H
#define _PRM80_H 1

#include <sys/time.h>
#include <hamlib/rig.h>
#include "misc.h"

#define BACKEND_VER "20210416"

#define PRM80_MEM_CAP {    \
        .freq = 1,  \
        .rptr_shift = 1, \
        .flags = 1, /* lockout */ \
        }

struct prm80_priv_data
{
    freq_t rx_freq;  /* last RX freq set */
    freq_t tx_freq;  /* last TX freq set */
    split_t split;   /* emulated split on/off */
    struct timeval status_tv; /* date of last "E" command */
    char cached_statebuf[32]; /* response of last "E" command */
};

int prm80_init(RIG *rig);
int prm80_cleanup(RIG *rig);
int prm80_reset(RIG *rig, reset_t reset);
int prm80_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int prm80_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int prm80_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
int prm80_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
int prm80_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int prm80_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int prm80_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int prm80_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int prm80_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int prm80_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int prm80_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int prm80_set_mem(RIG *rig, vfo_t vfo, int ch);
int prm80_get_mem(RIG *rig, vfo_t vfo, int *ch);
int prm80_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);
int prm80_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);
int prm80_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int prm80_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);

const char *prm80_get_info(RIG *rig);

extern const struct rig_caps prm8060_caps;

#endif /* _PRM80_H */
