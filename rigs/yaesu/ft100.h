/*
 * hamlib - (C) Frank Singleton 2000-2003 (vk3fcs@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2009
 *
 * ft100.h - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-100 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
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

#ifndef _FT100_H
#define _FT100_H 1

#define FT100_WRITE_DELAY                    0
#define FT100_POST_WRITE_DELAY               100     /* max is 200ms */
#define FT100_DEFAULT_READ_TIMEOUT           2000

enum ft100_native_cmd_e {

  FT100_NATIVE_CAT_LOCK_ON = 0,
  FT100_NATIVE_CAT_LOCK_OFF,
  FT100_NATIVE_CAT_PTT_ON,
  FT100_NATIVE_CAT_PTT_OFF,
  FT100_NATIVE_CAT_SET_FREQ,
  FT100_NATIVE_CAT_SET_MODE_LSB,
  FT100_NATIVE_CAT_SET_MODE_USB,
  FT100_NATIVE_CAT_SET_MODE_CW,
  FT100_NATIVE_CAT_SET_MODE_CWR,
  FT100_NATIVE_CAT_SET_MODE_AM,
  FT100_NATIVE_CAT_SET_MODE_FM,
  FT100_NATIVE_CAT_SET_MODE_DIG,
  FT100_NATIVE_CAT_SET_MODE_WFM,
  FT100_NATIVE_CAT_CLAR_ON,
  FT100_NATIVE_CAT_CLAR_OFF,
  FT100_NATIVE_CAT_SET_CLAR_FREQ,
  FT100_NATIVE_CAT_SET_VFOAB,
  FT100_NATIVE_CAT_SET_VFOA,
  FT100_NATIVE_CAT_SET_VFOB,
  FT100_NATIVE_CAT_SPLIT_ON,
  FT100_NATIVE_CAT_SPLIT_OFF,
  FT100_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
  FT100_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
  FT100_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
  FT100_NATIVE_CAT_SET_RPT_OFFSET,
/* fix me */
  FT100_NATIVE_CAT_SET_DCS_ON,
  FT100_NATIVE_CAT_SET_CTCSS_ENC_ON,
  FT100_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON,
  FT100_NATIVE_CAT_SET_CTCSS_DCS_OFF,
/* em xif */
  FT100_NATIVE_CAT_SET_CTCSS_FREQ,
  FT100_NATIVE_CAT_SET_DCS_CODE,
  FT100_NATIVE_CAT_GET_RX_STATUS,
  FT100_NATIVE_CAT_GET_TX_STATUS,
  FT100_NATIVE_CAT_GET_FREQ_MODE_STATUS,
  FT100_NATIVE_CAT_PWR_WAKE,
  FT100_NATIVE_CAT_PWR_ON,
  FT100_NATIVE_CAT_PWR_OFF,
  FT100_NATIVE_CAT_READ_STATUS,
  FT100_NATIVE_CAT_READ_METERS,
  FT100_NATIVE_CAT_READ_FLAGS,
  FT100_NATIVE_SIZE		/* end marker */
};


typedef enum ft100_native_cmd_e ft100_native_cmd_t;

/*
 *  we are able to get way more info
 *  than we can set
 *
 */
typedef struct
{
   unsigned char band_no;
   unsigned char freq[4];
   unsigned char mode;
   unsigned char ctcss;
   unsigned char dcs;
   unsigned char flag1;
   unsigned char flag2;
   unsigned char clarifier[2];
   unsigned char not_used;
   unsigned char step1;
   unsigned char step2;
   unsigned char filter;

   unsigned char stuffing[16];
}
 FT100_STATUS_INFO;


typedef struct
{
   unsigned char mic_switch_1;
   unsigned char tx_fwd_power;
   unsigned char tx_rev_power;
   unsigned char s_meter;
   unsigned char mic_level;
   unsigned char squelch_level;
   unsigned char mic_switch_2;
   unsigned char final_temp;
   unsigned char alc_level;
}
 FT100_METER_INFO;

typedef struct
{
   unsigned char byte[8];
}
FT100_FLAG_INFO;


struct ft100_priv_data {
  /* TODO: make use of cached data */
  FT100_STATUS_INFO status;
  FT100_FLAG_INFO flags;
};


static int ft100_init(RIG *rig);
static int ft100_open(RIG *rig);
static int ft100_cleanup(RIG *rig);
static int ft100_close(RIG *rig);

static int ft100_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft100_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft100_set_vfo(RIG *rig, vfo_t vfo);
static int ft100_get_vfo(RIG *rig, vfo_t *vfo);

static int ft100_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft100_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
#if 0
static int ft100_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);

static int ft100_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft100_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);

static int ft100_set_parm(RIG *rig, setting_t parm, value_t val);
static int ft100_get_parm(RIG *rig, setting_t parm, value_t *val);
#endif

static int ft100_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
static int ft100_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);

static int ft100_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
//static int ft100_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift);

static int ft100_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
//static int ft100_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code);

static int ft100_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
//static int ft100_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);

//static int ft100_get_info(RIG *rig, FT100_STATUS_INFO *ft100_status, FT100_METER_INFO *ft100_meter, FT100_FLAG_INFO *ft100_flags);

#endif /* _FT100_H */
