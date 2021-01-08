/*
 * ft980.c - (C) Stephane Fillod 2004-2010
 *           (C) Wolfgang Buesser 2010
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-980 using the "CAT" interface
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "bandplan.h"
#include "yaesu.h"

#define FT980_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_RTTY|RIG_MODE_FM)

#define FT980_ANTS (RIG_ANT_1)

/* TODO: RIG_VFO_HAM|RIG_VFO_GEN ? */
#define FT980_VFOS (RIG_VFO_MAIN)

/* TODO: RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN */
#define FT980_VFO_OPS (RIG_OP_NONE)

typedef struct _ft980_memory_t
{
    unsigned char mem_16[4]; unsigned char vfo_16; unsigned char mode_16;
    unsigned char mem_15[4]; unsigned char vfo_15; unsigned char mode_15;
    unsigned char mem_14[4]; unsigned char vfo_14; unsigned char mode_14;
    unsigned char mem_13[4]; unsigned char vfo_13; unsigned char mode_13;
    unsigned char mem_12[4]; unsigned char vfo_12; unsigned char mode_12;
    unsigned char mem_11[4]; unsigned char vfo_11; unsigned char mode_11;
    unsigned char mem_10[4]; unsigned char vfo_10; unsigned char mode_10;
    unsigned char mem_9[4]; unsigned char vfo_9; unsigned char mode_9;
    unsigned char mem_8[4]; unsigned char vfo_8; unsigned char mode_8;
    unsigned char mem_7[4]; unsigned char vfo_7; unsigned char mode_7;
    unsigned char mem_6[4]; unsigned char vfo_6; unsigned char mode_6;
    unsigned char mem_5[4]; unsigned char vfo_5; unsigned char mode_5;
    unsigned char mem_4[4]; unsigned char vfo_4; unsigned char mode_4;
    unsigned char mem_3[4]; unsigned char vfo_3; unsigned char mode_3;
    unsigned char mem_2[4]; unsigned char vfo_2; unsigned char mode_2;
    unsigned char mem_1[4]; unsigned char vfo_1; unsigned char mode_1;
    unsigned char CLAR[4];
    unsigned char GEN[4]; unsigned char HAM[4]; unsigned char vfo;
    unsigned char mode;
    unsigned char ff_1[4];
    unsigned char ff_2[4];
    unsigned char vfo_mem;
    unsigned char mode_mem;
    unsigned char LDB;
    unsigned char EXT_CTL;
    unsigned char IF_SHIFT;
    unsigned char SPLIT_CODE;
    unsigned char FSK_SHIFT;
    unsigned char IF_WIDTH;
    unsigned char MEM_SHIFT;
    unsigned char CLAR_FLAG;
    unsigned char TAB_FLAG;
    unsigned char SELECT_SW;
    unsigned char OFFSET_SW;
    unsigned char MODE_SW;
    unsigned char MEM_CH_SW;
    unsigned char LOW_TAB[4];
    unsigned char UP_TAB[4];
    unsigned char VFO_STATUS;
    unsigned char OP_MODE;
    unsigned char OP_FREQ[4];
    unsigned char STATUS_FLAG;
} _ft980_memory_t;

/*
 * Private data
 */
struct ft980_priv_data
{
    _ft980_memory_t update_data;          /* returned data */
    struct timeval status_tv;             /* update_data caching */
};

#define UPDATE_DATA_OFS(p, o) (((unsigned char*)((p)+1))-(o))

static const char cmd_OK[YAESU_CMD_LENGTH]     = { 0x00, 0x00, 0x00, 0x00, 0x0B};
static const char cmd_ON_OFF[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};

/* Private helper function prototypes */


static int ft980_transaction(RIG *rig, const unsigned char *cmd,
                             unsigned char *data, int expected_len);
static int ft980_get_status_data(RIG *rig);

static int ft980_open(RIG *rig);
static int ft980_close(RIG *rig);

static int ft980_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft980_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft980_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft980_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft980_set_mem(RIG *rig, vfo_t vfo, int ch);
static int ft980_get_mem(RIG *rig, vfo_t vfo, int *ch);

#if 0
static int ft980_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft980_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft980_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width);

static int ft980_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ft980_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
static int ft980_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
#endif

/*
 * ft980 rigs capabilities.
 */
#define FT980_MEM_CAP {          \
                    .freq = 1,       \
                    .mode = 1,       \
                    .width = 1,      \
}

#define FT980_CACHE_TIMEOUT 500 /* ms */

/*
 * ft980 rigs capabilities.
 *
 * Protocol is documented in FT 980 Technical Supplement, page 13.
 *
 * TODO:
 */

