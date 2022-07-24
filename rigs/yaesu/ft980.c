/*
 * ft980.c - (C) Stephane Fillod 2004-2010
 *           (C) Wolfgang Buesser 2010
 *
 *           (C) Mathew Breton 2021
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


/*
* FT-980 Hamlib API functions considered to be Stable:
*      sadly, none yet
*
* Functions considered to be Beta:
*      init              *      cleanup
*      set_freq          *      get_freq
*      set_mode          *      get_mode
*      set_mem           *      get_mem
*      open              *      close
*
* Functions considered to be Alpha:
*      set_vfo            *      get_vfo
*
* Functions not yet implemented
*      get_xit            *      set_xit
*      set_func            *      get_func
*      get_ptt             *      set_ptt
*      get_dcd
*      set_rptr_shift      *      get_rptr_shift
*      set_rptr_offs       *      get_rptr_offs
*      set_split_freq      *      get_split_freq
*      set_split_mode      *      get_split_mode
*      set_split_freq_mode *      get_split_freq_mode
*      set_split_vfo       *      get_split_vfo
*      set_ts              *      get_ts
*      vfo_op
*
* Functions the radio does not support: see readme.ft980 for more details
*      power2mW          *      mW2power
*      newcat_get_ant    *      newcat_set_ant
*      set_dcs_code      *      get_dcs_code
*      set_tone          *      get_tone
*      set_ctcss_tone    *      get_ctcss_tone
*      set_dcs_sql       *      get_dcs_sql
*      set_tone_sql      *      get_tone_sql
*      set_ctcss_sql     *      get_ctcss_sql
*      set_powerstat     *      get_powerstat
*      set_ant           *      get_ant
*      send_dtmf         *      recv_dtmf
*      send_morse        *      stop_morse
*      wait_morse        *      send_voice_mem
*      set_trn           *      get_trn
*      set_channel       *      get_channel
*      set_bank          *      scan
*      set_parm          *      get_parm
*      get_info
*      reset
*      set_vfo_opt
*      decode_event
*
*  No idea yet what these do
*      set_chan_all_cb
*      get_chan_all_cb
*      set_conf
*      get_conf
*/


#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <sys/time.h> /* for timeofday call */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "bandplan.h"
#include "yaesu.h"
#include "ft980.h"

/*
 * Private data
 */
struct ft980_priv_data
{
    _ft980_memory_t update_data;          /* returned data */
    vfo_t current_vfo;                    /* active VFO from last cmd */
    struct timeval status_tv;             /* update_data caching */
};

/*************************************************************************************
 * Private Prototype Declarations
 */
static int ft980_transaction(RIG *rig, const unsigned char *cmd,
                             unsigned char *data, int expected_len);
static int ft980_get_status_data(RIG *rig);

/* Dump routines are for debug purposes */
static void dump_freq(unsigned char *data);
static void dump_vfo(unsigned char data);
static void dump_mode(unsigned char data);
static void dump_switch(unsigned char data);
static void dump_if_shift(unsigned char data);
static void dump_rptr_split_code(unsigned char data);
static void dump_fsk_shift(unsigned char data);
static void dump_if_width(unsigned char data);
static void dump_mem_shift_flag(unsigned char data);
static void dump_clar_flag(unsigned char data);
static void dump_tab_flag(unsigned char data);
static void dump_freq_select_sws(unsigned char data);
static void dump_mode_sw(unsigned char data);
static void dump_mem_ch_sw(unsigned char data);
static void dump_status_flag_bits(unsigned char data);
static void dump_memory(_ft980_memory_t *memory);

static void dump_freq(unsigned char *data)
{
    rig_debug(RIG_DEBUG_TRACE, "%02x%02x%02x%02x ", data[3], data[2], data[1],
              data[0]);
}

static void dump_vfo(unsigned char data)
{
    switch ((unsigned int)data)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", "GEN");
        break;

    case 128:
        rig_debug(RIG_DEBUG_TRACE, "%s", "HAM");
        break;
    }
}

