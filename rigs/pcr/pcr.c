/*
 *  Hamlib PCR backend - main file
 *  Copyright (c) 2001-2005 by Darren Hatcher
 *  Copyright (c) 2001-2010 by Stephane Fillod
 *  Copyright (C) 2007-09 by Alessandro Zummo <a.zummo@towertech.it>
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
/*
 * Tested on
 *
 * (402) PCR100  fw 1.2, proto 1.0 (usb-to-serial) by IZ1PRB
 * (401) PCR1000 fw 1.0, proto 1.0 (serial) by KM3T
 * (403) PCR1500 fw 2.0, proto 2.0 (usb) by KM3T
 *
 */
#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* String function definitions */
#include <errno.h>
#include <ctype.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "cal.h"
#include "pcr.h"

/*
 * modes in use by the "MD" command
 */
#define MD_LSB  '0'
#define MD_USB  '1'
#define MD_AM   '2'
#define MD_CW   '3'
#define MD_FM   '5'
#define MD_WFM  '6'
#define MD_DSTAR    '7' /* PCR-2500 Only */
#define MD_P25  '8' /* PCR-2500 Only */


/* define 2.8kHz, 6kHz, 15kHz, 50kHz, and 230kHz */
#define FLT_2_8kHz  '0'
#define FLT_6kHz    '1'
#define FLT_15kHz   '2'
#define FLT_50kHz   '3'
#define FLT_230kHz  '4'

/* as returned by GD? */
#define OPT_UT106 (1 << 0)
#define OPT_UT107 (1 << 4)

/*
 * CTCSS sub-audible tones for PCR100 and PCR1000
 * Don't even touch a single bit! indexes will be used in the protocol!
 * 51 tones, the 60.0 Hz tone is missing.
 */
tone_t pcr_ctcss_list[] =
{
    670,  693,  710,  719,  744,  770,  797,  825,  854,  885,  915,
    948,  974,  1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
    1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
    2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
    0,
};

/*
 * DTCS SQL code list
 * Don't even touch a single bit! indexes will be used in the protocol!
 * 104 codes
 */
tone_t pcr_dcs_list[] =
{
    23,  25,  26,  31,  32,  36,  43,  47,       51,  53,
    54,  65,  71,  72,  73,  74, 114, 115, 116, 122, 125, 131,
    132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205,
    212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261,
    263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343,
    346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432,
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516,
    523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654,
    662, 664, 703, 712, 723, 731, 732, 734, 743, 754,
    0,
};

struct pcr_country
{
    int id;
    char *name;
};

struct pcr_country pcr_countries[] =
{
    { 0x00, "Japan" },
    { 0x01, "USA" },
    { 0x02, "EUR/AUS" },
    { 0x03, "FRA" },
    { 0x04, "DEN" },
    { 0x05, "CAN" },
    { 0x06, "Generic 1" },
    { 0x07, "Generic 2" },
    { 0x08, "FCC Japan" },
    { 0x09, "FCC USA" },
    { 0x0A, "FCC EUR/AUS" },
    { 0x0B, "FCC FRA" },
    { 0x0C, "FCC DEN" },
    { 0x0D, "FCC CAN" },
    { 0x0E, "FCC Generic 1" },
    { 0x0F, "FCC Generic 2" },
};


static int pcr_set_volume(RIG *rig, vfo_t vfo, float level);
static int pcr_set_squelch(RIG *rig, vfo_t vfo, float level);
static int pcr_set_if_shift(RIG *rig, vfo_t vfo, int level);
static int pcr_set_agc(RIG *rig, vfo_t vfo, int status);            // J45xx
static int pcr_set_afc(RIG *rig, vfo_t vfo, int status);            // LD820xx
static int pcr_set_nb(RIG *rig, vfo_t vfo, int status);         // J46xx
static int pcr_set_attenuator(RIG *rig, vfo_t vfo, int status);     // J47xx
static int pcr_set_anl(RIG *rig, vfo_t vfo, int status);            // J4Dxx
static int pcr_set_diversity(RIG *rig, vfo_t vfo,
                             int status);  // J00xx on PCR-2500

static int pcr_set_bfo_shift(RIG *rig, vfo_t vfo, int level);          // J4Axx
static int pcr_set_vsc(RIG *rig, vfo_t vfo, int level);                // J50xx
static int pcr_set_dsp(RIG *rig, vfo_t vfo, int level);                // J80xx
static int pcr_set_dsp_state(RIG *rig, vfo_t vfo,
                             int level);          // J8100=off J8101=on
#if 1 /* unused; re-enabled as needed. */
static int pcr_set_dsp_noise_reducer(RIG *rig, vfo_t vfo, int level);  // J82xx
#endif /* unused */
static int pcr_set_dsp_auto_notch(RIG *rig, vfo_t vfo, int level);     // J83xx

static int pcr_check_ok(RIG *rig);

static int is_sub_rcvr(RIG *rig, vfo_t vfo);


#define PCR_COUNTRIES (sizeof(pcr_countries) / sizeof(struct pcr_country))

#define is_valid_answer(x) \
    ((x) == 'I' || (x) == 'G' || (x) == 'N' || (x) == 'H')