const struct rig_caps ft980_caps =
{
    RIG_MODEL(RIG_MODEL_FT980),
    .model_name =         "FT-980",
    .mfg_name =           "Yaesu",
    .version =            "20200114.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_ALPHA,
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
    .write_delay =        80,
    .post_write_delay =   0,
    .timeout =            2000,
    .retry =              3,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =       RIG_FUNC_NONE,
    .has_get_level =      RIG_LEVEL_NONE,
    .has_set_level =      RIG_LEVEL_NONE,
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .vfo_ops =            FT980_VFO_OPS,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(10000),
    .max_xit =            Hz(10000),
    .max_ifshift =        Hz(1500),
    .targetable_vfo =     RIG_TARGETABLE_NONE,
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

    .rig_open =       ft980_open,
    .rig_close =      ft980_close,

    .set_freq =           ft980_set_freq,
    .get_freq =           ft980_get_freq,
    .set_mode =           ft980_set_mode,
    .get_mode =           ft980_get_mode,

    .set_mem  =           ft980_set_mem,
    .get_mem  =           ft980_get_mem,

#ifdef XXREMOVEDXX
    .get_level =          ft980_get_level,
    .set_level =          ft980_set_level,

    .set_split_vfo =      ft980_set_split_vfo,
    .set_split_freq =     ft980_set_split_freq,
    .set_split_mode =     ft980_set_split_mode,

    .set_rptr_shift =     ft980_set_rptr_shift,
    .set_rptr_offs =      ft980_set_rptr_offs,
#endif
};


static void dump_freq(unsigned char *data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%02x%02x%02x%02x ", data[3], data[2], data[1],
              data[0]);
}

static void dump_vfo(unsigned char data)
{
    switch ((unsigned int)data)
    {
    case 0:   rig_debug(RIG_DEBUG_VERBOSE, "%s", "GEN"); break;

    case 128: rig_debug(RIG_DEBUG_VERBOSE, "%s", "HAM"); break;
    }
}


static void dump_mode(unsigned char data)
{
    switch ((unsigned int)data)
    {
    case 0: rig_debug(RIG_DEBUG_VERBOSE, "%s", " LSB\n"); break;

    case 1: rig_debug(RIG_DEBUG_VERBOSE, "%s", " USB\n"); break;

    case 2: rig_debug(RIG_DEBUG_VERBOSE, "%s", " CW-W\n"); break;

    case 3: rig_debug(RIG_DEBUG_VERBOSE, "%s", " CW-N\n"); break;

    case 4: rig_debug(RIG_DEBUG_VERBOSE, "%s", " AM-W\n"); break;

    case 5: rig_debug(RIG_DEBUG_VERBOSE, "%s", " AM-N\n"); break;

    case 6: rig_debug(RIG_DEBUG_VERBOSE, "%s", " FSK\n"); break;

    case 7: rig_debug(RIG_DEBUG_VERBOSE, "%s", " FM\n"); break;
    }
}

static void dump_switch(unsigned char data)
{
    switch ((unsigned int)data)
    {
    case 0: rig_debug(RIG_DEBUG_VERBOSE, "%s", "OFF"); break;

    case 1: rig_debug(RIG_DEBUG_VERBOSE, "%s", "ON "); break;
    }
}

static void dump_IF_SHIFT(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "IF_SHIFT        :\%d\n", data - 15);
}

static void dump_SPLIT_CODE(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "SPLIT_CODE      :\%02x\n", data);
}

static void dump_FSK_SHIFT(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "FSK_SHIFT       :\%02x\n", data);
}

static void dump_IF_WIDTH(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "IF_WIDTH        :\%d\n", data);
}

