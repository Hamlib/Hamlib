/*
 *  Hamlib CI-V backend - description of IC-7300 and variations
 *  Adapted by J.Watson from IC-7000 code (c) 2004 by Stephane Fillod
 *  Adapted from IC-7200 (c) 2016 by Michael Black W9MDB
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "token.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"
#include "bandplan.h"
#include "tones.h"

/* AM Data mode needs adding - this would require one more mode 'RIG_MODE_PKTAM' to rig.h */
#define IC7300_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)
#define IC7300_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)
#define IC7300_NOT_TS_MODES (IC7300_ALL_RX_MODES &~IC7300_1HZ_TS_MODES)

#define IC7300_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define IC7300_AM_TX_MODES (RIG_MODE_AM)

#define IC7300_FUNCS (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_SCOPE)

#define IC7300_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC)

// Not implemented yet
//#define IC7300_EXT_LEVELS (TOK_LEVEL_MONITOR)

#define IC7300_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define IC7300_PARMS (RIG_PARM_BACKLIGHT|RIG_PARM_TIME|RIG_PARM_BEEP)

#define IC7300_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define IC7300_SCAN_OPS (RIG_SCAN_STOP|RIG_SCAN_MEM|RIG_SCAN_PROG|RIG_SCAN_SLCT)

#define IC7300_ANTS (RIG_ANT_1) /* ant-1 is Hf-6m */


/*
 * FIXME: This is a guess real measures please!
 */
#define IC7300_STR_CAL { 3, \
	{ \
		{   0, -54 }, \
		{  10, -24 }, \
		{ 120,  0 }, \
		{ 241,  64 } \
	} }

/*
 * IC-7300 rig capabilities.
 *
 * TODO: complete command set (esp. the $1A bunch!) and testing..
 */
static const struct icom_priv_caps IC7300_priv_caps = {
		0x94,	/* default address */
		0,		/* 731 mode */
    0,    /* no XCHG */
		ic7300_ts_sc_list,
		.civ_version = 1	/* new version of some commands, e.g. ic7200/7300 */
};

/* Private IC7300 extra levels definitions
 *
 * Token definitions for .cfgparams in rig_caps
 * See enum rig_conf_e and struct confparams in rig.h
 */
const struct confparams ic7300_ext_levels[] = {
        { TOK_LEVEL_MONITOR, "MONITORGAIN", "Monitor gain", "Monitor gain",
                NULL, RIG_CONF_NUMERIC, { .n = { 0, 1, 0 } }
        },
        { RIG_CONF_END, NULL, }
};
  

int ic7300_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *ts);
int ic7300_set_rit(RIG *rig, vfo_t vfo, shortfreq_t ts);
int ic7300_set_xit(RIG *rig, vfo_t vfo, shortfreq_t ts);
int ic7300_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int ic7300_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);

const struct rig_caps ic7300_caps = {
.rig_model =  RIG_MODEL_IC7300,
.model_name = "IC-7300",
.mfg_name =  "Icom",
.version =  BACKEND_VER ".2",
.copyright =  "LGPL",
.status =  RIG_STATUS_STABLE,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  19200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  1000,
.retry =  3,
.has_get_func =  IC7300_FUNCS,
.has_set_func =  IC7300_FUNCS,
.has_get_level =  IC7300_LEVELS,
.has_set_level =  RIG_LEVEL_SET(IC7300_LEVELS),
.has_get_parm =  IC7300_PARMS,
.has_set_parm =  RIG_PARM_SET(IC7300_PARMS),
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
},
.parm_gran =  {},
.extlevels = ic7300_ext_levels,
.ctcss_list =  full_ctcss_list,
.dcs_list =  NULL,
.preamp =   { 1, 2, RIG_DBLST_END, },
.attenuator =   { 1, RIG_DBLST_END, }, /* value taken from p.45 of manual*/
.max_rit =  Hz(9999),
.max_xit =  Hz(9999),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC7300_VFO_OPS,
.scan_ops =  IC7300_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   1,
.chan_desc_sz =  0,

