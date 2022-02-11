/*
 *  Hamlib PiHPSDR backend - TS-2000 Emulation (derived from ts2000.c)
 *  Copyright (c) 2017 by Jae Stutzman
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
#include "tones.h"


#define PIHPSDR_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define PIHPSDR_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define PIHPSDR_AM_TX_MODES RIG_MODE_AM

#define PIHPSDR_FUNC_ALL (RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_BC|RIG_FUNC_NB|RIG_FUNC_NR|RIG_FUNC_ANF|RIG_FUNC_COMP)

#define PIHPSDR_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_VOXDELAY|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_COMP|RIG_LEVEL_AGC|RIG_LEVEL_BKINDL|RIG_LEVEL_METER|RIG_LEVEL_VOXGAIN|RIG_LEVEL_ANTIVOX|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define PIHPSDR_MAINVFO (RIG_VFO_A|RIG_VFO_B)
#define PIHPSDR_SUBVFO (RIG_VFO_C)

#define PIHPSDR_VFO_OP (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN)
#define PIHPSDR_SCAN_OP (RIG_SCAN_VFO)
#define PIHPSDR_ANTS (RIG_ANT_1|RIG_ANT_2)

#define PIHPSDR_STR_CAL {9, {\
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
static int pihpsdr_open(RIG *rig);
static int pihpsdr_get_level(RIG *rig, vfo_t vfo, setting_t level,
                             value_t *val);
static int pihpsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int pihspdr_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                               int read_only);
static int pihspdr_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);


static struct kenwood_priv_caps  ts2000_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/* memory capabilities */
#define PIHPSDR_MEM_CAP {   \
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
 * PiHPSDR rig capabilities. (Emulates Kenwood TS-2000)
 */
