/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft817.h - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-817 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 *
 *    $Id: ft817.h,v 1.4 2005-04-03 19:27:59 fillods Exp $  
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

#ifndef _FT817_H
#define _FT817_H 1

#define FT817_WRITE_DELAY                    0
#define FT817_POST_WRITE_DELAY               300
#define FT817_DEFAULT_READ_TIMEOUT           2000

enum ft817_native_cmd_e {

  FT817_NATIVE_CAT_LOCK_ON = 0,
  FT817_NATIVE_CAT_LOCK_OFF,
  FT817_NATIVE_CAT_PTT_ON,
  FT817_NATIVE_CAT_PTT_OFF,
  FT817_NATIVE_CAT_SET_FREQ,
  FT817_NATIVE_CAT_SET_MODE_LSB,
  FT817_NATIVE_CAT_SET_MODE_USB,
  FT817_NATIVE_CAT_SET_MODE_CW,
  FT817_NATIVE_CAT_SET_MODE_CWR,
  FT817_NATIVE_CAT_SET_MODE_AM,
  FT817_NATIVE_CAT_SET_MODE_FM,
  FT817_NATIVE_CAT_SET_MODE_DIG,
  FT817_NATIVE_CAT_SET_MODE_PKT,
  FT817_NATIVE_CAT_CLAR_ON,
  FT817_NATIVE_CAT_CLAR_OFF,
  FT817_NATIVE_CAT_SET_CLAR_FREQ,
  FT817_NATIVE_CAT_SET_VFOAB,
  FT817_NATIVE_CAT_SPLIT_ON,
  FT817_NATIVE_CAT_SPLIT_OFF,
  FT817_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
  FT817_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
  FT817_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
  FT817_NATIVE_CAT_SET_RPT_OFFSET,
/* fix me */
  FT817_NATIVE_CAT_SET_DCS_ON,
  FT817_NATIVE_CAT_SET_CTCSS_ENC_ON,
  FT817_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON,
  FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF,
/* em xif */
  FT817_NATIVE_CAT_SET_CTCSS_FREQ,
  FT817_NATIVE_CAT_SET_DCS_CODE,
  FT817_NATIVE_CAT_GET_RX_STATUS,
  FT817_NATIVE_CAT_GET_TX_STATUS,
  FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS,
  FT817_NATIVE_CAT_PWR_WAKE,
  FT817_NATIVE_CAT_PWR_ON,
  FT817_NATIVE_CAT_PWR_OFF,
  FT817_NATIVE_SIZE		/* end marker */
};


typedef enum ft817_native_cmd_e ft817_native_cmd_t;


struct ft817_priv_data {
  unsigned char current_vfo;
  unsigned char p_cmd[YAESU_CMD_LENGTH];
  yaesu_cmd_set_t pcs[FT817_NATIVE_SIZE];  /* TODO:  why? */
  unsigned char rx_status;
  unsigned char tx_status;
  unsigned char freq_mode_status;
};


static int ft817_init(RIG *rig);
static int ft817_open(RIG *rig);
static int ft817_cleanup(RIG *rig);
static int ft817_close(RIG *rig);
static int ft817_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft817_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft817_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft817_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int ft817_set_vfo(RIG *rig, vfo_t vfo);
static int ft817_get_vfo(RIG *rig, vfo_t *vfo);
static int ft817_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
#if 0
static int ft817_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft817_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int ft817_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ft817_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft817_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int ft817_set_parm(RIG *rig, setting_t parm, value_t val);
static int ft817_get_parm(RIG *rig, setting_t parm, value_t *val);
#endif

#endif /* _FT817_H */