static void dump_MEM_SHIFT(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "MEM_SHIFT       :");

    switch ((unsigned int)data)
    {
    case 0:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "OFF\n"); break;

    case 16: rig_debug(RIG_DEBUG_VERBOSE, "%s", "ON\n"); break;
    }
}
static void dump_CLAR_FLAG(unsigned char data)
{
    unsigned char RX_CLAR = data & 0x20;
    unsigned char TX_CLAR = data & 0x40;
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "CLAR_SHIFT RX/TX:");

    switch ((unsigned int)RX_CLAR)
    {
    case 0:     rig_debug(RIG_DEBUG_VERBOSE, "%s", "OFF "); break;

    case 0x20:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "ON  "); break;
    }

    switch ((unsigned int)TX_CLAR)
    {
    case 0:     rig_debug(RIG_DEBUG_VERBOSE, "%s", " OFF "); break;

    case 0x40:  rig_debug(RIG_DEBUG_VERBOSE, "%s", " ON  "); break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");
}
static void dump_TAB_FLAG(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "TAB FLAG        :");

    switch (data)
    {
    case 0:     rig_debug(RIG_DEBUG_VERBOSE, "%s", "OFF\n"); break;

    case 0x80:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "ON\n"); break;
    }
}
static void dump_SELECT_SW(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "SELECT_SW       :");

    switch ((unsigned int)data)
    {
    case 0:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "VFO "); break;

    case 1:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "MR  "); break;

    case 2:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "RX_M"); break;

    case 3:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "RX_V"); break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");
}
static void dump_MODE_SW(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "MODE_SW         :");

    switch ((unsigned int)data)
    {
    case 0:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "LSB "); break;

    case 1:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "USB "); break;

    case 2:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "CW-W"); break;

    case 3:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "CW-N"); break;

    case 4:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "AM-W"); break;

    case 5:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "AM-N"); break;

    case 6:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "FSK"); break;

    case 7:  rig_debug(RIG_DEBUG_VERBOSE, "%s", "FM"); break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");
}
static void dump_MEM_CH_SW(unsigned char data)
{
    rig_debug(RIG_DEBUG_VERBOSE, "MEM_CH_SW       :%d\n", data + 1);
}
static void dump_STATUS_FLAG(unsigned char data)
{
    unsigned char TX    = data & 0x01;
    unsigned char SPLIT = data & 0x08;
    unsigned char VFO   = data & 0x20;
    unsigned char CLAR  = data & 0x40;
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "STATUS_FLAG     :");

    if (TX)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s", "TX ");
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s", "RX ");
    }

    if (SPLIT)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s", "SPLIT   ");
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s", "SIMPLEX ");
    }

    if (VFO)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s", "VFO    ");
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s", "MEMORY ");
    }

    if (CLAR)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s", "CLAR_ON ");
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s", "CLAR_OFF");
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");
}


static void dump_memory(_ft980_memory_t *memory)
{

    if (!rig_need_debug(RIG_DEBUG_VERBOSE))
    {
        return;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_1          :");
    dump_freq(memory->mem_1);
    dump_vfo(memory->vfo_1);
    dump_mode(memory->mode_1);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_2          :");
    dump_freq(memory->mem_2);
    dump_vfo(memory->vfo_2);
    dump_mode(memory->mode_2);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_3          :");
    dump_freq(memory->mem_3);
    dump_vfo(memory->vfo_3);
    dump_mode(memory->mode_3);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_4          :");
    dump_freq(memory->mem_4);
    dump_vfo(memory->vfo_4);
    dump_mode(memory->mode_4);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_5          :");
    dump_freq(memory->mem_5);
    dump_vfo(memory->vfo_5);
    dump_mode(memory->mode_5);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_6          :");
    dump_freq(memory->mem_6);
    dump_vfo(memory->vfo_6);
    dump_mode(memory->mode_6);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_7          :");
    dump_freq(memory->mem_7);
    dump_vfo(memory->vfo_7);
    dump_mode(memory->mode_7);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_8          :");
    dump_freq(memory->mem_8);
    dump_vfo(memory->vfo_8);
    dump_mode(memory->mode_8);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_9           :");
    dump_freq(memory->mem_9);
    dump_vfo(memory->vfo_9);
    dump_mode(memory->mode_9);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_10          :");
    dump_freq(memory->mem_10);
    dump_vfo(memory->vfo_10);
    dump_mode(memory->mode_10);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_11          :");
    dump_freq(memory->mem_11);
    dump_vfo(memory->vfo_11);
    dump_mode(memory->mode_11);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_12          :");
    dump_freq(memory->mem_12);
    dump_vfo(memory->vfo_12);
    dump_mode(memory->mode_12);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_13          :");
    dump_freq(memory->mem_13);
    dump_vfo(memory->vfo_13);
    dump_mode(memory->mode_13);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_14          :");
    dump_freq(memory->mem_14);
    dump_vfo(memory->vfo_14);
    dump_mode(memory->mode_14);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_15          :");
    dump_freq(memory->mem_15);
    dump_vfo(memory->vfo_15);
    dump_mode(memory->mode_15);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "mem_16          :");
    dump_freq(memory->mem_16);
    dump_vfo(memory->vfo_16);
    dump_mode(memory->mode_16);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "GEN             :");
    dump_freq(memory->GEN);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\nHAM             :");
    dump_freq(memory->HAM);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n                 ");
    dump_vfo(memory->vfo);
    dump_mode(memory->mode);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "CLAR            :");
    dump_freq(memory->CLAR);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "f_1             :");
    dump_freq(memory->ff_1);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "f_2             :");
    dump_freq(memory->ff_2);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "                 ");
    dump_vfo(memory->vfo);
    dump_mode(memory->mode);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "LDB             :");
    dump_switch(memory->LDB);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "EXT_CTL         :");
    dump_switch(memory->EXT_CTL);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    dump_IF_SHIFT(memory->IF_SHIFT);
    dump_SPLIT_CODE(memory->SPLIT_CODE);
    dump_FSK_SHIFT(memory->FSK_SHIFT);
    dump_IF_WIDTH(memory->IF_WIDTH);
    dump_MEM_SHIFT(memory->MEM_SHIFT);
    dump_CLAR_FLAG(memory->CLAR_FLAG);
    dump_TAB_FLAG(memory->TAB_FLAG);
    dump_SELECT_SW(memory->SELECT_SW);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "OFFSET_SW       :");
    dump_switch(memory->OFFSET_SW);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    dump_MODE_SW(memory->MODE_SW);
    dump_MEM_CH_SW(memory->MEM_CH_SW);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "LOW_TAB         :");
    dump_freq(memory->LOW_TAB);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "UP_TAB          :");
    dump_freq(memory->UP_TAB);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "                 ");
    dump_vfo(memory->VFO_STATUS);
    dump_mode(memory->OP_MODE);

    rig_debug(RIG_DEBUG_VERBOSE, "%s", "OP_FREQ         :");
    dump_freq(memory->OP_FREQ);
    rig_debug(RIG_DEBUG_VERBOSE, "%s", "\n");

    dump_STATUS_FLAG(memory->STATUS_FLAG);
}


