/*
 * ft897.h - (C) Tomi Manninen 2003 (oh2bns@sral.fi)
 *
 *  ...derived but heavily modified from:
 *
 * ft817.h - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-817 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 *
 *    $Id: ft897.h,v 1.1 2004-02-08 17:08:46 fillods Exp $  
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */

#ifndef _FT897_H
#define _FT897_H 1

/*
 * No need to wait between written characters.
 */
#define FT897_WRITE_DELAY		0

/*
 * Wait 'delay' milliseconds after writing a command sequence.
 *
 * Setting this to zero means no delay but wait for an acknowledgement
 * from the rig after a command. This is undocumented but seems to work.
 * It's also the most optimal way as long as it works...
 *
 * A non-zero value disables waiting for the ack. Processing a command
 * seems to take about 60 ms so set this to 80 or so to be safe.
 */
#define FT897_POST_WRITE_DELAY		0

/*
 * Read timeout.
 */
#define FT897_TIMEOUT			200

/*
 * The time the TX, RX and FREQ/MODE status are cached (in millisec).
 * This optimises the common case of doing eg. rig_get_freq() and
 * rig_get_mode() in a row.
 *
 * The timeout is deliberately set lower than the time taken to process
 * a single command (~ 60 ms) so that a sequence
 *
 *   rig_get_freq();
 *   rig_set_freq();
 *   rig_get_freq();
 *
 * doesn't return a bogus (cached) value in the last rig_get_freq().
 */
#define FT897_CACHE_TIMEOUT		50


enum ft897_native_cmd_e {
  FT897_NATIVE_CAT_LOCK_ON = 0,
  FT897_NATIVE_CAT_LOCK_OFF,
  FT897_NATIVE_CAT_PTT_ON,
  FT897_NATIVE_CAT_PTT_OFF,
  FT897_NATIVE_CAT_SET_FREQ,
  FT897_NATIVE_CAT_SET_MODE_LSB,
  FT897_NATIVE_CAT_SET_MODE_USB,
  FT897_NATIVE_CAT_SET_MODE_CW,
  FT897_NATIVE_CAT_SET_MODE_CWR,
  FT897_NATIVE_CAT_SET_MODE_AM,
  FT897_NATIVE_CAT_SET_MODE_FM,
  FT897_NATIVE_CAT_SET_MODE_FM_N,
  FT897_NATIVE_CAT_SET_MODE_DIG,
  FT897_NATIVE_CAT_SET_MODE_PKT,
  FT897_NATIVE_CAT_CLAR_ON,
  FT897_NATIVE_CAT_CLAR_OFF,
  FT897_NATIVE_CAT_SET_CLAR_FREQ,
  FT897_NATIVE_CAT_SET_VFOAB,
  FT897_NATIVE_CAT_SPLIT_ON,
  FT897_NATIVE_CAT_SPLIT_OFF,
  FT897_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
  FT897_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
  FT897_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
  FT897_NATIVE_CAT_SET_RPT_OFFSET,
  FT897_NATIVE_CAT_SET_DCS_ON,
  FT897_NATIVE_CAT_SET_DCS_DEC_ON,
  FT897_NATIVE_CAT_SET_DCS_ENC_ON,
  FT897_NATIVE_CAT_SET_CTCSS_ON,
  FT897_NATIVE_CAT_SET_CTCSS_DEC_ON,
  FT897_NATIVE_CAT_SET_CTCSS_ENC_ON,
  FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF,
  FT897_NATIVE_CAT_SET_CTCSS_FREQ,
  FT897_NATIVE_CAT_SET_DCS_CODE,
  FT897_NATIVE_CAT_GET_RX_STATUS,
  FT897_NATIVE_CAT_GET_TX_STATUS,
  FT897_NATIVE_CAT_GET_FREQ_MODE_STATUS,
  FT897_NATIVE_CAT_PWR_WAKE,
  FT897_NATIVE_CAT_PWR_ON,
  FT897_NATIVE_CAT_PWR_OFF,
  FT897_NATIVE_SIZE		/* end marker */
};


typedef enum ft897_native_cmd_e ft897_native_cmd_t;


struct ft897_priv_data {
  yaesu_cmd_set_t pcs[FT897_NATIVE_SIZE];  /* TODO:  why? */

  /* rx status */
  struct timeval rx_status_tv;
  unsigned char rx_status;

  /* tx status */
  struct timeval tx_status_tv;
  unsigned char tx_status;
       
  /* freq & mode status */
  struct timeval fm_status_tv;
  unsigned char fm_status[YAESU_CMD_LENGTH];
};


int ft897_init(RIG *rig);
int ft897_open(RIG *rig);
int ft897_cleanup(RIG *rig);
int ft897_close(RIG *rig);
int ft897_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ft897_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int ft897_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int ft897_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
// int ft897_set_vfo(RIG *rig, vfo_t vfo);
// int ft897_get_vfo(RIG *rig, vfo_t *vfo);
int ft897_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int ft897_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
// int ft897_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int ft897_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int ft897_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
// int ft897_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
// int ft897_set_parm(RIG *rig, setting_t parm, value_t val);
// int ft897_get_parm(RIG *rig, setting_t parm, value_t *val);
int ft897_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
int ft897_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t code);
int ft897_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
int ft897_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
int ft897_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
int ft897_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
int ft897_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int ft897_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
// int ft897_set_powerstat(RIG *rig, powerstat_t status);

#endif /* _FT897_H */
