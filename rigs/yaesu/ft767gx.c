/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft767gx.c - (C) Steve Conklin 2007
 * adapted from ft757gx.c  by Stephane Fillod 2004
 * This shared library provides an API for communicating
 * via serial interface to an FT-767GX using the "CAT" interface
 * box (FIF-232C) or similar
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
 * TODO
 *
 * 1. Allow cached reads
 * 2. set_mem/get_mem, get_channel, set_split/get_split,
 *  set_func/get_func
 *
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "ft767gx.h"


static int ft767_init(RIG *rig);
static int ft767_cleanup(RIG *rig);
static int ft767_open(RIG *rig);
static int ft767_close(RIG *rig);

static int ft767_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft767_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft767_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                          pbwidth_t width); /* select mode */
static int ft767_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                          pbwidth_t *width); /* get mode */

static int ft767_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
static int ft767_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

static int ft767_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);

static int ft767_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int ft767_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
static int ft767_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
static int ft767_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);

static int ft767_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int ft767_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int ft767_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                pbwidth_t tx_width);
static int ft767_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                pbwidth_t *tx_width);
static int ft767_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft767_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);

/* Private helper function prototypes */

static int ft767_send_block_and_ack(RIG *rig, unsigned char *cmd,
                                    size_t length);
static int ft767_get_update_data(RIG *rig);
static int ft767_enter_CAT(RIG *rig);
static int ft767_leave_CAT(RIG *rig);
static int ft767_set_split(RIG *rig, unsigned int split);
static unsigned char vfo2rig(RIG *rig, vfo_t vfo);
static vfo_t rig2vfo(unsigned char status);
static int mode2rig(RIG *rig, rmode_t mode);
static int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width);
/* static int ctcss2rig(RIG *rig, tone_t tone); */
static int rig2ctcss(RIG *rig, unsigned char tn, tone_t *tone);

/*
 * On the rig used by the author of this software, the values returned
 * by the rig for the CTCSS tones are different than those in the Yaesu
 * documentation. This could possibly be because he tone module in that
 * rig is an aftermarket board sold by the piexx company. Leaving the
 * following undefined uses the values discovered by experimentation. For
 * The original values defined in the Yaesu documentation, uncomment it.
 */
/* #define USE_YAESU_PUBLISHED_TONES */

#define FT767GX_VFOS (RIG_VFO_A|RIG_VFO_B)

/* Valid command codes */
#define CMD_CAT_SW  0x00
#define CMD_CHECK   0x01
#define CMD_UP10HZ  0x02
#define CMD_DN10HZ  0x03
#define CMD_PROG_UP 0x04
#define CMD_PROG_DN 0x05
#define CMD_BAND_UP 0x06
#define CMD_BAND_DN 0x07
#define CMD_FREQ_SET    0x08
#define CMD_VFOMR   0x09
#define CMD_MULTICMD    0x0A /* This command code overloaded */
#define CMD_TONE_SET    0x0C
#define CMD_ACK     0x0B

/* subcommands for command code 0x0A */
/* values 0 - 9 select memories */
#define SUBCMD_MEM0 0x00 /* 8 bytes returned */
#define SUBCMD_MEM1 0x01 /* 8 bytes returned */
#define SUBCMD_MEM2 0x02 /* 8 bytes returned */
#define SUBCMD_MEM3 0x03 /* 8 bytes returned */
#define SUBCMD_MEM4 0x04 /* 8 bytes returned */
#define SUBCMD_MEM5 0x05 /* 8 bytes returned */
#define SUBCMD_MEM6 0x06 /* 8 bytes returned */
#define SUBCMD_MEM7 0x07 /* 8 bytes returned */
#define SUBCMD_MEM8 0x08 /* 8 bytes returned */
#define SUBCMD_MEM9 0x09 /* 8 bytes returned */
#define SUBCMD_MODE_LSB 0x10 /* 8 bytes returned */
#define SUBCMD_MODE_USB 0x11 /* 8 bytes returned */
#define SUBCMD_MODE_CW  0x12 /* 8 bytes returned */
#define SUBCMD_MODE_AM  0x13 /* 8 bytes returned */
#define SUBCMD_MODE_FM  0x14 /* 8 bytes returned */
#define SUBCMD_MODE_FSK 0x15 /* 8 bytes returned */
#define SUBCMD_HG_HAM   0x20 /* ham coverage */
#define SUBCMD_HG_GEN   0x21 /* general coverage */
#define SUBCMD_SPLIT    0x30 /* toggle split */
#define SUBCMD_CLAR 0x40 /* toggle clarifier */
#define SUBCMD_MTOV 0x50 /* memory to VFO */
#define SUBCMD_VTOM 0x60 /* VFO to memory */
#define SUBCMD_SWAP 0x70 /* swap VFO, memory */
#define SUBCMD_ACLR 0x80 /* turn off SPLIT, CLAR< OFFSET */





/*
 * Some useful offsets in the status update map (offset)
 *
 * Status Update Chart, FT767GXII
 */
