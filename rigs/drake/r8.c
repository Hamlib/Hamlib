/*
 *  Hamlib Drake backend - R-8 description
 *  Copyright (c) 2001-2010, 2025 by Stephane Fillod
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

//#include <stdio.h>
#include <stdlib.h>
//#include <string.h>  /* String function definitions */
//#include <unistd.h>  /* UNIX standard function definitions */

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "idx_builtin.h"
//#include "cal.h"
//#include "register.h"

#include "drake.h"

int drake_r8_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int drake_r8_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int drake_r8_set_vfo(RIG *rig, vfo_t vfo);
int drake_r8_get_vfo(RIG *rig, vfo_t *vfo);
int drake_r8_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int drake_r8_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int drake_r8_init(RIG *rig);
int drake_r8_cleanup(RIG *rig);
int drake_r8_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);
int drake_r8_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option, ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);
int drake_r8_set_mem(RIG *rig, vfo_t vfo, int ch);
int drake_r8_get_mem(RIG *rig, vfo_t vfo, int *ch);
int drake_r8_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan);
int drake_r8_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);
int drake_r8_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int drake_r8_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int drake_r8_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int drake_r8_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int drake_r8_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int drake_r8_set_powerstat (RIG * rig, powerstat_t status);
int drake_r8_get_powerstat (RIG * rig, powerstat_t *status);
const char *drake_r8_get_info(RIG *rig);

#define BUFSZ 64

#define CR "\x0d"
#define LF "\x0a"
#define EOM CR

#define MD_USB  '1'
#define MD_LSB  '2'
#define MD_RTTY '3'
#define MD_CW   '4'
#define MD_FM   '5'
#define MD_AM   '6'

#define R8_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_AM|RIG_MODE_AMS|RIG_MODE_FM)

#define R8_FUNC (RIG_FUNC_MN|RIG_FUNC_NB|RIG_FUNC_NB2)

#define R8_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define R8_PARM_ALL (RIG_PARM_TIME)

#define R8_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_VFO|RIG_VFO_MEM)

#define R8_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

#define R8_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)

#define R8_STR_CAL { 2, { \
        {   0, -60 }, \
        {   1,   0 }, \
    } }

/*
 * channel caps.
 */
#define DRAKE_MEM_CAP { \
    .freq = 1,  \
    .mode = 1,  \
    .width = 1, \
    .ant = 1,   \
    .funcs = R8_FUNC,  \
    .levels = RIG_LEVEL_AGC|RIG_LEVEL_ATT|RIG_LEVEL_PREAMP, \
}

/*
 * R-8 rig capabilities.
 *
 * specs: http://www.dxing.com/rx/r8.htm
 *
 */

struct rig_caps r8_caps =
{
    RIG_MODEL(RIG_MODEL_DKR8),
    .model_name = "R-8",
    .mfg_name =  "Drake",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_NEW,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  7,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_EVEN,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  1,
    .post_write_delay =  100, //1,
    .timeout =  250,
    .retry =  3,

