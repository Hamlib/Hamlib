/*
 * hamlib - (C) Stephane Fillod 2002, 2003 (fillods at users.sourceforge.net)
 *
 * ft980.h - (C) Mat Breton, 2020
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-990 using the "CAT" interface
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

/* This file is a bit of a hack at the moment, containing both public and private declarations
 *    ToDo: Fix this point later, but temporarily necessary to sort stuff out right now
 */

#ifndef _FT980_H
#define _FT980_H 1

/* Lets make the Linters Happy - remove later*/
#ifndef _RIG_H
#include "hamlib/rig.h"
#endif

#ifndef _YAESU_H
#include "yaesu.h"
#endif

#ifndef _BANDPLAN_H
#include "bandplan.h"
#endif
/* End of Happy Linter Section */

/*************************************************************************************
 * Semi-Public prototypes: need to be placed before the caps construct
 *     HAMLIB API implementation
 */
static int ft980_init(RIG *rig);
static int ft980_cleanup(RIG *rig);
static int ft980_open(RIG *rig);
static int ft980_close(RIG *rig);

static int ft980_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft980_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft980_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft980_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft980_set_mem(RIG *rig, vfo_t vfo, int ch);
static int ft980_get_mem(RIG *rig, vfo_t vfo, int *ch);

static int ft980_set_vfo(RIG *rig, vfo_t vfo);
static int ft980_get_vfo(RIG *rig, vfo_t *vfo);

#if 0
static int ft980_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft980_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);

static int ft980_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
static int ft980_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);

static int ft980_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft980_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft980_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width);

static int ft980_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
static int ft980_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
#endif


/*************************************************************************************
 * Constants (enums and defines, w/ a couple typedefs thrown in)
 */

/* Handy constants */
#ifndef TRUE
#define TRUE 1
#endif
#define ON TRUE
#ifndef FALSE
#define FALSE 0
#endif
#define OFF FALSE

/* FT-980 Status Return Message Lengths */
#define FT980_ALL_STATUS_LENGTH           148
#define FT980_OTHER_STATUS_LENGTH         22
#define FT980_RPTRSPLT_STATUS_LENGTH      6
#define FT980_UPDN_STATUS_LENGTH          5
#define FT980_ONEBYTE_STATUS_LENGTH       1

/* FT-980 Timing Constants (for caps structure)*/
/* Serial write timing values, in mS */
#define FT980_WRITE_DELAY               80
/* Delay sequential fast writes */
#define FT980_POST_WRITE_DELAY          5
/* Serial write timing values, in mS */
#define FT980_PACING_INTERVAL           5
/* The following may be deprecated into rig.c cache with 4.1 release*/
#define FT980_CACHE_TIMEOUT 500
/* Rough safe value for default read timeout = # bytes x (xmit speed + pacing_interval) x safety factor
#define FT980_DEFAULT_READ_TIMEOUT FT980_ALL_DATA_LENGTH * ( 1.7 + FT980_PACING_INTERVAL) * 6
*/
#define FT980_DEFAULT_READ_TIMEOUT      2000

/* FT-980 Configuration Constants (for caps structure)*/
#define FT980_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT980_ANTS (RIG_ANT_1)

/*  */
#define FT980_VFOS (RIG_VFO_CURR|RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MEM)

/* TODO: RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN */
#define FT980_VFO_OPS (RIG_OP_NONE)
/* TODO: RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN
#define FT890_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_CPY|RIG_OP_UP|RIG_OP_DOWN)
*/

/*
 * ft980 rigs capabilities.
 *
 * Protocol is documented in FT 980 Technical Supplement, page 13.
 *
 */
#define FT980_MEM_CAP {          \
                      .freq = 1,       \
                      .mode = 1,       \
                      .width = 1,      \
  }

