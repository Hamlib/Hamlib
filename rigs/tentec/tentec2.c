/*
 *  Hamlib Tentec backend - Argonaut, Jupiter, RX-350
 *  Copyright (c) 2001-2004 by Stephane Fillod
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
 *  Module rewritten and tested by Dave Freese, W1HKJ
 *  Tested using the distributed test program "rigctl" to control an Argonaut V xcvr.
 *  Linked to digital modem program, "gmfsk" and bench tested and used on-air
 *  Linked to experimental digital modem program "fldigi", bench tested
 *  and used on-air.
 *
 *  Note for anyone wishing to expand on the command set.
 *  Recommend using the
 *
 *      tentec_transaction (rig, sendbuf, sendlen, rcvbuf, &rcv_len)
 *
 *  function to send the command and receive the response.
 *
 *  The Argo V always sends a response and ends the response with a "G\r" to
 *  indicate that the command was accepted.  A rejected command is responded to by a
 *  two character sequence "Z\r".  You should always expect a maximum response equal
 *  to the number of data bytes plus two.
 *
 *  For example:
 *      A request for the present receiver filter bandwidth is the the string:
 *          "?W\r" which is 3 bytes in length
 *      The response from the Argonaut V will be:
 *          "Wn\rG\r" which is 5 bytes in length, where n is an unsigned char (byte)
 *      If the transceiver failed to receive the command correctly it will respond:
 *          "Z\r" ----> you need to check for that condition
 *
 *  The tentec_transaction(...) function will always terminate the rcvbuf with a null
 *  character.  The pointer to the receive buffer length MUST be initialized to the
 *  length of the max # chars for that command PLUS 1 for the terminator.
 *  For the above command, rcv_len should be 6.
*/

#include <hamlib/config.h>

#include <stdio.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"

#include "tentec.h"
#include "tentec2.h"

#define TT_AM  '0'
#define TT_USB '1'
#define TT_LSB '2'
#define TT_CW  '3'
#define TT_FM  '4'

/*************************************************************************************
 *
 * Specs from http://www.rfsquared.com
 *
 * TODO: [sg]et_split
 *  [sg]et_level: ATT, NB, PBT, KEYER_SPD, RFPOWER, SWR, SQL, STRENGTH, ..
 *  vfo_op: TO_VFO, FROM_VFO + emulated set_mem/get_mem
 */


/*
 * tentec_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */
int tentec2_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval, ret_len;
    char freqbuf[16] = "*Axxxx\r";
    unsigned long f = (unsigned long)freq;

    if (vfo == RIG_VFO_CURR)
    {
        if ((retval = tentec2_get_vfo(rig, &vfo)) != RIG_OK)
        {
            return retval;
        }
    }

    switch (vfo)
    {
    case RIG_VFO_A: break;

    case RIG_VFO_B: freqbuf[1] = 'B'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    freqbuf[2] = (f >> 24) & 0xFF;
    freqbuf[3] = (f >> 16) & 0xFF;
    freqbuf[4] = (f >> 8) & 0xFF;
    freqbuf[5] = f & 0xFF;
// Argo V will respond
// "G\r" or "Z\r"
    ret_len = 3;
    retval = tentec_transaction(rig, freqbuf, 7, freqbuf, &ret_len);

    if (retval != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    if (ret_len != 2 || freqbuf[0] != 'G')
    {
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * tentec2_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tentec2_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval, ret_len;
    char freqbuf[16] = "?A\r";

    if (vfo == RIG_VFO_CURR)
    {
        if ((retval = tentec2_get_vfo(rig, &vfo)) != RIG_OK)
        {
            return retval;
        }
    }

    switch (vfo)
    {
    case RIG_VFO_A: break;

    case RIG_VFO_B: freqbuf[1] = 'B'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

// Argo V will respond with 8 characters
// "Annnn\rG\r" or "Bnnnn\rG\r"
// or it will respond
// "Z\r" meaning the command was rejected
    ret_len = 9;

    retval = tentec_transaction(rig, freqbuf, 3, freqbuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len == 2 && freqbuf[0] == 'Z')
    {
        return -RIG_ERJCTED;
    }

    if (ret_len != 8)
    {
        return -RIG_EINVAL;
    }

    *freq = (unsigned int)((freqbuf[1] & 0x0FF) << 24) +
            (unsigned int)((freqbuf[2] & 0x0FF) << 16) +
            (unsigned int)((freqbuf[3] & 0x0FF) << 8) +
            (unsigned int)(freqbuf[4] & 0x0FF);

    return RIG_OK;
}

/*
 * tentec2_set_vfo
 * Assumes rig!=NULL
 */
int tentec2_set_vfo(RIG *rig, vfo_t vfo)
{
    int retval, ret_len;
    char vfobuf[16] = "*EVA\r";

    if ((vfo & ~RIG_VFO_MEM) == RIG_VFO_NONE || vfo == RIG_VFO_VFO)
    {
        vfo_t cvfo;
        retval = tentec2_get_vfo(rig, &cvfo);

        if (retval != RIG_OK)
        {
            return retval;
        }

        vfo = (cvfo & (RIG_VFO_A | RIG_VFO_B)) | (vfo & RIG_VFO_MEM);
    }

    if (vfo & RIG_VFO_MEM) { vfobuf[2] = 'M'; }

    switch (vfo & ~RIG_VFO_MEM)
    {
    case RIG_VFO_A: break;

    case RIG_VFO_B: vfobuf[3] = 'B'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    ret_len = 3;

    retval = tentec_transaction(rig, vfobuf, 5, vfobuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len != 2 || vfobuf[0] != 'G')
    {
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * tentec2_get_vfo
 * Assumes rig!=NULL
 */
int tentec2_get_vfo(RIG *rig, vfo_t *vfo)
{
    int ret_len, retval;
    unsigned char vfobuf[16] = "?E\r";

    ret_len = 7;
    retval = tentec_transaction(rig, (char *) vfobuf, 3, (char *) vfobuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

// ArgoV sends back 6 character string
// "EVA\rG\r" or "EVB\rG\r"
// or 2 character failure string
// "Z\r"

    if (ret_len == 2 && vfobuf[0] == 'Z')
    {
        return -RIG_ERJCTED;
    }

    if (ret_len != 6)
    {
        return -RIG_EPROTO;
    }

    *vfo = vfobuf[2] == 'A' ? RIG_VFO_A : RIG_VFO_B;

    if (vfobuf[1] == 'M')
    {
        *vfo |= RIG_VFO_MEM;
    }

    return RIG_OK;
}



/*
 * tentec2_set_split_vfo
 * Assumes rig!=NULL
 */
int tentec2_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int retval, ret_len;
    char retbuf[10] = "*Ox\r";

    if (split == RIG_SPLIT_ON)
    {
        retbuf[2] = 1;
    }
    else
    {
        retbuf[2] = 0;
    }

    ret_len = 3;
    retval = tentec_transaction(rig, retbuf, 4, retbuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len != 2 || retbuf[0] != 'G')
    {
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * tentec2_get_split_vfo
 * Assumes rig!=NULL
 */
int tentec2_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int ret_len, retval;
    char splitbuf[16] = "?O\r";

    /*
     * TODO: handle tx_vfo
     */
    ret_len = 6;
    retval = tentec_transaction(rig, splitbuf, 3, splitbuf, &ret_len);
// Argo V returns
// "On\rG\r" or
// "Z\r"

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len == 2 && splitbuf[0] == 'Z')
    {
        return -RIG_ERJCTED;
    }

    if (ret_len != 5)
    {
        return -RIG_EPROTO;
    }

    *split = splitbuf[1] == 0 ? RIG_SPLIT_OFF : RIG_SPLIT_ON;

    return RIG_OK;
}



/*
 * tentec2_set_mode
 * Assumes rig!=NULL
 */
int tentec2_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char ttmode;
    int ttfilter, retval, ret_len;
    char mdbuf[16];

    if (vfo == RIG_VFO_CURR)
    {
        if ((retval = tentec2_get_vfo(rig, &vfo)) != RIG_OK)
        {
            return retval;
        }
    }

    switch (mode)
    {
    case RIG_MODE_USB:      ttmode = TT_USB; break;

    case RIG_MODE_LSB:      ttmode = TT_LSB; break;

    case RIG_MODE_CW:       ttmode = TT_CW; break;

    case RIG_MODE_AM:       ttmode = TT_AM; break;

    case RIG_MODE_FM:       ttmode = TT_FM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    /* get the mode because we want to leave other VFO unchanged */
    ret_len = 7;
    retval = tentec_transaction(rig, "?M\r", 3, &mdbuf[1], &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len != 6)
    {
        return -RIG_EPROTO;
    }

    mdbuf[0] = '*';

    switch (vfo)
    {
    case RIG_VFO_A: mdbuf[2] = ttmode; break;

    case RIG_VFO_B: mdbuf[3] = ttmode; break;

    default:
        return -RIG_EINVAL;
    }

    ret_len = 3;
    retval = tentec_transaction(rig, mdbuf, 5, mdbuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len != 2 || mdbuf[0] != 'G')
    {
        return -RIG_ERJCTED;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return retval; }

    if (RIG_PASSBAND_NORMAL == width)
    {
        width = rig_passband_normal(rig, mode);
    }

    /*
     * Filter  0:  200
     *              ..
     * Filter 16: 1000
     *              ..
     * Filter 36: 3000
     */
    if (width < 200)
    {
        ttfilter = 0;
    }
    else if (width > 3000)
    {
        ttfilter = 36;
    }
    else if (width < 1000)
    {
        ttfilter = (width / 50) - 4;
    }
    else
    {
        ttfilter = (width / 100) + 6;
    }

    strcpy(mdbuf, "*Wn\r");
    mdbuf[2] = ttfilter;
    ret_len = 3;
    retval = tentec_transaction(rig, mdbuf, 4, mdbuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len != 2 || mdbuf[0] != 'G')
    {
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}



/*
 * tentec2_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tentec2_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int ttfilter, retval, ret_len;
    char mdbuf[16];

    if (vfo == RIG_VFO_CURR)
    {
        if ((retval = tentec2_get_vfo(rig, &vfo)) != RIG_OK)
        {
            return retval;
        }
    }

// response to "?M\r" command:
// "M00" -> AM, "M1" -> USB, "M2" -> LSB, "M3" -> CW, "M4" -> FM
    ret_len = 7;
    retval = tentec_transaction(rig, "?M\r", 3, mdbuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len != 6)
    {
        return -RIG_EPROTO;
    }

    if (vfo != RIG_VFO_A && vfo != RIG_VFO_B)
    {
        return -RIG_EINVAL;
    }

    switch (mdbuf[vfo == RIG_VFO_A ? 1 : 2])
    {
    case TT_USB:    *mode = RIG_MODE_USB; break;

    case TT_LSB:    *mode = RIG_MODE_LSB; break;

    case TT_CW:     *mode = RIG_MODE_CW;  break;

    case TT_AM:     *mode = RIG_MODE_AM;  break;

    case TT_FM:     *mode = RIG_MODE_FM;  break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, mdbuf[vfo == RIG_VFO_A ? 1 : 2]);
        return -RIG_EPROTO;
    }

    ret_len = 6;
    retval = tentec_transaction(rig, "?W\r", 3, mdbuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len == 2 && mdbuf[0] == 'Z')
    {
        return -RIG_ERJCTED;
    }

    if (ret_len != 5)
    {
        return -RIG_EPROTO;
    }

    /*
     * Filter  0:  200
     *              ..
     * Filter 16: 1000
     *              ..
     * Filter 36: 3000
     */
    ttfilter = mdbuf[1];

    if (ttfilter < 0 || ttfilter > 36)
    {
        return -RIG_EPROTO;
    }

    if (ttfilter < 16)
    {
        *width = (ttfilter + 4) * 50;
    }
    else
    {
        *width = (ttfilter - 6) * 100;
    }

    return RIG_OK;
}


/*
 * tentec2_set_ptt
 * Assumes rig!=NULL
 */
int tentec2_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval, ret_len;
    char retbuf[10];

    ret_len = 3;
    retval = tentec_transaction(rig,
                                ptt == RIG_PTT_ON ? "#1\r" : "#0\r", 3,
                                retbuf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len != 2 || retbuf[0] != 'G')
    {
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}


/*
 * tentec2_get_ptt
 * Assumes rig!=NULL
 */
int tentec2_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int ret_len, retval;
    char buf[7] = "?C\r";

    ret_len = 7;
    retval = tentec_transaction(rig, buf, 3, buf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

// ArgoV sends back 6 character string
// "Cnn\rG\r" where nn is the status word
// or 2 character failure string
// "Z\r"

    if (ret_len == 2 && buf[0] == 'Z')
    {
        return -RIG_ERJCTED;
    }

    if (ret_len != 6)
    {
        return -RIG_EPROTO;
    }

    *ptt = (buf[2] & 0x01) ? RIG_PTT_ON : RIG_PTT_OFF;

    return RIG_OK;
}


/*
 * Software restart
 */
int tentec2_reset(RIG *rig, reset_t reset)
{
    int retval, reset_len;
    char reset_buf[32];

    reset_len = 32;
    retval = tentec_transaction(rig, "*X\r", 3, reset_buf, &reset_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!strstr(reset_buf, "RADIO START"))
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * tentec2_get_info
 * Assumes rig!=NULL
 */
const char *tentec2_get_info(RIG *rig)
{
    static char buf[100];   /* FIXME: reentrancy */
    int firmware_len, retval;

    /*
     * protocol version
     */
    buf[0] = 0;
    firmware_len = 100;
    retval = tentec_transaction(rig, "?V\r", 3, buf, &firmware_len);

    /* "VER 1010-516" */
    if (retval != RIG_OK || firmware_len != 12)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG, len=%d\n",
                  __func__, firmware_len);
        return NULL;
    }

    return buf;
}