    .has_get_func =  R8_FUNC,
    .has_set_func =  R8_FUNC,
    .has_get_level =  R8_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(R8_LEVEL_ALL),
    .has_get_parm =  R8_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(R8_PARM_ALL),
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_ATT] = { .min = { .i = 0 }, .max = { .i = 10 } },
        [LVL_PREAMP] = { .min = { .i = 0 }, .max = { .i = 10 } },
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END },
    .attenuator =   { 10, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .vfo_ops =  R8_VFO_OPS,
    .scan_ops = 0,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .priv =  NULL,

    .chan_list =  {
        {   0,  99, RIG_MTYPE_MEM, DRAKE_MEM_CAP },
        RIG_CHAN_END
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(30), R8_MODES, -1, -1, R8_VFO, R8_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(30), R8_MODES, -1, -1, R8_VFO, R8_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  {
        {R8_MODES, 10},
        {R8_MODES, 100},
        {R8_MODES, kHz(1)},
        {R8_MODES, kHz(10)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(6)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(4)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(2.3)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(1.8)},
        {RIG_MODE_AM | RIG_MODE_AMS, Hz(500)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(2.3)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(1.8)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(4)},
        {RIG_MODE_SSB | RIG_MODE_RTTY, kHz(6)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_CW, kHz(1.8)},
        {RIG_MODE_CW, kHz(2.3)},
        {RIG_MODE_CW, kHz(4)},
        {RIG_MODE_CW, kHz(6)},
        RIG_FLT_END,
    },
    .str_cal = R8_STR_CAL,

    .rig_init = drake_r8_init,
    .rig_cleanup = drake_r8_cleanup,

    .set_freq =  drake_r8_set_freq,
    .get_freq =  drake_r8_get_freq,
    .set_vfo =  drake_r8_set_vfo,
    .get_vfo =  drake_r8_get_vfo,
    .set_mode =  drake_r8_set_mode,
    .get_mode =  drake_r8_get_mode,
    .set_func = drake_r8_set_func,
    .get_func = drake_r8_get_func,
    .set_level = drake_r8_set_level,
    .get_level = drake_r8_get_level,
    .set_ant =  drake_r8_set_ant,
    .get_ant =  drake_r8_get_ant,
    .set_mem = drake_r8_set_mem,
    .get_mem = drake_r8_get_mem,
    .set_channel = drake_r8_set_chan,
    .get_channel = drake_r8_get_chan,
    .vfo_op = drake_r8_vfo_op,
    .set_powerstat = drake_r8_set_powerstat,
    .get_powerstat = drake_r8_get_powerstat,
    .get_info =  drake_r8_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */


/*
 * drake_r8_fix_string
 * recursively replaces all special characters so they are readable at output
 * 
 */
void drake_r8_fix_string(char* inStr)
{
    char  chChkAry[3] = {0x20, 0x0d, 0x0a};
    char* chRepAry[3] = {"<SP>", "<CR>", "<LF>"};
    char* chPos;
    int   newLen;
    int   offset;
    int   i;
    int   j;

    for (i = 0; i < 3; i++)
    {
        do {
            chPos = strchr(inStr, chChkAry[i]); 
            if (chPos != NULL)
            {
              newLen = strlen(inStr);  
              offset = chPos - inStr;   
              for (j = newLen; j > offset; j--)  
              {
                  inStr[j+3] = inStr[j]; 
              }
              for (j = 0; j < 4; j++)
              {
                  inStr[offset+j] = chRepAry[i][j]; 
              }
            }
        }
        while (chPos);
    }
}


/*
 * drake_r8_trans_rept
 * non-destructively echoes transaction in a readable way for debugging
 */
void drake_r8_trans_rept(char* hdrStr, char* sentStr, int sentLen, char* recdStr, int recdLen, int res)
{
    char sent[BUFSZ];
    char recd[BUFSZ];
    char nullStr[7] =  {'<','N','U','L','L','>',0x00};
    int  i;
    
    //in most cases each string is a buffer, so we need to ensure both command and response
    //are not NULL and null-terminated before duplicastion and conversion.
    
    if ((sentStr != NULL) && (sentLen > 0))
    {
        for (i = 0; i < sentLen; i++)
            sent[i] = sentStr[i];  
        sent[sentLen] = 0x00;
        drake_r8_fix_string((char*)sent);
    }
    else
    {
        for (i = 0; i < 7; i++)
            sent[i] = nullStr[i];  
    }
    
    if ((recdStr != NULL) && (recdLen > 0))
    {
        for (i = 0; i < recdLen; i++)
            recd[i] = recdStr[i];  
        recd[recdLen] = 0x00;
        drake_r8_fix_string((char*)recd);
    }
    else
    {
        for (i = 0; i < 7; i++)
            recd[i] = nullStr[i];  
    }
    
    rig_debug(RIG_DEBUG_WARN, "Hamlib %s: Result %d - Sent %d chars: %s, Recd %d chars: %s\n", hdrStr, res, sentLen, sent, recdLen, recd);

}


/*
 * drake_r8_transaction
 * We assume that rig!=NULL, STATE(rig)!= NULL, data!=NULL, data_len!=NULL
 */
int drake_r8_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                      int *data_len)
{
    int retval;
    hamlib_port_t *rp = RIGPORT(rig);
    
    rig_flush(rp);

    //fprintf(stderr, "Sending %d bytes: %s.\n", cmd_len, cmd);
    retval = write_block(rp, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        //fprintf(stderr, "Send error %d.\n", retval);
        if ((data) && (data_len))
        {
            data[0] = 0x00;
            *data_len = 0;
        }
        return retval;
    }

    /* no data expected, TODO: flush input? */
    if (!data || !data_len)
    {
        //fprintf(stderr, "No response expected.\n");
        return 0;
    }

    //fprintf(stderr, "Receiving...\n");
    retval = read_string(rp, (unsigned char *) data, BUFSZ,
                         LF, 1, 0, 1);

    //if (retval == -RIG_ETIMEOUT)
    //{
        //fprintf(stderr, "Receive timeout.\n");
        //data[0] = 0x00;
        //*data_len = 0;
    //}

    if (retval < 0)
    {
        //fprintf(stderr, "Receive error %d.\n", retval);
        data[0] = 0x00;
        *data_len = 0;
        return retval;
    }

    //fprintf(stderr, "Received %d bytes.\n", retval);
    *data_len = retval;
    data[*data_len] = 0x00;

    return RIG_OK;
}


int drake_r8_init(RIG *rig)
{
    struct drake_priv_data *priv;
    STATE(rig)->priv = calloc(1, sizeof(struct drake_priv_data));

    if (!STATE(rig)->priv)
    {
        return -RIG_ENOMEM;
    }

    priv = STATE(rig)->priv;

    priv->curr_ch = 0;

    return RIG_OK;
}


int drake_r8_cleanup(RIG *rig)
{
    struct drake_priv_data *priv = STATE(rig)->priv;

    free(priv);

    return RIG_OK;
}


/*
 * drake_set_freq
 * Assumes rig!=NULL
 */
int drake_r8_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[16], ackbuf[16];
    int ack_len, retval;

    /*
     * 10Hz resolution
     * TODO: round nearest?
     */
    SNPRINTF((char *) freqbuf, sizeof(freqbuf), "F%07u" EOM, (unsigned int)freq / 10);
             
    retval = drake_r8_transaction(rig, freqbuf, strlen(freqbuf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[2] = {0x0d, 0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ackbuf[1] = testbuf[1];
        ack_len = 2;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("set_freq", freqbuf, strlen(freqbuf), ackbuf, ack_len, retval);

    return retval;
}


/*
 * drake_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int drake_r8_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int freq_len, retval;
    char freqbuf[BUFSZ];
    double f;
    //char fmult;

    retval = drake_r8_transaction(rig, "RF" EOM, 3, freqbuf, &freq_len);

    //let's trick it
    /*
    char testbuf[15] = {' ', '1', '5', '.', '0', '0', '0', '0', '0', '#', 'm', 'H', 'z', 0x0d, 0x0a }; //mem off, ch 00, NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
    if (freq_len == 0)
    {
        for (int i=0; i < 15; i++) {
            freqbuf[i] = testbuf[i];
        }
        freq_len = 15;
        freqbuf[freq_len] = 0x00;
        retval = RIG_OK;
    }*/

    drake_r8_trans_rept("get_freq", "RF" EOM, 3, freqbuf, freq_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* RA command returns fff.fffff*mHz<CR><LF> */
    if (freq_len != 15)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_freq: wrong answer %s, "
                  "len=%d\n", freqbuf, freq_len);
        return -RIG_ERJCTED;
    }

    freqbuf[9] = '\0';

    /* extract freq */
    sscanf(freqbuf + 1, "%lf", &f);
    f *= 1000000.0;

    *freq = (freq_t)f;

    return RIG_OK;
}


/*
 * drake_set_vfo
 * Assumes rig!=NULL
 */
int drake_r8_set_vfo(RIG *rig, vfo_t vfo)
{
    char cmdbuf[16], ackbuf[16];
    int ack_len, retval;
    char vfo_function;

    switch (vfo)
    {
    case RIG_VFO_A  : vfo_function = 'A'; break;

    case RIG_VFO_B  : vfo_function = 'B'; break;

    case RIG_VFO_VFO: vfo_function = 'F'; break;

    case RIG_VFO_MEM: vfo_function = 'C'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "drake_set_vfo: unsupported VFO %s\n",
                  rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if ((vfo_function == 'A') || (vfo_function == 'B'))
    {
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "V%c" EOM, vfo_function);
    }

    if ((vfo_function == 'F') || (vfo_function == 'C'))
    {
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "%c" EOM, vfo_function);
    }

    retval = drake_r8_transaction(rig, cmdbuf, strlen(cmdbuf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[1] = {0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ack_len = 1;
        ackbuf[ack_len] = 0x00;
        retval = RIG_OK;
    }*/

    drake_r8_trans_rept("set_vfo", cmdbuf, strlen(cmdbuf), ackbuf, ack_len, retval);

    return retval;
}



/*
 * drake_get_vfo
 * Assumes rig!=NULL
 */
int drake_r8_get_vfo(RIG *rig, vfo_t *vfo)
{
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];

    retval = drake_r8_transaction(rig, "RA" EOM, 3, mdbuf, &mdbuf_len);

    //let's trick it
    /*
    char testbuf[25] = {' ','0','0',' ','2','0','2','<','8', ' ', ' ', '1', '5', '.', '0', '0', '0', '0', '0', '#', 'm', 'H', 'z', 0x0d, 0x0a }; //mem off, ch 00, NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
    if (mdbuf_len == 0)
    {
        for (int i=0; i < 25; i++) {
            mdbuf[i] = testbuf[i];
        }
        mdbuf_len = 25;
        mdbuf[mdbuf_len] = 0x00;
        retval = RIG_OK;
    }*/

    drake_r8_trans_rept("get_vfo", "RA" EOM, 3, mdbuf, mdbuf_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != 25)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_vfo: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    if (mdbuf[0] == '*')
    {
        *vfo = RIG_VFO_MEM;
    }
    else
    {
        char cvfo = (mdbuf[8] & 0x38);

        switch (cvfo)
        {
        case '0' : *vfo = RIG_VFO_B; break;

        case '8' : *vfo = RIG_VFO_A; break;

        default : rig_debug(RIG_DEBUG_ERR,
                                "drake_get_vfo: unsupported vfo %c\n", cvfo);
            *vfo = RIG_VFO_VFO;
            return -RIG_EINVAL;
        }
    }

    return RIG_OK;
}


/*
 * drake_set_mode
 * Assumes rig!=NULL
 */
int drake_r8_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[16], ackbuf[16];
    char mode_sel;
    int ack_len, retval;

    switch (mode)
    {
    case RIG_MODE_CW:       mode_sel = MD_CW; break;

    case RIG_MODE_USB:      mode_sel = MD_USB; break;

    case RIG_MODE_LSB:      mode_sel = MD_LSB; break;

    case RIG_MODE_FM:       mode_sel = MD_FM; break;

    case RIG_MODE_AMS:
    case RIG_MODE_AM:       mode_sel = MD_AM; break;

    case RIG_MODE_RTTY:     mode_sel = MD_RTTY; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "drake_set_mode: "
                  "unsupported mode %s\n", rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF((char *) mdbuf, sizeof(mdbuf), "M%c" EOM, mode_sel);
    retval = drake_r8_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[1] = {0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ack_len = 1;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("set_mode", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (mode != RIG_MODE_FM)
        {
            unsigned int width_sel;

            if (width == RIG_PASSBAND_NORMAL)
            {
                width = rig_passband_normal(rig, mode);
            }

            if (width <= 500)
            {
                width_sel = '0';
            }
            else if (width <= 1800)
            {
                width_sel = '1';
            }
            else if (width <= 2300)
            {
                width_sel = '2';
            }
            else if (width <= 4000)
            {
                width_sel = '4';
            }
            else
            {
                width_sel = '6';
            }

            SNPRINTF((char *) mdbuf, sizeof(mdbuf), "W%c" EOM, width_sel);
            retval = drake_r8_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);
            
            //let's trick it
            /*
            char testbuf[1] = {0x0a};
            if (ack_len == 0)
            {
                ackbuf[0] = testbuf[0];
                ack_len = 1;
                ackbuf[ack_len] = 0x00;
                retval = 0;
            }*/

            drake_r8_trans_rept("set_bw", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);

        }
    }

    if ((mode == RIG_MODE_AMS) || (mode == RIG_MODE_AM))
    {
        SNPRINTF((char *) mdbuf, sizeof(mdbuf), "S%c" EOM,
                 (mode == RIG_MODE_AMS) ? 'O' : 'F');
        retval = drake_r8_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

        //let's trick it
        /*
        char testbuf[1] = {0x0a};
        if (ack_len == 0)
        {
            ackbuf[0] = testbuf[0];
            ack_len = 1;
            ackbuf[ack_len] = 0x00;
            retval = 0;
        }*/

        drake_r8_trans_rept("set_synch", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);
    }

    return retval;
}


/*
 * drake_get_mode
 * Assumes rig!=NULL
 */
int drake_r8_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];
    char cmode;
    char cwidth;
    char csynch;

    retval = drake_r8_transaction(rig, "RM" EOM, 3, mdbuf, &mdbuf_len);

    //let's trick it
    /*
    char testbuf[7] = {'2','0','2','<','8', 0x0d, 0x0a}; //NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
    if (mdbuf_len == 0)
    {
        for (int i=0; i < 7; i++) {
            mdbuf[i] = testbuf[i];
        }
        mdbuf_len = 7;
        mdbuf[mdbuf_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("get_mode", "RM" EOM, 3, mdbuf, mdbuf_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len < 7)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_mode: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    cmode = mdbuf[2];
    cwidth = mdbuf[3];
    csynch = mdbuf[4];

    switch (cwidth & 0x37)
    {
    case '0': *width = s_Hz(500); break;

    case '1': *width = s_Hz(1800); break;

    case '2': *width = s_Hz(2300); break;

    case '3': *width = s_Hz(4000); break;

    case '4': *width = s_Hz(6000); break;

    default :
        rig_debug(RIG_DEBUG_ERR,
                  "drake_get_mode: unsupported width %c\n",
                  cwidth);
        *width = RIG_PASSBAND_NORMAL;
        return -RIG_EINVAL;
    }

    if ((cwidth >= '0') && (cwidth <= '4'))
    {
        switch (cmode & 0x33)
        {
        case '0': *mode = RIG_MODE_LSB; break;

        case '1': *mode = RIG_MODE_RTTY; break;

        case '2': *mode = RIG_MODE_FM; *width = s_Hz(12000); break;

        default :
            rig_debug(RIG_DEBUG_ERR,
                      "drake_get_mode: unsupported mode %c\n",
                      cmode);
            *mode = RIG_MODE_NONE;
            return -RIG_EINVAL;
        }
    }
    else
    {
        switch (cmode & 0x33)
        {
        case '0': *mode = RIG_MODE_USB; break;

        case '1': *mode = RIG_MODE_CW; break;

        case '2': *mode = RIG_MODE_AM; break;

        default :
            rig_debug(RIG_DEBUG_ERR,
                      "drake_get_mode: unsupported mode %c\n",
                      cmode);
            *mode = RIG_MODE_NONE;
            return -RIG_EINVAL;
        }
    }

    if ((csynch & 0x34) == '4')
    {
        if (*mode == RIG_MODE_AM)
        {
            *mode = RIG_MODE_AMS;
        }
        else if (*mode == RIG_MODE_USB)
        {
            *mode = RIG_MODE_ECSSUSB;
        }
        else if (*mode == RIG_MODE_LSB)
        {
            *mode = RIG_MODE_ECSSLSB;
        }
    }

    return RIG_OK;
}


/*
 * drake_set_ant
 * Assumes rig!=NULL
 */
