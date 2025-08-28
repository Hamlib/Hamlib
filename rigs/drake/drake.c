/*
 *  Hamlib Drake backend - main file
 *  Copyright (c) 2001-2008 by Stephane Fillod, 2025 Mark J. Fine
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <stdbool.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "cal.h"
#include "register.h"

#include "drake.h"


/*
 * Protocol information available at http://www.rldrake.com/swl/R8B.pdf
 */


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

/* DEBUGGING ROUTINES TO ECHO TRANSACTIONS WITH SCREEN READABLE CTRL CHARS */

/*
 * drake_fix_string
 * recursively replaces all special characters so they are readable at output
 *
 * input: inStr - the raw string to expand
 * 
 */
void drake_fix_string(char* inStr)
{
    const char  chChkAry[3] = {0x20, 0x0d, 0x0a};
    const char* chRepAry[3] = {"<SP>", "<CR>", "<LF>"};
    const char* chPos;
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
 * drake_trans_rept
 * non-destructively echoes transaction in a readable way for debugging
 *
 * inputs: hdrStr - the calling routine identifier
 *         sentStr - the command sent to the radio
 *         sentLen - the length of sentSt
 *         recdStr - the radio's response string
 *         recdLen - the length of recdStr
 *         res - The transaction response
 */
void drake_trans_rept(char* hdrStr, char* sentStr, int sentLen, char* recdStr, int recdLen, int res)
{
    char sent[BUFSZ];
    char recd[BUFSZ];
    const char nullStr[7] =  {'<','N','U','L','L','>',0x00};
    int  i;
    
    //in most cases each string is a buffer, so we need to ensure both command and response
    //are not NULL and null-terminated before duplication and conversion.
    
    if ((sentStr != NULL) && (sentLen > 0))
    {
        for (i = 0; i < sentLen; i++)
            sent[i] = sentStr[i];  
        sent[sentLen] = 0x00;
        drake_fix_string((char*)sent);
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
        drake_fix_string((char*)recd);
    }
    else
    {
        for (i = 0; i < 7; i++)
            recd[i] = nullStr[i];  
    }
    
    rig_debug(RIG_DEBUG_WARN, "Hamlib %s: Result %d - Sent %d chars: %s, Recd %d chars: %s\n", hdrStr, res, sentLen, sent, recdLen, recd);

}

/* PRIMARY COMMS LOOP TO RADIO */

/*
 * drake_transaction
 *
 * inputs:   rig - pointer to RIG structure
 *           cmd - buffer containing command to be sent to the radio
 *           cmd_len - the length of cmd
 *           data - buffer that will receive the radio's response string
 *           data_len - the length of data
 * returns : transaction error status
 *
 * We assume that rig!=NULL, STATE(rig)!= NULL, data!=NULL, data_len!=NULL
 * Note: size of data buffer must be BUFSZ otherwise a buffer overrun is possible
 */
int drake_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                      int *data_len)
{
    int retval;
    hamlib_port_t *rp = RIGPORT(rig);

    rig_flush(rp);

    // assume nothing.
    // initialize the buffer in case empty on return
    if ((data) && (data_len))
    {
        data[0] = 0x00;
        *data_len = 0;
    }

    retval = write_block(rp, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    // no data expected, TODO: flush input?
    if (!data || !data_len)
    {
        return 0;
    }

    retval = read_string(rp, (unsigned char *) data, BUFSZ,
                         LF, 1, 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;
    data[*data_len] = 0x00;

    return RIG_OK;
}

/* COMMON RADIO POLLING AND RESPONSE DECODING ROUTINES */

/*
 * drake_report_signal
 * Common routine to retrieve signal strength on R8A/B
 * Note that we use dcd to emulate signal presence on the R8
 * since it has no actual signal strength reporting capability
 *
 * inputs:  rig - pointer to RIG structure
 *          owner - the calling routine identifier
 * returns: command error status
 *
 * Assumes rig!=NULL 
 */
int drake_report_signal(RIG *rig, char* owner)
{
    char lvlbuf[BUFSZ];
    int lvl_len;
    int retval;
    struct drake_priv_data *priv = STATE(rig)->priv;
   
    retval = drake_transaction(rig, "RSS" EOM, 4, lvlbuf, &lvl_len);

    //char testbuf[5] = {'2', '5', '5', 0x0d, 0x0a };

    //drake_trans_rept(owner, "RSS" EOM, 4, lvlbuf, lvl_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (lvl_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                  "len=%d\n", owner, lvl_len);
        return -RIG_ERJCTED;
    }

    lvlbuf[3] = '\0';
    priv->curr_ss =  strtol(lvlbuf + 1, (char **)NULL, 16);
    
    return RIG_OK;
}


/*
 * drake_decode_frequency
 * Common routine to decode the frequency block 
 *
 * inputs: rig - pointer to RIG structure
 *         freqbuf - buffer containing radio's response string
 *         offset - offset to beginning of string due to differences in R8 vs. R8A/B
 *         Note: strings are virtually identical. offset is provided as a failsafe.
 *
 * Assumes rig!=NULL
 */
void drake_decode_frequency(RIG *rig, char* freqbuf, int offset)
{
    double f;
    char fmult;
    struct drake_priv_data *priv = STATE(rig)->priv;
  
    if ((freqbuf[9+offset] == '*') || (freqbuf[9+offset] == '#'))
        priv->curr_dcd = RIG_DCD_ON;
    else
        priv->curr_dcd = RIG_DCD_OFF;
    
    fmult = freqbuf[10+offset];
    
    freqbuf[9+offset] = '\0';

    // extract freq
    sscanf(freqbuf+offset, "%lf", &f);
    f *= 1000.0;
    
    if ((fmult == 'm') || (fmult == 'M'))
        f *= 1000.0;

    priv->curr_freq = (freq_t)f;
}


/*
 * drake_report_freq
 * Common routine to retrieve frequency and squelch settings (used for DCD)
 * Data stored in priv for any routine to use
 *
 * inputs:  rig - pointer to RIG structure
 *          owner - the calling routine identifier
 * returns: command error status
 *
 * Assumes rig!=NULL
 */
int drake_report_frequency(RIG *rig, char* owner)
{
    char freqbuf[BUFSZ];
    int freq_len;
    int retval;
    
    retval = drake_transaction(rig, "RF" EOM, 3, freqbuf, &freq_len);

    //char testbuf[15] = {' ', '1', '5', '.', '0', '0', '0', '0', '0', '#', 'm', 'H', 'z', 0x0d, 0x0a };
    
    //drake_trans_rept(owner, "RF" EOM, 3, freqbuf, freq_len, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (freq_len != 15)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer %s, "
                  "len=%d\n", owner, freqbuf, freq_len);
        return -RIG_ERJCTED;
    }
   
    drake_decode_frequency(rig, freqbuf, 0);
    
    return RIG_OK;    
}


