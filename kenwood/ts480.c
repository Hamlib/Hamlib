/*
 *  Hamlib Kenwood backend - TS480 description
 *  Copyright (c) 2000-2004 by Stephane Fillod and Juergen Rinas
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
#include <stdio.h>

#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "kenwood.h"

#define TS480_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS480_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS480_AM_TX_MODES RIG_MODE_AM
#define TS480_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS480_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC)
#define TS480_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC)


/*
 * kenwood_ts480_get_info
 * Assumes rig!=NULL
 */
static const char *
kenwood_ts480_get_info (RIG * rig)
{
  char firmbuf[50];
  size_t firm_len;
  int retval;

  firm_len = 50;
  retval = kenwood_transaction (rig, "TY", 2, firmbuf, &firm_len);
  if (retval != RIG_OK)
    return NULL;
  if (firm_len != 6)
    {
      rig_debug (RIG_DEBUG_ERR, "kenwood_get_info: wrong answer len=%d\n", firm_len);
      return NULL;
    }
  switch (firmbuf[4])
    {
    case '0':
      return "TS-480HX (200W)";
    case '1':
      return "TS-480SAT (100W + AT)";
    case '2':
      return "Japanese 50W type";
    case '3':
      return "Japanese 20W type";
    default:
      return "Firmware: unknown";
    }
}


/*
 * kenwood_ts480_set_level
 * Assumes rig!=NULL
 *
 * set levels of most functions
 *
 * WARNING: the commands differ slightly from the general versions in kenwood.c
 * e.g.: "SQ"=>"SQ0" , "AG"=>"AG0"
 */
int
kenwood_ts480_set_level (RIG * rig, vfo_t vfo, setting_t level, value_t val)
{
  char levelbuf[16];
  int kenwood_val;

  switch (level)
    {
    case RIG_LEVEL_RFPOWER:
      kenwood_val = val.f * 100;	/* level for TS480SAT is from 0.. 100W in SSB */
      sprintf (levelbuf, "PC%03d", kenwood_val);
      break;

    case RIG_LEVEL_AF:
      kenwood_val = val.f * 255;	/* possible values for TS480 are 000.. 255 */
      sprintf (levelbuf, "AG0%03d", kenwood_val);
      break;

    case RIG_LEVEL_RF:
      kenwood_val = val.f * 100;	/* possible values for TS480 are 000.. 100 */
      sprintf (levelbuf, "RG%03d", kenwood_val);
      break;

    case RIG_LEVEL_SQL:
      kenwood_val = val.f * 255;	/* possible values for TS480 are 000.. 255 */
      sprintf (levelbuf, "SQ0%03d", kenwood_val);
      break;

    case RIG_LEVEL_AGC:	/* possible values for TS480 000(=off), 001(=fast), 002(=slow) */
      /* hamlib argument is int, possible values rig.h:enum agc_level_e */
      switch (val.i)
	{
	case RIG_AGC_OFF:
	  kenwood_val = 0;
	  break;
	case RIG_AGC_FAST:
	  kenwood_val = 1;
	  break;
	case RIG_AGC_SLOW:
	  kenwood_val = 2;
	  break;
	default:
	  rig_debug (RIG_DEBUG_ERR, "Unsupported agc value");
	  return -RIG_EINVAL;
	};
      sprintf (levelbuf, "GT%03d", kenwood_val);
      break;

    default:
      rig_debug (RIG_DEBUG_ERR, "Unsupported set_level %d", level);
      return -RIG_EINVAL;
    }

  return kenwood_simple_cmd(rig, levelbuf);
}