int drake_r8_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    char buf[16], ackbuf[16];
    int ack_len, retval;

    SNPRINTF((char *) buf, sizeof(buf), "A%c" EOM,
             ant == RIG_ANT_1 ? '1' : (ant == RIG_ANT_2 ? '2' : 'C'));

    retval = drake_r8_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

     //let's trick it
    /*
    char testbuf[1] = {0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ack_len = 1;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

   drake_r8_trans_rept("set_ant", buf, strlen(buf), ackbuf, ack_len, retval);

    return retval;
}


/*
 * drake_get_ant
 * Assumes rig!=NULL
 */
int drake_r8_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                  ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];
    char cant;

    retval = drake_r8_transaction(rig, "RM" EOM, 3, mdbuf, &mdbuf_len);

    //let's trick it
    /*
    char testbuf[7] = {'2','0','2','<','8', 0x0d, 0x0a}; //NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
    if (mdbuf_len == 0)
    {
        for (int i=0; i < 7; i++) {
            mdbuf[i] = testbuf[i];
        }
        mdbuf_len = 7;
        mdbuf[mdbuf_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("get_ant", "RM" EOM, 3, mdbuf, mdbuf_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len < 7)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_ant: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    cant = mdbuf[2];

    switch (cant & 0x3c)
    {
    case '0': *ant_curr = RIG_ANT_1; break;

    case '4': *ant_curr = RIG_ANT_3; break;

    case '8': *ant_curr = RIG_ANT_2; break;

    default :
        rig_debug(RIG_DEBUG_ERR,
                  "drake_get_ant: unsupported antenna %c\n",
                  cant);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * drake_set_mem
 * Assumes rig!=NULL
 */