/*
 * drake_decode_mode
 * Common routine to break out the 5-character mode string
 *
 * inputs: rig - pointer to RIG structure
 *         mdbuf - buffer containing radio's response string
 *         offset - offset to beginning of string due to differences in R8 vs. R8A/B
 *
 * Assumes rig!=NULL
 */
void drake_decode_mode(RIG *rig, char* mdbuf, int offset)
{
    struct drake_priv_data *priv = STATE(rig)->priv;

    //noise blanker
    priv->curr_nb =  ((mdbuf[offset] >= '<') && (mdbuf[offset] <= '?'));
    priv->curr_nb2 =  ((mdbuf[offset] >= '4') && (mdbuf[offset] <= '7'));
    
    //agc
    switch (mdbuf[offset] & 0x33)
    {
    case '0': priv->curr_agc = RIG_AGC_OFF; break;

    case '2': priv->curr_agc = RIG_AGC_FAST; break;

    case '3': priv->curr_agc = RIG_AGC_SLOW; break;

    default : priv->curr_agc = RIG_AGC_OFF;
    }

    //preamp, attenuator and notch
    priv->curr_pre = ((mdbuf[1+offset] & 0x3c) == '8');
    priv->curr_att = ((mdbuf[1+offset] & 0x3c) == '4');
    priv->curr_notch = ((mdbuf[1+offset] & 0x32) == '2');

    //ant
    switch (mdbuf[2+offset] & 0x3c)
    {
    case '0': priv->curr_ant = RIG_ANT_1; break;

    case '4': priv->curr_ant = RIG_ANT_3; break;

    case '8': priv->curr_ant = RIG_ANT_2; break;

    default : priv->curr_ant = RIG_ANT_1;
    }

    //width
    switch (mdbuf[3+offset] & 0x37)
    {
    case '0': priv->curr_width = s_Hz(500); break;

    case '1': priv->curr_width = s_Hz(1800); break;

    case '2': priv->curr_width = s_Hz(2300); break;

    case '3': priv->curr_width = s_Hz(4000); break;

    case '4': priv->curr_width = s_Hz(6000); break;

    default : priv->curr_width = RIG_PASSBAND_NORMAL;
    }

    //mode
    if ((mdbuf[3+offset] >= '0') && (mdbuf[3+offset] <= '4'))
    {
        switch (mdbuf[2+offset] & 0x33)
        {
        case '0': priv->curr_mode = RIG_MODE_LSB; break;

        case '1': priv->curr_mode = RIG_MODE_RTTY; break;

        case '2': priv->curr_mode = RIG_MODE_FM; priv->curr_width = s_Hz(12000); break;

        default : priv->curr_mode = RIG_MODE_NONE;
        }
    }
    else
    {
        switch (mdbuf[2+offset] & 0x33)
        {
        case '0': priv->curr_mode = RIG_MODE_USB; break;

        case '1': priv->curr_mode = RIG_MODE_CW; break;

        case '2': priv->curr_mode = RIG_MODE_AM; break;

        default : priv->curr_mode = RIG_MODE_NONE;
        }
    }
    //synch
    if ((mdbuf[4+offset] & 0x34) == '4')
    {
        if (priv->curr_mode == RIG_MODE_AM)
        {
            priv->curr_mode = RIG_MODE_AMS;
        }
        else if (priv->curr_mode == RIG_MODE_USB)
        {
            priv->curr_mode = RIG_MODE_ECSSUSB;
        }
        else if (priv->curr_mode == RIG_MODE_LSB)
        {
            priv->curr_mode = RIG_MODE_ECSSLSB;
        }
    }

    //vfo
    switch (mdbuf[4+offset] & 0x38)
    {
    case '0' : priv->curr_vfo = RIG_VFO_B; break;

    case '8' : priv->curr_vfo = RIG_VFO_A; break;

    default : priv->curr_vfo = RIG_VFO_VFO;
    }
}