#define STATUS_FLAGS        0
#define STATUS_CURR_FREQ    1   /* Operating Frequency */
#define STATUS_CURR_TONE    5
#define STATUS_CURR_MODE    6
#define STATUS_VFOA_FREQ    14
#define STATUS_VFOA_MODE    19
#define STATUS_VFOB_FREQ    20
#define STATUS_VFOB_MODE    25

/* Status bit masks */
#define STATUS_MASK_PTT     0x01
#define STATUS_MASK_HG      0x02
#define STATUS_MASK_TXINH   0x04
#define STATUS_MASK_SPLIT   0x08
#define STATUS_MASK_VFO     0x10
#define STATUS_MASK_MEM     0x20
#define STATUS_MASK_CLAR    0x40
#define STATUS_MASK_CAT     0x80


/* mode byte */
#define MODE_LSB    0x0
#define MODE_USB    0x1
#define MODE_CW     0x2
#define MODE_AM     0x3
#define MODE_FM     0x4
#define MODE_FSK    0x5

/*
 * Things that I need to figure out how to fit in
 */
//#define VFO_OPERATIONS_LIST (RIG_OP_FROM_VFO | RIG_OP_TO_VFO | RIG_OP_TOGGLE)
/* These VFO OPS can only be applied to the current VFO */
//  RIG_OP_UP =     (1<<5), /*!< UP increment VFO freq by tuning step*/
//  RIG_OP_DOWN =       (1<<6), /*!< DOWN decrement VFO freq by tuning step*/
//  RIG_OP_BAND_UP =    (1<<7), /*!< Band UP */

/*
#define FT767_MEM_CAP { \
    .freq = 1, \
      .mode = 1, \
            .width = 1,
      .ctcss_tone = 1 \
      }
*/
/*
 * End of things not put in yet
 */


/*
 * Receiver caps
 */

#define FT767GX_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_PKTFM)
#define FT767GX_HF_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB)


/*
 * TX caps
 */

#define FT767GX_ALL_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_PKTFM)
#define FT767GX_HF_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB)

#define CTCSS_TONE_LIST \
  670,  710,  747,  770,  797,  825,  854,  885,  915,  948, \
 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318, 1365, \
 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862, 1928, \
 2035, 2107, 2181, 2257, 2336, 2418, 2503, 0

/*
 * future - private data
 *
 */

struct ft767_priv_data
{
    unsigned char pacing;     /* pacing value */
    unsigned char
    current_vfo;    /* active VFO from last cmd , can be either RIG_VFO_A or RIG_VFO_B only */
    unsigned char
    update_data[FT767GX_STATUS_UPDATE_DATA_LENGTH]; /* returned data */
    unsigned char rx_data[FT767GX_STATUS_UPDATE_DATA_LENGTH]; /* returned data */
    unsigned char ack_cmd[5];
};

tone_t static_767gx_ctcss_list[] =
{
    CTCSS_TONE_LIST
};


/*
 * ft767gx rigs capabilities.
 * Also this struct is READONLY!
 */

