/*
 *  Hamlib Kenwood backend - TS2000 description
 *  Copyright (c) 2000-2011 by Stephane Fillod
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

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>

#include <hamlib/rig.h>
#include "kenwood.h"

#define TS2000_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS2000_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS2000_AM_TX_MODES RIG_MODE_AM

#define TS2000_FUNC_ALL (RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_BC|RIG_FUNC_NB|RIG_FUNC_NR|RIG_FUNC_ANF|RIG_FUNC_COMP|RIG_FUNC_RIT|RIG_FUNC_XIT)

#define TS2000_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_COMP|RIG_LEVEL_AGC|RIG_LEVEL_BKINDL|RIG_LEVEL_METER|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define TS2000_MAINVFO (RIG_VFO_A|RIG_VFO_B)
#define TS2000_SUBVFO (RIG_VFO_C)

#define TS2000_VFO_OP (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN)
#define TS2000_SCAN_OP (RIG_SCAN_VFO)
#define TS2000_ANTS (RIG_ANT_1|RIG_ANT_2)

#define TS2000_STR_CAL {9, {\
               {0x00, -54},\
               {0x03, -48},\
               {0x06, -36},\
               {0x09, -24},\
               {0x0C, -12},\
               {0x0F,   0},\
               {0x14,  20},\
               {0x19,  40},\
               {0x1E,  60}}\
               }

/* prototypes */
static int ts2000_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ts2000_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                              int read_only);
static int ts2000_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);

/*
 * 38 CTCSS sub-audible tones + 1750 tone
 */
tone_t ts2000_ctcss_list[] =
{
    670,  719,  744,  770,  797,  825,  854,  885,  915,  948,
    974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
    1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862,
    1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503, 17500,
    0,
};



/*
 * 103 available DCS codes
 */
tone_t ts2000_dcs_list[] =
{
    23,  25,  26,  31,   32,  36,  43,  47,       51,  53,
    54,  65,  71,  72,  73,   74, 114, 115, 116, 122, 125, 131,
    132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205,
    212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261,
    263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343,
    346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432,
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516,
    523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654,
    662, 664, 703, 712, 723, 731, 732, 734, 743, 754,
    0,
};

static struct kenwood_priv_caps  ts2000_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/* memory capabilities */
#define TS2000_MEM_CAP {    \
    .freq = 1,      \
    .mode = 1,      \
    .tx_freq=1,     \
    .tx_mode=1,     \
    .split=1,       \
    .rptr_shift=1,      \
    .rptr_offs=1,       \
    .funcs=RIG_FUNC_REV|RIG_FUNC_TONE|RIG_FUNC_TSQL,\
    .tuning_step=1, \
    .ctcss_tone=1,      \
    .ctcss_sql=1,       \
    .dcs_code=1,        \
    .dcs_sql=1,     \
    .scan_group=1,  \
    .flags=1,       \
    .channel_desc=1     \
}


/*
 * ts2000 rig capabilities.
 *
 * part of infos comes from http://www.kenwood.net/
 */