const struct rig_caps ft980_caps =
{
    RIG_MODEL(RIG_MODEL_FT980),
    .model_name =         "FT-980",
    .mfg_name =           "Yaesu",
    .version =            "20200114.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_SERIAL_RTS,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    4800,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        FT980_WRITE_DELAY,
    .post_write_delay =   FT980_POST_WRITE_DELAY,
    .timeout =            FT980_DEFAULT_READ_TIMEOUT,
    .retry =              3,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =       RIG_FUNC_NONE,
    .has_get_level =      RIG_LEVEL_NONE,
    .has_set_level =      RIG_LEVEL_NONE,
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .vfo_ops =            FT980_VFO_OPS,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(10000),
    .max_xit =            Hz(10000),
    .max_ifshift =        Hz(1500),
    .targetable_vfo =     RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =         RIG_TRN_OFF,
    .bank_qty =           0,
    .chan_desc_sz =       0,

    .chan_list =          {
        {1, 16, RIG_MTYPE_MEM, FT980_MEM_CAP},
    },

    .rx_range_list1 =     {
        {kHz(150), MHz(30) - 100, FT980_MODES, -1, -1, FT980_VFOS, FT980_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, RIG_MODE_SSB | RIG_MODE_CW, W(5), W(100), FT980_VFOS, FT980_ANTS),
        FRQ_RNG_HF(1, RIG_MODE_FM | RIG_MODE_RTTY, W(2), W(50), FT980_VFOS, FT980_ANTS),
        FRQ_RNG_HF(1, RIG_MODE_AM, W(2), W(25), FT980_VFOS, FT980_ANTS),
        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(150), MHz(30) - 100, FT980_MODES, -1, -1, FT980_VFOS, FT980_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, RIG_MODE_SSB | RIG_MODE_CW, W(5), W(100), FT980_VFOS, FT980_ANTS),
        FRQ_RNG_HF(2, RIG_MODE_FM | RIG_MODE_RTTY, W(2), W(50), FT980_VFOS, FT980_ANTS),
        FRQ_RNG_HF(2, RIG_MODE_AM, W(2), W(25), FT980_VFOS, FT980_ANTS),
        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FT980_MODES,  Hz(10)},
        {FT980_MODES, kHz(5)},
        {FT980_MODES, kHz(500)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =            {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.5)},
        {RIG_MODE_CW,   Hz(300)},
        {RIG_MODE_FM,   kHz(12)},
        {RIG_MODE_AM,   kHz(5)},
        {RIG_MODE_AM,   kHz(3)},

        RIG_FLT_END,
    },

    .rig_init  =          ft980_init,
    .rig_cleanup =        ft980_cleanup,

    .rig_open  =          ft980_open,
    .rig_close =          ft980_close,

    .set_freq =           ft980_set_freq,
    .get_freq =           ft980_get_freq,
    .set_mode =           ft980_set_mode,
    .get_mode =           ft980_get_mode,

    .set_mem  =           ft980_set_mem,
    .get_mem  =           ft980_get_mem,

    .get_vfo  =           ft980_get_vfo,
    .set_vfo  =           ft980_set_vfo,

#ifdef XXREMOVEDXX
    .set_split_vfo  =     ft980_set_split_vfo,
    .set_split_freq =     ft980_set_split_freq,
    .set_split_mode =     ft980_set_split_mode,

    .set_rptr_shift =     ft980_set_rptr_shift,
    .set_rptr_offs  =     ft980_set_rptr_offs,
#endif
};

/* FT-980 Status Return Lengths (for memory structure)*/
#define FT980_ALL_STATUS_LENGTH           148
#define FT980_OTHER_STATUS_LENGTH         22
#define FT980_RPTRSPLT_STATUS_LENGTH      6
#define FT980_UPDN_STATUS_LENGTH          5
#define FT980_ONEBYTE_STATUS_LENGTH       1