static void dump_mode(unsigned char data)
{
    switch ((unsigned int)data)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", " LSB\n");
        break;

    case 1:
        rig_debug(RIG_DEBUG_TRACE, "%s", " USB\n");
        break;

    case 2:
        rig_debug(RIG_DEBUG_TRACE, "%s", " CW-W\n");
        break;

    case 3:
        rig_debug(RIG_DEBUG_TRACE, "%s", " CW-N\n");
        break;

    case 4:
        rig_debug(RIG_DEBUG_TRACE, "%s", " AM-W\n");
        break;

    case 5:
        rig_debug(RIG_DEBUG_TRACE, "%s", " AM-N\n");
        break;

    case 6:
        rig_debug(RIG_DEBUG_TRACE, "%s", " FSK\n");
        break;

    case 7:
        rig_debug(RIG_DEBUG_TRACE, "%s", " FM\n");
        break;
    }
}

static void dump_switch(unsigned char data)
{
    switch ((unsigned int)data)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", "OFF");
        break;

    case 1:
        rig_debug(RIG_DEBUG_TRACE, "%s", "ON ");
        break;
    }
}

static void dump_if_shift(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:%d\n", __func__, data - 15);
}

static void dump_rptr_split_code(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:%02x\n", __func__, data);
}

static void dump_fsk_shift(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:%02x\n", __func__, data);
}

static void dump_if_width(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:%d\n", __func__, data);
}

static void dump_mem_shift_flag(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:", __func__);

    switch ((unsigned int)data)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", "OFF\n");
        break;

    case 16:
        rig_debug(RIG_DEBUG_TRACE, "%s", "ON\n");
        break;
    }
}

static void dump_clar_flag(unsigned char data)
{
    unsigned char RX_CLAR = data & 0x20;
    unsigned char TX_CLAR = data & 0x40;
    rig_debug(RIG_DEBUG_TRACE, "%s", "CLAR_SHIFT RX/TX:");

    switch ((unsigned int)RX_CLAR)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", "OFF ");
        break;

    case 0x20:
        rig_debug(RIG_DEBUG_TRACE, "%s", "ON  ");
        break;
    }

    switch ((unsigned int)TX_CLAR)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", " OFF ");
        break;

    case 0x40:
        rig_debug(RIG_DEBUG_TRACE, "%s", " ON  ");
        break;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");
}
static void dump_tab_flag(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s", "TAB FLAG        :");

    switch (data)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", "OFF\n");
        break;

    case 0x80:
        rig_debug(RIG_DEBUG_TRACE, "%s", "ON\n");
        break;
    }
}
static void dump_freq_select_sws(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s", "freq_select_sws       :");

    switch ((unsigned int)data)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", "VFO ");
        break;

    case 1:
        rig_debug(RIG_DEBUG_TRACE, "%s", "MR  ");
        break;

    case 2:
        rig_debug(RIG_DEBUG_TRACE, "%s", "RX_M");
        break;

    case 3:
        rig_debug(RIG_DEBUG_TRACE, "%s", "RX_V");
        break;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");
}
static void dump_mode_sw(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "%s", "mode_sw         :");

    switch ((unsigned int)data)
    {
    case 0:
        rig_debug(RIG_DEBUG_TRACE, "%s", "LSB ");
        break;

    case 1:
        rig_debug(RIG_DEBUG_TRACE, "%s", "USB ");
        break;

    case 2:
        rig_debug(RIG_DEBUG_TRACE, "%s", "CW-W");
        break;

    case 3:
        rig_debug(RIG_DEBUG_TRACE, "%s", "CW-N");
        break;

    case 4:
        rig_debug(RIG_DEBUG_TRACE, "%s", "AM-W");
        break;

    case 5:
        rig_debug(RIG_DEBUG_TRACE, "%s", "AM-N");
        break;

    case 6:
        rig_debug(RIG_DEBUG_TRACE, "%s", "FSK");
        break;

    case 7:
        rig_debug(RIG_DEBUG_TRACE, "%s", "FM");
        break;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");
}
static void dump_mem_ch_sw(unsigned char data)
{
    rig_debug(RIG_DEBUG_TRACE, "mem_ch_sw       :%d\n", data + 1);
}
static void dump_status_flag_bits(unsigned char data)
{
    unsigned char TX    = data & 0x01;
    unsigned char SPLIT = data & 0x08;
    unsigned char VFO   = data & 0x20;
    unsigned char CLAR  = data & 0x40;
    rig_debug(RIG_DEBUG_TRACE, "%s", "status_flag_bits     :");

    if (TX)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s", "TX ");
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s", "RX ");
    }

    if (SPLIT)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s", "SPLIT   ");
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s", "SIMPLEX ");
    }

    if (VFO)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s", "VFO    ");
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s", "MEMORY ");
    }

    if (CLAR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s", "CLAR_ON ");
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s", "CLAR_OFF");
    }

    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");
}