/*
 * drake_report_mode
 * Common routine to retrieve NB, AGC, ATT, PRE, NF, ANT, MODE, BW, and VFO (and scanning) settings
 * Data stored in priv for any routine to use
 *
 * inputs:  rig - pointer to RIG structure
 *          owner - the calling routine identifier
 * returns: command error status
 *
 * Assumes rig!=NULL 
 */
int drake_report_mode(RIG *rig, char* owner)
{
    char mdbuf[BUFSZ];
    int mdbuf_len;
    int retval;
    int retlen;
    int mode_offset;
    struct drake_priv_data *priv = STATE(rig)->priv;

    retval = drake_transaction(rig, "RM" EOM, 3, mdbuf, &mdbuf_len);

    //r8
    //char testbuf[7] = {'2','0','2','<','8', 0x0d, 0x0a}; //NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
    //r8a/b - TODO Seems to be undocumented extra character at beginning of string, pushing everything to the right
    //char testbuf[8] = {'?','2','0','2','<','8', 0x0d, 0x0a}; //NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning

    //drake_trans_rept(owner, "RM" EOM, 3, mdbuf, mdbuf_len, retval);

    if (rig->caps->rig_model == RIG_MODEL_DKR8)
    {
        retlen = 7;
        mode_offset = 0;
    }
    else
    {
        retlen = 8;
        mode_offset = 1;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != retlen)
    {
        priv->curr_pwr = false;
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer %s, "
                  "len=%d\n", owner, mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    priv->curr_pwr = true;
    drake_decode_mode(rig, mdbuf, mode_offset);

    return RIG_OK;
}


/*
 * drake_decode_mem_channel
 * Common routine to break out the 3 or 4-character mem string
 *
 * inputs: rig - pointer to RIG structure
 *         mdbuf - buffer containing radio's response string
 *         offset - offset to beginning of string due to differences in R8 vs. R8A/B
 *         Note: except for channel number size, strings are virtually identical. offset is provided as a failsafe.
 *
 * Assumes rig!=NULL
 */
void drake_decode_mem_channel(RIG *rig, char* mdbuf, int offset)
{
    struct drake_priv_data *priv = STATE(rig)->priv;

    if (mdbuf[offset] == '*')
    {
        priv->curr_vfo = RIG_VFO_MEM;
    }

    if (rig->caps->rig_model == RIG_MODEL_DKR8)
    {
        priv->curr_ch = (mdbuf[1+offset] & 0x0f) * 10 + (mdbuf[2+offset] & 0x0f);
    }
    else
    {
        priv->curr_ch = (mdbuf[1+offset] & 0x0f) * 100 + (mdbuf[2+offset] & 0x0f) * 10 + (mdbuf[3+offset] & 0x0f);
    }
}

/*
 * drake_report_mem_channel
 * Common routine to retrieve the memory channel number
 * Data stored in priv for any routine to use
 *
 * inputs:  rig - pointer to RIG structure
 *          owner - the calling routine identifier
 * returns: command error status
 *
 * Assumes rig!=NULL
 */
int drake_report_mem_channel(RIG *rig, char* owner)
{
    char mdbuf[BUFSZ];
    int mdbuf_len;
    int retval;
    int retlen;
    int chan_offset;

    retval = drake_transaction(rig, "RC" EOM, 3, mdbuf, &mdbuf_len);

    // r8  00 - 99
    //char testbuf[5] = {' ','0','0', 0x0d, 0x0a };
    // r8a/b 000 - 439
    //char testbuf[6] = {' ','0','0','0', 0x0d, 0x0a };

    //drake_trans_rept(owner, "RC" EOM, 3, mdbuf, mdbuf_len, retval);

    if (rig->caps->rig_model == RIG_MODEL_DKR8)
    {
        retlen = 5;
        chan_offset = 0;
    }
    else
    {
        retlen = 6;
        chan_offset = 0;
    }
 
    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len != retlen)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer %s, "
                  "len=%d\n", owner, mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    drake_decode_mem_channel(rig, mdbuf, chan_offset);

    return RIG_OK;
}