const struct rig_caps pihpsdr_caps =
{
    RIG_MODEL(RIG_MODEL_HPSDR),
    .model_name = "PiHPSDR",
    .mfg_name =  "OpenHPSDR",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 38400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,    /* ms */
    .timeout =  500,
    .retry =  1,
    .has_get_func =  PIHPSDR_FUNC_ALL,
    .has_set_func =  PIHPSDR_FUNC_ALL,
    .has_get_level =  PIHPSDR_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(PIHPSDR_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .vfo_ops =  PIHPSDR_VFO_OP,
    .scan_ops =  PIHPSDR_SCAN_OP,
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { 20, RIG_DBLST_END, }, /* FIXME: real preamp? */
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  kHz(20),
    .max_xit =  kHz(20),
    .max_ifshift =  kHz(1),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  7,

    .chan_list =  {
        { 0, 299, RIG_MTYPE_MEM, PIHPSDR_MEM_CAP  },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(300), MHz(60), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(144), MHz(146), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_MAINVFO},
        {MHz(430), MHz(440), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_MAINVFO},
        {MHz(144), MHz(146), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_SUBVFO},
        {MHz(430), MHz(440), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_SUBVFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {kHz(1830), kHz(1850), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(1830), kHz(1850), PIHPSDR_AM_TX_MODES, 2000, 25000, PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(3500), kHz(3800), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(3500), kHz(3800), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(7), kHz(7100), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(7), kHz(7100), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(10.1), MHz(10.15), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(10.1), MHz(10.15), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(14), kHz(14350), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(14), kHz(14350), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(18068), kHz(18168), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(18068), kHz(18168), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(21), kHz(21450), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(21), kHz(21450), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(24890), kHz(24990), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(24890), kHz(24990), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(28), kHz(29700), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(28), kHz(29700), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(50), MHz(50.2), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(50), MHz(50.2), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(144), MHz(146), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO},
        {MHz(144), MHz(146), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO},
        {MHz(430), MHz(440), PIHPSDR_OTHER_TX_MODES, W(5), W(50), PIHPSDR_MAINVFO},
        {MHz(430), MHz(440), PIHPSDR_AM_TX_MODES, W(5), W(12.5), PIHPSDR_MAINVFO},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(300), MHz(60), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(142), MHz(152), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_MAINVFO},
        {MHz(420), MHz(450), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_MAINVFO},
        {MHz(118), MHz(174), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_SUBVFO},
        {MHz(220), MHz(512), PIHPSDR_ALL_MODES, -1, -1, PIHPSDR_SUBVFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(1800), MHz(2), PIHPSDR_AM_TX_MODES, 2000, 25000, PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(3500), MHz(4), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(3500), MHz(4), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(7), kHz(7300), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(7), kHz(7300), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(10.1), MHz(10.15), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(10.1), MHz(10.15), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(14), kHz(14350), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(14), kHz(14350), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(18068), kHz(18168), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(18068), kHz(18168), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(21), kHz(21450), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(21), kHz(21450), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(24890), kHz(24990), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {kHz(24890), kHz(24990), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(28), kHz(29700), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(28), kHz(29700), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(50), MHz(54), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(50), MHz(54), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO, PIHPSDR_ANTS},
        {MHz(144), MHz(148), PIHPSDR_OTHER_TX_MODES, W(5), W(100), PIHPSDR_MAINVFO},
        {MHz(144), MHz(148), PIHPSDR_AM_TX_MODES, W(5), W(25), PIHPSDR_MAINVFO},
        {MHz(430), MHz(450), PIHPSDR_OTHER_TX_MODES, W(5), W(50), PIHPSDR_MAINVFO},
        {MHz(430), MHz(450), PIHPSDR_AM_TX_MODES, W(5), W(12.5), PIHPSDR_MAINVFO},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, 1},
        {PIHPSDR_ALL_MODES, 10},
        {PIHPSDR_ALL_MODES, 100},
        {PIHPSDR_ALL_MODES, kHz(1)},
        {PIHPSDR_ALL_MODES, kHz(2.5)},
        {PIHPSDR_ALL_MODES, kHz(5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(6.25)},
        {PIHPSDR_ALL_MODES, kHz(10)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(15)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(20)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(25)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(30)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(50)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(100)},
        {PIHPSDR_ALL_MODES, MHz(1)},
        {PIHPSDR_ALL_MODES, 0}, /* any tuning step */
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

    .str_cal = PIHPSDR_STR_CAL,

    .priv = (void *)& ts2000_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = pihpsdr_open,
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
    .set_level =  pihpsdr_set_level,
    .get_level =  pihpsdr_get_level,
    .set_ant =  kenwood_set_ant,
    .get_ant =  kenwood_get_ant,
    .send_morse =  kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .vfo_op =  kenwood_vfo_op,
    .scan =  kenwood_scan,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .get_channel = pihspdr_get_channel,
    .set_channel = pihspdr_set_channel,
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
 * pihspdr_get_channel
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

int pihspdr_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    int err;
    int tmp;
    char buf[52];
    char cmd[8];
    size_t length;
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

int pihspdr_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char sqltype;
    char shift;
    char buf[128];
    char mode, tx_mode = 0;
    int err;
    int tone = 0;
    int tstep;
    short code;
    short dcscode;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
    sqltype = '0';

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
    code = 0;

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
    dcscode = 0;

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

    shift = '0';

    if (chan->rptr_shift == RIG_RPT_SHIFT_PLUS)
    {
        shift = '1';
    }

    if (chan->rptr_shift ==  RIG_RPT_SHIFT_MINUS)
    {
        shift = '2';
    }

    tstep = 0;

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

int pihpsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int i, kenwood_val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        kenwood_val = val.f * 255;
    }
    else
    {
        kenwood_val = val.i;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        /* level is float between 0.0 and 1.0, maps to 0 ... 100 */
        kenwood_val = val.f * 100;
        SNPRINTF(levelbuf, sizeof(levelbuf), "PC%03d", kenwood_val);
        break;

    case RIG_LEVEL_AF:
        SNPRINTF(levelbuf, sizeof(levelbuf), "AG%03d", kenwood_val);
        break;

    case RIG_LEVEL_RF:
        /* XXX check level range */
        SNPRINTF(levelbuf, sizeof(levelbuf), "RG%03d", kenwood_val);
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ%03d", kenwood_val);
        break;

    case RIG_LEVEL_AGC:
        if (kenwood_val == RIG_AGC_OFF) { kenwood_val = 0; }
        else if (kenwood_val == RIG_AGC_SUPERFAST) { kenwood_val = 5; }
        else if (kenwood_val == RIG_AGC_FAST) { kenwood_val = 10; }
        else if (kenwood_val == RIG_AGC_MEDIUM) { kenwood_val = 15; }
        else if (kenwood_val == RIG_AGC_SLOW) { kenwood_val = 20; }

        SNPRINTF(levelbuf, sizeof(levelbuf), "GT%03d", kenwood_val);
        break;

    case RIG_LEVEL_ATT:

        /* set the attenuator if a correct value is entered */
        if (val.i == 0)
        {
            SNPRINTF(levelbuf, sizeof(levelbuf), "RA00");
        }
        else
        {
            int foundit = 0;

            for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rig->state.attenuator[i]; i++)
            {
                if (val.i == rig->state.attenuator[i])
                {
                    SNPRINTF(levelbuf, sizeof(levelbuf), "RA%02d", i + 1);
                    foundit = 1;
                    break;
                }
            }

            if (!foundit)
            {
                return -RIG_EINVAL;
            }
        }

        break;

    case RIG_LEVEL_PREAMP:

        /* set the preamp if a correct value is entered */
        if (val.i == 0)
        {
            SNPRINTF(levelbuf, sizeof(levelbuf), "PA0");
        }
        else
        {
            int foundit = 0;

            for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rig->state.preamp[i]; i++)
            {
                if (val.i == rig->state.preamp[i])
                {
                    SNPRINTF(levelbuf, sizeof(levelbuf), "PA%01d", i + 1);
                    foundit = 1;
                    break;
                }
            }

            if (!foundit)
            {
                return -RIG_EINVAL;
            }
        }

        break;

    case RIG_LEVEL_SLOPE_HIGH:
        if (val.i > 20 || val.i < 0)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "SH%02d", (val.i));
        break;

    case RIG_LEVEL_SLOPE_LOW:
        if (val.i > 20 || val.i < 0)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "SL%02d", (val.i));
        break;

    case RIG_LEVEL_CWPITCH:
        if (val.i > 1000 || val.i < 400)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "PT%02d", (val.i / 50) - 8);
        break;

    case RIG_LEVEL_KEYSPD:
        if (val.i > 50 || val.i < 5)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "KS%03d", val.i);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return kenwood_transaction(rig, levelbuf, NULL, 0);
}