static void dump_memory(_ft980_memory_t *memory)
{

    if (!rig_need_debug(RIG_DEBUG_TRACE))
    {
        return;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_1          :");
    dump_freq(memory->mem_1);
    dump_vfo(memory->vfo_1);
    dump_mode(memory->mode_1);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_2          :");
    dump_freq(memory->mem_2);
    dump_vfo(memory->vfo_2);
    dump_mode(memory->mode_2);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_3          :");
    dump_freq(memory->mem_3);
    dump_vfo(memory->vfo_3);
    dump_mode(memory->mode_3);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_4          :");
    dump_freq(memory->mem_4);
    dump_vfo(memory->vfo_4);
    dump_mode(memory->mode_4);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_5          :");
    dump_freq(memory->mem_5);
    dump_vfo(memory->vfo_5);
    dump_mode(memory->mode_5);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_6          :");
    dump_freq(memory->mem_6);
    dump_vfo(memory->vfo_6);
    dump_mode(memory->mode_6);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_7          :");
    dump_freq(memory->mem_7);
    dump_vfo(memory->vfo_7);
    dump_mode(memory->mode_7);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_8          :");
    dump_freq(memory->mem_8);
    dump_vfo(memory->vfo_8);
    dump_mode(memory->mode_8);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_9           :");
    dump_freq(memory->mem_9);
    dump_vfo(memory->vfo_9);
    dump_mode(memory->mode_9);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_10          :");
    dump_freq(memory->mem_10);
    dump_vfo(memory->vfo_10);
    dump_mode(memory->mode_10);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_11          :");
    dump_freq(memory->mem_11);
    dump_vfo(memory->vfo_11);
    dump_mode(memory->mode_11);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_12          :");
    dump_freq(memory->mem_12);
    dump_vfo(memory->vfo_12);
    dump_mode(memory->mode_12);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_13          :");
    dump_freq(memory->mem_13);
    dump_vfo(memory->vfo_13);
    dump_mode(memory->mode_13);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_14          :");
    dump_freq(memory->mem_14);
    dump_vfo(memory->vfo_14);
    dump_mode(memory->mode_14);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_15          :");
    dump_freq(memory->mem_15);
    dump_vfo(memory->vfo_15);
    dump_mode(memory->mode_15);

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_16          :");
    dump_freq(memory->mem_16);
    dump_vfo(memory->vfo_16);
    dump_mode(memory->mode_16);

    rig_debug(RIG_DEBUG_TRACE, "%s", "gen_vfo_freq             :");
    dump_freq(memory->gen_vfo_freq);

    rig_debug(RIG_DEBUG_TRACE, "%s", "\nHAM             :");
    dump_freq(memory->ham_vfo_freq);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n                 ");
    dump_vfo(memory->vfo);
    dump_mode(memory->mode);

    rig_debug(RIG_DEBUG_TRACE, "%s", "clar_freq            :");
    dump_freq(memory->clar_freq);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_shift_freq             :");
    dump_freq(memory->mem_shift_freq);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    rig_debug(RIG_DEBUG_TRACE, "%s", "mem_clar_freq             :");
    dump_freq(memory->mem_clar_freq);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    rig_debug(RIG_DEBUG_TRACE, "%s", "                 ");
    dump_vfo(memory->vfo);
    dump_mode(memory->mode);

    rig_debug(RIG_DEBUG_TRACE, "%s", "ldb_flag             :");
    dump_switch(memory->ldb_flag);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    rig_debug(RIG_DEBUG_TRACE, "%s", "ext_ctl_flag         :");
    dump_switch(memory->ext_ctl_flag);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    dump_if_shift(memory->if_shift);
    dump_rptr_split_code(memory->rptr_split_code);
    dump_fsk_shift(memory->fsk_shift);
    dump_if_width(memory->if_width);
    dump_mem_shift_flag(memory->mem_shift_flag);
    dump_clar_flag(memory->clar_flag);
    dump_tab_flag(memory->tab_flag);
    dump_freq_select_sws(memory->freq_select_sws);

    rig_debug(RIG_DEBUG_TRACE, "%s", "offset_sw       :");
    dump_switch(memory->offset_sw);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    dump_mode_sw(memory->mode_sw);
    dump_mem_ch_sw(memory->mem_ch_sw);

    rig_debug(RIG_DEBUG_TRACE, "%s", "lower_tab_freq         :");
    dump_freq(memory->lower_tab_freq);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    rig_debug(RIG_DEBUG_TRACE, "%s", "upper_tab_freq          :");
    dump_freq(memory->upper_tab_freq);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    rig_debug(RIG_DEBUG_TRACE, "%s", "                 ");
    dump_vfo(memory->op_vfo);
    dump_mode(memory->op_mode);

    rig_debug(RIG_DEBUG_TRACE, "%s", "op_freq         :");
    dump_freq(memory->op_freq);
    rig_debug(RIG_DEBUG_TRACE, "%s", "\n");

    dump_status_flag_bits(memory->status_flag_bits);
}


int ft980_transaction(RIG *rig, const unsigned char *cmd, unsigned char *data,
                      int expected_len)
{
    int retval;
    unsigned char echo_back[YAESU_CMD_LENGTH];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig_flush(&rig->state.rigport);

    retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    retval = read_block(&rig->state.rigport, echo_back, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    if (retval != YAESU_CMD_LENGTH
            || (memcmp(echo_back, cmd, YAESU_CMD_LENGTH) != 0))
    {
        return -RIG_EPROTO;
    }

    retval = write_block(&rig->state.rigport, cmd_OK, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    retval = read_block(&rig->state.rigport, data, expected_len);

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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig_check_cache_timeout(&priv->status_tv, FT980_CACHE_TIMEOUT))
    {
        return RIG_OK;
    }

    retval = ft980_transaction(rig, cmd, (unsigned char *)&priv->update_data,
                               FT980_ALL_STATUS_LENGTH);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* update cache date */
    gettimeofday(&priv->status_tv, NULL);

    dump_memory(&priv->update_data);

    return retval;
}

/****************************************************************************
 * rig_init*
 *
 * Initialize memory & rig private data structure
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Nothing special here
 *
 */
int ft980_init(RIG *rig)
{
    struct ft980_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct ft980_priv_data *) calloc(1,
                      sizeof(struct ft980_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = (struct ft980_priv_data *)rig->state.priv;

    memset(priv, 0, sizeof(struct ft980_priv_data));

    // Initialize operating vfo mode to current VFO
    priv->current_vfo =  RIG_VFO_MAIN;

    return RIG_OK;
}

/****************************************************************************
 * rig_cleanup*
 *
 * Release memory in rig private data structure for a clean exit
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Nothing special here
 *
 */
int ft980_cleanup(RIG *rig)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

/****************************************************************************
 * rig_open*
 *
 * Initialize memory & rig private data structure
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Should be able to optimize
 * ToDo: add check so we don't get stuck in EXT CTRL toggle trap/loop
 *
 */
int ft980_open(RIG *rig)
{
    unsigned char echo_back[YAESU_CMD_LENGTH];
    struct ft980_priv_data *priv;
    int retry_count1 = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = (struct ft980_priv_data *)rig->state.priv;

    /* send Ext Cntl ON: Activate CAT */
    do
    {
        int retval;
        int retry_count2 = 0;

        do
        {
            write_block(&rig->state.rigport, cmd_ON_OFF,
                        YAESU_CMD_LENGTH);
            retval = read_block(&rig->state.rigport, echo_back, YAESU_CMD_LENGTH);
        }
        while (retval != 5 && retry_count2++ < rig->state.rigport.retry);

        write_block(&rig->state.rigport, cmd_OK, YAESU_CMD_LENGTH);
        retval = read_block(&rig->state.rigport, (unsigned char *) &priv->update_data,
                            FT980_ALL_STATUS_LENGTH);
    }
    while (!priv->update_data.ext_ctl_flag
            && retry_count1++ < rig->state.rigport.retry);

    return RIG_OK;
}

/****************************************************************************
 * rig_close*
 *
 * Send command to toggle out of EXT CTRL mode
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Nothing special here
 * Could be optimized.
 *
 */
int ft980_close(RIG *rig)
{
    unsigned char echo_back[YAESU_CMD_LENGTH];
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retry_count1 = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    do
    {
        int retval;
        int retry_count2 = 0;

        do
        {
            write_block(&rig->state.rigport, cmd_ON_OFF,
                        YAESU_CMD_LENGTH);
            retval = read_block(&rig->state.rigport, echo_back, YAESU_CMD_LENGTH);
        }
        while (retval != 5 && retry_count2++ < rig->state.rigport.retry);

        write_block(&rig->state.rigport, cmd_OK, YAESU_CMD_LENGTH);
        retval = read_block(&rig->state.rigport, (unsigned char *) &priv->update_data,
                            FT980_ALL_STATUS_LENGTH);
    }
    while (priv->update_data.ext_ctl_flag
            && retry_count1++ < rig->state.rigport.retry);

    return RIG_OK;
}

/*
 * Only the current VFO frequency can be set
 *   Other Hamlib backends (ex FT-990) switch VFO, change freq, then exit
 *     They do not return to the original VFO.
 *   We will stick with this convention for now.
 *
 *   ToDo: Check return data to verify frequency was set correctly
 */
/*
 * rig_set_freq*
 *
 * Set frequency for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   freq       | input  | 100000 - 30000000
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 *
 * Issues: an error will occur with the 4.0 rig.c set_cache_freq routine when
 *    targeting VFO_MEM.
 */
int ft980_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x08};
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "  %s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "  %s: passed freq = %lf Hz\n", __func__, freq);

    // Set to selected VFO
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "  %s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft980_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    /* store bcd format in cmd (MSB) */
    to_bcd(cmd, freq / 10, 8);

    /* why is this done ? */
    rig_force_cache_timeout(&priv->status_tv);

    /* Frequency set */
    return ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data, 5), 5);
}

