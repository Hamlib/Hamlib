/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * dummy.c - Copyright (C) 2001 Stephane Fillod.
 *
 * This shared library supports dummy backend, for debugging
 * purpose mainly.
 *
 *
 *	$Id: dummy.c,v 1.12 2001-06-20 06:08:21 f4cfe Exp $
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *  */

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>

#if defined(__CYGWIN__)
#  undef HAMLIB_DLL
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#  define HAMLIB_DLL
#  include <hamlib/rig_dll.h>
#else
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#endif

#include "dummy.h"


static unsigned char *decode_vfo(vfo_t vfo)
{
	switch (vfo) {
	case	RIG_VFO_A:
			return "VFOA";
	case	RIG_VFO_B:
			return "VFOA";
	case	RIG_VFO_C:
			return "VFOC";
	case	RIG_VFO_CURR:
			return "currVFO";
	case	RIG_VFO_ALL:
			return "VFOall";
#ifdef RIG_VFO_MEM
	case	RIG_VFO_MEM:
			return "MEM";
#endif
#ifdef RIG_VFO_VFO
	case	RIG_VFO_VFO:
			return "VFO";
#endif
		default:
			return "VFO?";
	}
	return "VFO?";
}

static unsigned char *decode_mode(rmode_t mode, pbwidth_t width)
{
	switch (mode) {
	case	RIG_MODE_AM:
			return "AM";
	case	RIG_MODE_FM:
			return "FM";
	case	RIG_MODE_LSB:
			return "LSB";
	case	RIG_MODE_USB:
			return "USB";
	case	RIG_MODE_CW:
			return "CW";
	case	RIG_MODE_RTTY:
			return "RTTY";
	case	RIG_MODE_WFM:
			return "WFM";
	case	RIG_MODE_NONE:
			return "None";
		default:
			return "MODE?";
	}
	return "MODE?";
}

/*
 * Huh! ugly code! these static buffers prevent reentrancy!
 */
#define BUF_SIZE 32

static unsigned char *decode_freq(freq_t freq)
{
	static unsigned char buf[BUF_SIZE];

	if (freq < kHz(1))
		snprintf(buf, BUF_SIZE, "%lldHz", freq);
	else if (freq < MHz(1))
			snprintf(buf, BUF_SIZE, "%lld.%lldkHz", freq/kHz(1), freq%kHz(1));
	else if (freq < GHz(1))
			snprintf(buf, BUF_SIZE, "%lld.%lldMHz", freq/MHz(1), freq%MHz(1));
	else
		snprintf(buf, BUF_SIZE, "%lld.%lldGHz", freq/GHz(1), freq%GHz(1));

	return buf;
}

/********************************************************************/

static int dummy_init(RIG *rig) {
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");
  rig->state.rigport.type.rig = RIG_PORT_NONE;

  return RIG_OK;
}

static int dummy_cleanup(RIG *rig) {
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_open(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_close(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called: %s %s\n", 
 			decode_vfo(vfo), decode_freq(freq));

  return RIG_OK;
}


static int dummy_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called: %s\n", decode_vfo(vfo));

  return RIG_OK;
}


static int dummy_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called: %s %s\n", 
  		decode_vfo(vfo), decode_mode(mode, width));

  return RIG_OK;
}


static int dummy_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called: %s\n", decode_vfo(vfo));

  return RIG_OK;
}


static int dummy_set_vfo(RIG *rig, vfo_t vfo)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called: %s\n", decode_vfo(vfo));

  return RIG_OK;
}


static int dummy_get_vfo(RIG *rig, vfo_t *vfo)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_ctcss(RIG *rig, vfo_t vfo, unsigned int tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_ctcss(RIG *rig, vfo_t vfo, unsigned int *tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_dcs(RIG *rig, vfo_t vfo, unsigned int code)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_dcs(RIG *rig, vfo_t vfo, unsigned int *code)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int *tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_split_freq(RIG *rig, vfo_t vfo, freq_t rx_freq, freq_t tx_freq)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_split_freq(RIG *rig, vfo_t vfo, freq_t *rx_freq, freq_t *tx_freq)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_set_split_mode(RIG *rig, vfo_t vfo, rmode_t rx_mode, pbwidth_t rx_width, rmode_t tx_mode, pbwidth_t tx_width)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *rx_mode, pbwidth_t *rx_width, rmode_t *tx_mode, pbwidth_t *tx_width)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_set_split(RIG *rig, vfo_t vfo, split_t split)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_split(RIG *rig, vfo_t vfo, split_t *split)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_powerstat(RIG *rig, powerstat_t status)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_powerstat(RIG *rig, powerstat_t *status)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_parm(RIG *rig, setting_t parm, value_t val)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_parm(RIG *rig, setting_t parm, value_t *val)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_bank(RIG *rig, vfo_t vfo, int bank)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_mem(RIG *rig, vfo_t vfo, int ch)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


#ifdef WANT_OLD_VFO_TO_BE_REMOVED
static int dummy_mv_ctl(RIG *rig, vfo_t vfo, mv_op_t op)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}
#else
static int dummy_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}
#endif

static int dummy_set_channel(RIG *rig, const channel_t *chan)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_channel(RIG *rig, channel_t *chan)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_set_trn(RIG *rig, vfo_t vfo, int trn)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int dummy_get_trn(RIG *rig, vfo_t vfo, int *trn)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static const char *dummy_get_info(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return "";
}