/*
 * drake_report_all
 * Common routine to retrieve all of the radio's settings
 * Data stored in priv for any routine to use
 * 
 * inputs:  rig - pointer to RIG structure
 *          owner - the calling routine identifier
 * returns: command error status
 *
 * Assumes rig!=NULL
 */
int drake_report_all(RIG *rig, char* owner)
{
    char mdbuf[BUFSZ];
    int mdbuf_len;
    int retval;
    int retlen;
    int mode_offset;
    int chan_offset;
    int freq_offset;
    
    retval = drake_transaction(rig, "RA" EOM, 3, mdbuf, &mdbuf_len);

    // strings show mem off, ch 00, NB off, AGC fast, RF off, MN off, ant 1, AM mode, 6.0 bw, VFOA, sync off, not scanning
    // r8
    //char testbuf[25] = {' ','0','0',' ','2','0','2','<','8',' ',' ','1','5','.','0','0','0','0','0','#','m','H','z', 0x0d, 0x0a };
    // r8a/b - TODO Note 7-char channel name that either starts at [25] or [26]
    //char testbuf[35] = {' ','0','0','0',' ','2','0','2','<','8',' ',' ','1','5','.','0','0','0','0','0','#','m','H','z',' ','M','E','M','N','A','M','E',' ', 0x0d, 0x0a };

    //drake_trans_rept(owner, "RA" EOM, 3, mdbuf, mdbuf_len, retval);

    if (rig->caps->rig_model == RIG_MODEL_DKR8)
    {
        retlen = 25;
        mode_offset = 4;
        chan_offset = 0;
        freq_offset = 10;
    }
    else
    {
        retlen = 35;
        mode_offset = 5;
        chan_offset = 0;
        freq_offset = 11;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mdbuf_len < retlen)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer %s, "
                  "len=%d\n", owner, mdbuf, mdbuf_len);
        return -RIG_ERJCTED;
    }

    // check RC *after* decoding the VFO in RM
    // otherwise RIG_VFO_MEM gets squashed
    drake_decode_mode(rig, mdbuf, mode_offset);
    drake_decode_mem_channel(rig, mdbuf, chan_offset);
    drake_decode_frequency(rig, mdbuf, freq_offset);
    
    // TODO handle channel name on R8A/B
    // TODO These models also have an additional RN (Report Name) command for polling channel names that is not handled here
        
    return RIG_OK;
}

/* COMMON HAMLIB INTERFACE ROUTINES */

int drake_init(RIG *rig)
{
    struct drake_priv_data *priv;
    STATE(rig)->priv = calloc(1, sizeof(struct drake_priv_data));

    if (!STATE(rig)->priv)
    {
        return -RIG_ENOMEM;
    }

    priv = STATE(rig)->priv;

    priv->curr_ch = 0;
    priv->curr_dcd = RIG_DCD_OFF;
    priv->curr_freq = 0.0;
    priv->curr_ant = RIG_ANT_1;
    priv->curr_vfo = RIG_VFO_VFO;
    priv->curr_agc = RIG_AGC_OFF;
    priv->curr_mode = RIG_MODE_NONE;
    priv->curr_width = RIG_PASSBAND_NORMAL;
    priv->curr_nb = false;
    priv->curr_nb2 = false;
    priv->curr_att = false;
    priv->curr_pre = false;
    priv->curr_pwr = false;
    priv->curr_ss = -60;

    return RIG_OK;
}


