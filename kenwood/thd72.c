/*
 *  Hamlib Kenwood backend - TH-D72 description
 *                            cloned after TH-D7
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "kenwood.h"
#include "th.h"
#include "num_stdio.h"
#include "iofunc.h"
#include "serial.h"


#define THD72_MODES	  (RIG_MODE_FM|RIG_MODE_AM)
#define THD72_MODES_TX (RIG_MODE_FM)

#define THD72_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_MON|    \
                       RIG_FUNC_SQL|    \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO)

#define THD72_LEVEL_ALL (RIG_LEVEL_STRENGTH| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_AF| \
                        RIG_LEVEL_RF|\
                        RIG_LEVEL_MICGAIN)

#define THD72_PARMS	(RIG_PARM_BACKLIGHT)

#define THD72_VFO_OP (RIG_OP_NONE)

/*
 * TODO: Band A & B
 */
#define THD72_VFO (RIG_VFO_A|RIG_VFO_B)

static rmode_t td72_mode_table[KENWOOD_MODE_TABLE_MAX] = {
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_AM,
};

static struct kenwood_priv_caps  thd72_priv_caps  = {
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = td72_mode_table,
};

static int thd72_open(RIG *rig);
static int thd72_get_chan_all_cb (RIG * rig, chan_cb_t chan_cb, rig_ptr_t arg);


/*
 * th-d72a rig capabilities.
 */
const struct rig_caps thd72a_caps = {
.rig_model =  RIG_MODEL_THD72A,
.model_name = "TH-D72A",
.mfg_name =  "Kenwood",
.version =  TH_VER ".1",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_HANDHELD|RIG_FLAG_APRS|RIG_FLAG_TNC|RIG_FLAG_DXCLUSTER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_XONXOFF,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  250,
.retry =  3,

.has_get_func =  THD72_FUNC_ALL,
.has_set_func =  THD72_FUNC_ALL,
.has_get_level =  THD72_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(THD72_LEVEL_ALL),
.has_get_parm =  THD72_PARMS,
.has_set_parm =  THD72_PARMS,    /* FIXME: parms */
.level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 5 } },
        [LVL_SQL] = { .min = { .i = 0 }, .max = { .i = 5 } },
        [LVL_RFPOWER] = { .min = { .i = 3 }, .max = { .i = 0 } },
},
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.vfo_ops =  THD72_VFO_OP,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  6, /* TBC */

.chan_list =  {
                {  0,  999, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* TBC MEM */
                RIG_CHAN_END,
                   },

.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
    {MHz(118),MHz(174),THD72_MODES,-1,-1,THD72_VFO},
    {MHz(320),MHz(524),THD72_MODES,-1,-1,THD72_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {MHz(144),MHz(148),THD72_MODES_TX,W(0.05),W(5),THD72_VFO},
    {MHz(430),MHz(440),THD72_MODES_TX,W(0.05),W(5),THD72_VFO},
	RIG_FRNG_END,
  }, /* tx range */

.tuning_steps =  {
	 {THD72_MODES,kHz(5)},
	 {THD72_MODES,kHz(6.25)},
     /* kHz(8.33)  ?? */
	 {THD72_MODES,kHz(10)},
	 {THD72_MODES,kHz(12.5)},
	 {THD72_MODES,kHz(15)},
	 {THD72_MODES,kHz(20)},
	 {THD72_MODES,kHz(25)},
	 {THD72_MODES,kHz(30)},
	 {THD72_MODES,kHz(50)},
	 {THD72_MODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_FM, kHz(14)},
		{RIG_MODE_AM, kHz(9)},
		RIG_FLT_END,
	},
.priv =  (void *)&thd72_priv_caps,

.rig_init = kenwood_init,
.rig_cleanup = kenwood_cleanup,
.rig_open = thd72_open,
.set_vfo =  th_set_vfo,
.get_vfo =  th_get_vfo,

.get_chan_all_cb = thd72_get_chan_all_cb,

.get_info =  th_get_info,

};


int thd72_open(RIG *rig)
{
    int ret;

    kenwood_transaction(rig, "", NULL, 0);

    ret = kenwood_transaction(rig, "TC1", NULL, 0);
    if (ret != RIG_OK)
        return ret;

    return RIG_OK;
}