int drake_r8_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int ack_len, retval;
    char buf[16], ackbuf[16];
    struct drake_priv_data *priv = STATE(rig)->priv;

    priv->curr_ch = ch;

    SNPRINTF(buf, sizeof(buf), "C%02d" , ch);

    ack_len = 0; // fix compile-time warning "possibly uninitialized"
    retval = drake_r8_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[2] = {0x0d, 0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ackbuf[1] = testbuf[1];
        ack_len = 2;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("set_mem", buf, strlen(buf), ackbuf, ack_len, retval);

    if (ack_len != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_set_mem: could not set channel %02d.\n", ch);
        retval = -RIG_ERJCTED;
    }

    return retval;
}

/*
 * drake_get_mem
 * Assumes rig!=NULL
 */
int drake_r8_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct drake_priv_data *priv = STATE(rig)->priv;
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];
    int chan;

    retval = drake_r8_transaction(rig, "RC" EOM, 3, mdbuf, &mdbuf_len);

    //let's trick it
    /*
    char testbuf[5] = {' ','0','0', 0x0d, 0x0a };
    if (mdbuf_len == 0)
    {
        for (int i=0; i < 5; i++) {
            mdbuf[i] = testbuf[i];
        }
        mdbuf_len = 5;
        mdbuf[mdbuf_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("get_mem", "RC" EOM, 3, mdbuf, mdbuf_len, retval);
 
    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_mem: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    mdbuf[3] = '\0';

    /* extract channel no */
    sscanf(mdbuf + 1, "%02d", &chan);
    *ch = chan;

    priv->curr_ch = chan;

    return RIG_OK;
}