/*
 * kenwood_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int
kenwood_ts480_get_level (RIG * rig, vfo_t vfo, setting_t level, value_t * val)
{
  char ackbuf[50];
  size_t ack_len = 50;
  int levelint;
  int retval;

  switch (level)
    {
    case RIG_LEVEL_RFPOWER:
      retval = kenwood_transaction (rig, "PC", 2, ackbuf, &ack_len);
      if (RIG_OK != retval)
	return retval;
      if (6 != ack_len)
	return -RIG_EPROTO;
      if (1 != sscanf (&ackbuf[2], "%d", &levelint))
	return -RIG_EPROTO;
      val->f = (float) levelint / 100.;
      return RIG_OK;

    case RIG_LEVEL_AF:
      retval = kenwood_transaction (rig, "AG0", 3, ackbuf, &ack_len);
      if (RIG_OK != retval)
	return retval;
      if (7 != ack_len)
	return -RIG_EPROTO;
      if (1 != sscanf (&ackbuf[3], "%d", &levelint))
	return -RIG_EPROTO;
      val->f = (float) levelint / 255.;
      return RIG_OK;

    case RIG_LEVEL_RF:
      retval = kenwood_transaction (rig, "RG", 2, ackbuf, &ack_len);
      if (RIG_OK != retval)
	return retval;
      if (6 != ack_len)
	return -RIG_EPROTO;
      if (1 != sscanf (&ackbuf[2], "%d", &levelint))
	return -RIG_EPROTO;
      val->f = (float) levelint / 100.;
      return RIG_OK;

    case RIG_LEVEL_SQL:
      retval = kenwood_transaction (rig, "SQ0", 3, ackbuf, &ack_len);
      if (RIG_OK != retval)
	return retval;
      if (7 != ack_len)
	return -RIG_EPROTO;
      if (1 != sscanf (&ackbuf[3], "%d", &levelint))
	return -RIG_EPROTO;
      val->f = (float) levelint / 255.;
      return RIG_OK;

    case RIG_LEVEL_AGC:
      retval = kenwood_transaction (rig, "GT", 2, ackbuf, &ack_len);
      if (RIG_OK != retval)
	return retval;
      if (6 != ack_len)
	return -RIG_EPROTO;
      switch (ackbuf[4])
	{
	case '0':
	  val->i = RIG_AGC_OFF;
	  break;
	case '1':
	  val->i = RIG_AGC_FAST;
	  break;
	case '2':
	  val->i = RIG_AGC_SLOW;
	  break;
	default:
	  return -RIG_EPROTO;
	}
      return RIG_OK;

    case RIG_LEVEL_MICGAIN:
    case RIG_LEVEL_PREAMP:
    case RIG_LEVEL_IF:
    case RIG_LEVEL_APF:
    case RIG_LEVEL_NR:
    case RIG_LEVEL_PBT_IN:
    case RIG_LEVEL_PBT_OUT:
    case RIG_LEVEL_CWPITCH:
    case RIG_LEVEL_KEYSPD:
    case RIG_LEVEL_NOTCHF:
    case RIG_LEVEL_COMP:
    case RIG_LEVEL_BKINDL:
    case RIG_LEVEL_BALANCE:
      return -RIG_ENIMPL;

    default:
      rig_debug (RIG_DEBUG_ERR, "Unsupported get_level %d", level);
      return -RIG_EINVAL;
    }

  return RIG_OK;		/* never reached */
}

static struct kenwood_priv_caps ts480_priv_caps = {
  .cmdtrm = EOM_KEN,
};