static int
pcr_read_block(RIG *rig, char *rxbuffer, size_t count)
{
    int read = 0, tries = 4;

    struct rig_state *rs = &rig->state;
    struct pcr_priv_caps *caps = pcr_caps(rig);
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    /* already in sync? */
    if (priv->sync && !caps->always_sync)
    {
        return read_block(&rs->rigport, (unsigned char *) rxbuffer, count);
    }

    /* read first char */
    do
    {
        char *p = &rxbuffer[0];

        /* read first char */
        int err = read_block(&rs->rigport, (unsigned char *) p, 1);

        if (err < 0)
        {
            return err;
        }

        if (err != 1)
        {
            return -RIG_EPROTO;
        }

        /* validate */
        if (*p != 0x0a && !is_valid_answer(*p))
        {
            continue;
        }

        /* sync ok, read remaining chars */
        read++;
        count--;
        p++;

        err = read_block(&rs->rigport, (unsigned char *) p, count);

        if (err < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: read failed - %s\n",
                      __func__, strerror(errno));

            return err;
        }

        if (err == count)
        {
            read += err;
            priv->sync = 1;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: RX %d bytes\n", __func__, read);

        return read;

    }
    while (--tries > 0);

    return -RIG_EPROTO;
}

/* expects a 4 byte buffer to parse */
static int
pcr_parse_answer(RIG *rig, char *buf, int len)
{
    struct rig_state *rs = &rig->state;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: len = %d\n", __func__, len);

    if (len < 4)
    {
        priv->sync = 0;
        return -RIG_EPROTO;
    }

    if (strncmp("G000", buf, 4) == 0)
    {
        return RIG_OK;
    }

    if (strncmp("G001", buf, 4) == 0)
    {
        return -RIG_ERJCTED;
    }

    if (strncmp("H101", buf, 4) == 0)
    {
        return RIG_OK;
    }

    if (strncmp("H100", buf, 4) == 0)
    {
        return -RIG_ERJCTED;
    }

    if (buf[0] == 'I')
    {
        switch (buf[1])
        {
        /* Main receiver */
        case '0':
            sscanf(buf, "I0%02X", &priv->main_rcvr.squelch_status);
            return RIG_OK;

        case '1':
            sscanf(buf, "I1%02X", &priv->main_rcvr.raw_level);
            return RIG_OK;

        case '2':
            rig_debug(RIG_DEBUG_VERBOSE, "%s: Signal centering %c%c\n",
                      __func__, buf[2], buf[3]);
            return RIG_OK;

        case '3':
            rig_debug(RIG_DEBUG_WARN, "%s: DTMF %c\n",
                      __func__, buf[3]);
            return RIG_OK;

        /* Sub receiver (on PCR-2500..) - TBC */
        case '4':
            sscanf(buf, "I4%02X", &priv->sub_rcvr.squelch_status);
            return RIG_OK;

        case '5':
            sscanf(buf, "I5%02X", &priv->sub_rcvr.raw_level);
            return RIG_OK;

        case '6':
            rig_debug(RIG_DEBUG_VERBOSE, "%s: Signal centering %c%c (Sub)\n",
                      __func__, buf[2], buf[3]);
            return RIG_OK;

        case '7':
            rig_debug(RIG_DEBUG_WARN, "%s: DTMF %c (Sub)\n",
                      __func__, buf[3]);
            return RIG_OK;
        }
    }
    else if (buf[0] == 'G')
    {
        switch (buf[1])
        {
        case '2': /* G2 */
            sscanf((char *) buf, "G2%d", &priv->protocol);
            return RIG_OK;

        case '4': /* G4 */
            sscanf((char *) buf, "G4%d", &priv->firmware);
            return RIG_OK;

        case 'D': /* GD */
            sscanf((char *) buf, "GD%d", &priv->options);
            return RIG_OK;

        case 'E': /* GE */
            sscanf((char *) buf, "GE%d", &priv->country);
            return RIG_OK;
        }
    }

    priv->sync = 0;

    return -RIG_EPROTO;

    /* XXX bandscope */
}

static int
pcr_send(RIG *rig, const char *cmd)
{
    int err;
    struct rig_state *rs = &rig->state;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

    int len = strlen(cmd);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd = %s, len = %d\n",
              __func__, cmd, len);

    /* XXX check max len */
    memcpy(priv->cmd_buf, cmd, len);

    /* append cr */
    /* XXX not required in auto update mode? (should not harm) */
    priv->cmd_buf[len + 0] = 0x0a;

    rs->transaction_active = 1;

    err = write_block(&rs->rigport, (unsigned char *) priv->cmd_buf, len + 1);

    rs->transaction_active = 0;

    return err;
}


static int
pcr_transaction(RIG *rig, const char *cmd)
{
    int err;
    struct rig_state *rs = &rig->state;
    struct pcr_priv_caps *caps = pcr_caps(rig);
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd = %s\n",
              __func__, cmd);

    if (!priv->auto_update)
    {
        rig_flush(&rs->rigport);
    }

    pcr_send(rig, cmd);

    /* the pcr does not give ack in auto update mode */
    if (priv->auto_update)
    {
        return RIG_OK;
    }

    err = pcr_read_block(rig, priv->reply_buf, caps->reply_size);

    if (err < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: read error, %s\n", __func__, strerror(errno));
        return err;
    }

    if (err != caps->reply_size)
    {
        priv->sync = 0;
        return -RIG_EPROTO;
    }

    return pcr_parse_answer(rig, &priv->reply_buf[caps->reply_offset], err);
}

static int
pcr_set_comm_speed(RIG *rig, int rate)
{
    int err;
    const char *rate_cmd;

    /* limit maximum rate */
    if (rate > 38400)
    {
        rate = 38400;
    }

    switch (rate)
    {
    case 300:
        rate_cmd = "G100";
        break;

    case 1200:
        rate_cmd = "G101";
        break;

    case 2400:
        rate_cmd = "G102";
        break;

    case 9600:
    default:
        rate_cmd = "G103";
        break;

    case 19200:
        rate_cmd = "G104";
        break;

    case 38400:
        rate_cmd = "G105";
        break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: setting speed to %d with %s\n",
              __func__, rate, rate_cmd);

    /* the answer will be sent at the new baudrate,
     * so we do not use pcr_transaction
     */
    err = pcr_send(rig, rate_cmd);

    if (err != RIG_OK)
    {
        return err;
    }

    rig->state.rigport.parm.serial.rate = rate;
    serial_setup(&rig->state.rigport);

    /* check if the pcr is still alive */
    return pcr_check_ok(rig);
}


