/*
 * hamlib - (C) Frank Singleton 2000,2001 (vk3fcs@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2009
 *
 * pmr171.c - borrowed from ft817.c
 *          (C) Michael Black W9MDB 2024
 * This shared library provides an API for communicating
 * via serial interface to an Guohe PMR-171 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 * Then, Tommi OH2BNS improved the code a lot in the framework of the
 * FT-857 backend. These improvements have now (August 2005) been
 * copied back and adopted for the FT-817 which has then been borrowed
 * for the PMR-171.
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

#include <stdlib.h>
#include <string.h>     /* String function definitions */
#include <stdbool.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
//#include "pmr171.h"
#include "misc.h"
#include "tones.h"
#include "bandplan.h"
#include "cal.h"

struct pmr171_priv_data
{
    /* rx status */
    struct timeval rx_status_tv;
    unsigned char rx_status;

    /* tx status */
    struct timeval tx_status_tv;
    unsigned char tx_status; /* Raw data from rig. Highest bit 0 = PTT */

    /* tx levels */
    struct timeval tx_level_tv;
    unsigned char swr_level;
    unsigned char alc_level;
    unsigned char mod_level;
    unsigned char pwr_level; /* TX power level */

    /* freq & mode status */
    struct timeval fm_status_tv;
    unsigned char fm_status[5]; /* 5 bytes, NOT related to YAESU_CMD_LENGTH */
    /* Digi mode is not part of regular fm_status response.
     * So keep track of it in a separate variable. */
    unsigned char dig_mode;
};

static int pmr171_init(RIG *rig);
static int pmr171_open(RIG *rig);
static int pmr171_cleanup(RIG *rig);
static int pmr171_close(RIG *rig);
static int pmr171_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int pmr171_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int pmr171_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int pmr171_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width);
static int pmr171_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int pmr171_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int pmr171_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                vfo_t *tx_vfo);
static int pmr171_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                vfo_t tx_vfo);
static int pmr171_set_powerstat(RIG *rig, powerstat_t status);
#if 0
static int pmr171_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int pmr171_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int pmr171_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
static int pmr171_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int pmr171_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
static int pmr171_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
static int pmr171_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
static int pmr171_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
static int pmr171_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int pmr171_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
static int pmr171_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
static int pmr171_power2mW(RIG *rig, unsigned int *mwpower, float power,
                           freq_t freq, rmode_t mode);
static int pmr171_mW2power(RIG *rig, float *power, unsigned int mwpower,
                           freq_t freq, rmode_t mode);
#endif

#define FT817_ALL_RX_MODES      (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_PKTFM|\
                                 RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_PSK|RIG_MODE_PSKR)
#define FT817_SSB_CW_RX_MODES   (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY)
#define FT817_CWN_RX_MODES      (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY)
#define FT817_AM_FM_RX_MODES    (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_PKTFM)

#define FT817_OTHER_TX_MODES    (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|\
                                 RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_PSK|RIG_MODE_PSKR)
#define FT817_AM_TX_MODES       (RIG_MODE_AM)

#define FT817_VFO_ALL           (RIG_VFO_A|RIG_VFO_B)
#define FT817_ANT_FRONT         (RIG_ANT_1)
#define FT817_ANT_REAR          (RIG_ANT_2)
#define FT817_ANTS              (FT817_ANT_FRONT | FT817_ANT_REAR)

#define FT817_STR_CAL { 16, \
                { \
                    { 0x00, -54 }, /* S0 */ \
                    { 0x01, -48 }, \
                    { 0x02, -42 }, \
                    { 0x03, -36 }, \
                    { 0x04, -30 }, \
                    { 0x05, -24 }, \
                    { 0x06, -18 }, \
                    { 0x07, -12 }, \
                    { 0x08, -6 }, \
                    { 0x09,  0 },  /* S9 */ \
                    { 0x0A,  10 }, /* +10 */ \
                    { 0x0B,  20 }, /* +20 */ \
                    { 0x0C,  30 }, /* +30 */ \
                    { 0x0D,  40 }, /* +40 */ \
                    { 0x0E,  50 }, /* +50 */ \
                    { 0x0F,  60 }  /* +60 */ \
                } }

