/*
 *  Hamlib Kenwood backend - TS140 description
 *  Copyright (c) 2000-2005 by Stephane Fillod
 *
 *	$Id: ts140.c,v 1.7 2006-03-14 20:06:46 pa4tu Exp $
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

#include <stdlib.h>
#include <stdio.h>

#include "hamlib/rig.h"
#include "bandplan.h"
#include "kenwood.h"


#define TS140_ALL_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_CWR)
#define TS140_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_CWR)
#define TS140_AM_TX_MODES RIG_MODE_AM
#define TS140_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define TS140_ANTS (0)

/*
 * mode defines
 */
#define MD_NONE '0'
#define MD_LSB  '1'
#define MD_USB  '2'
#define MD_CW   '3'
#define MD_FM   '4'
#define MD_AM   '5'
#define MD_CWR  '7'

/*
 * vfo defines
*/
#define VFO_A	'0'
#define	VFO_B	'1'
#define	VFO_MEM	'2'

static const struct kenwood_priv_caps  ts140_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

static int ts140_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
                char modebuf[50];
                int retval;
		size_t mode_len;

                mode_len = 50;
                retval = kenwood_transaction (rig, "IF;", 3, modebuf, &mode_len);
                if (retval != RIG_OK)
                        return retval;

                if (mode_len != 38 || modebuf[1] != 'F') {
                rig_debug(RIG_DEBUG_ERR,"ts140_get_mode: unexpected answer len=%d\n",
                                                mode_len);
                return -RIG_ERJCTED;
                }

                switch (modebuf[29]) {
                        case MD_CW:     *mode = RIG_MODE_CW; break;
                        case MD_CWR:    *mode = RIG_MODE_CWR; break; /* Not actually reverse CW. It's narrow where the optional filter is fitted */
                        case MD_USB:    *mode = RIG_MODE_USB; break;
                        case MD_LSB:    *mode = RIG_MODE_LSB; break;
                        case MD_FM:     *mode = RIG_MODE_FM; break;
                        case MD_AM:     *mode = RIG_MODE_AM; break;
                        case MD_NONE:   *mode = RIG_MODE_NONE; break;
                        default:
                                rig_debug(RIG_DEBUG_ERR,"ts140_get_mode: "
                                                                "unsupported mode '%c'\n", modebuf[29]);
                                return -RIG_EINVAL;
                }

                *width = rig_passband_normal(rig, *mode);

                return RIG_OK;
}

static int ts140_set_vfo(RIG *rig, vfo_t vfo)
{
                char cmdbuf[16], ackbuf[50];
                int cmd_len, retval;
		size_t ack_len;
                char vfo_function;

		ack_len = 0;
                switch (vfo) {
                case RIG_VFO_VFO:
                case RIG_VFO_A: vfo_function = VFO_A; break;
                case RIG_VFO_B: vfo_function = VFO_B; break;
                case RIG_VFO_MEM: vfo_function = VFO_MEM; break;
                case RIG_VFO_CURR: return RIG_OK;
                default:
                        rig_debug(RIG_DEBUG_ERR,"ts140_set_vfo: unsupported VFO %d\n",
                                                                vfo);
                        return -RIG_EINVAL;
                }

                cmd_len = sprintf(cmdbuf, "FN%c;", vfo_function); /* The 680 and 140 need this to set the VFO on the radio */
                ack_len = 0;
                retval = kenwood_transaction (rig, cmdbuf, cmd_len, ackbuf, &ack_len);
                if (retval != RIG_OK)
                        return retval;
				return RIG_OK;
}

static int ts140_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
                char freqbuf[50];
                size_t freq_len;
		int retval;
                long long f;

/* We're using IF; here because the TS-140S is incapable of supplying
 * frequency information in MEM mode with the kenwood_get_freq method. It may
 * entail more data over the serial port, but for fewer errors and unexplained
 * silence from rpc.rigd it is a small price to pay. */

		freq_len = 50;
		retval = kenwood_transaction (rig, "IF;", 3, freqbuf, &freq_len);
               	if (retval != RIG_OK)
                       return retval;
               	if (freq_len != 38 || freqbuf[1] != 'F') {
               		rig_debug(RIG_DEBUG_ERR,"ts140_get_freq: wrong answer len=%d\n",
               			                     freq_len);
               		return -RIG_ERJCTED;
               	}

               	freqbuf[14] = '\0';
                sscanf(freqbuf+2, "%lld", &f);
                *freq = (freq_t)f;

                return RIG_OK;
}