const struct rig_caps ft767gx_caps =
{
    RIG_MODEL(RIG_MODEL_FT767),
    .model_name =       "FT-767GX",
    .mfg_name =         "Yaesu",
    .version =           "20210221.0",
    .copyright =         "LGPL",
    .status =            RIG_STATUS_STABLE,
    .rig_type =          RIG_TYPE_TRANSCEIVER,
    .ptt_type =          RIG_PTT_RIG,
    .dcd_type =          RIG_DCD_NONE,
    .port_type =         RIG_PORT_SERIAL,
    .serial_rate_min =   4800,
    .serial_rate_max =   4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =       FT767GX_WRITE_DELAY,
    .post_write_delay =  FT767GX_POST_WRITE_DELAY,
    .timeout =           2000,
    .retry =             0,
    .has_get_func =      RIG_FUNC_NONE,
    .has_set_func =      RIG_FUNC_NONE,
    .has_get_level =     RIG_LEVEL_BAND_SELECT,
    .has_set_level =     RIG_LEVEL_BAND_SELECT,
    .has_get_parm =      RIG_PARM_NONE,
    .has_set_parm =      RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .ctcss_list =        static_767gx_ctcss_list,
    .dcs_list =          NULL,
    .preamp =            { RIG_DBLST_END, },
    .attenuator =        { RIG_DBLST_END, },
    .max_rit =           Hz(9999),
    .max_xit =           Hz(0),
    .max_ifshift =       Hz(0),
    .targetable_vfo =    0,
    .transceive =        RIG_TRN_OFF,
    .bank_qty =          0,
    .chan_desc_sz =      0,
    .chan_list =         {RIG_CHAN_END, }, /* TODO need memory channel capabilities here */
    //  .chan_list =         {0, 9, RIG_MTYPE_MEM, RIG_CHAN_END, }, /* TODO need memory channel capabilities here */

    .rx_range_list1 =    { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */

    .tx_range_list1 =    { RIG_FRNG_END, },

    .rx_range_list2 =    { {
            .startf = kHz(100), .endf = 29999999,
            .modes = FT767GX_ALL_RX_MODES, .low_power = -1, .high_power = -1
        },
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list2 =    { {kHz(1500), 1999900, FT767GX_HF_TX_MODES, .low_power = 5000, .high_power = 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = kHz(3500), 3999900, FT767GX_HF_TX_MODES, 5000, 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = kHz(7000), 7499900, FT767GX_HF_TX_MODES, 5000, 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = MHz(10), 10499900, FT767GX_HF_TX_MODES, 5000, 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = MHz(14), 14499900, FT767GX_HF_TX_MODES, 5000, 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = MHz(18), 18499900, FT767GX_HF_TX_MODES, 5000, 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = MHz(21), 21499900, FT767GX_HF_TX_MODES, 5000, 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = kHz(24500), 24999900, FT767GX_HF_TX_MODES, 5000, 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = MHz(28), 29999900, FT767GX_HF_TX_MODES, 5000, 100000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = MHz(50), 59999900, FT767GX_ALL_TX_MODES, 5000, 10000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = MHz(144), 147999900, FT767GX_ALL_TX_MODES, 5000, 10000, FT767GX_VFOS, RIG_ANT_CURR},

        {.startf = MHz(430), 449999990, FT767GX_ALL_TX_MODES, 5000, 10000, FT767GX_VFOS, RIG_ANT_CURR},

        RIG_FRNG_END,
    },


    .tuning_steps =  {
        {FT767GX_ALL_RX_MODES, 10},
        {FT767GX_ALL_RX_MODES, 100},
        RIG_TS_END,
    },

    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .priv =   NULL, /* private data */

    .rig_init =   ft767_init,
    .rig_cleanup =    ft767_cleanup,
    .rig_open =   ft767_open,             /* port opened */
    .rig_close =  ft767_close,                /* port closed */

    .set_freq =   ft767_set_freq,     /* set freq */
    .get_freq =   ft767_get_freq,     /* get freq */
    .set_mode =   ft767_set_mode,     /* set mode */
    .get_mode =   ft767_get_mode,     /* get mode */
    .set_vfo =    ft767_set_vfo,      /* set vfo */
    .get_vfo =    ft767_get_vfo,      /* get vfo */
    .set_ptt =    NULL,       /* set ptt */
    .get_ptt =    ft767_get_ptt,      /* get ptt */

    .set_ctcss_tone = ft767_set_ctcss_tone,
    .get_ctcss_tone = ft767_get_ctcss_tone,
    .set_ctcss_sql =  ft767_set_ctcss_sql,
    .get_ctcss_sql =  ft767_get_ctcss_sql,

    .set_split_freq = ft767_set_split_freq,
    .get_split_freq = ft767_get_split_freq,
    .set_split_mode = ft767_set_split_mode,
    .get_split_mode = ft767_get_split_mode,
    .set_split_vfo =  ft767_set_split_vfo,
    .get_split_vfo =  ft767_get_split_vfo,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * _init
 *
 */


int ft767_init(RIG *rig)
{
    struct ft767_priv_data *priv;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct ft767_priv_data *) calloc(1,
                      sizeof(struct ft767_priv_data));

    if (!rig->state.priv)           /* whoops! memory shortage! */
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    /* TODO: read pacing from preferences */

    priv->pacing = FT767GX_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */
    priv->current_vfo =  RIG_VFO_A;  /* default to VFO_A ? */
    priv->ack_cmd[0] = 00;
    priv->ack_cmd[1] = 00;
    priv->ack_cmd[2] = 00;
    priv->ack_cmd[3] = 00;
    priv->ack_cmd[4] = 0x0B;

    return RIG_OK;
}


/*
 * ft767_cleanup routine
 * the serial port is closed by the frontend
 */

int ft767_cleanup(RIG *rig)
{
    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

/*
 * ft767_open  routine
 *
 */

int ft767_open(RIG *rig)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;

    rig_flush(&rig->state.rigport);

    /* send 0 delay PACING cmd to rig  */
    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        memset(priv->update_data, 0, FT767GX_STATUS_UPDATE_DATA_LENGTH);
        return retval;
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
        memset(priv->update_data, 0, FT767GX_STATUS_UPDATE_DATA_LENGTH);
        return retval;
    }

    rig->state.vfo_list = RIG_VFO_A | RIG_VFO_B;
    return RIG_OK;
}

/*
 * ft767_close  routine
 *
 */

int ft767_close(RIG *rig)
{
    rig_flush(&rig->state.rigport);
    return RIG_OK;
}

/*
 * This command only functions when operating on a vfo.
 *
 */

int ft767_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_FREQ_SET};
    int retval;

    /* should the vfo be tested? */
    /* fill in first four bytes */
    to_bcd(cmd, freq / 10, 8);

    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        return retval;
    }

    retval =  ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send command: status %d\n",
                  __func__, retval);
        return retval;
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
    }

    return retval;
}

int ft767_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_MULTICMD};
    int retval;

    /* fill in p1 */
    cmd[3] = mode2rig(rig, mode);

    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        return retval;
    }

    retval =  ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send command: status %d\n",
                  __func__, retval);
        return retval;
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
    }

    return retval;
}

/*
 * Return Freq
 */

