/*
 *  Hamlib GNUradio backend - main header
 *  Copyright (c) 2001-2003 by Stephane Fillod
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

#ifndef _GNURADIO_H
#define _GNURADIO_H 1

#include <hamlib/rig.h>

__BEGIN_DECLS

struct gnuradio_priv_caps {
	rig_model_t tuner_model;
	shortfreq_t input_rate;
	shortfreq_t IF_center_freq;
};


int gr_init(RIG *rig);
int gr_cleanup(RIG *rig);
int gr_open(RIG *rig);
int gr_close(RIG *rig);
int gr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int gr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int gr_set_vfo(RIG *rig, vfo_t vfo);
int gr_get_vfo(RIG *rig, vfo_t *vfo);
int gr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int gr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

int gnuradio_set_conf(RIG *rig, token_t token, const char *val);
int gnuradio_get_conf(RIG *rig, token_t token, char *val);
int gnuradio_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int gnuradio_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

int gnuradio_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int gnuradio_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
int gnuradio_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
int gnuradio_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int gnuradio_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);

int mc4020_open(RIG *rig);
int graudio_open(RIG *rig);
int graudioiq_open(RIG *rig);

extern const struct confparams gnuradio_cfg_params[];

extern const struct rig_caps gr_caps;
extern const struct rig_caps mc4020_caps;
extern const struct rig_caps graudio_caps;
extern const struct rig_caps graudioiq_caps;

__END_DECLS

#endif /* _GNURADIO_H */