/* Do not change the order of the memory structure */
typedef struct _ft980_memory_t
{
    unsigned char mem_16[4];
    unsigned char vfo_16;
    unsigned char mode_16;
    unsigned char mem_15[4];
    unsigned char vfo_15;
    unsigned char mode_15;
    unsigned char mem_14[4];
    unsigned char vfo_14;
    unsigned char mode_14;
    unsigned char mem_13[4];
    unsigned char vfo_13;
    unsigned char mode_13;
    unsigned char mem_12[4];
    unsigned char vfo_12;
    unsigned char mode_12;
    unsigned char mem_11[4];
    unsigned char vfo_11;
    unsigned char mode_11;
    unsigned char mem_10[4];
    unsigned char vfo_10;
    unsigned char mode_10;
    unsigned char mem_9[4];
    unsigned char vfo_9;
    unsigned char mode_9;
    unsigned char mem_8[4];
    unsigned char vfo_8;
    unsigned char mode_8;
    unsigned char mem_7[4];
    unsigned char vfo_7;
    unsigned char mode_7;
    unsigned char mem_6[4];
    unsigned char vfo_6;
    unsigned char mode_6;
    unsigned char mem_5[4];
    unsigned char vfo_5;
    unsigned char mode_5;
    unsigned char mem_4[4];
    unsigned char vfo_4;
    unsigned char mode_4;
    unsigned char mem_3[4];
    unsigned char vfo_3;
    unsigned char mode_3;
    unsigned char mem_2[4];
    unsigned char vfo_2;
    unsigned char mode_2;
    unsigned char mem_1[4];
    unsigned char vfo_1;
    unsigned char mode_1;
    unsigned char clar_freq[4];
    unsigned char gen_vfo_freq[4];
    unsigned char ham_vfo_freq[4];
    unsigned char vfo; /* ??? */
    unsigned char mode; /* ??? */
    unsigned char mem_shift_freq[4];
    unsigned char mem_clar_freq[4];
    unsigned char mem_vfo;
    unsigned char mem_mode;
    unsigned char ldb_flag;
    unsigned char ext_ctl_flag;
    unsigned char if_shift;
    unsigned char rptr_split_code;
    unsigned char fsk_shift;
    unsigned char if_width;
    unsigned char mem_shift_flag;
    unsigned char clar_flag;
    unsigned char tab_flag;
    unsigned char freq_select_sws;
    unsigned char offset_sw;
    unsigned char mode_sw;
    unsigned char mem_ch_sw;
    unsigned char lower_tab_freq[4];
    unsigned char upper_tab_freq[4];
    unsigned char op_vfo;
    unsigned char op_mode;
    unsigned char op_freq[4];
    unsigned char status_flag_bits;
} _ft980_memory_t;

/*
 * Start of Return Status Code Defines
 * These are the memory decode define (for the _ft980_memory_structure)
 */

/* status_flag_bits: STATUS FLAG BYTE BIT decode MASKS */
#define FT_980_STATUSFLAG_TXRX_MASK    0x01
#define FT_980_STATUSFLAG_SPLIT_MASK   0x08
#define FT_980_STATUSFLAG_VFO_MASK     0x20
#define FT_980_STATUSFLAG_UPDN_MASK    0X30
#define FT_980_STATUSFLAG_CLAR_MASK    0x40

/* op_vfo: VFO decode: Main, Gen, and 3 unused AUX */
#define FT980_VFO_HAM_SEL  0x80
#define FT980_VFO_GEN_SEL  0x00
#define FT980_VFO_AUX1_SEL 0x81
#define FT980_VFO_AUX2_SEL 0x82
#define FT980_VFO_AUX3_SEL 0x83

/* freq_select_sws: Frequency source decode, either a VFO, Mem, or Split */
#define FT980_FREQ_SRC_SW_VFO  0x00
#define FT980_FREQ_SRC_SW_MR   0x01
#define FT980_FREQ_SRC_SW_RXM  0x02
#define FT980_FREQ_SRC_SW_RXV  0x03

/* Clarifier Active Decode Masks */
#define FT980_CLAR_RX_FLAG  0x20
#define FT980_CLAR_TX_FLAG  0x40

/* FSK Shift Decode */
#define FT980_FSK_SHIFT_INT  0x00
#define FT980_FSK_SHIFT_425  0x40
#define FT980_FSK_SHIFT_850  0x80
#define FT980_FSK_SHIFT_170  0xC0

