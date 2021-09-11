/*
 *  Hamlib Racal backend - RA37XX header
 *  Copyright (c) 2004-2010 by Stephane Fillod
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

#ifndef _RA37XX_H
#define _RA37XX_H 1

#include "hamlib/rig.h"
#include "racal.h"


#undef BACKEND_VER
#define BACKEND_VER "20210911"

extern const struct confparams ra37xx_cfg_params[];

/* Packet timeout at Master port, 5-9 */
#define RA37XX_TIMEOUT 1000 /* ms */


#define RA37XX_STR_CAL { 13, \
        { \
                    {  97,  86 }, /* 120 dBuV */ \
                    { 109,  76 }, /* 110 dBuV */ \
                    { 120,  66 }, /* 100 dBuV */ \
                    { 132,  56 }, /*  90 dBuV */ \
                    { 144,  46 }, /*  80 dBuV */ \
                    { 155,  36 }, /*  70 dBuV */ \
                    { 167,  26 }, /*  60 dBuV */ \
                    { 179,  16 }, /*  50 dBuV */ \
                    { 190,   6 }, /*  40 dBuV */ \
                    { 202,  -4 }, /*  30 dBuV */ \
                    { 214, -14 }, /*  20 dBuV */ \
                    { 225, -24 }, /*  10 dBuV */ \
                    { 255, -34 }, /*   0 dBuV */ \
                } }

#define RA37XX_MEM_CAP {    \
        .freq = 1,  \
        .mode = 1,  \
        .width = 1, \
        .levels = RIG_LEVEL_AGC|RIG_LEVEL_CWPITCH, \
        /* .flags = 1, */ \
}


struct ra37xx_priv_data {
	int receiver_id;
};

int ra37xx_set_conf(RIG *rig, token_t token, const char *val);
int ra37xx_get_conf(RIG *rig, token_t token, char *val);
int ra37xx_init(RIG *rig);
int ra37xx_cleanup(RIG *rig);
int ra37xx_open(RIG *rig);
int ra37xx_close(RIG *rig);

int ra37xx_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ra37xx_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int ra37xx_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int ra37xx_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int ra37xx_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int ra37xx_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int ra37xx_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int ra37xx_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
const char* ra37xx_get_info(RIG *rig);
int ra37xx_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);
int ra37xx_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option, ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);
int ra37xx_set_mem(RIG *rig, vfo_t vfo, int ch);
int ra37xx_get_mem(RIG *rig, vfo_t vfo, int *ch);
int ra37xx_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
int ra37xx_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);


#endif	/* _RA37XX_H */