int drake_cleanup(RIG *rig)
{
    struct drake_priv_data *priv = STATE(rig)->priv;

    free(priv);

    return RIG_OK;
}


/*
 * drake_set_freq
 * Assumes rig!=NULL
 */
int drake_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int retval;

    // 10 Hz resolution - TODO: round nearest?
    SNPRINTF((char *) freqbuf, sizeof(freqbuf), "F%07u" EOM, (unsigned int)freq / 10);
    
    retval = drake_transaction(rig, freqbuf, strlen(freqbuf), ackbuf, &ack_len);

    //drake_trans_rept("drake_set_freq", freqbuf, strlen(freqbuf), ackbuf, ack_len, retval);

    return retval;
}


/*
 * drake_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int drake_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    struct drake_priv_data *priv = STATE(rig)->priv;

    retval = drake_report_frequency(rig, "drake_get_freq");

    if (retval == RIG_OK)
    {
        *freq = priv->curr_freq;
    }

    return retval;
}


/*
 * drake_set_vfo
 * Assumes rig!=NULL
 */
int drake_set_vfo(RIG *rig, vfo_t vfo)
{
    char cmdbuf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int retval;
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

    retval = drake_transaction(rig, cmdbuf, strlen(cmdbuf), ackbuf, &ack_len);

    //drake_trans_rept("drake_set_vfo", cmdbuf, strlen(cmdbuf), ackbuf, ack_len, retval);
    
    return retval;
}


/*
 * drake_get_vfo
 * Assumes rig!=NULL
 */
int drake_get_vfo(RIG *rig, vfo_t *vfo)
{
    int retval;
    struct drake_priv_data *priv = STATE(rig)->priv;

    retval = drake_report_all(rig, "drake_get_vfo");

    if (retval == RIG_OK)
    {
        *vfo = priv->curr_vfo;
    }

    return retval;
}


/*
 * drake_set_mode
 * Assumes rig!=NULL
 */
int drake_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int retval;
    char mode_sel;

    switch (mode)
    {
    case RIG_MODE_CW:       mode_sel = MD_CW; break;

    case RIG_MODE_ECSSUSB:
    case RIG_MODE_USB:      mode_sel = MD_USB; break;

    case RIG_MODE_ECSSLSB:
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

    retval = drake_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

    //drake_trans_rept("drake_set_mode", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);

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
            retval = drake_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);
            
            //drake_trans_rept("drake_set_bw", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);

        }
    }

    if ((mode == RIG_MODE_AMS) || (mode == RIG_MODE_ECSSUSB) || (mode == RIG_MODE_ECSSLSB) ||
            (mode == RIG_MODE_AM) || (mode == RIG_MODE_USB) || (mode == RIG_MODE_LSB))
    {
        SNPRINTF((char *) mdbuf, sizeof(mdbuf), "S%c" EOM,
                 ((mode == RIG_MODE_AMS) || (mode == RIG_MODE_ECSSUSB) || 
                  (mode == RIG_MODE_ECSSLSB)) ? 'O' : 'F');
        retval = drake_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

        //drake_trans_rept("drake_set_synch", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);
    }

    return retval;
}


/*
 * drake_get_mode
 * Assumes rig!=NULL
 */
int drake_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    struct drake_priv_data *priv = STATE(rig)->priv;

    retval = drake_report_mode(rig, "drake_get_mode");

    if (retval == RIG_OK)
    {
        *mode = priv->curr_mode;
        *width = priv->curr_width;
    }
    
    return retval;
}


/*
 * drake_set_ant
 * Assumes rig!=NULL
 */
int drake_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    char buf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int retval;

    SNPRINTF((char *) buf, sizeof(buf), "A%c" EOM,
             ant == RIG_ANT_1 ? '1' : (ant == RIG_ANT_2 ? '2' : 'C'));

    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //drake_trans_rept("drake_set_ant", buf, strlen(buf), ackbuf, ack_len, retval);

    return retval;
}


/*
 * drake_get_ant
 * Assumes rig!=NULL
 */
int drake_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                  ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    int retval;
    struct drake_priv_data *priv = STATE(rig)->priv;

    retval = drake_report_mode(rig, "drake_get_ant");

    if (retval == RIG_OK)
    {
        *ant_curr = priv->curr_ant;
    }
    
    return retval;
}


/*
 * drake_set_mem
 * Assumes rig!=NULL
 */