const struct rig_caps ts2000_caps =
{
    RIG_MODEL(RIG_MODEL_TS2000),
    .model_name = "TS-2000",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0, /* ms */
    .timeout =  200,
    .retry =  10,

    .has_get_func =  TS2000_FUNC_ALL,
    .has_set_func =  TS2000_FUNC_ALL,
    .has_get_level =  TS2000_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TS2000_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .vfo_ops =  TS2000_VFO_OP,
    .scan_ops =  TS2000_SCAN_OP,
    .ctcss_list =  ts2000_ctcss_list,
    .dcs_list =  ts2000_dcs_list,
    .preamp =   { 20, RIG_DBLST_END, }, /* FIXME: real preamp? */
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  kHz(20),
    .max_xit =  kHz(20),
    .max_ifshift =  kHz(1),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .agc_level_count = 5,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST, RIG_AGC_ON },
    .bank_qty =   0,
    .chan_desc_sz =  7,

    .chan_list =  {
        { 0, 299, RIG_MTYPE_MEM, TS2000_MEM_CAP  },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(300), MHz(60), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO, TS2000_ANTS},
        {MHz(144), MHz(146), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO},
        {MHz(430), MHz(440), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO},
        {MHz(144), MHz(146), TS2000_ALL_MODES, -1, -1, TS2000_SUBVFO},
        {MHz(430), MHz(440), TS2000_ALL_MODES, -1, -1, TS2000_SUBVFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {kHz(1830), kHz(1850), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(1830), kHz(1850), TS2000_AM_TX_MODES, 2000, 25000, TS2000_MAINVFO, TS2000_ANTS},
        {kHz(3500), kHz(3800), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(3500), kHz(3800), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(7), kHz(7100), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(7), kHz(7100), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(10.1), MHz(10.15), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(10.1), MHz(10.15), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(14), kHz(14350), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(14), kHz(14350), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(18068), kHz(18168), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(18068), kHz(18168), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(21), kHz(21450), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(21), kHz(21450), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(24890), kHz(24990), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(24890), kHz(24990), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(28), kHz(29700), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(28), kHz(29700), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(50), MHz(50.2), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(50), MHz(50.2), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(144), MHz(146), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO},
        {MHz(144), MHz(146), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO},
        {MHz(430), MHz(440), TS2000_OTHER_TX_MODES, W(5), W(50), TS2000_MAINVFO},
        {MHz(430), MHz(440), TS2000_AM_TX_MODES, W(5), W(12.5), TS2000_MAINVFO},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(300), MHz(60), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO, TS2000_ANTS},
        {MHz(142), MHz(152), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO},
        {MHz(420), MHz(450), TS2000_ALL_MODES, -1, -1, TS2000_MAINVFO},
        {MHz(118), MHz(174), TS2000_ALL_MODES, -1, -1, TS2000_SUBVFO},
        {MHz(220), MHz(512), TS2000_ALL_MODES, -1, -1, TS2000_SUBVFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(1800), MHz(2), TS2000_AM_TX_MODES, 2000, 25000, TS2000_MAINVFO, TS2000_ANTS},
        {kHz(3500), MHz(4), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(3500), MHz(4), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(7), kHz(7300), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(7), kHz(7300), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(10.1), MHz(10.15), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(10.1), MHz(10.15), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(14), kHz(14350), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(14), kHz(14350), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(18068), kHz(18168), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(18068), kHz(18168), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(21), kHz(21450), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(21), kHz(21450), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(24890), kHz(24990), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {kHz(24890), kHz(24990), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(28), kHz(29700), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(28), kHz(29700), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(50), MHz(54), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(50), MHz(54), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO, TS2000_ANTS},
        {MHz(144), MHz(148), TS2000_OTHER_TX_MODES, W(5), W(100), TS2000_MAINVFO},
        {MHz(144), MHz(148), TS2000_AM_TX_MODES, W(5), W(25), TS2000_MAINVFO},
        {MHz(430), MHz(450), TS2000_OTHER_TX_MODES, W(5), W(50), TS2000_MAINVFO},
        {MHz(430), MHz(450), TS2000_AM_TX_MODES, W(5), W(12.5), TS2000_MAINVFO},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, 1},
        {TS2000_ALL_MODES, 10},
        {TS2000_ALL_MODES, 100},
        {TS2000_ALL_MODES, kHz(1)},
        {TS2000_ALL_MODES, kHz(2.5)},
        {TS2000_ALL_MODES, kHz(5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(6.25)},
        {TS2000_ALL_MODES, kHz(10)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(15)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(20)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(25)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(30)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(50)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(100)},
        {TS2000_ALL_MODES, MHz(1)},
        {TS2000_ALL_MODES, 0}, /* any tuning step */
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_CW, Hz(600)},
        {RIG_MODE_RTTY, Hz(1500)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(12)},
        RIG_FLT_END,
    },

    .str_cal = TS2000_STR_CAL,

    .priv = (void *)& ts2000_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  kenwood_set_xit,
    .get_xit =  kenwood_get_xit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .set_ctcss_tone =  kenwood_set_ctcss_tone_tn,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .set_ctcss_sql =  kenwood_set_ctcss_sql,
    .get_ctcss_sql =  kenwood_get_ctcss_sql,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  kenwood_set_func,
    .get_func =  kenwood_get_func,
    .set_level =  kenwood_set_level,
    .get_level =  ts2000_get_level,
    .set_ant =  kenwood_set_ant,
    .get_ant =  kenwood_get_ant,
    .send_morse =  kenwood_send_morse,
    .wait_morse = rig_wait_morse,
    .vfo_op =  kenwood_vfo_op,
    .scan =  kenwood_scan,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .get_channel = ts2000_get_channel,
    .set_channel = ts2000_set_channel,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .set_powerstat =  kenwood_set_powerstat,
    .get_powerstat =  kenwood_get_powerstat,
    .get_info =  kenwood_get_info,
    .reset =  kenwood_reset,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