int ft980_transaction(RIG *rig, const unsigned char *cmd, unsigned char *data,
                      int expected_len)
{
    int retval;
    char echo_back[YAESU_CMD_LENGTH];

    rig_flush(&rig->state.rigport);

    retval = write_block(&rig->state.rigport, (const char *)cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    retval = read_block(&rig->state.rigport, echo_back, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    if (retval != YAESU_CMD_LENGTH || memcmp(echo_back, cmd, YAESU_CMD_LENGTH))
    {
        return -RIG_EPROTO;
    }

    retval = write_block(&rig->state.rigport, cmd_OK, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    retval = read_block(&rig->state.rigport, (char *)data, expected_len);

    if (retval < 0)
    {
        return retval;
    }

    if (retval != expected_len)
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int ft980_get_status_data(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x01 };
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retval;

    if (!rig_check_cache_timeout(&priv->status_tv, FT980_CACHE_TIMEOUT))
    {
        return RIG_OK;
    }

    retval = ft980_transaction(rig, cmd, (unsigned char *)&priv->update_data, 148);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* update cache date */
    gettimeofday(&priv->status_tv, NULL);

    dump_memory(&priv->update_data);

    return retval;
}


/*
 * ft980_open  routine
 *
 */
int ft980_open(RIG *rig)
{
    unsigned char echo_back[YAESU_CMD_LENGTH];
    struct ft980_priv_data *priv;
    int retry_count1 = 0;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    rig->state.priv = calloc(1, sizeof(struct ft980_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = (struct ft980_priv_data *)rig->state.priv;

    memset(priv, 0, sizeof(struct ft980_priv_data));

    /* send Ext Cntl ON: Activate CAT */
    do
    {
        int retval;
        int retry_count2 = 0;

        do
        {
            write_block(&rig->state.rigport, (char *) cmd_ON_OFF,
                        YAESU_CMD_LENGTH);
            retval = read_block(&rig->state.rigport, (char *) echo_back, YAESU_CMD_LENGTH);
        }
        while (retval != 5 && retry_count2++ < rig->state.rigport.retry);

        write_block(&rig->state.rigport, (char *) cmd_OK, YAESU_CMD_LENGTH);
        retval = read_block(&rig->state.rigport, (char *) &priv->update_data, 148);
    }
    while (!priv->update_data.EXT_CTL && retry_count1++ < rig->state.rigport.retry);

    return RIG_OK;
}

int ft980_close(RIG *rig)
{
    unsigned char echo_back[YAESU_CMD_LENGTH];
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retry_count1 = 0;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    do
    {
        int retval;
        int retry_count2 = 0;

        do
        {
            write_block(&rig->state.rigport, (char *) cmd_ON_OFF,
                        YAESU_CMD_LENGTH);
            retval = read_block(&rig->state.rigport, (char *) echo_back, YAESU_CMD_LENGTH);
        }
        while (retval != 5 && retry_count2++ < rig->state.rigport.retry);

        write_block(&rig->state.rigport, (char *) cmd_OK, YAESU_CMD_LENGTH);
        retval = read_block(&rig->state.rigport, (char *) &priv->update_data, 148);
    }
    while (priv->update_data.EXT_CTL && retry_count1++ < rig->state.rigport.retry);

    free(priv);
    return RIG_OK;
}


int ft980_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x08};
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;

    /* store bcd format in cmd (MSB) */
    to_bcd(cmd, freq / 10, 8);

    rig_force_cache_timeout(&priv->status_tv);

    /* Frequency set */
    return ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data, 5), 5);
}

int ft980_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retval;
    freq_t f;

    /* Frequency get */
    retval = ft980_get_status_data(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    f = from_bcd(priv->update_data.OP_FREQ, 8);

    *freq = f * 10;                  /* return displayed frequency */

    return RIG_OK;
}


#define MD_LSB  0x10
#define MD_USB  0x11
#define MD_CW   0x12
#define MD_CWN  0x13
#define MD_AM   0x14
#define MD_AMN  0x15
#define MD_RTTY 0x16
#define MD_FM   0x17

int ft980_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0A};
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    unsigned char md;

    /*
     * translate mode from generic to ft980 specific
     */
    switch (mode)
    {
    case RIG_MODE_CW :    md = MD_CW; break;

    case RIG_MODE_USB:    md = MD_USB; break;

    case RIG_MODE_LSB:    md = MD_LSB; break;

    case RIG_MODE_FM: md = MD_FM; break;

    case RIG_MODE_AM: md = MD_AM; break;

    case RIG_MODE_RTTY:   md = MD_RTTY; break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    if (width != RIG_PASSBAND_NOCHANGE
            && width != RIG_PASSBAND_NORMAL
            && width < rig_passband_normal(rig, mode))
    {
        switch (md)
        {
        case MD_CW: md = MD_CWN; break;

        case MD_AM: md = MD_AMN; break;
        }
    }

    cmd[3] = md;

    rig_force_cache_timeout(&priv->status_tv);

    /* Mode set */
    return ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data, 22), 22);
}