int drake_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char buf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int retval;
    struct drake_priv_data *priv = STATE(rig)->priv;

    priv->curr_ch = ch;

    if (rig->caps->rig_model == RIG_MODEL_DKR8)
    {
        SNPRINTF(buf, sizeof(buf), "C%02d", ch);
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "C%03d", ch);
    }

    ack_len = 0; // fix compile-time warning "possibly uninitialized"
    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //drake_trans_rept("drake_set_mem", buf, strlen(buf), ackbuf, ack_len, retval);

    if (ack_len != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "drake_set_mem: could not set channel %03d.\n", ch);
        retval = -RIG_ERJCTED;
    }

    return retval;
}


/*
 * drake_get_mem
 * Assumes rig!=NULL
 */
int drake_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    int retval;
    struct drake_priv_data *priv = STATE(rig)->priv;

    retval = drake_report_mem_channel(rig, "drake_get_mem");

    if (retval == RIG_OK)
    {
      *ch = priv->curr_ch;
    }
    
    return retval;
}


/*
 * drake_set_chan
 * Assumes rig!=NULL
 */
int drake_set_chan(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char    mdbuf[16];
    char    ackbuf[BUFSZ];
    int     ack_len;
    vfo_t   old_vfo = RIG_VFO_A;
    int     old_chan;
    int     retval;
    value_t dummy;
    const struct drake_priv_data *priv = STATE(rig)->priv;

    dummy.i = 0;

    drake_get_vfo(rig, &old_vfo);
    old_chan = 0;

    // set to vfo if needed
    if (old_vfo == RIG_VFO_MEM)
    {
        old_chan = priv->curr_ch;
        retval = drake_set_vfo(rig, RIG_VFO_VFO);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    // set all memory features
    drake_set_ant(rig, RIG_VFO_CURR, chan->ant, dummy);
    drake_set_freq(rig, RIG_VFO_CURR, chan->freq);
    drake_set_mode(rig, RIG_VFO_CURR, chan->mode, chan->width);
    drake_set_func(rig, RIG_VFO_CURR, RIG_FUNC_NB,
                   (chan->funcs & RIG_FUNC_NB) == RIG_FUNC_NB);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_AGC,
                    chan->levels[rig_setting2idx(RIG_LEVEL_AGC)]);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_PREAMP,
                    chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)]);
    drake_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_ATT,
                    chan->levels[rig_setting2idx(RIG_LEVEL_ATT)]);
    drake_set_func(rig, RIG_VFO_CURR, RIG_FUNC_MN,
                   (chan->funcs & RIG_FUNC_MN) == RIG_FUNC_MN);

    if (rig->caps->rig_model == RIG_MODEL_DKR8)
    {
        SNPRINTF(mdbuf, sizeof(mdbuf), "PR" EOM "%02d" EOM, chan->channel_num);
    }
    else
    {
        SNPRINTF(mdbuf, sizeof(mdbuf), "PR" EOM "%03d" EOM, chan->channel_num);
    }
    retval = drake_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

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

    //drake_trans_rept("drake_set_chan", mdbuf, strlen(mdbuf), ackbuf, ack_len, retval);

    if (old_vfo == RIG_VFO_MEM)
    {
        drake_set_mem(rig, RIG_VFO_CURR, old_chan);
    }

    return retval;
}


/*
 * drake_get_chan
 * Assumes rig!=NULL
 */