/* Repeater Split Codes */
/* TBD: Is this table even needed */

/*
 * Start of Command Code Defines
 */
static const unsigned char cmd_OK[YAESU_CMD_LENGTH]     = { 0x00, 0x00, 0x00, 0x00, 0x0B};
static const unsigned char cmd_ON_OFF[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};

/* Opcodes 00 to 0B */
enum ft980_native_cmd_e {
    FT980_CMD_TOGGLE_EXT_CNTL   =  0x00,
    FT980_CMD_ALL_STATUS_CHECK  =  0x01,
    FT980_CMD_UP_10HZ           =  0x02,
    FT980_CMD_DN_10HZ           =  0x03,
    FT980_CMD_IF_WIDTH          =  0x04,
    FT980_CMD_IF_SHIFT          =  0x05,
    FT980_CMD_FSK_FREQ          =  0x06,
    FT980_CMD_RPTR_SPLIT_CODE   =  0x07,
    FT980_CMD_FREQ              =  0x08,
    FT980_CMD_LDB_SET           =  0x09,
    FT980_CMD_USER_PROG         =  0x0A,
    FT980_CMD0A_OK_SIGNAL       =  0x0B
};

/* OpCode command 0x0A byte 3 sub-codes */
/* The first 16 subcodes 00-0F are to command the memory channel */

// Operating Mode Status
enum ft980_native_cmd0A_e {
    FT980_CMD0A_MD_LSB  = 0x10,
    FT980_CMD0A_MD_USB  = 0x11,
    FT980_CMD0A_MD_CW   = 0x12,
    FT980_CMD0A_MD_CWN  = 0x13,
    FT980_CMD0A_MD_AM   = 0x14,
    FT980_CMD0A_MD_AMN  = 0x15,
    FT980_CMD0A_MD_RTTY = 0x16,
    FT980_CMD0A_MD_FM   = 0x17,

    FT980_CMD0A_SHOW_OFFSET   = 0x1B,

    /* Frequency Source Select */
    FT980_CMD0A_FREQ_SEL_RXM  = 0x1C,
    FT980_CMD0A_FREQ_SEL_RXV  = 0x1D,
    FT980_CMD0A_FREQ_SEL_MR   = 0x1E,
    FT980_CMD0A_FREQ_SEL_VFO  = 0x1F,

    /* VFO Select; Question ... how do you set AUX1,2,3? */
    FT980_CMD0A_VFO_SEL_GEN = 0x21,
    FT980_CMD0A_VFO_SEL_HAM = 0x22,

    /* Set Tab to Current Operating Frequency */
    FT980_CMD0A_SET_UPPER_TAB = 0x23,
    FT980_CMD0A_SET_LOWER_TAB = 0x24,
    FT980_CMD0A_TOGGLE_TABS   = 0x25,

    /* Clarifier Commands */
    FT980_CMD0A_TOGGLE_TX_CLAR   = 0x26,
    FT980_CMD0A_TOGGLE_RX_CLAR   = 0x27,

    /* Memory Commands */
    FT980_CMD0A_MEM_SHIFT   = 0x28,
    FT980_CMD0A_MEM_WRITE   = 0x29,

    /* More Frequency Skips */
    FT980_CMD0A_UP_100Hz       = 0x18,
    FT980_CMD0A_DN_100Hz       = 0x20,
    FT980_CMD0A_UP_5KHz        = 0x2C,
    FT980_CMD0A_DN_5KHz        = 0x2B,
    FT980_CMD0A_BANDUP         = 0x2F,
    FT980_CMD0A_BANDOWN        = 0x2D

                                 /* Missing 0A Sub-op-codes: 19,1A,2A,2E, after 2F */
};
/* End of OpCode command 0x0A byte 3 sub-codes */

/*************************************************************************************
 * Macro Definitions
 */
#define UPDATE_DATA_OFS(p, o) (((unsigned char*)((p)+1))-(o))

#endif /* _FT980_H */