/*
 * pihpsdr_get_level
 * Assumes rig!=NULL, val!=NULL
 */

int pihpsdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[50];
    size_t lvl_len;
    int lvl, retval;

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

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = lvl / 100.0; /* FIXME: for 1.2GHZ need to divide by 10 */
        break;

    case RIG_LEVEL_MICGAIN:
        retval = kenwood_transaction(rig, "MG", lvlbuf, sizeof(lvlbuf));

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
        val->f = lvl / 100.0;
        break;

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

    case RIG_LEVEL_AGC: /* pihpsdr defines the range 0 -20 for AGC (based on TS-2000) */
        retval = kenwood_transaction(rig, "GT", lvlbuf, sizeof(lvlbuf));

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

        if (lvl == 0) { val->i = RIG_AGC_OFF; } /*pihspdr: OFF */
        else if (lvl < 6) { val->i = RIG_AGC_SUPERFAST; } /*pihspdr: 001-005 = FAST */
        else if (lvl < 11) { val->i = RIG_AGC_FAST; } /*pihspdr: 006-010 = MEDIUM */
        else if (lvl < 16) { val->i = RIG_AGC_MEDIUM; } /*pihspdr: 011-015 = SLOW */
        else if (lvl <= 20) { val->i = RIG_AGC_SLOW; } /*pihspdr: 016-020 = LONG */

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
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n",
                      __func__, (int)lvl_len);
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

/* use custom open function since this rig emulates TS-2000 and we need
   to not use the TS-2000 backend open function */
int pihpsdr_open(RIG *rig)
{
    char id[KENWOOD_MAX_BUF_LEN];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* get id in buffer, will be null terminated */
    kenwood_get_id(rig, id);

    if (!strcmp(id, "ID019"))   /* matched */
    {
        /* Currently we cannot cope with AI mode so turn it off in
           case last client left it on */
        kenwood_set_trn(rig, RIG_TRN_OFF); /* ignore status in case
                                          it's not supported */
        return RIG_OK;
    }

    /* driver mismatch */
    rig_debug(RIG_DEBUG_ERR, "%s: wrong driver selected\n", __func__);
    return -RIG_EINVAL;
}