// Thanks to Olivier Schmitt sc.olivier@gmail.com for these tables
#define FT817_PWR_CAL { 9, \
                { \
                    { 0x00, 0 }, \
                    { 0x01, 10 }, \
                    { 0x02, 14 }, \
                    { 0x03, 20 }, \
                    { 0x04, 34 }, \
                    { 0x05, 50 }, \
                    { 0x06, 66 }, \
                    { 0x07, 82 }, \
                    { 0x08, 100 } \
                } }

#define FT817_ALC_CAL { 6, \
                { \
                    { 0x00, 0 }, \
                    { 0x01, 20 }, \
                    { 0x02, 40 }, \
                    { 0x03, 60 }, \
                    { 0x04, 80 }, \
                    { 0x05, 100 } \
                } }

#define FT817_SWR_CAL { 2, \
                { \
                    { 0, 0 }, \
                    { 15, 100 } \
                } }


// similar to FT817 but with header and CRC check
struct rig_caps pmr171_caps =
{
    RIG_MODEL(RIG_MODEL_PMR171),
    .model_name =          "PMR-171",
    .mfg_name =            "Guohe",
    .version =             "20240704.0",
    .copyright =           "LGPL",
    .status =              RIG_STATUS_ALPHA,
    .rig_type =            RIG_TYPE_TRANSCEIVER,
    .ptt_type =            RIG_PTT_RIG,
    .dcd_type =            RIG_DCD_RIG,
    .port_type =           RIG_PORT_SERIAL,
    .serial_rate_min =     4800,
    .serial_rate_max =     38400,
    .serial_data_bits =    8,
    .serial_stop_bits =    2,
    .serial_parity =       RIG_PARITY_NONE,
    .serial_handshake =    RIG_HANDSHAKE_NONE,
    .write_delay =         0,
    .post_write_delay =    0,
    .timeout =             200,
    .retry =               2,
    .has_get_func =        RIG_FUNC_NONE,
//    .has_set_func =        RIG_FUNC_LOCK | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_CSQL | RIG_FUNC_RIT,
//    .has_get_level =
//    RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR | RIG_LEVEL_RFPOWER |
//    RIG_LEVEL_ALC | RIG_LEVEL_SWR,
//    .has_set_level =       RIG_LEVEL_BAND_SELECT,
    .has_get_parm =        RIG_PARM_NONE,
    .has_set_parm =        RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =           {},
    .ctcss_list =          common_ctcss_list,
    .dcs_list =            common_dcs_list,   /* only 104 out of 106 supported */
    .preamp =              { RIG_DBLST_END, },
    .attenuator =          { RIG_DBLST_END, },
    .max_rit =             Hz(9990),
    .max_xit =             Hz(0),
    .max_ifshift =         Hz(0),
    .vfo_ops =             RIG_OP_TOGGLE,
    .targetable_vfo =      RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =          RIG_TRN_OFF,
    .bank_qty =            0,
    .chan_desc_sz =        0,
    .chan_list =           { RIG_CHAN_END, },