/*
 * drake_set_chan
 * Assumes rig!=NULL
 */
int drake_r8_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    const struct drake_priv_data *priv = STATE(rig)->priv;
    vfo_t   old_vfo;
    int     old_chan;
    char    mdbuf[16], ackbuf[16];
    int     ack_len, retval;
    value_t dummy;

    dummy.i = 0;

    drake_r8_get_vfo(rig, &old_vfo);
    old_chan = 0;

    /* set to vfo if needed */
    if (old_vfo == RIG_VFO_MEM)
    {
        old_chan = priv->curr_ch;
        retval = drake_r8_set_vfo(rig, RIG_VFO_VFO);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    /* set all memory features */
    drake_r8_set_ant(rig, RIG_VFO_CURR, chan->ant, dummy);
    drake_r8_set_freq(rig, RIG_VFO_CURR, chan->freq);
    drake_r8_set_mode(rig, RIG_VFO_CURR, chan->mode, chan->width);
    drake_r8_set_func(rig, RIG_VFO_CURR, RIG_FUNC_NB,
                   (chan->funcs & RIG_FUNC_NB) == RIG_FUNC_NB);
    drake_r8_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_AGC,
                    chan->levels[rig_setting2idx(RIG_LEVEL_AGC)]);
    drake_r8_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_PREAMP,
                    chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)]);
    drake_r8_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_ATT,
                    chan->levels[rig_setting2idx(RIG_LEVEL_ATT)]);
    drake_r8_set_func(rig, RIG_VFO_CURR, RIG_FUNC_MN,
                   (chan->funcs & RIG_FUNC_MN) == RIG_FUNC_MN);

    SNPRINTF(mdbuf, sizeof(mdbuf), "PR" EOM "%02d" EOM, chan->channel_num);
    retval = drake_r8_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[2] = {0x0d, 0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ackbuf[1] = testbuf[1];
        ack_len = 2;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("set_chan", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);

    if (old_vfo == RIG_VFO_MEM)
    {
        drake_r8_set_mem(rig, RIG_VFO_CURR, old_chan);
    }

    return retval;
}


/*
 * drake_get_chan
 * Assumes rig!=NULL
 */