/*
 * ts2000_get_channel
 * Read command format:
   M|R|P1|P2|P3|P3|;|
 * P1: 0 - RX frequency, 1 - TX frequency
     Memory channel 290 ~ 299: P1=0 (start frequency), P1=1 (end frequency)
   P2 - bank number
        allowed values: <space>, 0, 1 or 2
   P3 - channel number 00-99

   Returned value:
    M | R |P1 |P2 |P3 |P3 |P4 |P4 |P4 |P4 |
   P4 |P4 |P4 |P4 |P4 |P4 |P4 |P5 |P6 |P7 |
   P8 |P8 |P9 |P9 |P10|P10|P10|P11|P12|P13|
   P13|P13|P13|P13|P13|P13|P13|P13|P14|P14|
   P15|P16|P16|P16|P16|P16|P16|P16|P16| ; |
   P1 - P3 described above
   P4: Frequency in Hz (11-digit).
   P5: Mode. 1: LSB, 2: USB, 3: CW, 4: FM, 5: AM, 6: FSK, 7: CR-R, 8: Reserved, 9: FSK-R
   P6: Lockout status. 0: Lockout OFF, 1: Lockout ON.
   P7: 0: OFF, 1: TONE, 2: CTCSS, 3: DCS.
   P8: Tone Number. Allowed values 01 (67Hz) - 38 (250.3Hz)
   P9: CTCSS tone number. Allowed values 01 (67Hz) - 38 (250.3Hz)
  P10: DCS code. Allowed values  000 (023 DCS code) to 103 (754 DCS code).
  P11: REVERSE status.
  P12: SHIFT status. 0: Simplex, 1: +, 2: –, 3: = (All E-types)
  P13: Offset frequency in Hz (9-digit).
      Allowed values 000000000 - 059950000 in steps of 50000. Unused digits must be 0.
  P14: Step size. Allowed values:
      for SSB, CW, FSK mode: 00 - 03
      00: 1 kHz, 01: 2.5 kHz, 02: 5 kHz, 03: 10 kHz
      for AM, FM mode: 00 - 09
      00: 5 kHz, 01: 6.25 kHz, 02: 10 kHz, 03: 12.5 kHz, 04: 15 kHz,
      05: 20 kHz, 06: 25 kHz, 07: 30 kHz,  08: 50 kHz, 09: 100 kHz
  P15: Memory Group number (0 ~ 9).
  P16: Memory name. A maximum of 8 characters.

 */