    .rx_range_list1 =  {
        {kHz(100), MHz(56), FT817_ALL_RX_MODES,  -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(76), MHz(108), RIG_MODE_WFM,        -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(118), MHz(164), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(420), MHz(470), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_HF(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_6m(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_6m(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_2m(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_2m(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_70cm(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_70cm(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        RIG_FRNG_END,
    },


    .rx_range_list2 =  {
        {kHz(100), MHz(56), FT817_ALL_RX_MODES,  -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(76), MHz(108), RIG_MODE_WFM,        -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(118), MHz(164), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(420), MHz(470), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_HF(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),
        /* FIXME: 60 meters in US version */

        FRQ_RNG_6m(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_6m(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_2m(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_2m(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_70cm(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_70cm(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {FT817_SSB_CW_RX_MODES, Hz(10)},
        {FT817_AM_FM_RX_MODES | RIG_MODE_WFM, Hz(100)},
        RIG_TS_END,
    },

    .filters = {
        {FT817_SSB_CW_RX_MODES, kHz(2.2)},  /* normal passband */
        {FT817_CWN_RX_MODES, 500},          /* CW and RTTY narrow */
        {RIG_MODE_AM, kHz(6)},              /* AM normal */
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(9)},
        {RIG_MODE_WFM, kHz(15)},
        RIG_FLT_END,
    },

    .str_cal =          FT817_STR_CAL,
    .swr_cal =          FT817_SWR_CAL,
    .alc_cal =          FT817_ALC_CAL,
    .rfpower_meter_cal = FT817_PWR_CAL,

    .rig_init =         pmr171_init,
    .rig_cleanup =      pmr171_cleanup,
    .rig_open =         pmr171_open,
    .rig_close =        pmr171_close,
//    .get_vfo =          pmr171_get_vfo,
//    .set_vfo =          pmr171_set_vfo,
    .set_freq =         pmr171_set_freq,
    .get_freq =         pmr171_get_freq, // using cache for now until we determine timing
    .set_mode =         pmr171_set_mode,
    .get_mode =         pmr171_get_mode, // using cache for now until we determine timing
    .set_ptt =          pmr171_set_ptt,
    .get_ptt =          pmr171_get_ptt,
    .set_split_vfo =    pmr171_set_split_vfo,
    .get_split_vfo =    pmr171_get_split_vfo, // TBD
    .set_powerstat =    pmr171_set_powerstat,
#if 0
    .get_dcd =          pmr171_get_dcd,
    .set_rptr_shift =   pmr171_set_rptr_shift,
    .set_rptr_offs =    pmr171_set_rptr_offs,
    .set_rit =          pmr171_set_rit,
    .set_dcs_code =     pmr171_set_dcs_code,
    .set_ctcss_tone =   pmr171_set_ctcss_tone,
    .set_dcs_sql =      pmr171_set_dcs_sql,
    .set_ctcss_sql =    pmr171_set_ctcss_sql,
    .power2mW =         pmr171_power2mW,
    .mW2power =         pmr171_mW2power,
    .get_level =        pmr171_get_level,
    .set_func =         pmr171_set_func,
    .vfo_op =           pmr171_vfo_op,
#endif
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

//****************************************************
//** Function name: CRC16Check -- from the PMR-171 manual
//** Input: buf The data to verify;
//** len The length of the data to verify
//** Output: Check value
//** Function description: CRC16 cyclic redundancy check
//** Note: The check mode is CRC16/CCITT-FALSE, pay attention to the variable type
//*****************************************************/
#include <stdint.h>

uint16_t CRC16Check(const unsigned char *buf, int len)
{
    uint16_t crc = 0xFFFF; // Initial value
    uint16_t polynomial = 0x1021; // Polynomial x^16 + x^12 + x^5 + 1

    for (int i = 0; i < len; i++)
    {
        crc ^= ((uint16_t)buf[i] << 8); // XOR byte into the upper 8 bits of crc

        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ polynomial;
            }
            else
            {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

#if 0 // manual's method does not compute the correct CRC
// some day when I'm bored I'll debug this
unsigned int CRC16Checkx(unsigned char *buf, unsigned char len)
{
    unsigned char i, j;
    unsigned int uncrcReg = 0xFFFF;
    unsigned int uncur;

    for (i = 0; i < len; i++)
    {
        uncur = buf[i] << 8;
        printf("buf[%d]=x%04x\n", i, uncur);

        for (j = 0; j < 8; j++)
        {
            if ((int)(uncrcReg ^ uncur) < 0)
            {
                uncrcReg = (uncrcReg << 1) ^ 0x1021;
            }
            else
            {
                uncrcReg <<= 1;
            }

            uncur <<= 1;
        }
    }

    printf("CRC=%04x\n", uncrcReg);

    return uncrcReg;
}
#endif
/* ---------------------------------------------------------------------- */

static int pmr171_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called, version %s\n", __func__,
              rig->caps->version);

    if ((STATE(rig)->priv = calloc(1, sizeof(struct pmr171_priv_data))) == NULL)
    {
        return -RIG_ENOMEM;
    }

    CACHE(rig)->freqMainA = 14999000;
    CACHE(rig)->freqMainB = 14999000;

    return RIG_OK;
}

static int pmr171_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    free(STATE(rig)->priv);

    STATE(rig)->priv = NULL;

    return RIG_OK;
}

static int pmr171_open(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    return RIG_OK;
}

static int pmr171_close(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    return RIG_OK;
}

/* ---------------------------------------------------------------------- */

#if 0
static inline long timediff(const struct timeval *tv1,
                            const struct timeval *tv2)
{
    struct timeval tv;

    tv.tv_usec = tv1->tv_usec - tv2->tv_usec;
    tv.tv_sec  = tv1->tv_sec  - tv2->tv_sec;

    return ((tv.tv_sec * 1000L) + (tv.tv_usec / 1000L));
}
#endif

static int pmr171_send_cmd1(RIG *rig, unsigned char cmd, unsigned char *reply)
{
    hamlib_port_t *rp = RIGPORT(rig);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    unsigned char buf[64] = { 0xa5, 0xa5, 0xa5, 0xa5, 0x03, 0x00, 0x00, 0x00 };

    buf[5] = cmd;
    unsigned int crc = CRC16Check(&buf[4], 3);
    buf[6] = crc >> 8;
    buf[7] = crc & 0xff;
    rig_flush(rp);
    write_block(rp, buf, 9);
    return RIG_OK;
}


rmode_t pmr171_modes[] = { RIG_MODE_USB, RIG_MODE_LSB, RIG_MODE_CWR, RIG_MODE_CW, RIG_MODE_FM, RIG_MODE_FMN, RIG_MODE_PKTUSB, RIG_MODE_RTTY };

static int pmr171_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct rig_cache *cachep = CACHE(rig);
    hamlib_port_t *rp = RIGPORT(rig);
    unsigned char reply[80];
    freq_t vfoa, vfob;

    // probably already read VFO_A so just return cache for VFO_B for now
    if (vfo == RIG_VFO_B)
    {
        *freq = cachep->freqMainB;
        return RIG_OK;
    }

    pmr171_send_cmd1(rig, 0x0b, 0);
    read_block(rp, reply, 5);
    read_block(rp, &reply[5], reply[4]);
    vfoa = from_bcd_be(&reply[9], 8);
    vfob = from_bcd_be(&reply[13], 8);
    cachep->freqMainA = vfoa;
    cachep->freqMainB = vfob;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfoa=%.0f, vfob=%.0f\n", __func__, vfoa,
              vfob);

    // Now grab the ptt status
    cachep->ptt = reply[6] == 1;
    // And the mode
    cachep->modeMainA = pmr171_modes[reply[7]];
    cachep->modeMainB = pmr171_modes[reply[8]];

    if (vfo == RIG_VFO_B) { *freq = cachep->freqMainA; }
    else { *freq = cachep->freqMainB; }

    return RIG_OK;
}

static int pmr171_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct rig_cache *cachep = CACHE(rig);

    if (vfo == RIG_VFO_B) { *mode = cachep->modeMainA; }
    else { *mode = cachep->modeMainB; }

    *width = 2400;
    return RIG_OK;
}

static int pmr171_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                vfo_t *tx_vfo)
{
    *split = CACHE(rig)->split;

    if (*split) { *tx_vfo = RIG_VFO_B; }
    else { *tx_vfo = RIG_VFO_A; }

    return RIG_OK;
}

static int pmr171_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    //struct pmr171_priv_data *p = (struct pmr171_priv_data *) STATE(rig)->priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    *ptt = CACHE(rig)->ptt;

    return RIG_OK;
}

#if 0
static int pmr171_get_tx_level(RIG *rig, value_t *val, unsigned char *tx_level,
                               const cal_table_float_t *cal)
{
    struct pmr171_priv_data *p = (struct pmr171_priv_data *) STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->tx_level_tv))
    {
        int n;
        ptt_t ptt;

        /* Default to not keyed */
        *tx_level = 0;

        /* TX metering is special; it sends 1 byte if not keyed and 2 if keyed.
         * To handle this properly we first verify the rig is keyed.
         * Otherwise we experience at least a full timeout and
         * perhaps pointless retries + timeouts.
         */
        n = pmr171_get_ptt(rig, 0, &ptt);

        if (n != RIG_OK)
        {
            return n;
        }

        if (ptt == RIG_PTT_OFF)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: rig not keyed\n", __func__);
            return -RIG_ERJCTED; //Or return OK?
        }

        n = pmr171_get_status(rig, FT817_NATIVE_CAT_GET_TX_METERING);

        if (n != RIG_OK)
        {
            return n;
        }
    }

    val->f = rig_raw2val_float(*tx_level, cal);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: level %f\n", __func__, val->f);

    return RIG_OK;
}

/* frontend will always use RAWSTR+cal_table */
static int pmr171_get_smeter_level(RIG *rig, value_t *val)
{
    struct pmr171_priv_data *p = (struct pmr171_priv_data *) STATE(rig)->priv;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
        if ((n = pmr171_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }

    //n = (p->rx_status & 0x0F) - 9;

    //val->i = n * ((n > 0) ? 10 : 6);

    /* S-meter value is returned in the lower 4 bits.
       0x00 = S0 (-54dB)
       0x01 = S1
       0x02 = S2
       ...
       0x09 = S9 (0dB)
       0x0A = S9+10 (10dB)
       0x0B = S9+20 and so on
    */
    n = (p->rx_status & 0x0F);

    if (n < 0x0A)
    {
        val->i = (6 * n) - 54;
    }
    else
    {
        val->i = 10 * (n - 9);
    }

    return RIG_OK;
}


static int pmr171_get_raw_smeter_level(RIG *rig, value_t *val)
{
    struct pmr171_priv_data *p = (struct pmr171_priv_data *) STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
    {
        int n;

        if ((n = pmr171_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }
    }

    val->i = p->rx_status & 0x0F;

    return RIG_OK;
}


static int pmr171_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct pmr171_priv_data *p = (struct pmr171_priv_data *) STATE(rig)->priv;

    switch (level)
    {

    case RIG_LEVEL_STRENGTH:
        /* The front-end will always call for RAWSTR and use the cal_table */
        return pmr171_get_smeter_level(rig, val);

    case RIG_LEVEL_RAWSTR:
        return pmr171_get_raw_smeter_level(rig, val);

    case RIG_LEVEL_RFPOWER:
        return pmr171_get_tx_level(rig, val, &p->pwr_level,
                                   &rig->caps->rfpower_meter_cal);

    case RIG_LEVEL_ALC:
        return pmr171_get_tx_level(rig, val, &p->alc_level, &rig->caps->alc_cal);

    case RIG_LEVEL_SWR:
        return pmr171_get_tx_level(rig, val, &p->swr_level, &rig->caps->swr_cal);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int pmr171_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    struct pmr171_priv_data *p = (struct pmr171_priv_data *) STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
    {
        int n;

        if ((n = pmr171_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }
    }

    /* TODO: consider bit 6 too ??? (CTCSS/DCS code match) */
    if (p->rx_status & 0x80)
    {
        *dcd = RIG_DCD_OFF;
    }
    else
    {
        *dcd = RIG_DCD_ON;
    }

    return RIG_OK;
}
#endif

/* ---------------------------------------------------------------------- */

static int pmr171_read_ack(RIG *rig)
{
    unsigned char dummy;
    hamlib_port_t *rp = RIGPORT(rig);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (rp->post_write_delay == 0)
    {
        if (read_block(rp, &dummy, 1) < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error reading ack\n", __func__);
            rig_debug(RIG_DEBUG_ERR, "%s: adjusting post_write_delay to avoid ack\n",
                      __func__);
            rp->post_write_delay =
                10; // arbitrary choice right now of max 100 cmds/sec
            return RIG_OK; // let it continue without checking for ack now
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: ack value=0x%x\n", __func__, dummy);

    }

    return RIG_OK;
}

/*
 * private helper function to send a private command sequence.
 * Must only be complete 2-byte sequences.
 */
static int pmr171_send_cmd2(RIG *rig, unsigned char cmd, unsigned char value,
                            int response)
{
    unsigned char reply[256];
    hamlib_port_t *rp = RIGPORT(rig);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    unsigned char buf[64] = { 0xa5, 0xa5, 0xa5, 0xa5, 0x04, 0x00, 0x00, 0x00, 0x00 };

    buf[5] = cmd;
    buf[6] = value;
    unsigned int crc = CRC16Check(&buf[4], 3);
    buf[7] = crc >> 8;
    buf[8] = crc & 0xff;

    rig_flush(rp);
    write_block(rp, buf, 9);

    if (response)
    {
        read_block(rp, reply, 5);
        read_block(rp, &reply[5], reply[4]);
    }

    return pmr171_read_ack(rig);
}

/*
 * The same for incomplete commands.
 */
#if 0
static int pmr171_send_icmd(RIG *rig, int index, const unsigned char *data)
{
    unsigned char cmd[YAESU_CMD_LENGTH];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (ncmd[index].ncomp == 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Complete sequence\n", __func__);
        return -RIG_EINTERNAL;
    }

    cmd[YAESU_CMD_LENGTH - 1] = ncmd[index].nseq[YAESU_CMD_LENGTH - 1];
    memcpy(cmd, data, YAESU_CMD_LENGTH - 1);

    write_block(RIGPORT(rig), cmd, YAESU_CMD_LENGTH);
    return pmr171_read_ack(rig);
}
#endif

static int pmr171_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[16] = { 0xa5, 0xa5, 0xa5, 0xa5, 12, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    unsigned char reply[16];
    //int retval;
    hamlib_port_t *rp = RIGPORT(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "pmr171: requested freq = %"PRIfreq" Hz\n", freq);

    /* fill in the frequency */
    if (vfo == RIG_VFO_B)
    {
        to_bcd_be(&cmd[6], CACHE(rig)->freqMainA, 8);
        to_bcd_be(&cmd[10], freq, 8);
    }
    else
    {
        to_bcd_be(&cmd[6], freq, 8);
        to_bcd_be(&cmd[10], CACHE(rig)->freqMainB, 8);
    }

    unsigned int crc = CRC16Check(&cmd[4], 12);
    cmd[14] = crc >> 8;
    cmd[15] = crc & 0xff;
    rig_flush(rp);
    write_block(rp, cmd, 16);
    read_block(rp, reply, 16);
    dump_hex(reply, 16);

    return RIG_OK;
}

static int pmr171_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    hamlib_port_t *rp = RIGPORT(rig);
    unsigned char cmd[10] = { 0xa5, 0xa5, 0xa5, 0xa5, 3, 0x0a, 0x00, 0x00, 0x00, 0x00 };
    unsigned char reply[10];
    int i, crc;

    for (i = 0; i < sizeof(pmr171_modes) / sizeof(rmode_t); ++i)
    {
        if (pmr171_modes[i] == mode)
        {
            if (vfo == RIG_VFO_B)
            {
                cmd[6] = CACHE(rig)->modeMainA;
                cmd[7] = i;
            }
            else
            {
                cmd[6] = i;
                cmd[7] = CACHE(rig)->modeMainB;
            }

            crc = CRC16Check(&cmd[4], 4);
            cmd[8] = crc >> 8;
            cmd[9] = crc & 0xff;
            rig_flush(rp);
            write_block(rp, cmd, 16);
            read_block(rp, reply, 16);
            dump_hex(reply, 16);
            return RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: invalid mode=%s\n", __func__, rig_strrmode(mode));
    return -RIG_EINVAL;
}

static int pmr171_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char reply[9];
    hamlib_port_t *rp = RIGPORT(rig);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON:
        return pmr171_send_cmd2(rig, 0x07, 0x01, 1);

    case RIG_PTT_OFF:
        return pmr171_send_cmd2(rig, 0x07, 0x00, 0);

    default:
        return -RIG_EINVAL;
    }

    read_block(rp, reply, 9);
    dump_hex(reply, 9);
}

#if 0
static int pmr171_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_LOCK:
        if (status)
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_LOCK_ON);
        }
        else
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_LOCK_OFF);
        }

    case RIG_FUNC_TONE:
        if (status)
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ENC_ON);
        }
        else
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_TSQL:
        if (status)
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ON);
        }
        else
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_CSQL:
        if (status)
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_DCS_ON);
        }
        else
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_RIT:
        if (status)
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_CLAR_ON);
        }
        else
        {
            return pmr171_send_cmd(rig, FT817_NATIVE_CAT_CLAR_OFF);
        }

    default:
        return -RIG_EINVAL;
    }
}