int ft767_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    /* grab freq and convert */

    switch (vfo)
    {
    case RIG_VFO_CURR:
        *freq = from_bcd_be(priv->update_data + STATUS_CURR_FREQ, 8);
        break;

    case RIG_VFO_A:
        *freq = from_bcd_be(priv->update_data + STATUS_VFOA_FREQ, 8);
        break;

    case RIG_VFO_B:
        *freq = from_bcd_be(priv->update_data + STATUS_VFOB_FREQ, 8);
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong VFO */
    }

    *freq *= 10.0; /* rig reads in 10 Hz increments*/

    return RIG_OK;
}



int ft767_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    switch (vfo)
    {
    case RIG_VFO_CURR:
        retval = rig2mode(rig, priv->update_data[STATUS_CURR_MODE], mode, width);
        break;

    case RIG_VFO_A:
        retval = rig2mode(rig, priv->update_data[STATUS_VFOA_MODE], mode, width);
        break;

    case RIG_VFO_B:
        retval = rig2mode(rig, priv->update_data[STATUS_VFOB_MODE], mode, width);
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong VFO */
    }

    return retval;
}

/*
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */
int ft767_set_vfo(RIG *rig, vfo_t vfo)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_VFOMR};
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;

    switch (vfo)
    {
    case RIG_VFO_CURR:
        return RIG_OK;

    case RIG_VFO_A:
        cmd[3] = 0x00;
        break;

    case RIG_VFO_B:
        cmd[3] = 0x01;
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong VFO */
    }

    priv->current_vfo = vfo;

    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        return retval;
    }

    retval =  ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send command: status %d\n",
                  __func__, retval);
        return retval;
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
    }

    return retval;
}

int ft767_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    *vfo = rig2vfo(priv->update_data[STATUS_FLAGS]);

    return RIG_OK;
}

int ft767_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    *ptt = (priv->update_data[STATUS_FLAGS] & 0x01) ?  RIG_PTT_ON : RIG_PTT_OFF;
    return RIG_OK;
}

/* on this rig, only one tone can be set per VFO or memory,
 * and it's active for both tx and receive.
 */

int ft767_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_TONE_SET};
    int retval;

    /* determine whether we have to send high-Q */
    switch (tone)
    {
    case 747:
    case 797:
    case 854:
    case 915:
        cmd[1] = 1; /* High Q */
        break;

    default: break;
    }

    /* fill in p2 and p1 with bcd tone value */
    to_bcd(&cmd[2], tone, 4);

    /*  cmd[3] = tone2rig(rig, tone); */

    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        return retval;
    }

    retval =  ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send command: status %d\n",
                  __func__, retval);
        return retval;
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
    }

    return retval;
}

int ft767_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: get_update_data failed with status %d\n",
                  __func__, retval);
        return retval;
    }

    retval = rig2ctcss(rig, priv->update_data[STATUS_CURR_TONE], tone);
    return retval;
}

int ft767_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    return ft767_set_ctcss_tone(rig, vfo, tone);
}

int ft767_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    return ft767_get_ctcss_tone(rig, vfo, tone);
}

/* split functions */

int ft767_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    unsigned char freq_cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_FREQ_SET};
    unsigned char vfo_cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_VFOMR};
    vfo_t curr_vfo;
    vfo_t change_vfo;
    unsigned char curr_split;
    int retval;

    retval = rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    /* This appears to always pass in VFO_CURR as the vfo */
    /* My decision is to only update the xmit VFO if we're in split mode */

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    /* find out how we're currently configured */
    curr_vfo = rig2vfo(priv->update_data[STATUS_FLAGS]);
    curr_split = priv->update_data[STATUS_FLAGS] & STATUS_MASK_SPLIT;

    if (curr_split)
    {
        switch (curr_vfo)
        {
        /* we need to set something */
        case RIG_VFO_A:
            change_vfo = RIG_VFO_B;
            break;

        case RIG_VFO_B:
            change_vfo = RIG_VFO_A;
            break;

        case RIG_VFO_MEM:
            rig_debug(RIG_DEBUG_ERR, "%s: error, in both split and memory modes\n",
                      __func__);
            return RIG_OK;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: error, unknown vfo value %s\n", __func__,
                      rig_strvfo(curr_vfo));
            return RIG_OK;
        }
    }
    else
    {
        /* not in split mode, do nothing */
        return RIG_OK;
    }

    /* fill in first four bytes */
    to_bcd(freq_cmd, tx_freq / 10, 8);

    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        return retval;
    }

    /* change to the xmit VFO */
    vfo_cmd[3] = vfo2rig(rig, change_vfo);
    retval =  ft767_send_block_and_ack(rig, vfo_cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send command: status %d\n",
                  __func__, retval);
        return retval;
    }

    /* change the freq */
    retval =  ft767_send_block_and_ack(rig, freq_cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send command: status %d\n",
                  __func__, retval);
        return retval;
    }

    /* change back to the rcv VFO */
    vfo_cmd[3] = vfo2rig(rig, curr_vfo);
    retval =  ft767_send_block_and_ack(rig, vfo_cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send command: status %d\n",
                  __func__, retval);
        return retval;
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
    }

    return RIG_OK;
}