/*
 * ts480 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps ts480_caps = {
  .rig_model = RIG_MODEL_TS480,
  .model_name = "TS-480",
  .mfg_name = "Kenwood",
  .version = BACKEND_VER ".5",
  .copyright = "LGPL",
  .status = RIG_STATUS_UNTESTED,
  .rig_type = RIG_TYPE_TRANSCEIVER,
  .ptt_type = RIG_PTT_RIG_MICDATA,
  .dcd_type = RIG_DCD_RIG,
  .port_type = RIG_PORT_SERIAL,
  .serial_rate_min = 4800,
  .serial_rate_max = 115200,
  .serial_data_bits = 8,
  .serial_stop_bits = 1,
  .serial_parity = RIG_PARITY_NONE,
  .serial_handshake = RIG_HANDSHAKE_NONE,
  .write_delay = 0,
  .post_write_delay = 0,
  .timeout = 200,
  .retry = 3,
  .preamp = {12, RIG_DBLST_END,},
  .attenuator = {12, RIG_DBLST_END,},
  .max_rit = kHz (9.99),
  .max_xit = kHz (9.99),
  .max_ifshift = Hz (0),
  .targetable_vfo = RIG_TARGETABLE_FREQ,
  .transceive = RIG_TRN_RIG,


  .rx_range_list1 = {
                     {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
                     RIG_FRNG_END,
                     }, /*!< Receive frequency range list for ITU region 1 */
  .tx_range_list1 = {
                     {kHz(1810),  kHz(1850),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},   /* 100W class */
                     {kHz(1810),  kHz(1850),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},       /* 25W class */
                     {kHz(3500),  kHz(3800),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(3500),  kHz(3800),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(7),     kHz(7200),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(7),     kHz(7200),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     RIG_FRNG_END,
                     },  /*!< Transmit frequency range list for ITU region 1 */
  .rx_range_list2 = {
                     {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
                     RIG_FRNG_END,
                     },  /*!< Receive frequency range list for ITU region 2 */
  .tx_range_list2 = {
                     {kHz(1800),  MHz(2) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},  /* 100W class */
                     {kHz(1800),  MHz(2) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},      /* 25W class */
                     {kHz(3500),  MHz(4) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(3500),  MHz(4) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {kHz(5250),  kHz(5450),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(5250),  kHz(5450),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(7),     kHz(7300),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(7),     kHz(7300),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
                     {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
                     RIG_FRNG_END,
                     }, /*!< Transmit frequency range list for ITU region 2 */
 .tuning_steps =  {
         {TS480_ALL_MODES,kHz(1)},
         {TS480_ALL_MODES,Hz(2500)},
         {TS480_ALL_MODES,kHz(5)},
         {TS480_ALL_MODES,Hz(6250)},
         {TS480_ALL_MODES,kHz(10)},
         {TS480_ALL_MODES,Hz(12500)},
         {TS480_ALL_MODES,kHz(15)},
         {TS480_ALL_MODES,kHz(20)},
         {TS480_ALL_MODES,kHz(25)},
         {TS480_ALL_MODES,kHz(30)},
         {TS480_ALL_MODES,kHz(100)},
         {TS480_ALL_MODES,kHz(500)},
         {TS480_ALL_MODES,MHz(1)},
         {TS480_ALL_MODES,0},   /* any tuning step */
         RIG_TS_END,
        },
        /* mode/filter list, remember: order matters! */
  .filters =  {
                {RIG_MODE_SSB, kHz(2.4)},
                {RIG_MODE_CW, Hz(200)},
                {RIG_MODE_RTTY, Hz(500)},
                {RIG_MODE_AM, kHz(9)},
                {RIG_MODE_FM, kHz(14)},
                RIG_FLT_END,
        },
  .priv = (void *) &ts480_priv_caps,
  .rig_init = kenwood_init,
  .rig_cleanup = kenwood_cleanup,
  .set_freq = kenwood_set_freq,
  .get_freq = kenwood_get_freq,
  .set_rit = kenwood_set_rit,	/*  FIXME should this switch to rit mode or just set the frequency? */
  .get_rit = kenwood_get_rit,
  .set_xit = kenwood_set_xit,	/* FIXME should this switch to xit mode or just set the frequency?  */
  .get_xit = kenwood_get_xit,
  .set_mode = kenwood_set_mode,
  .get_mode = kenwood_get_mode,
  .set_vfo = kenwood_set_vfo,
  .get_vfo = kenwood_get_vfo_if,
  .set_split_vfo = kenwood_set_split_vfo,
  .get_split_vfo = kenwood_get_split_vfo_if,
  .get_ptt = kenwood_get_ptt,
  .set_ptt = kenwood_set_ptt,
  .get_dcd = kenwood_get_dcd,
  .set_powerstat = kenwood_set_powerstat,
  .get_powerstat = kenwood_get_powerstat,
  .get_info = kenwood_ts480_get_info,
  .reset = kenwood_reset,
  .set_ant = kenwood_set_ant,
  .get_ant = kenwood_get_ant,
  .scan = kenwood_scan,		/* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
  .has_set_level = TS480_LEVEL_ALL,
  .has_get_level = TS480_LEVEL_ALL,
  .set_level = kenwood_ts480_set_level,
  .get_level = kenwood_ts480_get_level,
  .has_get_func = TS480_FUNC_ALL,
  .has_set_func = TS480_FUNC_ALL,
  .set_func = kenwood_set_func,
  .get_func = kenwood_get_func,
};


/*
 * my notes:
 *  format with:  indent --line-length 200 ts480.c
 *
 * for the TS480 the function NR and BC have tree state: NR0,1,2 and BC0,1,2
 * this cannot be send through the on/off logic of set_function!
 */


/*
 * Function definitions below
 */