int drake_r8_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    const struct drake_priv_data *priv = STATE(rig)->priv;
    vfo_t   old_vfo;
    int     old_chan;
    char    mdbuf[BUFSZ], freqstr[BUFSZ];
    int     mdbuf_len, retval;

    chan->vfo = RIG_VFO_MEM;
    chan->ant = RIG_ANT_NONE;
    chan->freq = 0;
    chan->mode = RIG_MODE_NONE;
    chan->width = RIG_PASSBAND_NORMAL;
    chan->tx_freq = 0;
    chan->tx_mode = RIG_MODE_NONE;
    chan->tx_width = RIG_PASSBAND_NORMAL;
    chan->split = RIG_SPLIT_OFF;
    chan->tx_vfo = RIG_VFO_NONE;
    chan->rptr_shift = RIG_RPT_SHIFT_NONE;
    chan->rptr_offs = 0;
    chan->tuning_step = 0;
    chan->rit = 0;
    chan->xit = 0;
    chan->funcs = 0;
    chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_OFF;
    chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 0;
    chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = 0;
    chan->ctcss_tone = 0;
    chan->ctcss_sql = 0;
    chan->dcs_code = 0;
    chan->dcs_sql = 0;
    chan->scan_group = 0;
    chan->flags = RIG_CHFLAG_SKIP;
    strcpy(chan->channel_desc, "       ");

    drake_r8_get_vfo(rig, &old_vfo);
    old_chan = 0;

    if (old_vfo == RIG_VFO_MEM)
    {
        old_chan = priv->curr_ch;
    }

    //go to new channel
    retval = drake_r8_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

    if (retval != RIG_OK)
    {
        return RIG_OK;
    }

    //now decipher it
    retval = drake_r8_transaction(rig, "RA" EOM, 3, mdbuf, &mdbuf_len);

    drake_r8_trans_rept("get_chan", "RA" EOM, 3, mdbuf, mdbuf_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != 25)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_channel: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    if ((mdbuf[4] >= '<') && (mdbuf[4] <= '?'))
    {
        chan->funcs |= RIG_FUNC_NB;
    }
    if ((mdbuf[4] >= '4') && (mdbuf[4] <= '7'))
    {
        chan->funcs |= RIG_FUNC_NB2;
    }

    switch (mdbuf[4] & 0x33)
    {
    case '0': chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_OFF; break;

    case '2': chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_FAST; break;

    case '3': chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_SLOW; break;

    default : chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_FAST;
    }

    if ((mdbuf[5] & 0x3c) == '8')
    {
        chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = 10;
    }

    if ((mdbuf[5] & 0x3c) == '4')
    {
        chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 10;
    }

    if ((mdbuf[5] & 0x32) == '2')
    {
        chan->funcs |= RIG_FUNC_MN;
    }

    switch (mdbuf[6] & 0x3c)
    {
    case '0': chan->ant = RIG_ANT_1; break;

    case '4': chan->ant = RIG_ANT_3; break;

    case '8': chan->ant = RIG_ANT_2; break;

    default : chan->ant = RIG_ANT_NONE;
    }

    switch (mdbuf[7] & 0x37)
    {
    case '0': chan->width = s_Hz(500); break;

    case '1': chan->width = s_Hz(1800); break;

    case '2': chan->width = s_Hz(2300); break;

    case '3': chan->width = s_Hz(4000); break;

    case '4': chan->width = s_Hz(6000); break;

    default : chan->width = RIG_PASSBAND_NORMAL;
    }

    if ((mdbuf[7] >= '0') && (mdbuf[7] <= '4'))
    {
        switch (mdbuf[6] & 0x33)
        {
        case '0': chan->mode = RIG_MODE_LSB; break;

        case '1': chan->mode = RIG_MODE_RTTY; break;

        case '2': chan->mode = RIG_MODE_FM;
            chan->width = s_Hz(12000); break;

        default : chan->mode = RIG_MODE_NONE;
        }
    }
    else
    {
        switch (mdbuf[6] & 0x33)
        {
        case '0': chan->mode = RIG_MODE_USB; break;

        case '1': chan->mode = RIG_MODE_CW; break;

        case '2': chan->mode = RIG_MODE_AM; break;

        default : chan->mode = RIG_MODE_NONE;
        }
    }

    if ((mdbuf[8] & 0x34) == '4')
    {
        if (chan->mode == RIG_MODE_AM)
        {
            chan->mode = RIG_MODE_AMS;
        }
    }

    strncpy(freqstr, mdbuf + 10, 9);
    freqstr[9] = 0x00;

    if ((mdbuf[20] == 'k') || (mdbuf[20] == 'K'))
    {
        chan->freq = strtod(freqstr, NULL) * 1000.0;
    }

    if ((mdbuf[20] == 'm') || (mdbuf[20] == 'M'))
    {
        chan->freq = strtod(freqstr, NULL) * 1000000.0;
    }

    //now put the radio back the way it was
    //we apparently can't do a read-only channel read
    if (old_vfo != RIG_VFO_MEM)
    {
        retval = drake_r8_set_vfo(rig, RIG_VFO_VFO);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }
    else
    {
        retval = drake_r8_set_mem(rig, RIG_VFO_CURR, old_chan);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return RIG_OK;
}


