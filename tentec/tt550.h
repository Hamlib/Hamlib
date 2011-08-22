/*
 *  Hamlib Tentec backend - main header
 *  Copyright (c) 2001-2004 by Stephane Fillod
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

#ifndef _TT550_H
#define _TT550_H 1

#include <hamlib/rig.h>
#include <cal.h>


#define EOM "\015"		/* CR */

#define TT_AM  '0'
#define TT_USB '1'
#define TT_LSB '2'
#define TT_CW  '3'
#define TT_FM  '4'

/*
 * Transmit and receive flags used by tt550_tuning_factor_calc
 * to modify behavior for transmit versus receive vfo's
 */
#define RECEIVE 0
#define TRANSMIT 1


/*
 * Transmitter control operations
 */
#define DISABLE_TX '0'
#define ENABLE_TX  '1'
#define DISABLE_AMP '2'
#define ENABLE_AMP  '3'
#define ENABLE_KEYER '6'
#define DISABLE_KEYER '7'
#define DISABLE_KEEPALIVE  '8'
#define ENABLE_KEEPALIVE  '9'

/* This is needed until we come up with a way to guarantee that the
** KEEPALIVE mechanism of the Pegasus is met.
*/
#define BYPASS_KEEPALIVE 1

#define KEY_F1_DOWN 0x11
#define KEY_F2_DOWN 0x12
#define KEY_F3_DOWN 0x13
#define KEY_F1_UP 0x91
#define KEY_F2_UP 0x92
#define KEY_F3_UP 0x93

struct tt550_priv_data
{
  rmode_t tx_mode;		/* transmitter mode - may be different from receiver in split mode */
  rmode_t rx_mode;		/* Current RX Mode */
  freq_t tx_freq;		/* tuned transmitter frequency - may be different from
				 * 'freq' when in split mode
				 */
  freq_t rx_freq;		/* Current RX Frequency */
  shortfreq_t rit;		/* Current RIT Value */
  shortfreq_t xit;		/* Current XIT Value */
  shortfreq_t pbtadj;		/* Current pass band tuning value */
  pbwidth_t width;		/* filter bandwidth in Hz */
  pbwidth_t tx_width;		/* transmit filter bandwidth in Hz */
  int pb;			/* RX passband offset in Hz 0 [0-10000] */
  int cwbfo;			/* BFO frequency: 700 [0-2000Hz] */
  int tx_cwbfo;			/* TX_BFO frequency: 700 [0-2000Hz] */
  float lineout;			/* line-out volume: 30 [0..63] */
  float spkvol;			/* speaker volume: 30 [0..63] */
  int agc;			/* AGC: medium */
  float rflevel;		/* RF Gain Level: [0..255] */
  float sql;			/* Squelch: [0..255] */
  int att;			/* Attenuator level [0..1] */
  int keyspd;			/* Keyer speed: [0..255] */
  float nr;			/* Noise reduction: [0..1] */
  float autonotch;		/* Autonotch filter: [0..1] */
  float rfpower;		/* RF Power: [0..255] */
  float speechcomp;		/* Speech compressor: [0..127] */
  float voxgain;		/* Vox Gain: [0..255] */
  float voxdelay;		/* Vox delay: [0..255] */
  float antivox;		/* AntiVox gain: [0..255] */
  float mikegain;		/* Microphone gain: [0..15] */
  float bkindl;			/* CW QSK level */
  int split;			/* split - ON/OFF */
  shortfreq_t stepsize; 	/*current step size */
  int anf;			/* ANF on/off 1/0 */
  int en_nr;			/* Noise Reduction on/off 1/0 */
  int tuner;			/* automatic tuner on/off 1/0 */
  int vox;			/* VOX on/off 1/0 */

  /* calculated by tt550_tuning_factor_calc() */
  int ctf;			/* Coarse Tune Factor */
  int ftf;			/* Fine Tune Factor */
  int btf;			/* Bfo Tune Factor, btval is ignored by RX-320 in AM MODE */

  /* S-meter calibration data */
  cal_table_t str_cal;
};

int tt550_init (RIG * rig);
int tt550_cleanup (RIG * rig);
int tt550_trx_open (RIG * rig);
int tt550_reset(RIG * rig, reset_t reset);
int tt550_set_freq (RIG * rig, vfo_t vfo, freq_t freq);
int tt550_get_freq (RIG * rig, vfo_t vfo, freq_t * freq);
int tt550_set_mode (RIG * rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int tt550_get_mode (RIG * rig, vfo_t vfo, rmode_t * mode, pbwidth_t * width);
int tt550_set_level (RIG * rig, vfo_t vfo, setting_t level, value_t val);
int tt550_get_level (RIG * rig, vfo_t vfo, setting_t level, value_t * val);
int tt550_set_ptt (RIG * rig, vfo_t vfo, ptt_t ptt);
int tt550_get_ptt (RIG * rig, vfo_t vfo, ptt_t * ptt);
int tt550_decode_event (RIG * rig);
const char *tt550_get_info (RIG * rig);
int tt550_set_tx_freq (RIG * rig, vfo_t vfo, freq_t freq);
int tt550_get_tx_freq (RIG * rig, vfo_t vfo, freq_t * freq);
int tt550_set_rx_freq (RIG * rig, vfo_t vfo, freq_t freq);
int tt550_get_rx_freq (RIG * rig, vfo_t vfo, freq_t * freq);
int tt550_set_tx_mode (RIG * rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int tt550_get_tx_mode (RIG * rig, vfo_t vfo, rmode_t * mode, pbwidth_t * width);
int tt550_set_rx_mode (RIG * rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int tt550_get_rx_mode (RIG * rig, vfo_t vfo, rmode_t * mode, pbwidth_t * width);
int tt550_get_split_vfo (RIG * rig, vfo_t vfo, split_t * split, vfo_t * tx_vfo);
int tt550_set_split_vfo (RIG * rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
int tt550_get_tuning_step (RIG * rig, vfo_t vfo, shortfreq_t * stepsize);
int tt550_set_tuning_step (RIG * rig, vfo_t vfo, shortfreq_t stepsize);
int tt550_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int tt550_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int tt550_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int tt550_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int tt550_get_rit(RIG *rig, vfo_t vfo, shortfreq_t * rit);
int tt550_set_xit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int tt550_get_xit(RIG *rig, vfo_t vfo, shortfreq_t * rit);


#endif /* _TT550_H */