/* Basically, it sets up *priv */
int
pcr_init(RIG *rig)
{
    struct pcr_priv_data *priv;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct pcr_priv_data *) calloc(1,
                      sizeof(struct pcr_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    memset(priv, 0x00, sizeof(struct pcr_priv_data));

    /*
     * FIXME: how can we retrieve initial status?
     * The protocol doesn't allow this.
     */
    /* Some values are already at zero due to the memset above,
     * but we reinitialize here for sake of completeness
     */
    priv->country       = -1;
    priv->sync      = 0;
    priv->power     = RIG_POWER_OFF;

    priv->main_rcvr.last_att        = 0;
    priv->main_rcvr.last_agc        = 0;
    priv->main_rcvr.last_ctcss_sql  = 0;
    priv->main_rcvr.last_freq       = MHz(145);
    priv->main_rcvr.last_mode       = MD_FM;
    priv->main_rcvr.last_filter = FLT_15kHz;
    priv->main_rcvr.volume      = 0.25;
    priv->main_rcvr.squelch     = 0.00;

    priv->sub_rcvr = priv->main_rcvr;
    priv->current_vfo = RIG_VFO_MAIN;

    return RIG_OK;
}

/*
 * PCR Generic pcr_cleanup routine
 * the serial port is closed by the frontend
 */
int
pcr_cleanup(RIG *rig)
{
    if (!rig)
    {
        return -RIG_EINVAL;
    }

    free(rig->state.priv);

    rig->state.priv = NULL;

    return RIG_OK;
}

/*
 * pcr_open
 * - send power on
 * - set auto update off
 *
 * Assumes rig!=NULL
 */
int
pcr_open(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rs->priv;

    int err;
    int wanted_serial_rate;
    int startup_serial_rate;

    /*
     * initial communication is at 9600bps for PCR 100/1000
     * once the power is on, the serial speed can be changed with G1xx
     */
    if (rig->caps->rig_model == RIG_MODEL_PCR1500 ||
            rig->caps->rig_model == RIG_MODEL_PCR2500)
    {
        startup_serial_rate = 38400;
    }
    else
    {
        startup_serial_rate = 9600;
    }

    wanted_serial_rate = rs->rigport.parm.serial.rate;
    rs->rigport.parm.serial.rate = startup_serial_rate;

    serial_setup(&rs->rigport);

    /* let the pcr settle and flush any remaining data*/
    hl_usleep(100 * 1000);
    rig_flush(&rs->rigport);

    /* try powering on twice, sometimes the pcr answers H100 (off) */
    pcr_send(rig, "H101");
    hl_usleep(100 * 250);

    pcr_send(rig, "H101");
    hl_usleep(100 * 250);

    rig_flush(&rs->rigport);

    /* return RIG_ERJCTED if power is off */
    err = pcr_transaction(rig, "H1?");

    if (err != RIG_OK)
    {
        return err;
    }

    priv->power = RIG_POWER_ON;

    /* turn off auto update (just to be sure) */
    err = pcr_transaction(rig, "G300");

    if (err != RIG_OK)
    {
        return err;
    }

    /* set squelch and volume */
    err = pcr_set_squelch(rig, RIG_VFO_MAIN, priv->main_rcvr.squelch);

    if (err != RIG_OK)
    {
        return err;
    }

    err = pcr_set_volume(rig, RIG_VFO_MAIN, priv->main_rcvr.volume);

    if (err != RIG_OK)
    {
        return err;
    }

    /* get device features */
    pcr_get_info(rig);

    /* tune to last freq */
    err = pcr_set_freq(rig, RIG_VFO_MAIN, priv->main_rcvr.last_freq);

    if (err != RIG_OK)
    {
        return err;
    }

    if ((rig->state.vfo_list & RIG_VFO_SUB) == RIG_VFO_SUB)
    {
        err = pcr_set_squelch(rig, RIG_VFO_SUB, priv->sub_rcvr.squelch);

        if (err != RIG_OK)
        {
            return err;
        }

        err = pcr_set_volume(rig, RIG_VFO_SUB, priv->sub_rcvr.volume);

        if (err != RIG_OK)
        {
            return err;
        }

        err = pcr_set_freq(rig, RIG_VFO_SUB, priv->sub_rcvr.last_freq);

        if (err != RIG_OK)
        {
            return err;
        }

        pcr_set_vfo(rig, RIG_VFO_MAIN);
    }

    /* switch to different speed if requested */
    if (wanted_serial_rate != startup_serial_rate && wanted_serial_rate >= 300)
    {
        return pcr_set_comm_speed(rig, wanted_serial_rate);
    }

    return RIG_OK;
}

/*
 * pcr_close - send power off
 * Assumes rig!=NULL
 */
int
pcr_close(RIG *rig)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    /* when the pcr turns itself off sometimes we receive
     * a malformed answer, so don't check for it.
     */
    priv->power = RIG_POWER_OFF;
    return pcr_send(rig, "H100");
}

/*
 * pcr_set_vfo
 *
 * Only useful on PCR-2500 which is a double receiver.
 * Simply remember what the current VFO is for RIG_VFO_CURR.
 */
int
pcr_set_vfo(RIG *rig, vfo_t vfo)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo = %s\n",
              __func__, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_MAIN:
    case RIG_VFO_SUB:
        break;

    default:
        return -RIG_EINVAL;
    }

    priv->current_vfo = vfo;

    return RIG_OK;
}