static int pmr171_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    /*  int n; */

    rig_debug(RIG_DEBUG_VERBOSE, "pmr171: set DCS code (%u)\n", code);

    if (code == 0)
    {
        return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the DCS code - the rig doesn't support separate codes... */
    to_bcd_be(data,     code, 4);
    to_bcd_be(data + 2, code, 4);


    /* FT-817 does not have the DCS_ENC_ON command, so we just set the tone here */

    /*  if ((n = pmr171_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data)) < 0) */
    /*      return n; */

    /*  return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_DCS_ENC_ON); */

    return pmr171_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data);
}

static int pmr171_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "pmr171: set DCS sql (%u)\n", code);

    if (code == 0)
    {
        return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the DCS code - the rig doesn't support separate codes... */
    to_bcd_be(data,     code, 4);
    to_bcd_be(data + 2, code, 4);

    if ((n = pmr171_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data)) < 0)
    {
        return n;
    }

    return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_DCS_ON);
}


static int pmr171_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "pmr171: set CTCSS tone (%.1f)\n", tone / 10.0);

    if (tone == 0)
    {
        return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the CTCSS freq - the rig doesn't support separate tones... */
    to_bcd_be(data,     tone, 4);
    to_bcd_be(data + 2, tone, 4);

    if ((n = pmr171_send_icmd(rig, FT817_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
    {
        return n;
    }

    return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ENC_ON);
}


static int pmr171_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "pmr171: set CTCSS sql (%.1f)\n", tone / 10.0);

    if (tone == 0)
    {
        return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the CTCSS freq - the rig doesn't support separate tones... */
    to_bcd_be(data,     tone, 4);
    to_bcd_be(data + 2, tone, 4);

    if ((n = pmr171_send_icmd(rig, FT817_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
    {
        return n;
    }

    return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ON);
}

static int pmr171_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
    /* Note: this doesn't have effect unless FT817 is in FM mode
       although the command is accepted in any mode.
    */
    rig_debug(RIG_DEBUG_VERBOSE, "pmr171: set repeter shift = %i\n", shift);

    switch (shift)
    {

    case RIG_RPT_SHIFT_NONE:
        return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX);

    case RIG_RPT_SHIFT_MINUS:
        return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_MINUS);

    case RIG_RPT_SHIFT_PLUS:
        return pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_PLUS);

    }

    return -RIG_EINVAL;
}