static int dummy_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int dummy_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}



/*
 * Dummy rig capabilities.
 */

#define DUMMY_FUNC  0
#define DUMMY_SET_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AF)
#define DUMMY_LEVEL (DUMMY_SET_LEVEL | RIG_LEVEL_STRENGTH)

#define DUMMY_MODES (RIG_MODE_AM | RIG_MODE_CW | \
                     RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_FM | RIG_MODE_WFM)

const struct rig_caps dummy_caps = {
  rig_model:     RIG_MODEL_DUMMY,
  model_name:    "Dummy",
  mfg_name:      "Hamlib",
  version:       "0.1",
  copyright:	 "GPL",
  status:        RIG_STATUS_NEW,
  rig_type:      RIG_TYPE_OTHER,
  targetable_vfo:	 0,
  ptt_type:      RIG_PTT_NONE,
  dcd_type:      RIG_DCD_NONE,
  port_type:     RIG_PORT_NONE,
  has_get_func:  DUMMY_FUNC,
  has_set_func:  DUMMY_FUNC,
  has_get_level: DUMMY_LEVEL,
  has_set_level: DUMMY_SET_LEVEL,
  has_get_parm:	 RIG_PARM_NONE,	/* FIXME */
  has_set_parm:	 RIG_PARM_NONE,	/* FIXME */
  ctcss_list:	 NULL,	/* FIXME */
  dcs_list:  	 NULL,  /* FIXME */
  chan_list:	 { RIG_CHAN_END, },	/* FIXME */
  transceive:    RIG_TRN_OFF,
  attenuator:    { 10, 20, 30, RIG_DBLST_END, },
  rx_range_list2: { {start:kHz(150),end:MHz(1500),modes:DUMMY_MODES,
		    low_power:-1,high_power:-1,RIG_VFO_A|RIG_VFO_B},
		    RIG_FRNG_END, },
  tx_range_list2: { RIG_FRNG_END, },
  tuning_steps: { {DUMMY_MODES,1}, RIG_TS_END, },
  priv: NULL,	/* priv */

  rig_init:    dummy_init,
  rig_cleanup: dummy_cleanup,
  rig_open:    dummy_open,
  rig_close:   dummy_close,

  set_freq:    dummy_set_freq,
  get_freq:    dummy_get_freq,
  set_mode:    dummy_set_mode,
  get_mode:    dummy_get_mode,
  set_vfo:     dummy_set_vfo,
  get_vfo:     dummy_get_vfo,
  
  set_powerstat: dummy_set_powerstat,
  get_powerstat: dummy_get_powerstat,
  set_level:    dummy_set_level,
  get_level:    dummy_get_level,
  set_func:     dummy_set_func,
  get_func:     dummy_get_func,
  set_parm:     dummy_set_parm,
  get_parm:     dummy_get_parm,

  get_info:     dummy_get_info,


  set_ptt:	dummy_set_ptt,
  get_ptt:	dummy_get_ptt,
  get_dcd:	dummy_get_dcd,
  set_rptr_shift:	dummy_set_rptr_shift,
  get_rptr_shift:	dummy_get_rptr_shift,
  set_rptr_offs:	dummy_set_rptr_offs,
  get_rptr_offs:	dummy_get_rptr_offs,
  set_ctcss:	dummy_set_ctcss,
  get_ctcss:	dummy_get_ctcss,
  set_dcs:	dummy_set_dcs,
  get_dcs:	dummy_get_dcs,
  set_ctcss_sql:	dummy_set_ctcss_sql,
  get_ctcss_sql:	dummy_get_ctcss_sql,
  set_dcs_sql:	dummy_set_dcs_sql,
  get_dcs_sql:	dummy_get_dcs_sql,
  set_split_freq:	dummy_set_split_freq,
  get_split_freq:	dummy_get_split_freq,
  set_split_mode:	dummy_set_split_mode,
  get_split_mode:	dummy_get_split_mode,
  set_split:	dummy_set_split,
  get_split:	dummy_get_split,
  set_rit:	dummy_set_rit,
  get_rit:	dummy_get_rit,
  set_xit:	dummy_set_xit,
  get_xit:	dummy_get_xit,
  set_ts:	dummy_set_ts,
  get_ts:	dummy_get_ts,
  power2mW:	dummy_power2mW,
  mW2power:	dummy_mW2power,
  set_ant:	dummy_set_ant,
  get_ant:	dummy_get_ant,
  set_bank:	dummy_set_bank,
  set_mem:	dummy_set_mem,
  get_mem:	dummy_get_mem,
#ifdef WANT_OLD_VFO_TO_BE_REMOVED
  mv_ctl:	dummy_mv_ctl,
#else
  vfo_op:	dummy_vfo_op,
#endif
  send_dtmf: dummy_send_dtmf,
  recv_dtmf: dummy_recv_dtmf,
  send_morse: dummy_send_morse,
  set_channel:	dummy_set_channel,
  get_channel:	dummy_get_channel,
  set_trn:	dummy_set_trn,
  get_trn:	dummy_get_trn,
};

int init_dummy(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "dummy: _init called\n");

	rig_register(&dummy_caps);

	return RIG_OK;
}