/*
 * We can get HAM, GEN, Memory Shift (?), "Operating"
 *   What is memory "Shift"?
 */
/*
 * rig_get_freq*
 *
 * Get frequency for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   freq *     | output | 100000 - 30000000
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft980_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retval;
    freq_t f;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "  %s: passed vfo = 0x%02x\n", __func__, vfo);

    /* Frequency get */
    retval = ft980_get_status_data(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (vfo)
    {
    case RIG_VFO_CURR:
        f = from_bcd(priv->update_data.op_freq, 8);
        break;

    case RIG_VFO_MAIN:
        f = from_bcd(priv->update_data.ham_vfo_freq, 8);
        break;

    case RIG_VFO_SUB:
        f = from_bcd(priv->update_data.gen_vfo_freq, 8);
        break;

    case RIG_VFO_MEM:
        f = from_bcd(priv->update_data.mem_shift_freq, 8);
        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "%s: Selected Memory Freq = %lf\n", __func__, f * 10);

    *freq = f * 10;                  /* return displayed frequency */

    return RIG_OK;
}

/*
 * Only the current VFO mode can be set
 *   Other Hamlib backends (ex FT-990) switch VFO, change mode, then exit
 *     They do not return to the original VFO.
 *   We will stick with this convention for now.
 *
 *   ToDo: Check return data to verify mode was set correctly
 */