static int pmr171_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];

    rig_debug(RIG_DEBUG_VERBOSE, "pmr171: set repeter offs = %li\n", offs);

    /* fill in the offset freq */
    to_bcd_be(data, offs / 10, 8);

    return pmr171_send_icmd(rig, FT817_NATIVE_CAT_SET_RPT_OFFSET, data);
}

static int pmr171_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "pmr171: set rit = %li)\n", rit);

    /* fill in the RIT freq */
    data[0] = (rit < 0) ? 255 : 0;
    data[1] = 0;
    to_bcd_be(data + 2, labs(rit) / 10, 4);

    if ((n = pmr171_send_icmd(rig, FT817_NATIVE_CAT_SET_CLAR_FREQ, data)) < 0)
    {
        return n;
    }

    /* the rig rejects if these are repeated - don't confuse user with retcode */

    /* not used anymore, RIG_FUNC_RIT implemented
    if (rit == 0)
    {
        pmr171_send_cmd(rig, FT817_NATIVE_CAT_CLAR_OFF);
    }
    else
    {
        pmr171_send_cmd(rig, FT817_NATIVE_CAT_CLAR_ON);
    } */

    return RIG_OK;
}
#endif

static int pmr171_set_powerstat(RIG *rig, powerstat_t status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (status)
    {
    case RIG_POWER_OFF:
        return pmr171_send_cmd2(rig, 0x0c, 0x00, 0);

    case RIG_POWER_ON:
        return pmr171_send_cmd2(rig, 0x0c, 0x01, 0);

    case RIG_POWER_STANDBY:
    default:
        return -RIG_EINVAL;
    }
}

