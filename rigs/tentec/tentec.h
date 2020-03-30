/*
 *  Hamlib Tentec backend - main header
 *  Copyright (c) 2001-2011 by Stephane Fillod
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

/**
 * \addtogroup tentec
 * @{
 */
/**
 * \file tentec.h
 * \brief Includes for Tentec Backends
 */

#ifndef _TENTEC_H
#define _TENTEC_H 1

#include <hamlib/rig.h>

// The include order will determine which BACKEND_VER is used
// tentec2.h may also be included and the last include is the BACKEND_VER used
#undef BACKEND_VER
#define BACKEND_VER "20200113"

/**
 * \brief Private tentec info
 */
struct tentec_priv_data {
	rmode_t mode;		/*!< detection mode */
	freq_t freq;		/*!< tuned frequency */
	pbwidth_t width;	/*!< filter bandwidth in Hz */
	int cwbfo;		    /*!< BFO frequency: 1000 [0-2000Hz] */
    int pbt;            /*!< Passband Tuning, IF shift: 0 [-2000Hz to 2000Hz] */
	float lnvol;		/*!< line-out volume: 30 [0..63] */
	float spkvol;		/*!< speaker volume: 30 [0..63] */
	int agc;		/*!< AGC: medium */

	/* calculated by tentec_tuning_factor_calc() */
	int ctf; /*!< Coarse Tune Factor */
	int ftf; /*!< Fine Tune Factor */
	int btf; /*!< Bfo Tune Factor, btval is ignored by RX-320 in AM MODE */
};

int tentec_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len);

int tentec_init(RIG *rig);
int tentec_cleanup(RIG *rig);
int tentec_trx_open(RIG *rig);
int tentec_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int tentec_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int tentec_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int tentec_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int tentec_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int tentec_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
const char* tentec_get_info(RIG *rig);

extern const struct rig_caps rx320_caps;
extern const struct rig_caps rx340_caps;
extern const struct rig_caps rx350_caps;
extern const struct rig_caps tt516_caps;
extern const struct rig_caps tt538_caps;
extern const struct rig_caps tt585_caps;
extern const struct rig_caps tt588_caps;
extern const struct rig_caps tt550_caps;
extern const struct rig_caps tt565_caps;
extern const struct rig_caps tt599_caps;
extern const struct rig_caps rx331_caps;


#endif /* _TENTEC_H */
/** @} */