/*
 * rig_set_mode*
 *
 * Set mode for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, Sub, MEM
 *   mode       | input  | byte
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft980_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0A};
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    unsigned char md;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "  %s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "  %s: passed mode = %s\n", __func__,
              rig_strrmode(mode));
    rig_debug(RIG_DEBUG_TRACE, "  %s: passed width = %ld Hz\n", __func__, width);

    // Set to selected VFO
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft980_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    /*
     * translate mode from generic to ft980 specific
     */
    switch (mode)
    {
    case RIG_MODE_CW :
        md = FT980_CMD0A_MD_CW;
        break;

    case RIG_MODE_USB:
        md = FT980_CMD0A_MD_USB;
        break;

    case RIG_MODE_LSB:
        md = FT980_CMD0A_MD_LSB;
        break;

    case RIG_MODE_FM:
        md = FT980_CMD0A_MD_FM;
        break;

    case RIG_MODE_AM:
        md = FT980_CMD0A_MD_AM;
        break;

    case RIG_MODE_RTTY:
        md = FT980_CMD0A_MD_RTTY;
        break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    if (width != RIG_PASSBAND_NOCHANGE
            && width != RIG_PASSBAND_NORMAL
            && width < rig_passband_normal(rig, mode))
    {
        switch (md)
        {
        case FT980_CMD0A_MD_CW:
            md = FT980_CMD0A_MD_CWN;
            break;

        case FT980_CMD0A_MD_AM:
            md = FT980_CMD0A_MD_AMN;
            break;
        }
    }

    cmd[3] = md;

    /* Might be deprecated in Hamlib 4.1 */
    rig_force_cache_timeout(&priv->status_tv);

    /* Mode set */
    return ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data,
                             FT980_OTHER_STATUS_LENGTH), FT980_OTHER_STATUS_LENGTH);
}

