/*
 *  Hamlib GUOHETEC PMR-171 backend - main file
 *  Copyright (c) 2024 by GUOHETEC
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

#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "hamlib/rig.h"
#include "iofunc.h"
#include "guohetec.h"
#include "cache.h"
#include "tones.h"
#include "bandplan.h"

typedef struct pmr171_data_s
{
    char ptt;
    char freqA_mode;
    char freqB_mode;
    long freqA;
    long freqB;
    char vfo;
    char NR_NB;
    char RIT;
    char XIT;
    char filterBW;
    char BW;
    char vol;
    char hour;
    char min;
    char sec;
    char stateline; 
    char S_PO;
    char SWR;
} pmr171_data_t;

static rmode_t pmr171_modes[GUOHE_MODE_TABLE_MAX] = 
{
    RIG_MODE_USB,
    RIG_MODE_LSB,
    RIG_MODE_CWR,
    RIG_MODE_CW,
    RIG_MODE_AM,
    RIG_MODE_WFM,
    RIG_MODE_FM,   // NFM in protocol
   //  RIG_MODE_DIGI,
    RIG_MODE_PKTUSB 
};
 
static int pmr171_init(RIG *rig);
static int pmr171_open(RIG *rig);
static int pmr171_cleanup(RIG *rig);
static int pmr171_close(RIG *rig);
static int pmr171_set_vfo(RIG *rig, vfo_t vfo);
static int pmr171_get_vfo(RIG *rig, vfo_t *vfo);
static int pmr171_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int pmr171_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int pmr171_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int pmr171_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int pmr171_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int pmr171_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int pmr171_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                 vfo_t *tx_vfo);
static int pmr171_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                 vfo_t tx_vfo);
static int pmr171_set_powerstat(RIG *rig, powerstat_t status);
#if 0

#endif
 
#define PMR171_ALL_RX_MODES      (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_PKTFM|\
                                 RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_PSK|RIG_MODE_PSKR)
#define PMR171_SSB_CW_RX_MODES   (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY)
#define PMR171_CWN_RX_MODES      (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY)
#define PMR171_AM_FM_RX_MODES    (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_PKTFM)
 
#define PMR171_OTHER_TX_MODES    (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|\
                                 RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_PSK|RIG_MODE_PSKR)
#define PMR171_AM_TX_MODES       (RIG_MODE_AM)
 
#define PMR171_VFO_ALL           (RIG_VFO_A|RIG_VFO_B)
#define PMR171_ANT_FRONT         (RIG_ANT_1)
#define PMR171_ANT_REAR          (RIG_ANT_2)
#define PMR171_ANTS              (PMR171_ANT_FRONT | PMR171_ANT_REAR)
 
#define PMR171_STR_CAL { 16, \
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
#define PMR171_PWR_CAL { 9, \
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
 
#define PMR171_ALC_CAL { 6, \
                 { \
                     { 0x00, 0 }, \
                     { 0x01, 20 }, \
                     { 0x02, 40 }, \
                     { 0x03, 60 }, \
                     { 0x04, 80 }, \
                     { 0x05, 100 } \
                 } }
 
#define PMR171_SWR_CAL { 2, \
                 { \
                     { 0, 0 }, \
                     { 15, 100 } \
                 } }
 
 
// With packet header and CRC 
struct rig_caps pmr171_caps =
{
    RIG_MODEL(RIG_MODEL_PMR171),
    .model_name =          "PMR-171",
    .mfg_name =            "GUOHETEC",
    .version =             "20250611.0",
    .copyright =           "LGPL",
    .status =              RIG_STATUS_STABLE,
    .rig_type =            RIG_TYPE_TRANSCEIVER,
    .ptt_type =            RIG_PTT_RIG,
    .dcd_type =            RIG_DCD_RIG,
    .port_type =           RIG_PORT_SERIAL,
    .serial_rate_min =     115200,
    .serial_rate_max =     115200,
    .serial_data_bits =    8,
    .serial_stop_bits =    1,
    .serial_parity =       RIG_PARITY_NONE,
    .serial_handshake =    RIG_HANDSHAKE_NONE,
    .write_delay =         0,
    .post_write_delay =    0,
    .timeout =             200,
    .retry =               2,
    .has_get_func =        RIG_FUNC_NONE,

    .has_get_parm =        RIG_PARM_NONE,
    .has_set_parm =        RIG_PARM_NONE,

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
        {kHz(100), MHz(56), PMR171_ALL_RX_MODES,  -1, -1, PMR171_VFO_ALL, PMR171_ANTS},
        {MHz(76), MHz(108), RIG_MODE_WFM,        -1, -1, PMR171_VFO_ALL, PMR171_ANTS},
        {MHz(118), MHz(164), PMR171_ALL_RX_MODES, -1, -1, PMR171_VFO_ALL, PMR171_ANTS},
        {MHz(420), MHz(470), PMR171_ALL_RX_MODES, -1, -1, PMR171_VFO_ALL, PMR171_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, PMR171_OTHER_TX_MODES, W(0.5), W(5), PMR171_VFO_ALL, PMR171_ANTS),
        FRQ_RNG_HF(1, PMR171_AM_TX_MODES, W(0.5), W(1.5), PMR171_VFO_ALL, PMR171_ANTS),

        FRQ_RNG_6m(1, PMR171_OTHER_TX_MODES, W(0.5), W(5), PMR171_VFO_ALL, PMR171_ANTS),
        FRQ_RNG_6m(1, PMR171_AM_TX_MODES, W(0.5), W(1.5), PMR171_VFO_ALL, PMR171_ANTS),

        FRQ_RNG_2m(1, PMR171_OTHER_TX_MODES, W(0.5), W(5), PMR171_VFO_ALL, PMR171_ANTS),
        FRQ_RNG_2m(1, PMR171_AM_TX_MODES, W(0.5), W(1.5), PMR171_VFO_ALL, PMR171_ANTS),

        FRQ_RNG_70cm(1, PMR171_OTHER_TX_MODES, W(0.5), W(5), PMR171_VFO_ALL, PMR171_ANTS),
        FRQ_RNG_70cm(1, PMR171_AM_TX_MODES, W(0.5), W(1.5), PMR171_VFO_ALL, PMR171_ANTS),

        RIG_FRNG_END,
    },


    .rx_range_list2 =  {
        {kHz(100), MHz(56), PMR171_ALL_RX_MODES,  -1, -1, PMR171_VFO_ALL, PMR171_ANTS},
        {MHz(76), MHz(108), RIG_MODE_WFM,        -1, -1, PMR171_VFO_ALL, PMR171_ANTS},
        {MHz(118), MHz(164), PMR171_ALL_RX_MODES, -1, -1, PMR171_VFO_ALL, PMR171_ANTS},
        {MHz(420), MHz(470), PMR171_ALL_RX_MODES, -1, -1, PMR171_VFO_ALL, PMR171_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =  {
        FRQ_RNG_HF(2, PMR171_OTHER_TX_MODES, W(0.5), W(5), PMR171_VFO_ALL, PMR171_ANTS),
        FRQ_RNG_HF(2, PMR171_AM_TX_MODES, W(0.5), W(1.5), PMR171_VFO_ALL, PMR171_ANTS),
        /* FIXME: 60 meters in US version */

        FRQ_RNG_6m(2, PMR171_OTHER_TX_MODES, W(0.5), W(5), PMR171_VFO_ALL, PMR171_ANTS),
        FRQ_RNG_6m(2, PMR171_AM_TX_MODES, W(0.5), W(1.5), PMR171_VFO_ALL, PMR171_ANTS),

        FRQ_RNG_2m(2, PMR171_OTHER_TX_MODES, W(0.5), W(5), PMR171_VFO_ALL, PMR171_ANTS),
        FRQ_RNG_2m(2, PMR171_AM_TX_MODES, W(0.5), W(1.5), PMR171_VFO_ALL, PMR171_ANTS),

        FRQ_RNG_70cm(2, PMR171_OTHER_TX_MODES, W(0.5), W(5), PMR171_VFO_ALL, PMR171_ANTS),
        FRQ_RNG_70cm(2, PMR171_AM_TX_MODES, W(0.5), W(1.5), PMR171_VFO_ALL, PMR171_ANTS),

        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {PMR171_SSB_CW_RX_MODES, Hz(10)},
        {PMR171_AM_FM_RX_MODES | RIG_MODE_WFM, Hz(100)},
        RIG_TS_END,
    },

    .filters = {
        {PMR171_SSB_CW_RX_MODES, kHz(2.2)},  /* normal passband */
        {PMR171_CWN_RX_MODES, 500},          /* CW and RTTY narrow */
        {RIG_MODE_AM, kHz(6)},              /* AM normal */
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(9)},
        {RIG_MODE_WFM, kHz(15)},
        RIG_FLT_END,
    },

    .str_cal =          PMR171_STR_CAL,
    .swr_cal =          PMR171_SWR_CAL,
    .alc_cal =          PMR171_ALC_CAL,
    .rfpower_meter_cal = PMR171_PWR_CAL,

    .rig_init =         pmr171_init,
    .rig_cleanup =      pmr171_cleanup,
    .rig_open =         pmr171_open,
    .rig_close =        pmr171_close,
    .get_vfo =          pmr171_get_vfo,
    .set_vfo =          pmr171_set_vfo,
    .set_freq =         pmr171_set_freq,
    .get_freq =         pmr171_get_freq, 
    .set_mode =         pmr171_set_mode,
    .get_mode =         pmr171_get_mode, 
    .set_ptt =          pmr171_set_ptt,
    .get_ptt =          pmr171_get_ptt,
    .set_split_vfo =    pmr171_set_split_vfo,
    .get_split_vfo =    pmr171_get_split_vfo, // TBD
    .set_powerstat =    pmr171_set_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
 