int
pcr_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

    *vfo = priv->current_vfo;
    return RIG_OK;
}

/*
 * pcr_set_freq
 * Assumes rig!=NULL
 *
 * K0GMMMKKKHHHmmff00
 * GMMMKKKHHH is frequency GHz.MHz.KHz.Hz
 * mm is the mode setting
 *  00 = LSB
 *  01 = USB
 *  02 = AM
 *  03 = CW
 *  04 = Not used or Unknown
 *  05 = NFM
 *  06 = WFM
 * ff is the filter setting
 *  00 = 2.8 Khz (CW USB LSB AM)
 *  01 = 6.0 Khz (CW USB LSB AM NFM)
 *  02 = 15  Khz (AM NFM)
 *  03 = 50  Khz (AM NFM WFM)
 *  04 = 230 Khz (WFM)
 *
 */

int
pcr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct pcr_priv_data *priv;
    struct pcr_rcvr *rcvr;
    unsigned char buf[20];
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo = %s, freq = %.0f\n",
              __func__, rig_strvfo(vfo), freq);

    priv = (struct pcr_priv_data *) rig->state.priv;
    rcvr = is_sub_rcvr(rig, vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    // cppcheck-suppress *
    SNPRINTF((char *) buf, sizeof(buf), "K%c%010" PRIll "0%c0%c00",
             is_sub_rcvr(rig, vfo) ? '1' : '0',
             (int64_t) freq,
             rcvr->last_mode, rcvr->last_filter);

    err = pcr_transaction(rig, (char *) buf);

    if (err != RIG_OK)
    {
        return err;
    }

    rcvr->last_freq = freq;

    return RIG_OK;
}

/*
 * pcr_get_freq
 * frequency can't be read back, so report the last one that was set.
 * Assumes rig != NULL, freq != NULL
 */
int
pcr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    *freq = rcvr->last_freq;

    return RIG_OK;
}

/*
 * pcr_set_mode
 * Assumes rig != NULL
 */

int
pcr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    unsigned char buf[20];
    int err;
    int pcrmode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode = %s, width = %d\n",
              __func__, rig_strrmode(mode), (int)width);

    /* XXX? */
    if (mode == RIG_MODE_NONE)
    {
        mode = RIG_MODE_FM;
    }

    /*
     * not so sure about modes and filters
     * as I write this from manual (no testing) --SF
     */
    switch (mode)
    {
    case RIG_MODE_CW:
        pcrmode = MD_CW;
        break;

    case RIG_MODE_USB:
        pcrmode = MD_USB;
        break;

    case RIG_MODE_LSB:
        pcrmode = MD_LSB;
        break;

    case RIG_MODE_AM:
        pcrmode = MD_AM;
        break;

    case RIG_MODE_WFM:
        pcrmode = MD_WFM;
        break;

    case RIG_MODE_FM:
        pcrmode = MD_FM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        int pcrfilter;

        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: will set to %d\n",
                  __func__, (int)width);

        switch (width)
        {
        /* nop, pcrfilter already set
         * TODO: use rig_passband_normal instead?
         */
        case s_kHz(2.8):
            pcrfilter = FLT_2_8kHz;
            break;

        case s_kHz(6):
            pcrfilter = FLT_6kHz;
            break;

        case s_kHz(15):
            pcrfilter = FLT_15kHz;
            break;

        case s_kHz(50):
            pcrfilter = FLT_50kHz;
            break;

        case s_kHz(230):
            pcrfilter = FLT_230kHz;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported width %d\n",
                      __func__, (int)width);
            return -RIG_EINVAL;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: filter set to %d (%c)\n",
                  __func__, (int)width, pcrfilter);

        SNPRINTF((char *) buf, sizeof(buf), "K%c%010" PRIll "0%c0%c00",
                 is_sub_rcvr(rig, vfo) ? '1' : '0',
                 (int64_t) rcvr->last_freq, pcrmode, pcrfilter);

        err = pcr_transaction(rig, (char *) buf);

        if (err != RIG_OK)
        {
            return err;
        }

        rcvr->last_filter = pcrfilter;
    }
    else
    {
        SNPRINTF((char *) buf, sizeof(buf), "K%c%010" PRIll "0%c0%c00",
                 is_sub_rcvr(rig, vfo) ? '1' : '0',
                 (int64_t) rcvr->last_freq, pcrmode, rcvr->last_filter);

        err = pcr_transaction(rig, (char *) buf);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    rcvr->last_mode = pcrmode;

    return RIG_OK;
}

/*
 * hack! pcr_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int
pcr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct pcr_priv_data *priv;
    struct pcr_rcvr *rcvr;

    priv = (struct pcr_priv_data *) rig->state.priv;
    rcvr = is_sub_rcvr(rig, vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_VERBOSE, "%s, last_mode = %c, last_filter = %c\n",
              __func__,
              rcvr->last_mode, rcvr->last_filter);

    switch (rcvr->last_mode)
    {
    case MD_CW:
        *mode = RIG_MODE_CW;
        break;

    case MD_USB:
        *mode = RIG_MODE_USB;
        break;

    case MD_LSB:
        *mode = RIG_MODE_LSB;
        break;

    case MD_AM:
        *mode = RIG_MODE_AM;
        break;

    case MD_WFM:
        *mode = RIG_MODE_WFM;
        break;

    case MD_FM:
        *mode = RIG_MODE_FM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "pcr_get_mode: unsupported mode %d\n",
                  rcvr->last_mode);
        return -RIG_EINVAL;
    }

    switch (rcvr->last_filter)
    {
    case FLT_2_8kHz:
        *width = kHz(2.8);
        break;

    case FLT_6kHz:
        *width = kHz(6);
        break;

    case FLT_15kHz:
        *width = kHz(15);
        break;

    case FLT_50kHz:
        *width = kHz(50);
        break;

    case FLT_230kHz:
        *width = kHz(230);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "pcr_get_mode: unsupported "
                  "width %d\n", rcvr->last_filter);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * pcr_get_info
 * Assumes rig!=NULL
 */