/*
 * rig_get_mode
 *
 * get mode eg AM, CW etc
 * ??? What is the difference between byte 6 (operating mode vfo_op
 *   and byte 30 (selected VFO mode) ???
 */
/*
 * rig_get_mode*
 *
 * Get frequency for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   mode *     | output | byte
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft980_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    unsigned char my_mode;              /* ft890 mode, mode offset */
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retval, norm;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "  %s: passed vfo = 0x%02x\n", __func__, vfo);

    retval = ft980_get_status_data(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (vfo)
    {
    case RIG_VFO_CURR:
        my_mode = priv->update_data.op_mode;

        rig_debug(RIG_DEBUG_TRACE,
                  "  %s: Current VFO Mode = 0x%02x\n", __func__, my_mode);

        break;

    case RIG_VFO_MAIN:
    case RIG_VFO_SUB:

        /* This is a point of confusion: what exactly is byte 30? */

        my_mode = priv->update_data.mode;
        rig_debug(RIG_DEBUG_TRACE,
                  "  %s: HAM/GEN VFO Mode = 0x%02x\n", __func__, my_mode);
        break;

    case RIG_VFO_MEM:
        my_mode = priv->update_data.mem_mode;

        rig_debug(RIG_DEBUG_TRACE,
                  "  %s: MEM VFO Mode = 0x%02x\n", __func__, my_mode);
        break;

    default:
        return -RIG_EVFO;
    }

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

    rig_debug(RIG_DEBUG_TRACE, "  %s: Hamlib mode = %s\n", __func__,
              rig_strrmode(*mode));

    if (norm)
    {
        *width = rig_passband_normal(rig, *mode);
    }
    else
    {
        *width = rig_passband_narrow(rig, *mode);
    }

    rig_debug(RIG_DEBUG_TRACE, "  %s: Filter width = %d Hz\n", __func__,
              (int)*width);

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

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (ch > 16 || ch < 1)
    {
        return -RIG_EINVAL;
    }

    cmd[3] = ch - 1;

    return ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data,
                             FT980_OTHER_STATUS_LENGTH), FT980_OTHER_STATUS_LENGTH);
}

/****************************************************************************
 * rig_get_mem
 *
 * Get the number of the currently selected memory
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | Not applicable for FT-980
 *   ch         | output | pointer to channel integer to be returned
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: returns currently selected memory channel regardlessof front
 *    panel knob selection (can be different when CAT is enabled).
 */
int ft980_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct ft980_priv_data *priv = (struct ft980_priv_data *)rig->state.priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = ft980_get_status_data(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ch = priv->update_data.mem_ch_sw + 1;

    return RIG_OK;
}