int ft767_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;
    unsigned int offset;
    vfo_t curr_vfo;
    unsigned char curr_split;

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: get_update_data failed with status %d\n",
                  __func__, retval);
        return retval;
    }

    curr_vfo = rig2vfo(priv->update_data[STATUS_FLAGS]);
    curr_split = priv->update_data[STATUS_FLAGS] & STATUS_MASK_SPLIT;

    if (curr_split)
    {
        switch (curr_vfo)
        {
        /* we need to get something */
        case RIG_VFO_A:
            offset = STATUS_VFOB_FREQ;
            break;

        case RIG_VFO_B:
            offset = STATUS_VFOA_FREQ;
            break;

        case RIG_VFO_MEM:
            rig_debug(RIG_DEBUG_ERR, "%s: error, in both split and memory modes\n",
                      __func__);
            return RIG_OK;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: error, unknown vfo value %s\n", __func__,
                      rig_strvfo(curr_vfo));
            return RIG_OK;
        }
    }
    else
    {
        /* not in split mode, do nothing */
        return RIG_OK;
    }

    *tx_freq = from_bcd_be(priv->update_data + offset, 8);

    return RIG_OK;
}

int ft767_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                         pbwidth_t tx_width)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    unsigned char mode_cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_MULTICMD};
    unsigned char vfo_cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_VFOMR};
    vfo_t curr_vfo;
    vfo_t change_vfo;
    unsigned char curr_split;
    int retval;

    /* This appears to always pass in VFO_CURR as the vfo */
    /* My decision is to only update the xmit mode if we're in split mode */

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    /* find out how we're currently configured */
    curr_vfo = rig2vfo(priv->update_data[STATUS_FLAGS]);
    curr_split = priv->update_data[STATUS_FLAGS] & STATUS_MASK_SPLIT;

    if (curr_split)
    {
        switch (curr_vfo)
        {
        /* we need to set something */
        case RIG_VFO_A:
            change_vfo = RIG_VFO_B;
            break;

        case RIG_VFO_B:
            change_vfo = RIG_VFO_A;
            break;

        case RIG_VFO_MEM:
            rig_debug(RIG_DEBUG_ERR, "%s: error, in both split and memory modes\n",
                      __func__);
            return RIG_OK;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: error, unknown vfo value %s\n", __func__,
                      rig_strvfo(curr_vfo));
            return RIG_OK;
        }
    }
    else
    {
        /* not in split mode, do nothing */
        return RIG_OK;
    }

    /* fill in p1 */
    mode_cmd[3] = mode2rig(rig, tx_mode);

    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        return retval;
    }

    /* change to the xmit VFO */
    vfo_cmd[3] = vfo2rig(rig, change_vfo);
    retval =  ft767_send_block_and_ack(rig, vfo_cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send vfo change 1 command: status %d\n",
                  __func__, retval);
        return retval;
    }

    /* change the mode */
    retval =  ft767_send_block_and_ack(rig, mode_cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send mode command: status %d\n",
                  __func__, retval);
        return retval;
    }

    /* change back to the rcv VFO */
    vfo_cmd[3] = vfo2rig(rig, curr_vfo);
    retval =  ft767_send_block_and_ack(rig, vfo_cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to send vfo change 2command: status %d\n",
                  __func__, retval);
        return retval;
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
    }

    return RIG_OK;
}

int ft767_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                         pbwidth_t *tx_width)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;
    unsigned int offset;
    vfo_t curr_vfo;
    unsigned char curr_split;

    retval = ft767_get_update_data(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: get_update_data failed with status %d\n",
                  __func__, retval);
        return retval;
    }

    curr_vfo = rig2vfo(priv->update_data[STATUS_FLAGS]);
    curr_split = priv->update_data[STATUS_FLAGS] & STATUS_MASK_SPLIT;

    if (curr_split)
    {
        switch (curr_vfo)
        {
        /* we need to get something */
        case RIG_VFO_A:
            offset = STATUS_VFOB_MODE;
            break;

        case RIG_VFO_B:
            offset = STATUS_VFOA_MODE;
            break;

        case RIG_VFO_MEM:
            rig_debug(RIG_DEBUG_ERR, "%s: error, in both split and memory modes\n",
                      __func__);
            return RIG_OK;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: error, unknown vfo value %s\n", __func__,
                      rig_strvfo(curr_vfo));
            return RIG_OK;
        }
    }
    else
    {
        /* not in split mode, do nothing */
        return RIG_OK;
    }

    /* get the actual mode */
    retval = rig2mode(rig, priv->update_data[offset], tx_mode, tx_width);
    return retval;
}