.chan_list =  {
	   {   1,  99, RIG_MTYPE_MEM  },
	   { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
	   RIG_CHAN_END,
	},

.rx_range_list1 =   { {kHz(30),MHz(60),IC7300_ALL_RX_MODES,-1,-1,IC7300_VFOS}, RIG_FRNG_END, },
.tx_range_list1 =   {
	FRQ_RNG_HF(1,IC7300_OTHER_TX_MODES, W(2),W(100),IC7300_VFOS,RIG_ANT_1),
	FRQ_RNG_6m(1,IC7300_OTHER_TX_MODES, W(2),W(100),IC7300_VFOS,RIG_ANT_1),
	FRQ_RNG_HF(1,IC7300_AM_TX_MODES, W(1),W(40),IC7300_VFOS,RIG_ANT_1),   /* AM class */
	FRQ_RNG_6m(1,IC7300_AM_TX_MODES, W(1),W(40),IC7300_VFOS,RIG_ANT_1),   /* AM class */
   	RIG_FRNG_END, },

.rx_range_list2 =   { {kHz(30),MHz(60),IC7300_ALL_RX_MODES,-1,-1,IC7300_VFOS}, RIG_FRNG_END, },
.tx_range_list2 =  { /* needs the 5 mhz channels added */
	FRQ_RNG_HF(2,IC7300_OTHER_TX_MODES, W(2),W(100),IC7300_VFOS,RIG_ANT_1),
	FRQ_RNG_6m(2,IC7300_OTHER_TX_MODES, W(2),W(100),IC7300_VFOS,RIG_ANT_1),
	FRQ_RNG_HF(2,IC7300_AM_TX_MODES, W(1),W(40),IC7300_VFOS,RIG_ANT_1),   /* AM class */
	FRQ_RNG_6m(2,IC7300_AM_TX_MODES, W(1),W(40),IC7300_VFOS,RIG_ANT_1),   /* AM class */
    	RIG_FRNG_END, },

.tuning_steps = {
         {IC7300_ALL_RX_MODES,Hz(1)},
         {IC7300_ALL_RX_MODES,kHz(1)},
         {IC7300_ALL_RX_MODES,kHz(5)},
         {IC7300_ALL_RX_MODES,kHz(9)},
         {IC7300_ALL_RX_MODES,kHz(10)},
         {IC7300_ALL_RX_MODES,kHz(12.5)},
         {IC7300_ALL_RX_MODES,kHz(20)},
         {IC7300_ALL_RX_MODES,kHz(25)},
         RIG_TS_END,
        },

	/* mode/filter list, remember: order matters! But duplication may speed up search.  Put the most commonly used modes first!  Remember these are defaults, with dsp rigs you can change them to anything you want except FM and WFM which are fixed */
.filters = 	{
		{RIG_MODE_SSB, kHz(2.4)},
		{RIG_MODE_SSB, kHz(1.8)},
		{RIG_MODE_SSB, kHz(3)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_RTTYR, Hz(500)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_RTTYR, Hz(250)},
		{RIG_MODE_CW|RIG_MODE_CWR, kHz(1.2)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2.4)},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_AM, kHz(3)},
		{RIG_MODE_AM, kHz(9)},
		RIG_FLT_END,
	},

.str_cal = IC7300_STR_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&IC7300_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode_with_data,
.get_mode =  icom_get_mode_with_data,
//.get_vfo =  icom_get_vfo,
.set_vfo =  icom_set_vfo,
.set_ant =  NULL,  /*automatically set by rig depending band */
.get_ant =  NULL,

.set_rit =  ic7300_set_rit,
.get_rit =  ic7300_get_rit,
// the 7300 has only one register for both RIT and Delta TX
// you can turn one or both on -- but both end up just being in sync
// so we'll just reuse the rit settings
.get_xit =  ic7300_get_rit,
.set_xit =  ic7300_set_xit,

.decode_event =  icom_decode_event,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
.set_ext_level =  icom_set_ext_level,
.get_ext_level =  icom_get_ext_level,
.set_func =  ic7300_set_func,
.get_func =  ic7300_get_func,
.set_parm =  icom_set_parm,
.get_parm =  icom_get_parm,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.set_ptt =  icom_set_ptt,
.get_ptt =  icom_get_ptt,
.get_dcd =  icom_get_dcd,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  NULL,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_ctcss_tone =  icom_set_ctcss_tone,
.get_ctcss_tone =  icom_get_ctcss_tone,
.set_ctcss_sql =  icom_set_ctcss_sql,
.get_ctcss_sql =  icom_get_ctcss_sql,
.set_split_freq =  icom_set_split_freq,
.get_split_freq =  icom_get_split_freq,
.set_split_mode =  icom_set_split_mode,
.get_split_mode =  icom_get_split_mode,
.set_split_vfo =  icom_set_split_vfo,
.get_split_vfo =  NULL,
.set_powerstat = icom_set_powerstat,
.power2mW = icom_power2mW,
.mW2power = icom_mW2power,
.send_morse = icom_send_morse

};