const char *
pcr_get_info(RIG *rig)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

    char *country = NULL;

    pcr_transaction(rig, "G2?"); /* protocol */
    pcr_transaction(rig, "G4?"); /* firmware */
    pcr_transaction(rig, "GD?"); /* options */
    pcr_transaction(rig, "GE?"); /* country */

    /* translate country id to name */
    if (priv->country > -1)
    {
        int i;

        for (i = 0; i < PCR_COUNTRIES; i++)
        {
            if (pcr_countries[i].id == priv->country)
            {
                country = pcr_countries[i].name;
                break;
            }
        }

        if (country == NULL)
        {
            country = "Unknown";
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unknown country code %#x, "
                      "please report to Hamlib maintainer\n",
                      __func__, priv->country);
        }
    }
    else
    {
        country = "Not queried yet";
    }

    SNPRINTF(priv->info, sizeof(priv->info), "Firmware v%d.%d, Protocol v%d.%d, "
             "Optional devices:%s%s%s, Country: %s",
             priv->firmware / 10, priv->firmware % 10,
             priv->protocol / 10, priv->protocol % 10,
             (priv->options & OPT_UT106) ? " DSP" : "",
             (priv->options & OPT_UT107) ? " DARC" : "",
             priv->options ? "" : " none",
             country);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Firmware v%d.%d, Protocol v%d.%d, "
              "Optional devices:%s%s%s, Country: %s\n",
              __func__,
              priv->firmware / 10, priv->firmware % 10,
              priv->protocol / 10, priv->protocol % 10,
              (priv->options & OPT_UT106) ? " DSP" : "",
              (priv->options & OPT_UT107) ? " DARC" : "",
              priv->options ? "" : " none",
              country);

    return priv->info;
}



/* *********************************************************************** */
/* load of new stuff added in by Darren Hatcher - G0WCW                    */
/* *********************************************************************** */

/*
 * pcr_set_level called by generic set level handler
 *
 * We are missing a way to set the BFO on/off here,
 */

int
pcr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int err = -RIG_ENIMPL;

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: level = %s, val = %f\n",
                  __func__, rig_strlevel(level), val.f);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: level = %s, val = %ul\n",
                  __func__, rig_strlevel(level), val.i);
    }

    switch (level)
    {
    case RIG_LEVEL_ATT:
        /* This is only on or off, but hamlib forces to use set level
         * and pass as a float. Here we'll use 0 for off and 1 for on.
         * If someone finds out how to set the ATT for the PCR in dB, let us
         * know and the function can be changed to allow a true set level.
         *
         * Experiment shows it seems to have an effect, but unsure by how many db
         */
        return pcr_set_attenuator(rig, vfo, val.i);

    case RIG_LEVEL_IF:
        return pcr_set_if_shift(rig, vfo, val.i);

    case RIG_LEVEL_CWPITCH: /* BFO */
        return pcr_set_bfo_shift(rig, vfo, val.i);

    case RIG_LEVEL_AGC:
        /* Only AGC on/off supported by PCR's */
        return pcr_set_agc(rig, vfo, val.i == RIG_AGC_OFF ? 0 : 1);

    /* floats */

    case RIG_LEVEL_AF:
        /* "val" can be 0.0 to 1.0 float which is 0 to 255 levels
         * 0.3 seems to be ok in terms of loudness
         */
        return pcr_set_volume(rig, vfo, val.f);

    case RIG_LEVEL_SQL:
        /* "val" can be 0.0 to 1.0 float
         *      .... rig supports 0 to FF - look at function for
         *      squelch "bands"
         */
        return pcr_set_squelch(rig, vfo, val.f);

    case RIG_LEVEL_NR:
        /* This selectss the DSP unit - this isn't a level per se,
         * but in the manual it says that we have to switch it on first
         * we'll assume 1 is for the UT-106, and anything else as off
         *
         * Later on we can set if the DSP features are on or off in set_func
         */
        /* return pcr_set_dsp(rig, vfo, (int) val.f); */
        return pcr_set_dsp_noise_reducer(rig, vfo, val.f);
    }

    return err;
}

/*
 * pcr_get_level
 *
 * This needs a set of stored variables as the PCR doesn't return the current status of settings.
 * So we'll need to store them as we go along and keep them in sync.
 */

int
pcr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int err;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

//  rig_debug(RIG_DEBUG_TRACE, "%s: level = %d\n", __func__, level);

    switch (level)
    {
    case RIG_LEVEL_SQL:
        val->f = rcvr->squelch;
        return RIG_OK;

    case RIG_LEVEL_AF:
        val->f = rcvr->volume;
        return RIG_OK;

    case RIG_LEVEL_STRENGTH:
        if (priv->auto_update == 0)
        {
            err = pcr_transaction(rig, is_sub_rcvr(rig, vfo) ? "I5?" : "I1?");

            if (err != RIG_OK)
            {
                return err;
            }
        }

        val->i = rig_raw2val(rcvr->raw_level, &rig->state.str_cal);
        /*      rig_debug(RIG_DEBUG_TRACE, "%s, raw = %d, converted = %d\n",
                         __func__, rcvr->raw_level, val->i);
        */
        return RIG_OK;

    case RIG_LEVEL_RAWSTR:
        if (priv->auto_update == 0)
        {
            err = pcr_transaction(rig, is_sub_rcvr(rig, vfo) ? "I5?" : "I1?");

            if (err != RIG_OK)
            {
                return err;
            }
        }

        val->i = rcvr->raw_level;
        return RIG_OK;

    case RIG_LEVEL_IF:
        val->i = rcvr->last_shift;
        return RIG_OK;

    case RIG_LEVEL_ATT:
        val->i = rcvr->last_att;
        return RIG_OK;

    case RIG_LEVEL_AGC:
        val->i = rcvr->last_agc;
        return RIG_OK;
    }

    return -RIG_ENIMPL;
}