int ft767_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};
    int retval;
    vfo_t curr_vfo;
    vfo_t future_vfo;
    unsigned char curr_split;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed tx_vfo = 0x%02x\n", __func__, tx_vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed split = 0x%02x\n", __func__, split);

    if ((tx_vfo != RIG_VFO_A) && (tx_vfo != RIG_VFO_B))
    {
        return -RIG_EINVAL;
    }

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        return retval;
    }

    /* find out which VFO we're currently using */
    curr_vfo = rig2vfo(priv->update_data[STATUS_FLAGS]);

    /*
     * If split is enabled, Set the current VFO to the opposite of
     * the one specified in tx_vfo. If split is not enabled, then don't change anything.
    */

    switch (split)
    {
    case RIG_SPLIT_OFF:
        /* turn off split, leave everything else alone */
        return ft767_set_split(rig, 0);
        break;

    case RIG_SPLIT_ON:
        switch (tx_vfo)
        {
        case RIG_VFO_CURR:

            // we need to switch VFOs
            if (curr_vfo == RIG_VFO_A)
            {
                future_vfo = RIG_VFO_B;
            }
            else if (curr_vfo == RIG_VFO_B)
            {
                future_vfo = RIG_VFO_B;
            }
            else
            {
                /* Currently using memory! */
                rig_debug(RIG_DEBUG_ERR, "%s: RIG_VFO_CURR requested when it is a memory\n",
                          __func__);
                return -RIG_EINVAL;
            }

            break;

        case RIG_VFO_A:
            future_vfo = RIG_VFO_B;
            break;

        case RIG_VFO_B:
            future_vfo = RIG_VFO_A;
            break;

        default:
            return -RIG_EINVAL;       /* sorry, wrong VFO */
        }

        rig_flush(&rig->state.rigport);

        retval = ft767_enter_CAT(rig);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
            return retval;
        }

        /* See whether we need to toggle the split state */
        curr_split = priv->update_data[STATUS_FLAGS] & STATUS_MASK_SPLIT;

        if (curr_split) { curr_split = RIG_SPLIT_ON; }

        if (curr_split != split)
        {
            cmd[3] = SUBCMD_SPLIT;
            cmd[4] = CMD_MULTICMD;
            retval =  ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);

            if (retval < 0)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: failed to send split command: status %d\n",
                          __func__, retval);
                return retval;
            }
        }

        /* now set the rx vfo if needed */
        if (curr_vfo != future_vfo)
        {
            cmd[3] = vfo2rig(rig, future_vfo);
            cmd[4] = CMD_VFOMR;
            retval =  ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);

            if (retval < 0)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: failed to send set vfo command: status %d\n",
                          __func__, retval);
                return retval;
            }
        }

        /* Make sure clarifier is off */
        if (priv->update_data[STATUS_FLAGS] & STATUS_MASK_CLAR)
        {
            cmd[3] = SUBCMD_CLAR;
            cmd[4] = CMD_MULTICMD;
            retval =  ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);

            if (retval < 0)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: failed to send set clar command: status %d\n",
                          __func__, retval);
                return retval;
            }
        }

        retval = ft767_leave_CAT(rig);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
            return retval;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int ft767_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;
    vfo_t curr_vfo;

    retval = ft767_get_update_data(rig);  /* get whole shebang from rig */

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: get_update_data failed with status %d\n",
                  __func__, retval);
        return retval;
    }

    /* TODO if SPLIT is enabled, return which VFO is the transmit VFO!!
     */

    if (priv->update_data[STATUS_FLAGS] & STATUS_MASK_SPLIT)
    {
        *split = RIG_SPLIT_ON;
    }
    else
    {
        *split = RIG_SPLIT_OFF;
    }

    curr_vfo = rig2vfo(priv->update_data[STATUS_FLAGS]);

    switch (curr_vfo)
    {
    case RIG_VFO_A:
        *tx_vfo = RIG_VFO_B;
        break;

    case RIG_VFO_B:
        *tx_vfo = RIG_VFO_A;
        break;

    default:
        /* we don't know how to deal with MEM, anything else is an error */
        /* TODO make sure this is what we want to do here */
        rig_debug(RIG_DEBUG_ERR, "%s: current vfo is %s with split\n", __func__,
                  rig_strvfo(curr_vfo));
        return -RIG_EINVAL;
        break;
    }

    return RIG_OK;
}

/* End of hamlib API-mapped functions */


/*
 * This function puts the radio in CAT mode
 */
int ft767_enter_CAT(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, CMD_CAT_SW};
    rig_debug(RIG_DEBUG_TRACE, "%s: Entered\n", __func__);
    return ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);
}

/*
 * This function takes the radio out of CAT mode
 */
int ft767_leave_CAT(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x01, CMD_CAT_SW};
    rig_debug(RIG_DEBUG_TRACE, "%s: Entered\n", __func__);
    return ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);
}


/*
 * The Yaesu interface is convoluted and braindead.
 *
 * Private helper function. The 767GX has a handshaking system that works like this:
 *
 * 5 byte command block sent to rig
 * (5 to 20 mS delay)
 * Rig echos 5 byte command block back
 * Send a 5 byte ACK block to the rig
 * (5 to 20 mS delay)
 *
 * Rig sends back a status update block (5-86 bytes)
 * The status update block is received in reverse byte order from the way it's structured
 *
 * In addition, You must send a command to enable CAT mode, and disable when done.
 * When in CAT mode, the front panel of the radio is locked out.
 *
 * There is an error in the manual, the response length for a TONE SET command
 * is 26 bytes, not 5.
 */

