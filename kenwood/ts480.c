/*
 *  Hamlib Kenwood backend - TS480 description
 *  Copyright (c) 2000-2004 by Stephane Fillod and Juergen Rinas
 *
 *	$Id: ts480.c,v 1.2 2004-11-28 21:06:40 jrinas Exp $
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

#include <hamlib/rig.h>
#include "kenwood.h"

#define TS480_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS480_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS480_AM_TX_MODES RIG_MODE_AM
#define TS480_VFO (RIG_VFO_A|RIG_VFO_B)

/*
 * kenwood_ts480_set_ptt
 * Assumes rig!=NULL
 *
 * set PTT with audio from data connector (NOT microphone!!!)
 */
static int kenwood_ts480_set_ptt (RIG * rig, vfo_t vfo, ptt_t ptt)
{
  unsigned char ackbuf[16];
  int ack_len = 0;

  return kenwood_transaction (rig, ptt == RIG_PTT_ON ? "TX1;" : "RX;", 3, ackbuf, &ack_len);
}


/*
 * kenwood_ts480_set_ant
 * Assumes rig!=NULL
 *
 * set the aerial/antenna  to use
 */
static int kenwood_ts480_set_ant (RIG *rig, vfo_t vfo, ant_t ant)
{
  unsigned char ackbuf[16];
  int ack_len = 0;
  if ( RIG_ANT_1==ant)
    return kenwood_transaction (rig,"AN1;", 4, ackbuf, &ack_len);
  if ( RIG_ANT_2==ant)
    return kenwood_transaction (rig,"AN2;", 4, ackbuf, &ack_len);
  return -RIG_EINVAL;
}


/*
 * kenwood_ts480_get_ant
 * Assumes rig!=NULL
 *
 * get the aerial/antenna  in use
 */
static int kenwood_ts480_get_ant (RIG *rig, vfo_t vfo, ant_t * ant)
{
  unsigned char ackbuf[16];
  int ack_len=16;
  int retval;
  
  retval = kenwood_transaction (rig,"AN;", 3, ackbuf, &ack_len);
  if (RIG_OK != retval)
    return retval;
  if (4!=ack_len)
     return -RIG_EPROTO;   
  switch (ackbuf[2])
  {
    case '1':
      *ant=RIG_ANT_1;
      break;
    case '2':
      *ant=RIG_ANT_2;
      break;
    default:
      /* can only be a protocol error since the ts480 has only two antenna connectors */
      return -RIG_EPROTO;  
  }
  return RIG_OK;
}


/*
 * kenwood_ts480_get_info
 * Assumes rig!=NULL
 */
static const char *
kenwood_ts480_get_info (RIG * rig)
{
  unsigned char firmbuf[50];
  int firm_len, retval;

  firm_len = 50;
  retval = kenwood_transaction (rig, "TY;", 3, firmbuf, &firm_len);
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


static const struct kenwood_priv_caps ts480_priv_caps = {
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
  .version = "0.0.1",
  .copyright = "LGPL",
  .status = RIG_STATUS_NEW,
  .rig_type = RIG_TYPE_TRANSCEIVER,
  .ptt_type = RIG_PTT_RIG,
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
		     {kHz (100), Hz (59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
		     RIG_FRNG_END,
		     },		/* rx range */
  .tx_range_list1 = {
		     {kHz (1810), kHz (1850), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},	/* 100W class */
		     {kHz (1810), kHz (1850), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},	/* 25W class */
		     {kHz (3500), kHz (3800), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (3500), kHz (3800), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (7), kHz (7100), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (7), kHz (7100), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {kHz (10100), kHz (10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (10100), kHz (10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (14), kHz (14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (14), kHz (14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {kHz (18068), kHz (18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (18068), kHz (18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (21), kHz (21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (21), kHz (21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {kHz (24890), kHz (24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (24890), kHz (24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (28), kHz (29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (28), kHz (29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (50), kHz (52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (50), kHz (52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     RIG_FRNG_END,
		     },
  .rx_range_list2 = {
		     {kHz (100), Hz (59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
		     RIG_FRNG_END,
		     },		/* rx range */
  .tx_range_list2 = {
		     {kHz (1800), MHz (2) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},	/* 100W class */
		     {kHz (1800), MHz (2) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},	/* 25W class */
		     {kHz (3500), MHz (4) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (3500), MHz (4) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {kHz (5250), kHz (5450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (5250), kHz (5450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (7), kHz (7300), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (7), kHz (7300), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {kHz (10100), kHz (10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (10100), kHz (10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (14), kHz (14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (14), kHz (14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {kHz (18068), kHz (18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (18068), kHz (18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (21), kHz (21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (21), kHz (21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {kHz (24890), kHz (24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {kHz (24890), kHz (24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (28), kHz (29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (28), kHz (29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     {MHz (50), kHz (52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
		     {MHz (50), kHz (52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
		     RIG_FRNG_END,
		     },		/* tx range */
  .priv = (void *) &ts480_priv_caps,
  .set_freq = kenwood_set_freq,
  .get_freq = kenwood_get_freq,
  .set_rit = kenwood_set_rit,	/*  FIXME should this switch to rit mode or just set the frequency? */
  .get_rit = kenwood_get_rit,
  .set_xit = kenwood_set_xit,	/* FIXME should this switch to xit mode or just set the frequency?  */
  .get_xit = kenwood_get_xit,
  .set_mode = kenwood_set_mode,
  .get_mode = kenwood_get_mode,
  .set_vfo = kenwood_set_vfo,
  .get_vfo = kenwood_get_vfo,
  .get_ptt = kenwood_get_ptt,
  .set_ptt = kenwood_ts480_set_ptt,
  .get_dcd = kenwood_get_dcd,
  .set_powerstat = kenwood_set_powerstat,
  .get_powerstat = kenwood_get_powerstat,
  .get_info = kenwood_ts480_get_info,
  .reset = kenwood_reset,
  .set_ant = kenwood_ts480_set_ant,
  .get_ant = kenwood_ts480_get_ant,
};

/*
 * Function definitions below
 */