static int ts140_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
                char fctbuf[16], ackbuf[50];
                int fct_len;
		size_t ack_len;

                ack_len = 0;
                switch (func) {
                case RIG_FUNC_LOCK:
                if (status > 1) status = 1;
		fct_len = sprintf(fctbuf,"LK%d;", status); /* The only way I could get the rig to drop the lock once asserted */
                return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

                default:
                        rig_debug(RIG_DEBUG_ERR,"Unsupported set_func %#x", func);
                        return -RIG_EINVAL;
                }

                return RIG_OK;
}

/*
 * ts140 rig capabilities.
 * 	MattD.. 2005-01-29
 */

const struct rig_caps ts140_caps = {
.rig_model =  RIG_MODEL_TS140S,
.model_name = "TS-140S",
.mfg_name =  "Kenwood",
.version =  BACKEND_VER ".0",
.copyright =  "LGPL",
.status =  RIG_STATUS_BETA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  9600,	/* Rig only capable of 4800 baud from factory and 9k6 with jumper change */
.serial_data_bits =  8,
.serial_stop_bits =  2,		/* TWO stop bits. This is correct. */
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  300,
.retry =  3,

.has_get_func =  RIG_FUNC_LOCK,
.has_set_func =  RIG_FUNC_LOCK,
.has_get_level =  RIG_LEVEL_NONE,
.has_set_level =  RIG_LEVEL_NONE,
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* No PARAMS controllable */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.preamp =   { RIG_DBLST_END, },	/* Not controllable */
.attenuator =   { RIG_DBLST_END, },	/* Not controllable */
.max_rit =  kHz(1.2),
.max_xit =  kHz(1.2),
.max_ifshift =  Hz(0), /* Not controllable */
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,


.chan_list =  {
			{  0, 19, RIG_MTYPE_MEM  },
			{ 20, 30, RIG_MTYPE_EDGE },
			RIG_CHAN_END,
		},

.rx_range_list1 =  { 
	{kHz(50),kHz(34999),TS140_ALL_MODES,-1,-1,TS140_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  {
    FRQ_RNG_HF(1,TS140_OTHER_TX_MODES,W(5),W(100),TS140_VFO,TS140_ANTS),
    FRQ_RNG_HF(1,TS140_AM_TX_MODES,W(2),W(40),TS140_VFO,TS140_ANTS),
    RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(50),kHz(34999),TS140_ALL_MODES,-1,-1,TS140_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    FRQ_RNG_HF(2,TS140_OTHER_TX_MODES,W(5),W(100),TS140_VFO,TS140_ANTS),
    FRQ_RNG_HF(2,TS140_AM_TX_MODES,W(2),W(40),TS140_VFO,TS140_ANTS),
    RIG_FRNG_END,    
  }, /* tx range */
.tuning_steps =  {		/* FIXME: Done */
	 {TS140_ALL_MODES,10},
	 {TS140_ALL_MODES,100},
	 {TS140_ALL_MODES,kHz(1)},
	 {TS140_ALL_MODES,kHz(5)},
	 {TS140_ALL_MODES,kHz(9)},
	 {TS140_ALL_MODES,kHz(10)},
	 {TS140_ALL_MODES,12500},
	 {TS140_ALL_MODES,kHz(20)},
	 {TS140_ALL_MODES,kHz(25)},
	 {TS140_ALL_MODES,kHz(100)},
	 {TS140_ALL_MODES,MHz(1)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_AM, kHz(2.2)},
		{RIG_MODE_CWR, 600},
		{RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts140_priv_caps,

.set_freq =  kenwood_set_freq,
.get_freq =  ts140_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_mode =  kenwood_set_mode,
.get_mode = ts140_get_mode,
.set_vfo = ts140_set_vfo,
.get_vfo =  kenwood_get_vfo,
.set_ptt =  kenwood_set_ptt,
.set_func =  ts140_set_func,
.get_func =  kenwood_get_func,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  kenwood_get_mem,
.reset =  kenwood_reset,

};

/*
 * Function definitions below
 */