int ft767_send_block_and_ack(RIG *rig, unsigned char *cmd, size_t length)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    size_t replylen, cpycnt;
    unsigned char cmd_echo_buf[5];
    int retval;
    unsigned char *src, *dst;

    /* Validate command and set length of data returned */
    switch (cmd[4])
    {
    case CMD_CHECK:
    case CMD_CAT_SW:
        replylen = 86;
        break;

    case CMD_UP10HZ:
    case CMD_DN10HZ:
    case CMD_PROG_UP:
    case CMD_PROG_DN:
    case CMD_BAND_UP:
    case CMD_BAND_DN:
    case CMD_FREQ_SET:
    case CMD_VFOMR:
    case CMD_ACK:
        replylen = 5;
        break;

    case CMD_TONE_SET:
        replylen = 26; /* the manual is wrong */
        break;

    case CMD_MULTICMD:
        if (cmd[3] <= 0x15)
        {
            replylen = 8;
        }
        else
        {
            switch (cmd[3])
            {
            case SUBCMD_HG_HAM:
            case SUBCMD_HG_GEN:
            case SUBCMD_SPLIT:
            case SUBCMD_CLAR:
            case SUBCMD_MTOV:
                replylen = 26;
                break;

            case SUBCMD_VTOM:
                replylen = 68;
                break;

            case SUBCMD_SWAP:
            case SUBCMD_ACLR:
                replylen = 5;
                break;

            default:
                /* invalid or unknown sub-command */
                rig_debug(RIG_DEBUG_ERR, "%s: invalid sub-command 0x%x for command 0x%x\n",
                          __func__, cmd[3], cmd[4]);
                return -RIG_EINVAL;
            }
        }

        break;

    default:
        /* invalid or unknown command */
        rig_debug(RIG_DEBUG_ERR, "%s: invalid command 0x%x\n",
                  __func__, cmd[4]);
        return -RIG_EINVAL;
    }

    /* send the command block */
    write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    /* read back the command block echo */
    retval = read_block(&rig->state.rigport,
                        cmd_echo_buf,
                        YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: read_block failed: %s\n", __func__,
                  rigerror(retval));
        return retval;
    }

    /* see if it matches the command we sent */
    if (memcmp(cmd_echo_buf, cmd, YAESU_CMD_LENGTH))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Command echo doesn't match\n",
                  __func__);
        return -RIG_EINVAL;
    }

    /* send the ACK */
    write_block(&rig->state.rigport, priv->ack_cmd, YAESU_CMD_LENGTH);

    /* read back the response (status bytes) */
    retval = read_block(&rig->state.rigport,
                        priv->rx_data,
                        replylen);

    // update data
    if (retval != replylen)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Got unexpected number of bytes %d in response\n",
                  __func__, retval);
        return -RIG_EINVAL;
    }

    /* reverse the data buffer returned from the rig */
    src = &priv->rx_data[0];
    dst = &priv->update_data[replylen - 1];
    cpycnt = replylen;

    while (cpycnt--)
    {
        *dst-- = *src++;
    }

    return RIG_OK;
}

/*
 * private helper function. Retrieves update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 *
 * need to use this when doing ft767_get_* stuff
 */


int ft767_get_update_data(RIG *rig)
{
    /*  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x01, CMD_CHECK}; */
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;

    rig_flush(&rig->state.rigport);

    /* Entering CAT updates our data structures */
    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        return retval;
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: status = 0x%02x\n", __func__,
              priv->update_data[STATUS_FLAGS]);

    return RIG_OK;
}

int ft767_set_split(RIG *rig, unsigned int split)
{
    struct ft767_priv_data *priv = (struct ft767_priv_data *)rig->state.priv;
    int retval;
    unsigned int curr_split;

    rig_flush(&rig->state.rigport);

    /* Entering CAT updates our data structures */
    retval = ft767_enter_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: enter_CAT %d\n", __func__, retval);
        return retval;
    }

    /* See whether we need to toggle */
    curr_split = priv->update_data[STATUS_FLAGS] & STATUS_MASK_SPLIT;

    rig_debug(RIG_DEBUG_TRACE, "%s called curr_split = %u, split = %u\n", __func__,
              curr_split, split);

    if (curr_split ^ split)
    {
        unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, SUBCMD_SPLIT, CMD_MULTICMD};
        retval =  ft767_send_block_and_ack(rig, cmd, YAESU_CMD_LENGTH);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: failed to send command: status %d\n",
                      __func__, retval);
            return retval;
        }
    }

    retval = ft767_leave_CAT(rig);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: leave_CAT %d\n", __func__, retval);
        return retval;
    }

    return RIG_OK;
}

unsigned char vfo2rig(RIG *rig, vfo_t vfo)
{
    switch (vfo)
    {
    case RIG_VFO_CURR:
        return RIG_OK;

    case RIG_VFO_A:
        return 0x00;
        break;

    case RIG_VFO_B:
        return 0x01;
        break;

    case RIG_VFO_MEM:
        return 0x02;
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong VFO */
    }
}

vfo_t rig2vfo(unsigned char status)
{
    if (status & 0x20)
    {
        return RIG_VFO_MEM;
    }
    else if (status & 0x10)
    {
        return RIG_VFO_B;
    }
    else
    {
        return RIG_VFO_A;
    }
}