int ic7300_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
        unsigned char tsbuf[MAXFRAMELEN];
        int ts_len, retval;

        retval = icom_transaction (rig, 0x21, 0x00, NULL, 0, tsbuf, &ts_len);
        if (retval != RIG_OK)
                return retval;

        /*
         * tsbuf nibbles should contain 10,1,1000,100 hz digits and 00=+, 01=- bit
         */
	rig_debug(RIG_DEBUG_VERBOSE,"ts_len=%d\n",ts_len);
        if (ts_len != 5) {
                rig_debug(RIG_DEBUG_ERR,"ic7300_get_ts: wrong frame len=%d\n",
                                        ts_len);
                return -RIG_ERJCTED;
        }

	*ts = tsbuf[2] & 0x0f   * 1;
	*ts += (tsbuf[2] >> 4)  * 10;
	*ts += (tsbuf[3]& 0x0f) * 100;
	*ts += (tsbuf[3] >> 4)  * 1000;
	if (tsbuf[4]!=0) *ts *= -1;

        return RIG_OK;
}

static int xit_flag = 0;

int ic7300_set_rit(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
	unsigned char tsbuf[8];
	char tmpbuf[8];
	unsigned char ackbuf[16];
	int ack_len;
	int retval;

	rig_debug(RIG_DEBUG_VERBOSE,"ic7300_set_rit: ts=%d\n",ts);
	tsbuf[2] = 0;

	// set sign bit
	if (ts < 0) tsbuf[2] = 1;
	snprintf(tmpbuf,sizeof(tmpbuf),"%04d",abs(ts));
	unsigned int b1,b2;
	sscanf(tmpbuf,"%02x%02x",&b1,&b2);
	tsbuf[1] = b1;
	tsbuf[0] = b2;


	if (ts != 0) {
	        retval =  icom_transaction (rig, 0x21, 0x00, tsbuf, 3, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;
	}
	
	if (ts == 0) { // turn off both rit/xit
		retval = ic7300_set_func(rig,vfo,RIG_FUNC_XIT,0);
		if (retval != RIG_OK) return retval;
		retval = ic7300_set_func(rig,vfo,RIG_FUNC_RIT,0);
	}
	else {
		if (xit_flag) retval = ic7300_set_func(rig,vfo,RIG_FUNC_XIT,1);
		else retval = ic7300_set_func(rig,vfo,RIG_FUNC_RIT,1);
	}
	return retval;
}

int ic7300_set_xit(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
	int retval;
	xit_flag = 1;
	retval = ic7300_set_rit(rig,vfo,ts);
	xit_flag = 0;
	return retval;
}

int ic7300_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
	unsigned char ackbuf[16];
	int ack_len;
	int retval;

	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !status)
		return -RIG_EINVAL;

	switch (func) {
		case RIG_FUNC_RIT:
        		retval = icom_transaction (rig, 0x21, 0x01, NULL, 0, ackbuf, &ack_len);
			if (retval != RIG_OK) return retval;
			if (ack_len != 3) return RIG_BUSERROR;
			*status = ackbuf[2];
			break;

		case RIG_FUNC_XIT:
        		retval = icom_transaction (rig, 0x21, 0x02, NULL, 0, ackbuf, &ack_len);
			if (ack_len != 3)  return RIG_BUSERROR;
			
			*status = ackbuf[2];
			break;

		default:
			return icom_get_func(rig, vfo, func, status);
	}
	return retval;
}

int ic7300_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
	unsigned char ts_buf[4];
	unsigned char ackbuf[8];
	int ack_len;

	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	switch (func) {
		case RIG_FUNC_RIT:
			ts_buf[0] = status;	
        		return  icom_transaction (rig, 0x21, 0x01, ts_buf, 1, ackbuf, &ack_len);

		case RIG_FUNC_XIT:
			ts_buf[0] = status;	
        		return  icom_transaction (rig, 0x21, 0x02, ts_buf, 1, ackbuf, &ack_len);
		default:
			return icom_set_func(rig, vfo, func, status);
	}
	return RIG_OK;
}