int ts2000_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    int err;
    int tmp;
    size_t length;
    char buf[52];
    char cmd[8];
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!chan || chan->vfo != RIG_VFO_MEM)
    {
        return -RIG_EINVAL;
    }

    /* put channel num in the command string */
    SNPRINTF(cmd, sizeof(cmd), "MR0%03d;", chan->channel_num);

    err = kenwood_transaction(rig, cmd, buf, sizeof(buf));

    if (err != RIG_OK)
    {
        return err;
    }

    length = strlen(buf);
    memset(chan, 0x00, sizeof(channel_t));

    chan->vfo = RIG_VFO_MEM;

    /* parse from right to left */

    /* XXX based on the available documentation, there is no command
     * to read out the filters of a given memory channel. The rig, however,
     * stores this information.
     */
    /* First check if a name is assigned.
       Name is returned at positions 41-48 (counting from 0) */
    if (length > 41)
    {
//    rig_debug(RIG_DEBUG_VERBOSE, "Copying channel description: %s\n", &buf[ 41 ] );
        strcpy(chan->channel_desc, &buf[ 41 ]);
    }

    /* Memory group no */
    chan->scan_group = buf[ 40 ] - '0';
    /* Fields 38-39 contain tuning step as a number 00 - 09.
       Tuning step depends on this number and the mode,
       just save it for now */
    buf[ 40 ] = '\0';
    tmp = atoi(&buf[ 38]);
    /* Offset frequency */
    buf[ 38 ] = '\0';
    chan->rptr_offs = atoi(&buf[ 29 ]);

    /* Shift type
       WARNING: '=' shift type not programmed */
    if (buf[ 28 ] == '1')
    {
        chan->rptr_shift = RIG_RPT_SHIFT_PLUS;
    }
    else
    {
        if (buf[ 28 ] == '2')
        {
            chan->rptr_shift = RIG_RPT_SHIFT_MINUS;
        }
        else
        {
            chan->rptr_shift =  RIG_RPT_SHIFT_NONE;
        }
    }

    /* Reverse status */
    if (buf[27] == '1')
    {
        chan->funcs |= RIG_FUNC_REV;
    }

    /* Check for tone, CTCSS and DCS */
    /* DCS code first */
    if (buf[ 19 ] == '3')
    {
        if (rig->caps->dcs_list)
        {
            buf[ 27 ] = '\0';
            chan->dcs_code = rig->caps->dcs_list[ atoi(&buf[ 24 ]) ];
            chan->dcs_sql = chan->dcs_code;
            chan->ctcss_tone = 0;
            chan->ctcss_sql = 0;
        }
    }
    else
    {
        chan->dcs_code = 0;
        chan->dcs_sql = 0;
        /* CTCSS code
           Caution, CTCSS codes, unlike DCS codes, are numbered from 1! */
        buf[ 24 ] = '\0';

        if (buf[ 19 ] == '2')
        {
            chan->funcs |= RIG_FUNC_TSQL;

            if (rig->caps->ctcss_list)
            {
                chan->ctcss_sql = rig->caps->ctcss_list[ atoi(&buf[22]) - 1 ];
                chan->ctcss_tone = 0;
            }
        }
        else
        {
            chan->ctcss_sql = 0;

            /* CTCSS tone */
            if (buf[ 19 ] == '1')
            {
                chan->funcs |= RIG_FUNC_TONE;
                buf[ 22 ] = '\0';

                if (rig->caps->ctcss_list)
                {
                    chan->ctcss_tone = rig->caps->ctcss_list[ atoi(&buf[20]) - 1 ];
                }
            }
            else
            {
                chan->ctcss_tone = 0;
            }
        }
    }


    /* memory lockout */
    if (buf[18] == '1')
    {
        chan->flags |= RIG_CHFLAG_SKIP;
    }

    /* mode */
    chan->mode = kenwood2rmode(buf[17] - '0', caps->mode_table);

    /* Now we have the mode, let's finish the tuning step */
    if ((chan->mode == RIG_MODE_AM) || (chan->mode == RIG_MODE_FM))
    {
        switch (tmp)
        {
        case 0: chan->tuning_step = kHz(5); break;

        case 1: chan->tuning_step = kHz(6.25); break;

        case 2: chan->tuning_step = kHz(10); break;

        case 3: chan->tuning_step = kHz(12.5); break;

        case 4: chan->tuning_step = kHz(15); break;

        case 5: chan->tuning_step = kHz(20); break;

        case 6: chan->tuning_step = kHz(25); break;

        case 7: chan->tuning_step = kHz(30); break;

        case 8: chan->tuning_step = kHz(50); break;

        case 9: chan->tuning_step = kHz(100); break;

        default: chan->tuning_step = 0;
        }
    }
    else
    {
        switch (tmp)
        {
        case 0: chan->tuning_step = kHz(1); break;

        case 1: chan->tuning_step = kHz(2.5); break;

        case 2: chan->tuning_step = kHz(5); break;

        case 3: chan->tuning_step = kHz(10); break;

        default: chan->tuning_step = 0;
        }
    }

    /* Frequency */
    buf[17] = '\0';
    chan->freq = atoi(&buf[6]);

    if (chan->freq == RIG_FREQ_NONE)
    {
        return -RIG_ENAVAIL;
    }

    buf[6] = '\0';
    chan->channel_num = atoi(&buf[3]);


    /* Check split freq */
    cmd[2] = '1';
    err = kenwood_transaction(rig, cmd, buf, sizeof(buf));

    if (err != RIG_OK)
    {
        return err;
    }

    chan->tx_mode = kenwood2rmode(buf[17] - '0', caps->mode_table);

    buf[17] = '\0';
    chan->tx_freq = atoi(&buf[6]);

    if (chan->freq == chan->tx_freq)
    {
        chan->tx_freq = RIG_FREQ_NONE;
        chan->tx_mode = RIG_MODE_NONE;
        chan->split = RIG_SPLIT_OFF;
    }
    else
    {
        chan->split = RIG_SPLIT_ON;
    }

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