int mode2rig(RIG *rig, rmode_t mode)
{
    int md;

    /*
     * translate mode from generic to ft767 specific
     */
    switch (mode)
    {
    case RIG_MODE_LSB:    md = SUBCMD_MODE_LSB; break;

    case RIG_MODE_USB:    md = SUBCMD_MODE_USB; break;

    case RIG_MODE_CW: md = SUBCMD_MODE_CW; break;

    case RIG_MODE_AM: md = SUBCMD_MODE_AM; break;

    case RIG_MODE_FM: md = SUBCMD_MODE_FM; break;

    case RIG_MODE_PKTFM:  md = SUBCMD_MODE_FSK; break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    return md;
}

int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width)
{
    /*
     * translate mode from ft767 specific to generic
     */
    switch (md & 0x07)
    {
    case MODE_LSB:    *mode = RIG_MODE_LSB; break;

    case MODE_USB:    *mode = RIG_MODE_USB; break;

    case MODE_CW:     *mode = RIG_MODE_CW; break;

    case MODE_AM:     *mode = RIG_MODE_AM; break;

    case MODE_FM:     *mode = RIG_MODE_FM; break;

    case MODE_FSK:    *mode = RIG_MODE_PKTFM; break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    return RIG_OK;
}

int rig2ctcss(RIG *rig, unsigned char tn, tone_t *tone)
{
    /*
     * translate tone from ft767 specific to generic
     */
    switch (tn)
    {
#ifdef USE_YAESU_PUBLISHED_TONES

    /* Yaesu documentation */
    case 0x3E: *tone = 670; break;

    case 0x1D: *tone = 670; break; /* High Q */

    case 0x3D: *tone = 719; break;

    case 0x1C: *tone = 719; break; /* High Q */

    case 0x1B: *tone = 747; break; /* High Q */

    case 0x3C: *tone = 770; break;

    case 0x1A: *tone = 770; break; /* High Q */

    case 0x19: *tone = 797; break; /* High Q */

    case 0x3B: *tone = 825; break;

    case 0x18: *tone = 825; break; /* High Q */

    case 0x17: *tone = 854; break; /* High Q */

    case 0x3A: *tone = 885; break;

    case 0x16: *tone = 885; break; /* High Q */

    case 0x15: *tone = 915; break; /* High Q */

    case 0x39: *tone = 948; break;

    case 0x38: *tone = 1000; break;

    case 0x37: *tone = 1035; break;

    case 0x36: *tone = 1072; break;

    case 0x35: *tone = 1109; break;

    case 0x34: *tone = 1148; break;

    case 0x33: *tone = 1188; break;

    case 0x32: *tone = 1230; break;

    case 0x31: *tone = 1273; break;

    case 0x30: *tone = 1318; break;

    case 0x2F: *tone = 1365; break;

    case 0x2E: *tone = 1413; break;

    case 0x2D: *tone = 1462; break;

    case 0x2C: *tone = 1514; break;

    case 0x2B: *tone = 1567; break;

    case 0x2A: *tone = 1622; break;

    case 0x29: *tone = 1679; break;

    case 0x28: *tone = 1738; break;

    case 0x27: *tone = 1799; break;

    case 0x26: *tone = 1862; break;

    case 0x25: *tone = 1928; break;

    case 0x24: *tone = 2035; break;

    case 0x23: *tone = 2107; break;

    case 0x22: *tone = 2181; break;

    case 0x21: *tone = 2257; break;

    case 0x20: *tone = 2336; break;

    case 0x1F: *tone = 2418; break;

    case 0x1E: *tone = 2503; break;
#else

    /* values found by experimentation */
    case  0: *tone = 670; break;

    case 33: *tone = 670; break; /* High Q */

    case 01: *tone = 719; break;

    case 34: *tone = 719; break; /* High Q */

    case 35: *tone = 747; break; /* High Q */

    case  2: *tone = 770; break;

    case 36: *tone = 770; break; /* High Q */

    case 37: *tone = 797; break; /* High Q */

    case  3: *tone = 825; break;

    case 38: *tone = 825; break; /* High Q */

    case 39: *tone = 854; break; /* High Q */

    case  4: *tone = 885; break;

    case 40: *tone = 885; break; /* High Q */

    case 41: *tone = 915; break; /* High Q */

    case  5: *tone = 948; break;

    case  6: *tone = 1000; break;

    case  7: *tone = 1035; break;

    case  8: *tone = 1072; break;

    case  9: *tone = 1109; break;

    case 10: *tone = 1148; break;

    case 11: *tone = 1188; break;

    case 12: *tone = 1230; break;

    case 13: *tone = 1273; break;

    case 14: *tone = 1318; break;

    case 15: *tone = 1365; break;

    case 16: *tone = 1413; break;

    case 17: *tone = 1462; break;

    case 18: *tone = 1514; break;

    case 19: *tone = 1567; break;

    case 20: *tone = 1622; break;

    case 21: *tone = 1679; break;

    case 22: *tone = 1738; break;

    case 23: *tone = 1799; break;

    case 24: *tone = 1862; break;

    case 25: *tone = 1928; break;

    case 26: *tone = 2035; break;

    case 27: *tone = 2107; break;

    case 28: *tone = 2181; break;

    case 29: *tone = 2257; break;

    case 30: *tone = 2336; break;

    case 31: *tone = 2418; break;

    case 32: *tone = 2503; break;
#endif

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid tone value from rig: 0x%02x\n",
                  __func__, tn);
        return -RIG_EINVAL;         /* sorry, wrong TONE */
        break;
    }

    return RIG_OK;
}