/* ---------------------------------------------------------------------- */
 
static int pmr171_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called, version %s\n", __func__,
        rig->caps->version);

    pmr171_data_t *data = (pmr171_data_t *)malloc(sizeof(pmr171_data_t));
    if (data == NULL)
    {
        return -RIG_ENOMEM;
    }
    STATE(rig)->priv = data;
    
    return RIG_OK;
}

static int pmr171_cleanup(RIG *rig)
 {
     rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    
     if (STATE(rig)->priv != NULL) {
         free(STATE(rig)->priv);
         STATE(rig)->priv = NULL;
     }
 
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
 
 static int pmr171_send(RIG *rig, const unsigned char* buff, int len, unsigned char *reply, int rlen)
{
    hamlib_port_t *rp = RIGPORT(rig);
    int retry = 5;
    
    while (retry > 0) {
        rig_flush(rp);
        write_block(rp, buff, len);
        
        int r = read_block(rp, reply, rlen);
        if (r > 0) {
            break;
        }
        
        retry--;
        hl_usleep(20 * 1000); 
    }

    return RIG_OK;
}

 static int pmr171_send_cmd1(RIG *rig, unsigned char cmd, unsigned char *reply)
 {
     hamlib_port_t *rp = RIGPORT(rig);
     rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
     unsigned char buf[8] = { 0xa5, 0xa5, 0xa5, 0xa5, 0x03, 0x00, 0x00, 0x00 };
 
     buf[5] = cmd;
     unsigned int crc = CRC16Check(&buf[4], 2);
     buf[6] = crc >> 8;
     buf[7] = crc & 0xff;
     rig_flush(rp);
     write_block(rp, buf, 8);
     return RIG_OK;
 }
 

 /* ---------------------------------------------------------------------- */
 
 static int pmr171_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
 {
    // Send status query command (0x0B)
    unsigned char cmd[8] = {
        0xA5, 0xA5, 0xA5, 0xA5, 
        0x03,                    
        0x0B,                    
        0x00, 0x00               
    };

    // Calculate CRC and fill
    uint16_t crc = CRC16Check(&cmd[4], 2);
    cmd[6] = crc >> 8;
    cmd[7] = crc & 0xFF;

    {
        unsigned char reply[40];
        pmr171_send(rig, cmd, sizeof(cmd), reply, sizeof(reply));

        // Validate response using common function
        if (validate_freq_response(rig, reply, sizeof(reply), __func__) < 0) {
            RETURN_CACHED_FREQ(rig, vfo, freq);
        }

        // Parse frequency (big-endian)
        int freq_a_offset = 9;  // VFOA frequency starting position
        int freq_b_offset = 13; // VFOB frequency starting position

        uint32_t freq_a = (reply[freq_a_offset] << 24) | 
                         (reply[freq_a_offset+1] << 16) | 
                         (reply[freq_a_offset+2] << 8) | 
                         reply[freq_a_offset+3];

        uint32_t freq_b = (reply[freq_b_offset] << 24) | 
                         (reply[freq_b_offset+1] << 16) | 
                         (reply[freq_b_offset+2] << 8) | 
                         reply[freq_b_offset+3];

        // Update cache
        CACHE(rig)->freqMainA = (freq_t)freq_a;
        CACHE(rig)->freqMainB = (freq_t)freq_b;

        // Return requested VFO frequency
        *freq = (vfo == RIG_VFO_A) ? CACHE(rig)->freqMainA : CACHE(rig)->freqMainB;

        rig_debug(RIG_DEBUG_VERBOSE, "%s: Successfully got VFOA=%.0f Hz, VFOB=%.0f Hz\n",
                 __func__, CACHE(rig)->freqMainA, CACHE(rig)->freqMainB);
    }
    return RIG_OK;
 }
 
 static int pmr171_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
 {
    struct rig_cache *cachep = CACHE(rig);
    const pmr171_data_t *p = (pmr171_data_t *) STATE(rig)->priv;
    {
        unsigned char reply[40];
        // Get latest status from hardware
        pmr171_send_cmd1(rig, 0x0b, 0);
        // Read and validate response using common function
        if (read_rig_response(rig, reply, sizeof(reply), __func__) < 0) {
            RETURN_CACHED_MODE(rig, vfo, mode, width, cachep, p);
        }
        // Validate mode response using common function
        if (validate_mode_response(rig, reply, sizeof(reply), __func__, 5) < 0) {
            RETURN_CACHED_MODE(rig, vfo, mode, width, cachep, p);
        }
        // Update cache
        cachep->modeMainA = guohe2rmode(reply[7], pmr171_modes);
        cachep->modeMainB = guohe2rmode(reply[8], pmr171_modes);
        // Return requested mode
        *mode = (vfo == RIG_VFO_A) ? cachep->modeMainA : cachep->modeMainB;
        *width = p->filterBW;
    }
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

 static int pmr171_get_vfo(RIG *rig, vfo_t *vfo)
 {
    {
        unsigned char reply[40];
        // Send status sync command to get current VFO state
        pmr171_send_cmd1(rig, 0x0b, 0);
        // Read and validate response using common function
        if (read_rig_response(rig, reply, sizeof(reply), __func__) < 0) {
            RETURN_CACHED_VFO(rig, vfo);
        }
        // Validate VFO status field index won't overflow
        if (reply[4] < 13) { // Need at least 13 bytes to access reply[17]
            rig_debug(RIG_DEBUG_ERR, "%s: Response too short for VFO data, using cached values\n", __func__);
            RETURN_CACHED_VFO(rig, vfo);
        }
        // According to protocol doc, reply[17] is A/B frequency status
        *vfo = (reply[17] == 1) ? RIG_VFO_B : RIG_VFO_A;
    }
    return RIG_OK;
 }

 
 static int pmr171_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
 {
    struct rig_cache *cachep = CACHE(rig);
    {
        unsigned char reply[40];
        pmr171_send_cmd1(rig, 0x0b, 0);
        // Read and validate response using common function
        if (read_rig_response(rig, reply, sizeof(reply), __func__) < 0) {
            RETURN_CACHED_PTT(rig, ptt, cachep);
        }
        // Validate PTT status field index won't overflow
        if (reply[4] < 2) { // Need at least 2 bytes to access reply[6]
            rig_debug(RIG_DEBUG_ERR, "%s: Response too short for PTT data, using cached values\n", __func__);
            RETURN_CACHED_PTT(rig, ptt, cachep);
        }
        // Get PTT status
        cachep->ptt = reply[6];
        *ptt = cachep->ptt;
    }
    return RIG_OK;
 }
 
 
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
 

 static int pmr171_send_cmd2(RIG *rig, unsigned char cmd, unsigned char value,
                             int response)
 {
     unsigned char buf[64] = { 0xa5, 0xa5, 0xa5, 0xa5, 0x04, 0x00, 0x00, 0x00, 0x00 };
     hamlib_port_t *rp = RIGPORT(rig);
     rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
     buf[5] = cmd;
     buf[6] = value;
     unsigned int crc = CRC16Check(&buf[4], 3);
     buf[7] = crc >> 8;
     buf[8] = crc & 0xff;
     rig_flush(rp);
     write_block(rp, buf, 9);
     if (response)
     {
         unsigned char reply[40];
         // Use common response reading function
         if (read_rig_response(rig, reply, sizeof(reply), __func__) < 0) {
             return RIG_OK; // Return OK to use cached values
         }
     }
     return pmr171_read_ack(rig);
 }
 

 
 static int pmr171_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
 {
     unsigned char cmd[16] = { 0xa5, 0xa5, 0xa5, 0xa5, 11, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
     unsigned char reply[16];
     hamlib_port_t *rp = RIGPORT(rig);

     rig_debug(RIG_DEBUG_VERBOSE, "pmr171: requested freq = %"PRIfreq" Hz\n", freq);

    /* Update frequency */
    if (vfo == RIG_VFO_B)
    {
        to_be(&cmd[6], CACHE(rig)->freqMainA, 4);
        to_be(&cmd[10], freq, 4);
    }
    else
    {
        to_be(&cmd[6], freq, 4);
        to_be(&cmd[10], CACHE(rig)->freqMainB, 4);
    }
 
     unsigned int crc = CRC16Check(&cmd[4], 10);
     cmd[14] = crc >> 8;
     cmd[15] = crc & 0xff;
     rig_flush(rp);
     write_block(rp, cmd, 16);
     
     // Read response and validate length
     int ret = read_block(rp, reply, sizeof(reply));
     if (ret < 0) {
         rig_debug(RIG_DEBUG_ERR, "%s: Failed to read response, using cached values\n", __func__);
         // Update cache with requested frequency even if response failed
         if (vfo == RIG_VFO_B)
         {
             CACHE(rig)->freqMainB = freq;
         }
         else
         {
             CACHE(rig)->freqMainA = freq;
         }
         return RIG_OK;
     }
     
     // Update cache with requested frequency
     if (vfo == RIG_VFO_B)
     {
         CACHE(rig)->freqMainB = freq;
     }
     else
     {
         CACHE(rig)->freqMainA = freq;
     }

     return RIG_OK;
 }
 
 static int pmr171_set_vfo(RIG *rig, vfo_t vfo)
 {
    unsigned char cmd[9] = { 0xa5, 0xa5, 0xa5, 0xa5, 0x04, 0x1b, 0x00, 0x00, 0x00 };
    
    // Set AB frequency command parameters according to VFO
    cmd[6] = (vfo == RIG_VFO_B) ? 1 : 0;
    
    // Calculate CRC and send command
    uint16_t crc = CRC16Check(&cmd[4], 3);
    cmd[7] = crc >> 8;
    cmd[8] = crc & 0xFF;
    
    rig_flush(RIGPORT(rig));
    write_block(RIGPORT(rig), cmd, sizeof(cmd));
    
    return RIG_OK;
 }

 static int pmr171_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
 {
     hamlib_port_t *rp = RIGPORT(rig);
     unsigned char cmd[10] = { 0xa5, 0xa5, 0xa5, 0xa5, 5, 0x0a, 0x00, 0x00, 0x00, 0x00 };
     unsigned char reply[10];
     unsigned char i = rmode2guohe(mode, pmr171_modes);

     if (vfo == RIG_VFO_B)
     {
         cmd[6] = rmode2guohe(CACHE(rig)->modeMainA, pmr171_modes);
         cmd[7] = i;
     }
     else
     {
         cmd[6] = i;
         cmd[7] = rmode2guohe(CACHE(rig)->modeMainB, pmr171_modes);
     }

     int crc = CRC16Check(&cmd[4], 4);
     cmd[8] = crc >> 8;
     cmd[9] = crc & 0xff;
     rig_flush(rp);
     write_block(rp, cmd, 10);
     
     // Use common response reading function
     if (read_rig_response(rig, reply, sizeof(reply), __func__) < 0) {
         // Update cache with requested mode even if response failed
         if (vfo == RIG_VFO_B)
         {
             CACHE(rig)->modeMainB = mode;
         }
         else
         {
             CACHE(rig)->modeMainA = mode;
         }
         return RIG_OK;
     }
     
     // Validate mode field index won't overflow
     if (reply[4] < 3) { // Need at least 3 bytes to access reply[6] and reply[7]
         rig_debug(RIG_DEBUG_ERR, "%s: Response too short for mode data, using cached values\n", __func__);
         // Update cache with requested mode even if validation failed
         if (vfo == RIG_VFO_B)
         {
             CACHE(rig)->modeMainB = mode;
         }
         else
         {
             CACHE(rig)->modeMainA = mode;
         }
         return RIG_OK;
     }
     
     // Update cache with response data
     CACHE(rig)->modeMainA = guohe2rmode(reply[6], pmr171_modes);
     CACHE(rig)->modeMainB = guohe2rmode(reply[7], pmr171_modes);

     return RIG_OK;
 }
 
static int pmr171_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char cmd[9] = {
        0xa5, 0xa5, 0xa5, 0xa5,  
        0x04,                    
        0x07,                     
        (unsigned char)(ptt == RIG_PTT_ON ? 0x00 : 0x01), 
        0x00, 0x00                
    };
    
    uint16_t crc = CRC16Check(&cmd[4], 3);
    cmd[7] = crc >> 8;
    cmd[8] = crc & 0xff;

    unsigned char reply[9];
    pmr171_send(rig, cmd, sizeof(cmd), reply, sizeof(reply));

    CACHE(rig)->ptt = ptt;

    return RIG_OK;
}
 
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
         /* Treat standby as power off for this rig */
         return pmr171_send_cmd2(rig, 0x0c, 0x00, 0);

     case RIG_POWER_OPERATE:
         /* Treat operate as power on for this rig */
         return pmr171_send_cmd2(rig, 0x0c, 0x01, 0);

     case RIG_POWER_UNKNOWN:
     default:
         rig_debug(RIG_DEBUG_ERR, "%s: unsupported power status %d\n", __func__, status);
         return RIG_OK;
     }
 }
 
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
     }
 
     CACHE(rig)->split = split;
 
     return RIG_OK;
 
 }
 
 /* ---------------------------------------------------------------------- */