int ts2000_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char sqltype = '0';
    char buf[128];
    char mode, tx_mode = 0;
    char shift = '0';
    short dcscode = 0;
    short code = 0;
    int tstep = 0;
    int err;
    int tone = 0;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!chan)
    {
        return -RIG_EINVAL;
    }

    mode = rmode2kenwood(chan->mode, caps->mode_table);

    if (mode < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(chan->mode));
        return -RIG_EINVAL;
    }

    if (chan->split == RIG_SPLIT_ON)
    {
        tx_mode = rmode2kenwood(chan->tx_mode, caps->mode_table);

        if (tx_mode < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                      __func__, rig_strrmode(chan->tx_mode));
            return -RIG_EINVAL;
        }
    }

    /* find tone */

    if (chan->ctcss_tone)
    {
        for (; rig->caps->ctcss_list[tone] != 0; tone++)
        {
            if (chan->ctcss_tone == rig->caps->ctcss_list[tone])
            {
                break;
            }
        }

        if (chan->ctcss_tone != rig->caps->ctcss_list[tone]) { tone = -1; }
        else { sqltype = '1'; }
    }
    else
    {
        tone = -1; /* -1 because we will add 1 when outputting; this is necessary as CTCSS codes are numbered from 1 */
    }

    /* find CTCSS code */

    if (chan->ctcss_sql)
    {
        for (; rig->caps->ctcss_list[code] != 0; code++)
        {
            if (chan->ctcss_sql == rig->caps->ctcss_list[code])
            {
                break;
            }
        }

        if (chan->ctcss_sql != rig->caps->ctcss_list[code]) { code = -1; }
        else { sqltype = '2'; }
    }
    else
    {
        code = -1;
    }

    /* find DCS code */

    if (chan->dcs_code)
    {
        for (; rig->caps->dcs_list[dcscode] != 0; dcscode++)
        {
            if (chan->dcs_code == rig->caps->dcs_list[dcscode])
            {
                break;
            }
        }

        if (chan->dcs_code != rig->caps->dcs_list[dcscode]) { dcscode = 0; }
        else { sqltype = '3'; }
    }
    else
    {
        dcscode = 0;
    }

    if (chan->rptr_shift == RIG_RPT_SHIFT_PLUS)
    {
        shift = '1';
    }

    if (chan->rptr_shift ==  RIG_RPT_SHIFT_MINUS)
    {
        shift = '2';
    }


    if ((chan->mode == RIG_MODE_AM) || (chan->mode == RIG_MODE_FM))
    {
        switch (chan->tuning_step)
        {
        case s_kHz(6.25):   tstep = 1; break;

        case s_kHz(10):     tstep = 2; break;

        case s_kHz(12.5):   tstep = 3; break;

        case s_kHz(15):     tstep = 4; break;

        case s_kHz(20):     tstep = 5; break;

        case s_kHz(25):     tstep = 6; break;

        case s_kHz(30):     tstep = 7; break;

        case s_kHz(50):     tstep = 8; break;

        case s_kHz(100):    tstep = 9; break;

        default:            tstep = 0;
        }
    }
    else
    {
        switch (chan->tuning_step)
        {
        case s_kHz(2.5):    tstep = 1; break;

        case s_kHz(5):      tstep = 2; break;

        case s_kHz(10):     tstep = 3; break;

        default:            tstep = 0;
        }
    }

    /* P-number       2-3    4 5 6 7   8   9  101112  13 141516  */
    SNPRINTF(buf, sizeof(buf), "MW0%03d%011u%c%c%c%02d%02d%03d%c%c%09d0%c%c%s;",
             chan->channel_num,
             (unsigned) chan->freq,      /*  4 - frequency */
             '0' + mode,                 /*  5 - mode */
             (chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',    /*  6 - lockout status */
             sqltype,                    /*  7 - squelch and tone type */
             tone + 1,                   /*  8 - tone code */
             code + 1,                   /*  9 - CTCSS code */
             dcscode,                    /* 10 - DCS code */
             (chan->funcs & RIG_FUNC_REV) ? '1' : '0', /* 11 - Reverse status */
             shift,                      /* 12 - shift type */
             (int) chan->rptr_offs,              /* 13 - offset frequency */
             tstep + '0',                        /* 14 - Step size */
             chan->scan_group + '0',         /* 15 - Memory group no */
             chan->channel_desc              /* 16 - description */
            );
    rig_debug(RIG_DEBUG_VERBOSE, "The command will be: %s\n", buf);

    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    if (chan->split == RIG_SPLIT_ON)
    {
        SNPRINTF(buf, sizeof(buf), "MW1%03d%011u%c%c%c%02d%02d%03d%c%c%09d0%c%c%s;\n",
                 chan->channel_num,
                 (unsigned) chan->tx_freq,       /*  4 - frequency */
                 '0' + tx_mode,                  /*  5 - mode */
                 (chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',    /*  6 - lockout status */
                 sqltype,                    /*  7 - squelch and tone type */
                 tone + 1,                   /*  8 - tone code */
                 code + 1,                   /*  9 - CTCSS code */
                 dcscode + 1,                /* 10 - DCS code */
                 (chan->funcs & RIG_FUNC_REV) ? '1' : '0', /* 11 - Reverse status */
                 shift,                      /* 12 - shift type */
                 (int) chan->rptr_offs,              /* 13 - offset frequency */
                 tstep + '0',                        /* 14 - Step size */
                 chan->scan_group + '0',         /* Memory group no */
                 chan->channel_desc              /* 16 - description */
                );
        rig_debug(RIG_DEBUG_VERBOSE, "Split, the command will be: %s\n", buf);

        err = kenwood_transaction(rig, buf, NULL, 0);
    }

    return err;
}

/*
 * ts2000_get_level
 * Assumes rig!=NULL, val!=NULL
 */

int ts2000_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[50];
    size_t lvl_len;
    int lvl, retval, ret, agclevel;

    lvl_len = 50;

    switch (level)
    {

    case RIG_LEVEL_PREAMP:
        retval = kenwood_transaction(rig, "PA", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if ((lvl_len != 4))  /*TS-2000 returns 4 chars for PA; */
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n",
                      __func__, (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);

        if (lvl < 10) /* just checking for main receiver preamp setting */
        {
            val->i = 0;
        }

        if (lvl > 9)
        {
            val->i = rig->state.preamp[0];
        }

        break;

    case RIG_LEVEL_ATT:
        retval = kenwood_transaction(rig, "RA", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if ((lvl_len != 6))  /* TS-2000 returns 6 chars for RA; */
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);

        if (lvl < 100)  /* just checking main band attenuator */
        {
            val->i = 0;
        }

        if (lvl > 99)
        {
            val->i = rig->state.attenuator[0];    /* Since the TS-2000 only has one step on the attenuator */
        }

        break;

    case RIG_LEVEL_VOXDELAY:
        retval = kenwood_transaction(rig, "VD", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->i = lvl / 100;
        break;

    case RIG_LEVEL_AF:
        return kenwood_get_level(rig, vfo, level, val);

    case RIG_LEVEL_RF:
        retval = kenwood_transaction(rig, "RG", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = lvl / 255.0;
        break;

    case RIG_LEVEL_SQL:
        retval = kenwood_transaction(rig, "SQ0", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 3, "%d", &lvl);
        val->f = lvl / 255.0;
        break;

    case RIG_LEVEL_CWPITCH:
        retval = kenwood_transaction(rig, "EX0310000", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 15)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d answer=%s\n", __func__,
                      (int)lvl_len, lvlbuf);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 8, "%d", &lvl);
        val->i = 400 + (50 * lvl);
        break;

    case RIG_LEVEL_RFPOWER:
        retval = kenwood_transaction(rig, "PC", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 3, "%d", &lvl);
        val->f = lvl / 100.0; /* FIXME: for 1.2GHZ need to divide by 10 */
        break;

    case RIG_LEVEL_MICGAIN:
        return kenwood_get_level(rig, vfo, level, val);

    case RIG_LEVEL_KEYSPD:
        retval = kenwood_transaction(rig, "KS", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->i = lvl;
        break;

    case RIG_LEVEL_NOTCHF:
        return -RIG_ENIMPL;
        break;

    case RIG_LEVEL_COMP:
        retval = kenwood_transaction(rig, "PL", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 8)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        lvl = lvl / 1000;
        val->f = lvl / 100.0;
        break;


    case RIG_LEVEL_AGC: /* FIX ME: ts2000 returns 0 -20 for AGC */
        ret = get_kenwood_level(rig, "GT", &val->f, NULL);
        agclevel = 255.0 * val->f;

        if (agclevel == 0) { val->i = 0; }
        else if (agclevel < 85) { val->i = 1; }
        else if (agclevel < 170) { val->i = 2; }
        else if (agclevel <= 255) { val->i = 3; }

        return ret;
        break;

    case RIG_LEVEL_BKINDL:
        retval = kenwood_transaction(rig, "SD", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->i = lvl / 100;
        break;

    case RIG_LEVEL_BALANCE:
        return -RIG_ENIMPL;
        break;

    case RIG_LEVEL_METER:
        retval = kenwood_transaction(rig, "RM", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 7)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->i = lvl / 10000;
        break;

    case RIG_LEVEL_VOXGAIN:
        retval = kenwood_transaction(rig, "VG", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = lvl / 9.0;
        break;

    case RIG_LEVEL_ANTIVOX:
        return -RIG_ENIMPL;
        break;

    case RIG_LEVEL_RAWSTR:
    case RIG_LEVEL_STRENGTH:
        retval = kenwood_transaction(rig, "SM0", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (((lvl_len != 7)) || lvlbuf[1] != 'M')
        {
            /* TS-2000 returns 8 bytes for S meter level */
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n", __func__, (int)lvl_len);
            return -RIG_ERJCTED;
        }

        /* Frontend expects:  -54 = S0, 0 = S9  */
        sscanf(lvlbuf + 3, "%d", &val->i);

        /* TS-2000 main receiver returns values from 0 - 30 */
        /* so scale the value */
        if (level == RIG_LEVEL_STRENGTH)
        {
            val->i = (val->i * 3.6) - 54;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}