/*
 * pcr_set_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * This is missing a way to call the set DSP noise reducer, as we don't have a func to call it
 * based on the flags in rig.h -> see also missing a flag for setting the BFO.
 */
int
pcr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d, func = %s\n", __func__,
              status, rig_strfunc(func));

    switch (func)
    {
    case RIG_FUNC_NR: /* sets DSP noise reduction on or off */

        /* status = 00 for off or 01 for on
         * always enable the DSP unit
         * even if only to turn it off
         */
        if (status == 1)
        {
            pcr_set_dsp(rig, vfo, 1);
            return pcr_set_dsp_state(rig, vfo, 1);
        }
        else
        {
            pcr_set_dsp(rig, vfo, 1);
            return pcr_set_dsp_state(rig, vfo, 0);
        }

        break;

    case RIG_FUNC_ANF: /* DSP auto notch filter */
        if (status == 1)
        {
            return pcr_set_dsp_auto_notch(rig, vfo, 1);
        }
        else
        {
            return pcr_set_dsp_auto_notch(rig, vfo, 0);
        }

        break;

    case RIG_FUNC_NB: /* noise blanker */
        if (status == 0)
        {
            return pcr_set_nb(rig, vfo, 0);
        }
        else
        {
            return pcr_set_nb(rig, vfo, 1);
        }

        break;

    case RIG_FUNC_AFC: /* Tracking Filter */
        if (status == 0)
        {
            return pcr_set_afc(rig, vfo, 0);
        }
        else
        {
            return pcr_set_afc(rig, vfo, 1);
        }

        break;

    case RIG_FUNC_TSQL:
        if (rcvr->last_mode != MD_FM)
        {
            return -RIG_ERJCTED;
        }

        if (status == 0)
        {
            return pcr_set_ctcss_sql(rig, vfo, 0);
        }
        else
        {
            return pcr_set_ctcss_sql(rig, vfo, rcvr->last_ctcss_sql);
        }

    case RIG_FUNC_VSC: /* Voice Scan Control */
        if (status == 0)
        {
            return pcr_set_vsc(rig, vfo, 0);
        }
        else
        {
            return pcr_set_vsc(rig, vfo, 1);
        }

        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: default\n", __func__);
        return -RIG_EINVAL;
    }
}

/*
 * pcr_get_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * This will need similar variables/flags as get_level. The PCR doesn't offer much in the way of
 *  confirmation of current settings (according to the docs).
 */
int
pcr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    /* stub here ... */
    return -RIG_ENIMPL;
}


int
pcr_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: tok = %s\n", __func__, rig_strlevel(token));

    switch (token)
    {

    case TOK_EL_ANL: /* automatic noise limiter */

        return pcr_set_anl(rig, vfo, (0 == val.i) ? 0 : 1);

    case TOK_EL_DIVERSITY: /* antenna diversity */

        return pcr_set_diversity(rig, vfo, (0 == val.i) ? 0 : 2);

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: unknown token: %s\n", __func__,
                  rig_strlevel(token));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/* --------------------------------------------------------------------------------------- */
/* The next functions are all "helper types". These are called by the base functions above */
/* --------------------------------------------------------------------------------------- */

/*
 * Asks if the rig is ok = G0? response is G000 if ok or G001 if not
 *
 * Is only useful in fast transfer mode (when the CR/LF is stripped off all commands) ...
 * but also works on standard mode.
 */
static int
pcr_check_ok(RIG *rig)
{
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);
    return pcr_transaction(rig, "G0?");
}

static int
is_sub_rcvr(RIG *rig, vfo_t vfo)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

    return vfo == RIG_VFO_SUB ||
           (vfo == RIG_VFO_CURR && priv->current_vfo == RIG_VFO_SUB);
}

static int
pcr_set_level_cmd(RIG *rig, const char *base, int level)
{
    char buf[12];

    rig_debug(RIG_DEBUG_TRACE, "%s: base is %s, level is %d\n",
              __func__, base, level);

    if (level < 0x0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: too low: %d\n",
                  __func__, level);
        return -RIG_EINVAL;
    }
    else if (level > 0xff)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: too high: %d\n",
                  __func__, level);
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, 12, "%s%02X", base, level);
    buf[11] = '\0';
    return pcr_transaction(rig, buf);
}

/*
 * Sets the volume of the rig to the level specified in the level integer.
 * Format is J40xx - where xx is 00 to FF in hex, and specifies 255 volume levels
 */

static int
pcr_set_volume(RIG *rig, vfo_t vfo, float level)
{
    int err;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_TRACE, "%s: level = %f\n", __func__, level);

    err = pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J60" : "J40",
                            level * 0xff);

    if (err == RIG_OK)
    {
        rcvr->volume = level;
    }

    return err;
}

/*
 * pcr_set_squelch
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Format is J41xx - where xx is 00 to FF in hex, and specifies 255 squelch levels
 *
 * Sets the squelch of the rig to the level specified in the level integer.
 * There are some bands though ...
 *  00  Tone squelch clear and squelch open
 *  01-3f   Squelch open
 *  40-7f   Noise squelch
 *  80-ff   Noise squelch + S meter squelch ...
 *       Comparative S level = (squelch setting - 128) X 2
 *
 *  Could do with some constants to add together to allow better (and more accurate)
 *  use of Hamlib API. Otherwise may get unexpected squelch settings if have to do by hand.
 */

