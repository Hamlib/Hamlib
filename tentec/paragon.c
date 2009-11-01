/*
 *  Hamlib TenTenc backend - TT-585 Paragon description
 *  Copyright (c) 2003-2009 by Stephane Fillod
 *
 *	$Id: jupiter.c,v 1.4 2006-10-07 17:38:05 csete Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>
#include "tentec.h"
#include "bandplan.h"

struct tt585_priv_data {
    char status[30];
};


#define TT585_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_AM)
#define TT585_RXMODES (TT585_MODES)

#define TT585_FUNCS (RIG_FUNC_NONE)

#define TT585_LEVELS (RIG_LEVEL_NONE)

#define TT585_ANTS (RIG_ANT_1) 

#define TT585_PARMS (RIG_PARM_NONE)

#define TT585_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT585_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO|\
        RIG_OP_CPY|RIG_OP_MCL|RIG_OP_TOGGLE\
        RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN)

/*
 * Mem caps to be checked..
 */
#define TT585_MEM_CAP {        \
        .freq = 1,      \
        .mode = 1,      \
        .width = 1,     \
        .channel_desc = 1,  \
}

static int tt585_init(RIG *rig);
static int tt585_cleanup(RIG *rig);
static int tt585_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tt585_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tt585_set_vfo(RIG *rig, vfo_t vfo);
static int tt585_get_vfo(RIG *rig, vfo_t *vfo);
static int tt585_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tt585_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);

/*
 * tt585 transceiver capabilities,
 * with the optional model 258 RS232 Interface board.
 *
 * This is a skelton.
 */
const struct rig_caps tt585_caps = {
.rig_model =  RIG_MODEL_TT585,
.model_name = "TT-585 Paragon",
.mfg_name =  "Ten-Tec",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  1200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  50,
.post_write_delay =  0,
.timeout =  500,
.retry =  3,

.has_get_func =  TT585_FUNCS,
.has_set_func =  TT585_FUNCS,
.has_get_level =  TT585_LEVELS,
.has_set_level =  RIG_LEVEL_SET(TT585_LEVELS),
.has_get_parm =  TT585_PARMS,
.has_set_parm =  TT585_PARMS,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END },
.attenuator =   { RIG_DBLST_END },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo = RIG_TARGETABLE_NONE,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  7,

.chan_list =  {
		{   0, 61, RIG_MTYPE_MEM, TT585_MEM_CAP },
		},

.rx_range_list1 =  {
	{kHz(100),MHz(30)-10,TT585_RXMODES,-1,-1,TT585_VFO,TT585_ANTS},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,TT585_MODES, W(5),W(100),TT585_VFO,TT585_ANTS),
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(100),MHz(30)-10,TT585_RXMODES,-1,-1,TT585_VFO,TT585_ANTS},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,TT585_MODES, W(5),W(100),TT585_VFO,TT585_ANTS),
	{MHz(5.25),MHz(5.40),TT585_MODES,W(5),W(100),TT585_VFO,TT585_ANTS},
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {TT585_RXMODES,10},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_AM, kHz(2.4)},
		{RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM, kHz(1.8)},
		{RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM, 500},
		{RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM, 250},
		{RIG_MODE_FM, kHz(15)},
		RIG_FLT_END,
	},
.priv =  (void*) NULL,

.rig_init =  tt585_init,
.rig_cleanup =  tt585_cleanup,
.set_freq =  tt585_set_freq,
.get_freq =  tt585_get_freq,
.set_vfo =  tt585_set_vfo,
.get_vfo =  tt585_get_vfo,
.set_mode =  tt585_set_mode,
.get_mode =  tt585_get_mode,

};

/*
 * Function definitions below
 */

/*
 * tt585_init:
 * Basically, it just sets up *priv
 */
int tt585_init(RIG *rig)
{
	struct tt585_priv_data *priv;

	priv = (struct tt585_priv_data *) malloc(sizeof(struct tt585_priv_data));
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	memset(priv, 0, sizeof(struct tt585_priv_data));

	return RIG_OK;
}

int tt585_cleanup(RIG *rig)
{
    if (rig->state.priv)
    {
        free(rig->state.priv);
        rig->state.priv = NULL;
    }

	return RIG_OK;
}

int tt585_get_vfo(RIG *rig, vfo_t *vfo) {

	struct tt585_priv_data *priv = (struct tt585_priv_data *) rig->state.priv;

    return -RIG_ENIMPL;
}

/*
 * tt585_set_vfo
 * Assumes rig!=NULL
 */
int tt585_set_vfo(RIG *rig, vfo_t vfo)
{
	struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;

    return -RIG_ENIMPL;
}

/*
 * tt585_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tt585_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;

    return -RIG_ENIMPL;
}

/*
 * tt585_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */

int tt585_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    return -RIG_ENIMPL;
}

/*
 * tt585_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tt585_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	struct tt585_priv_data *priv = (struct tt585_priv_data *)rig->state.priv;

    return -RIG_ENIMPL;
}

/*
 * tt585_set_mode
 * Assumes rig!=NULL
 */
int tt585_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    return -RIG_ENIMPL;
}