/*
 * drake_vfo_op
 * Assumes rig!=NULL
 */
int drake_r8_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    const struct drake_priv_data *priv = STATE(rig)->priv;
    char buf[16], ackbuf[16];
    int len, ack_len, retval;

    switch (op)
    {
    case RIG_OP_UP:
        SNPRINTF(buf, sizeof(buf), "U");
        break;

    case RIG_OP_DOWN:
        SNPRINTF(buf, sizeof(buf), "D");
        break;

    case RIG_OP_TO_VFO:
        SNPRINTF(buf, sizeof(buf), "F" EOM);
        break;

    case RIG_OP_FROM_VFO:
        SNPRINTF(buf, sizeof(buf), "PR" EOM "%02d" EOM, priv->curr_ch);
        break;

    default:
        return -RIG_EINVAL;
    }

    len = strlen(buf);
    retval = drake_r8_transaction(rig, buf, len, len == 1 ? ackbuf : NULL, 
                                                 len == 1 ? &ack_len : NULL);

    //let's trick it
    /*
    if ((op == RIG_OP_TO_VFO) || (op == RIG_OP_FROM_VFO))
    {
        char testbuf[2] = {0x0d, 0x0a};
        if (ack_len == 0)
        {
            ackbuf[0] = testbuf[0];
            ackbuf[1] = testbuf[1];
            ack_len = 2;
            ackbuf[ack_len] = 0x00;
            retval = 0;
        }
    }*/

    drake_r8_trans_rept("vfo_op", buf, len, buf[len - 1] == 0x0d ? ackbuf : NULL, ack_len, retval);

    return retval;
}


/*
 * drake_set_func
 * Assumes rig!=NULL
 */
int drake_r8_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[16], ackbuf[16];
    int ack_len, retval;
    char blanker = ' ';

    switch (func)
    {
    case RIG_FUNC_MN:
        SNPRINTF(buf, sizeof(buf), "N%c" EOM, status ? 'O' : 'F');
        break;

    case RIG_FUNC_NB:
    case RIG_FUNC_NB2:
        if (!status)
            blanker = 'F';
        else if (func == RIG_FUNC_NB)
            blanker = 'W';
        else //if (func == RIG_FUNC_NB2)
            blanker = 'N';        
        SNPRINTF(buf, sizeof(buf), "B%c" EOM, blanker);
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = drake_r8_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[1] = {0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ack_len = 1;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("set_func", buf, strlen(buf), ackbuf, ack_len, retval);

    return retval;
}


/*
 * drake_get_func
 * Assumes rig!=NULL
 */
int drake_r8_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int mdbuf_len, retval;
    char mdbuf[BUFSZ];
    char mc;
    char blanker;

    retval = drake_r8_transaction(rig, "RM" EOM, 3, mdbuf, &mdbuf_len);

    //let's trick it
    /*
    char testbuf[7] = {'2','0','2','<','8', 0x0d, 0x0a}; //NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
    if (mdbuf_len == 0)
    {
        for (int i=0; i < 7; i++) {
            mdbuf[i] = testbuf[i];
        }
        mdbuf_len = 7;
        mdbuf[mdbuf_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("get_func", "RM" EOM, 3, mdbuf, mdbuf_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len < 7)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_func: wrong answer %s, "
                  "len=%d\n", mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    switch (func)
    {
    case RIG_FUNC_MN:
        mc = mdbuf[1];
        *status = ((mc & 0x32) == '2');
        break;

    case RIG_FUNC_NB:
    case RIG_FUNC_NB2:
        mc = mdbuf[0];
        blanker = ' ';
        if ((mc >= '<') && (mc <= '?'))
            blanker = 'W';
        else
          if ((mc >= '4') && (mc <= '7'))
              blanker = 'N';
          else
              blanker = 'F';
        *status = (((func == RIG_FUNC_NB) && (blanker == 'W')) || ((func == RIG_FUNC_NB2) && (blanker == 'N')));
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get func %s\n", rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * drake_set_level
 * Assumes rig!=NULL
 */
int drake_r8_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[16], ackbuf[16];
    int ack_len, retval;

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        SNPRINTF(buf, sizeof(buf), "G%c" EOM, val.i ? '+' : '0');
        break;

    case RIG_LEVEL_ATT:
        SNPRINTF(buf, sizeof(buf), "G%c" EOM, val.i ? '-' : '0');
        break;

    case RIG_LEVEL_AGC:
        SNPRINTF(buf, sizeof(buf), "A%c" EOM,
                 val.i == RIG_AGC_OFF ? 'O' :
                 (val.i == RIG_AGC_FAST ? 'F' : 'S'));
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = drake_r8_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[1] = {0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ack_len = 1;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("set_level", buf, strlen(buf), ackbuf, ack_len, retval);

    return retval;
}


/*
 * drake_get_level
 * Assumes rig!=NULL
 */
int drake_r8_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int lvl_len, retval;
    char lvlbuf[BUFSZ];
    char mc;

    if ((level == RIG_LEVEL_STRENGTH) || (level == RIG_LEVEL_RAWSTR))
    {
        retval = drake_r8_transaction(rig, "RA" EOM, 3, lvlbuf, &lvl_len);

        //let's trick it
        /*
        char testbuf[25] = {' ','0','0',' ','2','0','2','<','8', ' ', ' ', '1', '5', '.', '0', '0', '0', '0', '0', '#', 'm', 'H', 'z', 0x0d, 0x0a }; //mem off, ch 00, NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
        if (lvl_len == 0)
        {
            for (int i=0; i < 25; i++) {
                lvlbuf[i] = testbuf[i];
            }
            lvl_len = 25;
            lvlbuf[lvl_len] = 0x00;
            retval = 0;
        }*/

        drake_r8_trans_rept("get_level", "RA" EOM, 3, lvlbuf, lvl_len, retval);

    }
    else
    {
        retval = drake_r8_transaction(rig, "RM" EOM, 3, lvlbuf, &lvl_len);

        //let's trick it
        /*
        char testbuf[7] = {'2','0','2','<','8', 0x0d, 0x0a}; //NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
        if (lvl_len == 0)
        {
            for (int i=0; i < 7; i++) {
                lvlbuf[i] = testbuf[i];
            }
            lvl_len = 7;
            lvlbuf[lvl_len] = 0x00;
            retval = 0;
        }*/
        
        drake_r8_trans_rept("get_level", "RM" EOM, 3, lvlbuf, lvl_len, retval);

    }
    

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (lvl_len < 7)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_get_level: wrong answer %s, "
                  "len=%d\n", lvlbuf, lvl_len);
        return -RIG_ERJCTED;
    }

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        mc = lvlbuf[1];

        if ((mc & 0x3c) == '8')
        {
            val->i = 10;
        }
        else
        {
            val->i = 0;
        }

        break;

    case RIG_LEVEL_ATT:
        mc = lvlbuf[1];

        if ((mc & 0x3c) == '4')
        {
            val->i = 10;
        }
        else
        {
            val->i = 0;
        }

        break;

    case RIG_LEVEL_AGC:
        mc = lvlbuf[0];

        switch (mc & 0x33)
        {
        case '0': val->i = RIG_AGC_OFF; break;

        case '2': val->i = RIG_AGC_FAST; break;

        case '3': val->i = RIG_AGC_SLOW; break;

        default : val->i = RIG_AGC_FAST;
        }

        break;

    case RIG_LEVEL_RAWSTR:
        mc = lvlbuf[19];

        switch (mc)
        {
        case ' ': val->i = 0; break;

        case '#': val->i = 1; break;

        default : val->i = 0;
        }
        
        break;

    case RIG_LEVEL_STRENGTH:
        mc = lvlbuf[19];

        switch (mc)
        {
        case ' ': val->i = -60; break;

        case '#': val->i = 0; break;

        default : val->i = -60;
        }
        
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %s\n", rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