int drake_get_chan(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    vfo_t   old_vfo = RIG_VFO_A;
    int     old_chan;
    int     retval;
    const struct drake_priv_data *priv = STATE(rig)->priv;

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

    drake_get_vfo(rig, &old_vfo);
    old_chan = 0;

    if (old_vfo == RIG_VFO_MEM)
    {
        old_chan = priv->curr_ch;
    }

    // go to new channel
    retval = drake_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

    if (retval != RIG_OK)
    {
        return RIG_OK;
    }

    // now decipher it into priv
    retval = drake_report_all(rig, "drake_get_chan");

    if (retval != RIG_OK)
    {
        return RIG_OK;
    }

    if (priv->curr_nb)
    {
        chan->funcs |= RIG_FUNC_NB;
    }
    if (priv->curr_nb2)
    {
        chan->funcs |= RIG_FUNC_NB2;
    }

    chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = priv->curr_agc;
    chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = (priv->curr_pre ? 10 : 0);
    chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = (priv->curr_att ? 10 : 0);

    if (priv->curr_notch)
    {
        chan->funcs |= RIG_FUNC_MN;
    }
 
    chan->ant = priv->curr_ant;
    chan->width = priv->curr_width;
    chan->mode = priv->curr_mode;
    chan->freq = priv->curr_freq;

    // now put the radio back the way it was
    // we apparently can't do a read-only channel read
    if (old_vfo != RIG_VFO_MEM)
    {
        retval = drake_set_vfo(rig, RIG_VFO_VFO);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }
    else
    {
        retval = drake_set_mem(rig, RIG_VFO_CURR, old_chan);

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
int drake_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    char buf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int len;
    int retval;
    const struct drake_priv_data *priv = STATE(rig)->priv;

    switch (op)
    {
    case RIG_OP_UP:
        SNPRINTF(buf, sizeof(buf), "U");
        break;

    case RIG_OP_DOWN:
        SNPRINTF(buf, sizeof(buf), "D");
        break;

    case RIG_OP_CPY:
        SNPRINTF(buf, sizeof(buf), "A E B" EOM);
        break;

    case RIG_OP_TO_VFO:
        SNPRINTF(buf, sizeof(buf), "F" EOM);
        break;

    case RIG_OP_MCL:
        SNPRINTF(buf, sizeof(buf), "EC%03d" EOM, priv->curr_ch);
        break;

    case RIG_OP_FROM_VFO:
        if (rig->caps->rig_model == RIG_MODEL_DKR8)
        {
            SNPRINTF(buf, sizeof(buf), "PR" EOM "%02d" EOM, priv->curr_ch);
        }
        else
        {
            SNPRINTF(buf, sizeof(buf), "PR" EOM "%03d" EOM, priv->curr_ch);
        }
        break;

    default:
        return -RIG_EINVAL;
    }

    len = strlen(buf);
    retval = drake_transaction(rig, buf, len, buf[len - 1] == 0x0d ? ackbuf : NULL,
                                              buf[len - 1] == 0x0d ? &ack_len : NULL);

    //drake_trans_rept("drake_vfo_op", buf, len, buf[len - 1] == 0x0d ? ackbuf : NULL, 
    //                                           buf[len - 1] == 0x0d ? ack_len : 0, retval);

    return retval;
}


/*
 * drake_set_func
 * Assumes rig!=NULL
 */
int drake_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int retval;
    char blanker = ' ';

    switch (func)
    {
    case RIG_FUNC_MN:
        SNPRINTF(buf, sizeof(buf), "N%c" EOM, status ? 'O' : 'F');
        break;

    case RIG_FUNC_LOCK:
        SNPRINTF(buf, sizeof(buf), "L%c" EOM, status ? 'O' : 'F');
        break;

    case RIG_FUNC_NB:
    case RIG_FUNC_NB2:
        if (!status)
        {
            blanker = 'F';
        }
        else
        {
            if (func == RIG_FUNC_NB)
            {
                blanker = 'W';
            }
            else //if (func == RIG_FUNC_NB2)
            {
                blanker = 'N';
            }
        }
        SNPRINTF(buf, sizeof(buf), "B%c" EOM, blanker);
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //drake_trans_rept("drake_set_func", buf, strlen(buf), ackbuf, ack_len, retval);

    return retval;
}


/*
 * drake_get_func
 * Assumes rig!=NULL
 */
int drake_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int retval;
    const struct drake_priv_data *priv = STATE(rig)->priv;

    retval = drake_report_mode(rig, "drake_get_func");

    if (retval == RIG_OK)
    {
        switch (func)
        {
        case RIG_FUNC_MN: *status = priv->curr_notch; break;

        case RIG_FUNC_NB: *status = priv->curr_nb; break;
        
        case RIG_FUNC_NB2: *status = priv->curr_nb2; break;

        default: rig_debug(RIG_DEBUG_ERR, "Unsupported get func %s\n", rig_strfunc(func));
                 return -RIG_EINVAL;
        }
    }
    
    return retval;
}


/*
 * drake_set_level
 * Assumes rig!=NULL
 */
int drake_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int retval;

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

    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //drake_trans_rept("set_level", buf, strlen(buf), ackbuf, ack_len, retval);

    return retval;
}


/*
 * drake_get_level
 * Assumes rig!=NULL
 */
