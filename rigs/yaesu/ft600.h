/*
 * hamlib - (C) Frank Singleton 2000-2003 (vk3fcs@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2009
 *
 * ft600.h -(C) Kārlis Millers YL3ALK 2019
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-600 using the "CAT" interface.
 * The starting point for this code was Chris Karpinsky's ft100 implementation.
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

#ifndef _FT600_H
#define _FT600_H 1

#define FT600_STATUS_UPDATE_DATA_LENGTH      19
#define FT600_METER_INFO_LENGTH              5

#define FT600_WRITE_DELAY                    5
#define FT600_POST_WRITE_DELAY               200
#define FT600_DEFAULT_READ_TIMEOUT           2000

enum ft600_native_cmd_e {

  FT600_NATIVE_CAT_LOCK_ON = 0,
  FT600_NATIVE_CAT_LOCK_OFF,
  FT600_NATIVE_CAT_PTT_ON,
  FT600_NATIVE_CAT_PTT_OFF,
  FT600_NATIVE_CAT_SET_FREQ,
  FT600_NATIVE_CAT_SET_MODE_LSB,
  FT600_NATIVE_CAT_SET_MODE_USB,
  FT600_NATIVE_CAT_SET_MODE_DIG,
  FT600_NATIVE_CAT_SET_MODE_CW,
  FT600_NATIVE_CAT_SET_MODE_AM,
  FT600_NATIVE_CAT_CLAR_ON,
  FT600_NATIVE_CAT_CLAR_OFF,
  FT600_NATIVE_CAT_SET_CLAR_FREQ,
  FT600_NATIVE_CAT_SET_VFOAB,
  FT600_NATIVE_CAT_SET_VFOA,
  FT600_NATIVE_CAT_SET_VFOB,
  FT600_NATIVE_CAT_SPLIT_ON,
  FT600_NATIVE_CAT_SPLIT_OFF,
  FT600_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
  FT600_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
  FT600_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
  FT600_NATIVE_CAT_SET_RPT_OFFSET,
/* fix me */
  FT600_NATIVE_CAT_SET_DCS_ON,
  FT600_NATIVE_CAT_SET_CTCSS_ENC_ON,
  FT600_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON,
  FT600_NATIVE_CAT_SET_CTCSS_DCS_OFF,
/* em xif */
  FT600_NATIVE_CAT_SET_CTCSS_FREQ,
  FT600_NATIVE_CAT_SET_DCS_CODE,
  FT600_NATIVE_CAT_GET_RX_STATUS,
  FT600_NATIVE_CAT_GET_TX_STATUS,
  FT600_NATIVE_CAT_GET_FREQ_MODE_STATUS,
  FT600_NATIVE_CAT_PWR_WAKE,
  FT600_NATIVE_CAT_PWR_ON,
  FT600_NATIVE_CAT_PWR_OFF,
  FT600_NATIVE_CAT_READ_STATUS,
  FT600_NATIVE_CAT_READ_METERS,
  FT600_NATIVE_CAT_READ_FLAGS
};


typedef enum ft600_native_cmd_e ft600_native_cmd_t;

/*
 *  we are able to get way more info
 *  than we can set
 *
 */
typedef struct
{
   unsigned char band_no;
   unsigned char freq[16];
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
 FT600_STATUS_INFO;


typedef struct
{
   unsigned char byte[8];
}
FT600_FLAG_INFO;


struct ft600_priv_data {
  FT600_STATUS_INFO status;
  FT600_FLAG_INFO flags;
  unsigned char s_meter;

};


static int ft600_init(RIG *rig);
static int ft600_open(RIG *rig);
static int ft600_cleanup(RIG *rig);
static int ft600_close(RIG *rig);

static int ft600_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft600_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft600_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft600_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft600_get_vfo(RIG *rig, vfo_t *vfo);

static int ft600_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft600_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

#endif /* _FT600_H */