static int
pcr_set_squelch(RIG *rig, vfo_t vfo, float level)
{
    int err;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_TRACE, "%s: level = %f\n", __func__, level);

    err = pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J61" : "J41",
                            level * 0xff);

    if (err == RIG_OK)
    {
        rcvr->squelch = level;
    }

    return err;
}


/*
 * pcr_set_if_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the IF shift  of the rig to the level specified in the level integer.
 *  IF-SHIFT position (in 256 stages, 80 = centre):
 *
 *      < 80    Minus shift (in 10 Hz steps)
 *      > 80    Plus shift (in 10 Hz steps)
 *        80    Centre
 *
 * Format is J43xx - where xx is 00 to FF in hex
 *
 */
int
pcr_set_if_shift(RIG *rig, vfo_t vfo, int level)
{
    int err;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);

    err = pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J63" : "J43",
                            (level / 10) + 0x80);

    if (err == RIG_OK)
    {
        rcvr->last_shift = level;
    }

    return err;
}

/*
 * pcr_set_agc
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the AGC on or off based on the level specified in the level integer.
 *  00 = off, 01 (non zero) is on
 *
 * Format is J45xx - where xx is 00 to 01 in hex
 *
 */
int
pcr_set_agc(RIG *rig, vfo_t vfo, int status)
{
    int err;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);

    err = pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J65" : "J45",
                            status ? 1 : 0);

    if (err == RIG_OK)
    {
        rcvr->last_agc = status ? 1 : 0;
    }

    return err;
}

/*
 * pcr_set_afc(RIG *rig, vfo_t vfo, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the Tracking Filter on or off based on the status argument.
 *  00 = on, 01 (non zero) is off
 *
 * Format is LD820xx - where xx is 00 to ff in hex
 *
 */
int
pcr_set_afc(RIG *rig, vfo_t vfo, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);
    return pcr_set_level_cmd(rig, "LD820", status ? 0 : 1);
}

/*
 * pcr_set_nb(RIG *rig, vfo_t vfo, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the noise blanker on or off based on the level specified in the level integer.
 *  00 = off, 01 (non zero) is on
 *
 * Format is J46xx - where xx is 00 to 01 in hex
 *
 */
int
pcr_set_nb(RIG *rig, vfo_t vfo, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);
    return pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J66" : "J46",
                             status ? 1 : 0);
}

/* Automatic Noise Limiter - J4Dxx - 00 off, 01 on */
int
pcr_set_anl(RIG *rig, vfo_t vfo, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);
    return pcr_set_level_cmd(rig, "J4D", status ? 1 : 0);
}


/* Antenna Diversity/Tuners - J00xx -
 *      02=Dual Diversity ON, 1 display using 2 tuners
 *      01=Single Diversity OFF, 1 display using 1 tuner
 *      00=OFF Diversity OFF, 2 displays using 2 tuners
 */
int
pcr_set_diversity(RIG *rig, vfo_t vfo, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);

    if (status < 0 || status > 2)
    {
        return -RIG_EINVAL;
    }

    return pcr_set_level_cmd(rig, "J00", status);
}

/*
 * pcr_set_attenuator(RIG *rig, vfo_t vfo, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the attenuator on or off based on the level specified in the level integer.
 *  00 = off, 01 (non zero) is on
 * The attenuator seems to be fixed at ~ -20dB
 *
 * Format is J47xx - where xx is 00 to 01 in hex
 *
 */

int
pcr_set_attenuator(RIG *rig, vfo_t vfo, int status)
{
    int err;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: status = %d\n", __func__, status);

    err = pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J67" : "J47",
                            status ? 1 : 0);

    if (err == RIG_OK)
    {
        rcvr->last_att = status;
    }

    return err;
}

/*
 * pcr_set_bfo_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the BFO of the rig to the level specified in the level integer.
 *  BFO-SHIFT position (in 256 stages, 80 = centre):
 *
 *      < 80    Minus shift (in 10 Hz steps)
 *      > 80    Plus shift (in 10 Hz steps)
 *        80    Centre
 *
 * Format is J4Axx - where xx is 00 to FF in hex, and specifies 255 levels
 * XXX command undocumented?
 */
int
pcr_set_bfo_shift(RIG *rig, vfo_t vfo, int level)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);
    return pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J6A" : "J4A",
                             0x80 + level / 10);
}

/*
 * pcr_set_dsp(RIG *rig, vfo_t vfo, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP to UT106 (01) or off (non 01)
 *
 */
int
pcr_set_dsp(RIG *rig, vfo_t vfo, int level)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);

    if (is_sub_rcvr(rig, vfo))
    {
        return -RIG_ENAVAIL;
    }

    return pcr_set_level_cmd(rig, "J80", level);
}

/*
 * pcr_set_dsp_state(RIG *rig, vfo_t vfo, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP on or off (> 0 = on, 0 = off)
 *
 */

int
pcr_set_dsp_state(RIG *rig, vfo_t vfo, int level)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);

    if (is_sub_rcvr(rig, vfo))
    {
        return -RIG_ENAVAIL;
    }

    return pcr_set_level_cmd(rig, "J81", level);
}

/*
 * pcr_set_dsp_noise_reducer(RIG *rig, vfo_t vfo, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the DSP noise reducer on or off (0x01 = on, 0x00 = off)
 *  the level of NR set by values 0x01 to 0x10 (1 to 16 inclusive)
 */

#if 1 /* unused; re-enabled as needed. */
int
pcr_set_dsp_noise_reducer(RIG *rig, vfo_t vfo, int level)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, level);

    if (is_sub_rcvr(rig, vfo))
    {
        return -RIG_ENAVAIL;
    }

    return pcr_set_level_cmd(rig, "J82", level);
}
#endif /* unused */