#if 0
static int pmr171_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (op)
    {
        int n;

    case RIG_OP_TOGGLE:
        rig_force_cache_timeout(&((struct pmr171_priv_data *)
                                  STATE(rig)->priv)->fm_status_tv);
        n = pmr171_send_cmd(rig, FT817_NATIVE_CAT_SET_VFOAB);
        hl_usleep(100 * 1000); // rig needs a little time to do this
        return n;

    default:
        return -RIG_EINVAL;
    }
}
#endif


/* FIXME: this function silently ignores the vfo args and just turns
   split ON or OFF.
*/
static int pmr171_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                vfo_t tx_vfo)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (split)
    {
    case RIG_SPLIT_ON:
        pmr171_send_cmd2(rig, 0x07, 0x1c, 1);
        break;

    case RIG_SPLIT_OFF:
        pmr171_send_cmd2(rig, 0x07, 0x1c, 0);
        break;

    default:
        return -RIG_EINVAL;
    }

    CACHE(rig)->split = split;

    return RIG_OK;

}



#if 0
/* FIXME: currently ignores mode and freq */
/*
   No documentation on how to interpret it but the max number
   of bars on the display is 10 and I measure:
                          8 bars = 5W
                          5 bars = 2.5W
                          3 bars = 1W
                          1 bar  = 0.5W
*/
static int pmr171_power2mW(RIG *rig, unsigned int *mwpower, float power,
                           freq_t freq, rmode_t mode)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    *mwpower = (int)(power * 6000);
    return RIG_OK;
}


/* FIXME: currently ignores mode and freq */
static int pmr171_mW2power(RIG *rig, float *power, unsigned int mwpower,
                           freq_t freq, rmode_t mode)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    *power = mwpower / 6000.0;
    return RIG_OK;
}
#endif


/* ---------------------------------------------------------------------- */