int drake_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval;
    const struct drake_priv_data *priv = STATE(rig)->priv;

    if ((level != RIG_LEVEL_RAWSTR) && (level != RIG_LEVEL_STRENGTH))
    {
        retval = drake_report_mode(rig, "drake_get_level");
    }
    else
    {
        if (rig->caps->rig_model == RIG_MODEL_DKR8)
        {
            retval = drake_report_all(rig, "drake_get_level");
        }
        else
        {
            retval = drake_report_signal(rig, "drake_get_level");
        }
    }

    if (retval == RIG_OK)
    {
        switch (level)
        {
        case RIG_LEVEL_PREAMP: val->i = (priv->curr_pre ? 10 : 0); break;

        case RIG_LEVEL_ATT: val->i = (priv->curr_att ? 10 : 0); break;

        case RIG_LEVEL_AGC: val->i = priv->curr_agc; break;

        case RIG_LEVEL_RAWSTR:
            if (rig->caps->rig_model == RIG_MODEL_DKR8)
            {
                val->i = ((priv->curr_dcd == RIG_DCD_ON) ? 1 : 0);
            }
            else
            {
                val->i =  priv->curr_ss;
            }
            break;

        case RIG_LEVEL_STRENGTH:
            if (rig->caps->rig_model == RIG_MODEL_DKR8)
            {
                val->i = ((priv->curr_dcd == RIG_DCD_ON) ? 0 : -60);
            }
            else
            {
                val->i = (int)rig_raw2val(priv->curr_ss, &rig->caps->str_cal);    
            }
            break;
            
        default: rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %s\n", rig_strlevel(level));
                 return -RIG_EINVAL;
        }
    }

    return retval;
}


int drake_set_powerstat(RIG *rig, powerstat_t status)
{
    char buf[16];
    char ackbuf[BUFSZ];
    int ack_len;
    int retval;

    SNPRINTF(buf, sizeof(buf), "P%c" EOM, status == RIG_POWER_OFF ? 'F' : 'O');

    retval = drake_transaction(rig, buf, strlen(buf), ackbuf, &ack_len);

    //drake_trans_rept("set_power", buf, strlen(buf), ackbuf, ack_len, retval);
    
    return retval;
}


int drake_get_powerstat(RIG *rig, powerstat_t *status)
{
    int retval;
    const struct drake_priv_data *priv = STATE(rig)->priv;

    retval = drake_report_mode(rig, "drake_get_powerstat");

    if (retval == RIG_OK)
    {
        *status = priv->curr_pwr;
    }
    
    return retval;
}


/*
 * drake_get_info
 * Assumes rig!=NULL
 */
const char *drake_get_info(RIG *rig)
{
    static char idbuf[BUFSZ];
    int id_len;
    int retval;

    retval = drake_transaction(rig, "ID" EOM, 3, idbuf, &id_len);

    //drake_trans_rept("get_id", "ID" EOM, 3, idbuf, id_len, retval);

    if (retval != RIG_OK)
    {
        return "";
    }

    idbuf[id_len - 2] = '\0';

    return idbuf;
}


/*
 * initrigs_drake is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(drake)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&r8_caps);
    rig_register(&r8a_caps);
    rig_register(&r8b_caps);

    return RIG_OK;
}

/*
 * probe_drake(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(drake)
{
    static char idbuf[BUFSZ];
    int retval, id_len;

    if (!port)
    {
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
    }

    port->parm.serial.rate = r8b_caps.serial_rate_max;
    port->write_delay = port->post_write_delay = 0;
    port->timeout = 50;
    port->retry = 1;

    retval = serial_open(port);

    if (retval != RIG_OK)
    {
        return RIG_MODEL_NONE;
    }

    retval = write_block(port, (unsigned char *) "ID" EOM, 3);
    id_len = read_string(port, (unsigned char *) idbuf, BUFSZ, LF, 1, 0, 1);

    close(port->fd);

    if (retval != RIG_OK || id_len <= 1 || id_len >= BUFSZ)
    {
        return RIG_MODEL_NONE;
    }

    idbuf[id_len - 2] = '\0'; //strip off <CR><LF>

    if (!strcmp(idbuf, "R8B"))
    {
        if (cfunc)
        {
            (*cfunc)(port, RIG_MODEL_DKR8B, data);
        }

        return RIG_MODEL_DKR8B;
    }

    if (!strcmp(idbuf, "R8A"))      // TBC
    {
        if (cfunc)
        {
            (*cfunc)(port, RIG_MODEL_DKR8A, data);
        }

        return RIG_MODEL_DKR8A;
    }

    if (!strcmp(idbuf, "R8"))
    {
        if (cfunc)
        {
            (*cfunc)(port, RIG_MODEL_DKR8, data);
        }

        return RIG_MODEL_DKR8;
    }

    // not found...
    if (memcmp(idbuf, "ID" EOM, 3)) // catch loopback serial
        rig_debug(RIG_DEBUG_VERBOSE, "probe_drake: found unknown device "
                  "with ID '%s', please report to Hamlib "
                  "developers.\n", idbuf);

    return RIG_MODEL_NONE;
}