int drake_r8_set_powerstat(RIG *rig, powerstat_t status)
{
    char buf[16], ackbuf[16];
    int ack_len, retval;

    SNPRINTF(buf, sizeof(buf), "P%c" EOM, status == RIG_POWER_OFF ? 'F' : 'O');

    retval = drake_r8_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //let's trick it
    /*
    char testbuf[1] = {0x0a};
    if (ack_len == 0)
    {
        ackbuf[0] = testbuf[0];
        ack_len = 1;
        ackbuf[ack_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("set_power", buf, strlen(buf), ackbuf, ack_len, retval);
    
    return retval;
}


int drake_r8_get_powerstat(RIG *rig, powerstat_t *status)
{
    int mdlen, retval;
    char mdbuf[BUFSZ];

    retval = drake_r8_transaction(rig, "RM" EOM, 3, &mdbuf[0], &mdlen);

    //let's trick it
    /*
    char testbuf[7] = {'2','0','2','<','8', 0x0d, 0x0a}; //NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
    if (mdlen == 0)
    {
        for (int i=0; i < 7; i++) {
            mdbuf[i] = testbuf[i];
        }
        mdlen = 7;
        mdbuf[mdlen] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("get_power", "RM" EOM, 3, mdbuf, mdlen, retval);    

    if (retval != RIG_OK)
    {
        return retval;
    }

    *status = (mdlen >= 7);

    return RIG_OK;
}


/*
 * drake_get_info
 * Assumes rig!=NULL
 */
const char *drake_r8_get_info(RIG *rig)
{
    static char idbuf[BUFSZ];
    int retval, id_len;

    retval = drake_r8_transaction(rig, "ID" EOM, 3, idbuf, &id_len);

    //let's trick it
    /*
    char testbuf[4] = {'R','8',0x0d,0x0a};
    if (id_len == 0)
    {
        for (int i = 0; i < 4; i++)
          idbuf[i] = testbuf[i];
        id_len = 4;
        idbuf[id_len] = 0x00;
        retval = 0;
    }*/

    drake_r8_trans_rept("get_id", "ID" EOM, 3, idbuf, id_len, retval);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    idbuf[id_len - 2] = '\0';

    return idbuf;
}