/*
 * pcr_set_dsp_auto_notch(RIG *rig, vfo_t vfo, int level);
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Sets the auto notch on or off (1 = on, 0 = off)
 */

int
pcr_set_dsp_auto_notch(RIG *rig, vfo_t vfo, int status)  // J83xx
{
    rig_debug(RIG_DEBUG_TRACE, "%s: level is %d\n", __func__, status);

    if (is_sub_rcvr(rig, vfo))
    {
        return -RIG_ENAVAIL;
    }

    return pcr_set_level_cmd(rig, "J83", status ? 1 : 0);
}


int
pcr_set_vsc(RIG *rig, vfo_t vfo, int status)  // J50xx
{
    /* Not sure what VSC for so skipping the function here ... */
    return pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J70" : "J50",
                             status ? 1 : 0);
}

int pcr_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    *tone = rcvr->last_ctcss_sql;
    return RIG_OK;
}

int pcr_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    int i, err;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone = %d\n", __func__, tone);

    if (tone == 0)
    {
        return pcr_transaction(rig, is_sub_rcvr(rig, vfo) ? "J7100" : "J5100");
    }

    for (i = 0; rig->caps->ctcss_list[i] != 0; i++)
    {
        if (rig->caps->ctcss_list[i] == tone)
        {
            break;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: index = %d, tone = %d\n",
              __func__, i, rig->caps->ctcss_list[i]);

    if (rig->caps->ctcss_list[i] != tone)
    {
        return -RIG_EINVAL;
    }

    err = pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J71" : "J51", i + 1);

    if (err == RIG_OK)
    {
        rcvr->last_ctcss_sql = tone;
    }

    return RIG_OK;
}

int pcr_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    *tone = rcvr->last_dcs_sql;
    return RIG_OK;
}

int pcr_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    int i, err;
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone = %d\n", __func__, tone);

    if (tone == 0)
    {
        return pcr_transaction(rig, is_sub_rcvr(rig, vfo) ? "J720000" : "J520000");
    }

    for (i = 0; rig->caps->dcs_list[i] != 0; i++)
    {
        if (rig->caps->dcs_list[i] == tone)
        {
            break;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: index = %d, tone = %d\n",
              __func__, i, rig->caps->dcs_list[i]);

    if (rig->caps->dcs_list[i] != tone)
    {
        return -RIG_EINVAL;
    }

    err = pcr_set_level_cmd(rig, is_sub_rcvr(rig, vfo) ? "J7200" : "J5200", i + 1);

    if (err == RIG_OK)
    {
        rcvr->last_dcs_sql = tone;
    }

    return RIG_OK;
}

int pcr_set_trn(RIG *rig, int trn)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: trn = %d\n", __func__, trn);

    if (trn == RIG_TRN_OFF)
    {
        priv->auto_update = 0;
        return pcr_transaction(rig, "G300");
    }
    else if (trn == RIG_TRN_RIG)
    {
        priv->auto_update = 1;
        return pcr_send(rig, "G301");
    }
    else
    {
        return -RIG_EINVAL;
    }
}

int pcr_decode_event(RIG *rig)
{
    int err;
    char buf[4];

    /* XXX check this */
    err = pcr_read_block(rig, buf, 4);

    if (err == 4)
    {
        return pcr_parse_answer(rig, buf, 4);
    }

    return RIG_OK;
}

int pcr_set_powerstat(RIG *rig, powerstat_t status)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;

    if (status == priv->power)
    {
        return RIG_OK;
    }

    if (status == RIG_POWER_ON)
    {
        return pcr_open(rig);
    }
    else if (status == RIG_POWER_OFF)
    {
        return pcr_close(rig);
    }

    return -RIG_ENIMPL;
}

int pcr_get_powerstat(RIG *rig, powerstat_t *status)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    int err;

    /* return RIG_ERJCTED if power is off */
    err = pcr_transaction(rig, "H1?");

    if (err != RIG_OK && err != -RIG_ERJCTED)
    {
        return err;
    }

    priv->power = err == RIG_OK ? RIG_POWER_ON : RIG_POWER_OFF;

    *status = priv->power;

    return RIG_OK;
}

int pcr_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    struct pcr_priv_data *priv = (struct pcr_priv_data *) rig->state.priv;
    struct pcr_rcvr *rcvr = is_sub_rcvr(rig,
                                        vfo) ? &priv->sub_rcvr : &priv->main_rcvr;

    if (priv->auto_update == 0)
    {
        int err = pcr_transaction(rig, is_sub_rcvr(rig, vfo) ? "I4?" : "I0?");

        if (err != RIG_OK)
        {
            return err;
        }
    }

    /* 04 = Closed, 07 = Open
     *
     * Bit 0: busy
     * Bit 1: AF open (CTCSS open)
     * Bit 2: VSC open
     * Bit 3: RX error (not ready to receive)
     */
    *dcd = (rcvr->squelch_status & 0x02) ? RIG_DCD_ON : RIG_DCD_OFF;

    return RIG_OK;
}

/* *********************************************************************************************
 * int pcr_set_comm_mode(RIG *rig, int mode_type);  // Set radio to fast/diagnostic mode  G3xx
 * int pcr_soft_reset(RIG *rig);                    // Ask rig to reset itself            H0xx
 ********************************************************************************************* */

DECLARE_INITRIG_BACKEND(pcr)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: init called\n", __func__);

    rig_register(&pcr100_caps);
    rig_register(&pcr1000_caps);
    rig_register(&pcr1500_caps);
    rig_register(&pcr2500_caps);

    return RIG_OK;
}