#define CMD_SZ 5
#define BLOCK_SZ 256
#define BLOCK_COUNT 256

static int thd72_get_block (RIG *rig, int block_num, char *block)
{
    hamlib_port_t *rp = &rig->state.rigport;
    char cmd[CMD_SZ] = "R\0\0\0\0";
    char resp[CMD_SZ];
    int ret;

    /* fetching block i */
    cmd[2] = block_num & 0xff;

    ret = write_block(rp, cmd, CMD_SZ);
    if (ret != RIG_OK)
        return ret;

    /* read response first */
    ret = read_block(rp, resp, CMD_SZ);
    if (ret != RIG_OK)
        return ret;

    if (resp[0] != 'W' || memcmp(cmd+1, resp+1, CMD_SZ-1))
        return -RIG_EPROTO;

    /* read block */
    ret = read_block(rp, block, BLOCK_SZ);
    if (ret != BLOCK_SZ)
        return ret;

    ret = write_block(rp, "\006", 1);
    if (ret != RIG_OK)
        return ret;

    ret = read_block(rp, resp, 1);
    if (ret != 1)
        return ret;

    if (resp[0] != 0x06)
        return -RIG_EPROTO;

    return RIG_OK;
}

int thd72_get_chan_all_cb (RIG * rig, chan_cb_t chan_cb, rig_ptr_t arg)
{
    int i, j, ret;
    hamlib_port_t *rp = &rig->state.rigport;
	channel_t *chan;
	chan_t *chan_list = rig->state.chan_list;
	int chan_next = chan_list[0].start;
    char block[BLOCK_SZ];
    char resp[CMD_SZ];

    ret = kenwood_transaction(rig, "0M PROGRAM", NULL, 0);
    if (ret != RIG_OK)
        return ret;

    rp->parm.serial.rate = 57600;

    serial_setup(rp);

    /* let the pcr settle and flush any remaining data*/
    usleep(100*1000);
    serial_flush(rp);

    /* setRTS or Hardware flow control? */
    ret = ser_set_rts(rp, 1);
    if (ret != RIG_OK)
        return ret;

	/*
	 * setting chan to NULL means the application
	 * has to provide a struct where to store data
	 * future data for channel channel_num
	 */
    chan = NULL;
    ret = chan_cb(rig, &chan, chan_next, chan_list, arg);
    if (ret != RIG_OK)
        return ret;
    if (chan == NULL)
        return -RIG_ENOMEM;


    for (i=0; i<BLOCK_COUNT; i++) {

        ret = thd72_get_block (rig, i, block);
        if (ret != RIG_OK)
            return ret;

        /*
         * Most probably, there's 64 bytes per channel (256*256 / 1000+)
         */
#define CHAN_PER_BLOCK 4

		for (j=0; j<CHAN_PER_BLOCK; j++) {

            char *block_chan = block + j*(BLOCK_SZ/CHAN_PER_BLOCK);

            memset(chan, 0, sizeof(channel_t));
			chan->vfo = RIG_VFO_MEM;
			chan->channel_num = i*CHAN_PER_BLOCK + j;

            /* What are the extra 64 channels ? */
            if (chan->channel_num >= 1000)
                break;

            /* non-empty channel ? */
            if (block_chan[0] != 0xff) {

                memcpy(chan->channel_desc, block_chan, 8);
                /* TODO: chop off trailing chars */
                chan->channel_desc[8] = '\0';

                /* TODO: parse block and fill in chan */
            }

			/* notify the end? */
			chan_next = chan_next < chan_list[i].end ? chan_next+1 : chan_next;

			/*
			 * provide application with channel data,
			 * and ask for a new channel structure
			 */
			chan_cb(rig, &chan, chan_next, chan_list, arg);
        }
    }

    ret = write_block(rp, "E", 1);
    if (ret != RIG_OK)
        return ret;

    ret = read_block(rp, resp, 1);
    if (ret != 1)
        return ret;

    if (resp[0] != 0x06) {
        return -RIG_EPROTO;
    }

    /* setRTS?? getCTS needed? */
    ret = ser_set_rts(rp, 1);
    if (ret != RIG_OK)
        return ret;

    return RIG_OK;
}