/****************************************************************************
 * rig_set_vfo*
 *
 * Set operational VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFO_MAIN, VFOB/GEN, MEM
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will essentially "no op"
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
/*     VFO_CURR: Whatever is shown in op_freq/op_mode */
/*     VFO_MAIN: Mode = VFO, VFO = MAIN */
/*     VFO_SUB: Mode = VFO, VFO = GEN */
/*     VFO_MEM: Mode = Memory, VFO = don't care */
int ft980_set_vfo(RIG *rig, vfo_t vfo)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0A };
    struct ft980_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EARG;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));

    priv = (struct ft980_priv_data *)rig->state.priv;

    switch (vfo)
    {
    case RIG_VFO_CURR:
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo = 0x%02x\n", __func__, priv->current_vfo);
        return RIG_OK;
        break;

    case RIG_VFO_MAIN:
        cmd[3] = FT980_CMD0A_VFO_SEL_HAM;
        rig_debug(RIG_DEBUG_TRACE, "%s: set VFO GEN/HAM = 0x%02x\n",
                  __func__, cmd[3]);
        err = ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data,
                                FT980_OTHER_STATUS_LENGTH), FT980_OTHER_STATUS_LENGTH);

        if (err != RIG_OK)
        {
            return err;
        }

        cmd[3] = FT980_CMD0A_FREQ_SEL_VFO;
        break;

    case RIG_VFO_SUB:
        cmd[3] = FT980_CMD0A_VFO_SEL_GEN;
        rig_debug(RIG_DEBUG_TRACE, "%s: set VFO GEN/HAM = 0x%02x\n",
                  __func__, cmd[3]);
        err = ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data,
                                FT980_OTHER_STATUS_LENGTH), FT980_OTHER_STATUS_LENGTH);

        if (err != RIG_OK)
        {
            return err;
        }

        cmd[3] = FT980_CMD0A_FREQ_SEL_VFO;
        break;

    case RIG_VFO_MEM:
        cmd[3] = FT980_CMD0A_FREQ_SEL_MR;
        break;

    default:
        return -RIG_EVFO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set VFO Status = %s\n",
              __func__, rig_strvfo(vfo));

    return ft980_transaction(rig, cmd, UPDATE_DATA_OFS(&priv->update_data,
                             FT980_OTHER_STATUS_LENGTH), FT980_OTHER_STATUS_LENGTH);
}

/****************************************************************************
 * rig_get_vfo*
 *
 * Get operational VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo *      | output | currVFO, VFO_MAIN, VFOB/GEN, MEM
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:
 *           VFO_MAIN: If (Status = VFO && VFO = MAIN) || (Status = RXV && VFO = MAIN)
 *           VFO_SUB: If (Status = VFO && VFO = GEN) || (Status = RXV && VFO = GEN)
 *           VFO_MEM: If *Status = Memory) || (Status = RXM)
 *
 *           If operating in split (RXM, RXV) then get_vfo returns the receive vfo
 */
int ft980_get_vfo(RIG *rig, vfo_t *vfo)
{
    int err;
    struct ft980_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EARG;
    }

    priv = (struct ft980_priv_data *)rig->state.priv;

    /* Get flags for VFO status */
    err = ft980_get_status_data(rig);

    if (err != RIG_OK)
    {
        return err;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "%s: status_flag_bits = 0x%02x\n", __func__,
              priv->update_data.status_flag_bits);
    rig_debug(RIG_DEBUG_TRACE,
              "%s: op_vfo = %s\n", __func__,
              rig_strvfo(priv->update_data.op_vfo));

    /* Decode the VFO Setting and VFO States */
    if (!(priv->update_data.status_flag_bits & FT_980_STATUSFLAG_VFO_MASK))
    {
        priv->current_vfo = RIG_VFO_MEM;
    }
    else  if (priv->update_data.op_vfo == FT980_VFO_HAM_SEL)
    {
        priv->current_vfo = RIG_VFO_MAIN;
    }
    else if (priv->update_data.op_vfo == FT980_VFO_GEN_SEL)
    {
        priv->current_vfo = RIG_VFO_SUB;
    }
    else
    {
        return -RIG_EVFO;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "%s: stat_vfo = %s\n", __func__, rig_strvfo(priv->current_vfo));

    *vfo = priv->current_vfo;

    return RIG_OK;
}