/*
 * rig_get_mode
 *
 * get mode eg AM, CW etc
 *
 */
int ft980_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    unsigned char my_mode;              /* ft890 mode, mode offset */
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retval, norm;

    retval = ft980_get_status_data(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    my_mode = priv->update_data.OP_MODE;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /*
     * translate mode from ft980 to generic.
     */
    switch (my_mode)
    {
    case 0:
        *mode = RIG_MODE_LSB;
        norm = 1;
        break;

    case 1:
        *mode = RIG_MODE_USB;
        norm = 1;
        break;

    case 2:
        *mode = RIG_MODE_CW;
        norm = 1;
        break;

    case 3:
        *mode = RIG_MODE_CW;
        norm = 0;
        break;

    case 4:
        *mode = RIG_MODE_AM;
        norm = 1;
        break;

    case 5:
        *mode = RIG_MODE_AM;
        norm = 0;
        break;

    case 6:
        *mode = RIG_MODE_RTTY;
        norm = 1;
        break;

    case 7:
        *mode = RIG_MODE_FM;
        norm = 1;
        break;

    default:
        return -RIG_EPROTO;         /* Oops! file bug report */
    }

    if (norm)
    {
        *width = rig_passband_normal(rig, *mode);
    }
    else
    {
        *width = rig_passband_narrow(rig, *mode);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set mode = %s\n", __func__,
              rig_strrmode(*mode));
    rig_debug(RIG_DEBUG_TRACE, "%s: set width = %d Hz\n", __func__, (int)*width);

    return RIG_OK;
}


int ft980_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    return -RIG_ENIMPL;
#if 0 // deprecated as was ignored before now
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x8e};


    /*
     * this can be misleading as Yaesu call it "Full duplex"
     * or "sat mode", and split Yaesu terms is repeater shift.
     */
    cmd[4] = split == RIG_SPLIT_ON ? 0x0e : 0x8e;

    return write_block(&rig->state.rigport, (char *) cmd, YAESU_CMD_LENGTH);
#endif
}

int ft980_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    return -RIG_ENIMPL;
}


int ft980_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    return -RIG_ENIMPL;
}

int ft980_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0A };
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;

    if (ch > 16 || ch < 1)
    {
        return -RIG_EINVAL;
    }

    cmd[3] = ch - 1;

    return ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data, 22), 22);
}

int ft980_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retval;

    retval = ft980_get_status_data(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ch = priv->update_data.MEM_CH_SW + 1;

    return RIG_OK;
}
