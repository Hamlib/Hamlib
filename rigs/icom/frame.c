/*
 *  Hamlib CI-V backend - low level communication routines
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

#include <hamlib/config.h>

#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"

/*
 * Build a CI-V frame.
 * The whole frame is placed in frame[],
 * "re_id" is the transceiver's CI-V address,
 * "cmd" is the Command number,
 * "subcmd" is the Sub command number, set to -1 if not present in frame,
 * if the frame has no data, then the "data" pointer must be NULL,
 *      and data_len==0.
 * "data_len" holds the Data area length pointed by the "data" pointer.
 * REM: if "data" is NULL, then "data_len" MUST be 0.
 *
 * NB: the frame array must be big enough to hold the frame.
 *      The smallest frame is 6 bytes, the biggest is at least 13 bytes.
 */
int make_cmd_frame(unsigned char frame[], unsigned char re_id,
                   unsigned char ctrl_id,
                   unsigned char cmd, int subcmd,
                   const unsigned char *data, int data_len)
{
    int i = 0;

#if 0
    frame[i++] = PAD;   /* give old rigs a chance to flush their rx buffers */
#endif
    frame[i++] = PR;    /* Preamble code */
    frame[i++] = PR;
    frame[i++] = re_id;
    frame[i++] = ctrl_id;
    frame[i++] = cmd;

    if (subcmd != -1)
    {
#ifdef MULTIB_SUBCMD
        register int j;

        if ((j = subcmd & 0xff0000))    /* allows multi-byte subcmd for dsp rigs */
        {
            frame[i++] = j >> 16;
            frame[i++] = (subcmd & 0xff00) >> 8;
        }
        else if ((j = subcmd & 0xff00)) { frame[i++] = j >> 8; }

#endif
        frame[i++] = subcmd & 0xff;
    }

    if (data_len != 0)
    {
        memcpy(frame + i, data, data_len);
        i += data_len;
    }

    frame[i++] = FI;        /* EOM code */

    return (i);
}

int icom_frame_fix_preamble(int frame_len, unsigned char *frame)
{
    if (frame[0] == PR)
    {
        // Sometimes the second preamble byte is missing -> TODO: Find out why!
        if (frame[1] != PR)
        {
            memmove(frame + 1, frame, frame_len);
            frame_len++;
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_WARN, "%s: invalid Icom CI-V frame, no preamble found\n",
                  __func__);
        return (-RIG_EPROTO);
    }

    return frame_len;
}

/*
 * icom_one_transaction
 *
 * We assume that rig!=NULL, rig->state!= NULL, payload!=NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * payload can be NULL if payload_len == 0
 * subcmd can be equal to -1 (no subcmd wanted)
 * if no answer is to be expected, data_len must be set to NULL to tell so
 *
 * return RIG_OK if transaction completed,
 * or a negative value otherwise indicating the error.
 */
int icom_one_transaction(RIG *rig, unsigned char cmd, int subcmd,
                         const unsigned char *payload, int payload_len, unsigned char *data,
                         int *data_len)
{
    struct icom_priv_data *priv;
    const struct icom_priv_caps *priv_caps;
    struct rig_state *rs;
    struct timeval start_time, current_time, elapsed_time;
    // this buf needs to be large enough for 0xfe strings for power up
    // at 115,200 this is now at least 150
    unsigned char buf[200];
    unsigned char sendbuf[MAXFRAMELEN];
    int frm_len, frm_data_len, retval;
    unsigned char ctrl_id;

    ENTERFUNC;
    memset(buf, 0, 200);
    memset(sendbuf, 0, MAXFRAMELEN);
    rs = &rig->state;
    priv = (struct icom_priv_data *)rs->priv;
    priv_caps = (struct icom_priv_caps *)rig->caps->priv;

    ctrl_id = priv_caps->serial_full_duplex == 0 ? CTRLID : 0x80;

    frm_len = make_cmd_frame(sendbuf, priv->re_civ_addr, ctrl_id, cmd,
                             subcmd, payload, payload_len);

    /*
     * should check return code and that write wrote cmd_len chars!
     */
    set_transaction_active(rig);

    rig_flush(&rs->rigport);

    if (data_len) { *data_len = 0; }

    retval = write_block(&rs->rigport, sendbuf, frm_len);

    if (retval != RIG_OK)
    {
        set_transaction_inactive(rig);
        RETURNFUNC(retval);
    }

    if (!priv_caps->serial_full_duplex && !priv->serial_USB_echo_off)
    {

        /*
         * read what we just sent, because TX and RX are looped,
         * and discard it...
         * - if what we read is not what we sent, then it means
         *          a collision on the CI-V bus occurred!
         *      - if we get a timeout, then retry to send the frame,
         *          up to rs->retry times.
         */

        retval = read_icom_frame(&rs->rigport, buf, sizeof(buf));

        if (retval == -RIG_ETIMEOUT || retval == 0)
        {
            /* Nothing received, CI-V interface is not echoing */
            set_transaction_inactive(rig);
            RETURNFUNC(-RIG_BUSERROR);
        }

        if (retval < 0)
        {
            set_transaction_inactive(rig);
            /* Other error, return it */
            RETURNFUNC(retval);
        }

        if (retval < 1)
        {
            set_transaction_inactive(rig);
            RETURNFUNC(-RIG_EPROTO);
        }

        // we might have 0xfe string during rig wakeup
        rig_debug(RIG_DEBUG_TRACE, "%s: DEBUG retval=%d, frm_len=%d, cmd=0x%02x\n",
                  __func__, retval, frm_len, cmd);

        if (retval != frm_len && cmd == C_SET_PWR)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: removing 0xfe power up echo, len=%d", __func__,
                      frm_len);

            while (buf[2] == 0xfe)
            {
                memmove(buf, &buf[1], frm_len--);
            }

            dump_hex(buf, frm_len);
        }

        switch (buf[retval - 1])
        {
        case COL:
            /* Collision */
            set_transaction_inactive(rig);
            RETURNFUNC(-RIG_BUSBUSY);

        case FI:
            /* Ok, normal frame */
            break;

        default:
            /* Timeout after reading at least one character */
            /* Problem on ci-v bus? */
            set_transaction_inactive(rig);
            RETURNFUNC(-RIG_BUSERROR);
        }

        if (retval != frm_len)
        {
            /* Not the same length??? */
            /* Problem on ci-v bus? */
            /* Someone else got a packet in? */
            set_transaction_inactive(rig);
            RETURNFUNC(-RIG_EPROTO);
        }

        if (memcmp(buf, sendbuf, frm_len) != 0)
        {
            /* Frames are different? */
            /* Problem on ci-v bus? */
            /* Someone else got a packet in? */
            set_transaction_inactive(rig);
            RETURNFUNC(-RIG_EPROTO);
        }
    }

    /*
     * expect an answer?
     */
    if (data_len == NULL)
    {
        set_transaction_inactive(rig);
        RETURNFUNC(RIG_OK);
    }

    gettimeofday(&start_time, NULL);

read_another_frame:
    /*
     * wait for ACK ...
     * FIXME: handle padding/collisions
     * ACKFRMLEN is the smallest frame we can expect from the rig
     */
    buf[0] = 0;
    frm_len = read_icom_frame(&rs->rigport, buf, sizeof(buf));

#if 0

    // this was causing rigctld to fail on IC706 and WSJT-X
    // This dynamic detection is therefore disabled for now
    if (memcmp(buf, sendbuf, frm_len) == 0 && priv->serial_USB_echo_off)
    {
        // Hmmm -- got an echo back when not expected so let's change
        priv->serial_USB_echo_off = 0;
        // And try again
        frm_len = read_icom_frame(&rs->rigport, buf, sizeof(buf));
    }

#endif

    if (frm_len < 0)
    {
        set_transaction_inactive(rig);
        /* RIG_TIMEOUT: timeout getting response, return timeout */
        /* other error: return it */
        RETURNFUNC(frm_len);
    }

    if (frm_len < 1)
    {
        set_transaction_inactive(rig);
        RETURNFUNC(-RIG_EPROTO);
    }

    retval = icom_frame_fix_preamble(frm_len, buf);

    if (retval < 0)
    {
        set_transaction_inactive(rig);
        RETURNFUNC(retval);
    }

    frm_len = retval;

    if (frm_len < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "Unexpected frame len=%d\n", frm_len);
        RETURNFUNC(-RIG_EPROTO);
    }

    switch (buf[frm_len - 1])
    {
    case COL:
        set_transaction_inactive(rig);
        /* Collision */
        RETURNFUNC(-RIG_BUSBUSY);

    case FI:
        /* Ok, normal frame */
        break;

    case NAK:
        set_transaction_inactive(rig);
        RETURNFUNC(-RIG_ERJCTED);

    default:
        set_transaction_inactive(rig);
        /* Timeout after reading at least one character */
        /* Problem on ci-v bus? */
        RETURNFUNC(-RIG_EPROTO);
    }

    if (frm_len < ACKFRMLEN)
    {
        set_transaction_inactive(rig);
        RETURNFUNC(-RIG_EPROTO);
    }

    // if we send a bad command we will get back a NAK packet
    // e.g. fe fe e0 50 fa fd
    if (frm_len == 6 && NAK == buf[frm_len - 2])
    {
        set_transaction_inactive(rig);
        RETURNFUNC(-RIG_ERJCTED);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: frm_len=%d, frm_len-1=%02x, frm_len-2=%02x\n",
              __func__, frm_len, buf[frm_len - 1], buf[frm_len - 2]);

    // has to be one of these two now or frame is corrupt
    if (FI != buf[frm_len - 1] && ACK != buf[frm_len - 1])
    {
        set_transaction_inactive(rig);
        RETURNFUNC(-RIG_BUSBUSY);
    }

    frm_data_len = frm_len - (ACKFRMLEN - 1);

    if (frm_data_len <= 0)
    {
        set_transaction_inactive(rig);
        RETURNFUNC(-RIG_EPROTO);
    }

    // TODO: Does ctrlid (detected by icom_is_async_frame) vary (seeing some code above using 0x80 for non-full-duplex)?
    if (icom_is_async_frame(rig, frm_len, buf))
    {
        int elapsed_ms;
        icom_process_async_frame(rig, frm_len, buf);

        gettimeofday(&current_time, NULL);
        timersub(&current_time, &start_time, &elapsed_time);

        elapsed_ms = (int)(elapsed_time.tv_sec * 1000 + elapsed_time.tv_usec / 1000);

        if (elapsed_ms > rs->rigport.timeout)
        {
            set_transaction_inactive(rig);
            RETURNFUNC(-RIG_ETIMEOUT);
        }

        goto read_another_frame;
    }

    set_transaction_inactive(rig);

    *data_len = frm_data_len;

    if (data != NULL && data_len != NULL) { memcpy(data, buf + 4, *data_len); }

    /*
     * TODO: check addresses in reply frame
     */

    RETURNFUNC(RIG_OK);
}

/*
 * icom_transaction
 *
 * This function honors rigport.retry count.
 *
 * We assume that rig!=NULL, rig->state!= NULL, payload!=NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * payload can be NULL if payload_len == 0
 * subcmd can be equal to -1 (no subcmd wanted)
 *
 * return RIG_OK if transaction completed,
 * or a negative value otherwise indicating the error.
 */
int icom_transaction(RIG *rig, int cmd, int subcmd,
                     const unsigned char *payload, int payload_len, unsigned char *data,
                     int *data_len)
{
    int retval, retry;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: cmd=0x%02x, subcmd=0x%02x, payload_len=%d\n", __func__,
              cmd, subcmd, payload_len);

    retry = rig->state.rigport.retry;

    do
    {
        retval = icom_one_transaction(rig, cmd, subcmd, payload, payload_len, data,
                                      data_len);

        if (retval == RIG_OK || retval == -RIG_ERJCTED)
        {
            break;
        }

        rig_debug(RIG_DEBUG_WARN, "%s: retry=%d: %s\n", __func__, retry,
                  rigerror(retval));

        // On some serial errors we may need a bit of time
        hl_usleep(100 * 1000); // pause just a bit
    }
    while (retry-- > 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: failed: %s\n", __func__, rigerror(retval));
    }

    RETURNFUNC(retval);
}

/* used in read_icom_frame as end of block */
static const char icom_block_end[2] = { FI, COL};
#define icom_block_end_length 2

/*
 * Read a whole CI-V frame (until 0xfd is encountered).
 *
 * TODO: strips padding/collisions
 * FIXME: check return codes/bytes read
 */
static int read_icom_frame_generic(hamlib_port_t *p,
                                   const unsigned char rxbuffer[],
                                   size_t rxbuffer_len, int direct)
{
    int read = 0;
    int retries = 10;
    unsigned char *rx_ptr = (unsigned char *) rxbuffer;

    // zeroize the buffer so we can still check contents after timeouts
    memset(rx_ptr, 0, rxbuffer_len);

    /*
     * OK, now sometimes we may time out, e.g. the IC7000 can time out
     * during a PTT operation. So, we will ensure that the last thing we
     * read was a proper end marker - if not, we will try again.
     */
    do
    {
        int i;

        if (direct)
        {
            i = read_string_direct(p, rx_ptr, MAXFRAMELEN - read,
                                   icom_block_end, icom_block_end_length, 0, 1);
        }
        else
        {
            i = read_string(p, rx_ptr, MAXFRAMELEN - read,
                            icom_block_end, icom_block_end_length, 0, 1);
        }

        if (i < 0 && i != RIG_BUSBUSY) /* die on errors */
        {
            return (i);
        }

        if (i == 0) /* nothing read?*/
        {
            if (--retries <= 0) /* Tried enough times? */
            {
                return (read);
            }
        }

        /* OK, we got something. add it in and continue */
        if (i > 0)
        {
            read   += i;
            rx_ptr += i;
        }
    }
    while ((read < rxbuffer_len) && (rxbuffer[read - 1] != FI)
            && (rxbuffer[read - 1] != COL));

    return (read);
}

int read_icom_frame(hamlib_port_t *p, const unsigned char rxbuffer[],
                    size_t rxbuffer_len)
{
    return read_icom_frame_generic(p, rxbuffer, rxbuffer_len, 0);
}

int read_icom_frame_direct(hamlib_port_t *p, const unsigned char rxbuffer[],
                           size_t rxbuffer_len)
{
    return read_icom_frame_generic(p, rxbuffer, rxbuffer_len, 1);
}

/*
 * convert mode and width as expressed by Hamlib frontend
 * to mode and passband data understandable by a CI-V rig
 *
 * if pd == -1, no passband data is to be sent
 *
 * return RIG_OK if everything's fine, negative value otherwise
 *
 * TODO: be more exhaustive
 * assumes rig!=NULL
 */
int rig2icom_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                  unsigned char *md, signed char *pd)
{
    unsigned char icmode;
    signed char icmode_ext;
    pbwidth_t width_tmp = width;
    struct icom_priv_data *priv_data = (struct icom_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: mode=%d, width=%d\n", __func__, (int)mode,
              (int)width);
    icmode_ext = -1;

    if (width == RIG_PASSBAND_NOCHANGE) // then we read width so we can reuse it
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: width==RIG_PASSBAND_NOCHANGE\n", __func__);
        rmode_t tmode;
        int ret = rig_get_mode(rig, vfo, &tmode, &width);

        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: Failed to get width for passband nochange err=%s\n", __func__,
                      rigerror(ret));
        }
    }

    switch (mode)
    {
    case RIG_MODE_AM:   icmode = S_AM; break;

    case RIG_MODE_PKTAM:   icmode = S_AM; break;

    case RIG_MODE_AMN:  icmode = S_AMN; break;

    case RIG_MODE_AMS:  icmode = S_AMS; break;

    case RIG_MODE_CW:   icmode = S_CW; break;

    case RIG_MODE_CWR:  icmode = S_CWR; break;

    case RIG_MODE_USB:  icmode = S_USB; break;

    case RIG_MODE_PKTUSB:
        icmode = S_USB;

        if (rig->caps->rig_model == RIG_MODEL_IC7800)
        {
            icmode = S_PSK;
        }

        break;

    case RIG_MODE_LSB:  icmode = S_LSB; break;

    case RIG_MODE_PKTLSB:
        icmode = S_LSB;

        if (rig->caps->rig_model == RIG_MODEL_IC7800)
        {
            icmode = S_PSKR;
        }

        break;

    case RIG_MODE_RTTY: icmode = S_RTTY; break;

    case RIG_MODE_RTTYR:    icmode = S_RTTYR; break;

    case RIG_MODE_PSK:  icmode = S_PSK; break;

    case RIG_MODE_PSKR: icmode = S_PSKR; break;

    case RIG_MODE_FM:   icmode = S_FM; break;

    case RIG_MODE_PKTFM: icmode = S_FM; break;

    case RIG_MODE_FMN:  icmode = S_FMN; break;

    case RIG_MODE_PKTFMN:  icmode = S_FMN; break;

    case RIG_MODE_WFM:  icmode = S_WFM; break;

    case RIG_MODE_P25:  icmode = S_P25; break;

    case RIG_MODE_DSTAR:    icmode = S_DSTAR; break;

    case RIG_MODE_DPMR: icmode = S_DPMR; break;

    case RIG_MODE_NXDNVN:   icmode = S_NXDNVN; break;

    case RIG_MODE_NXDN_N:   icmode = S_NXDN_N; break;

    case RIG_MODE_DCR:  icmode = S_DCR; break;

    case RIG_MODE_DD:   icmode = S_DD; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Hamlib mode %s\n", __func__,
                  rig_strrmode(mode));
        RETURNFUNC(-RIG_EINVAL);
    }

    if (width_tmp != RIG_PASSBAND_NOCHANGE)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: width_tmp=%ld\n", __func__, width_tmp);
        pbwidth_t medium_width = rig_passband_normal(rig, mode);

        if (width == RIG_PASSBAND_NORMAL)
        {
            // Use rig default for "normal" passband
            icmode_ext = -1;
        }
        else if (width < medium_width)
        {
            icmode_ext = PD_NARROW_3;
        }
        else if (width == medium_width)
        {
            icmode_ext = PD_MEDIUM_3;
        }
        else
        {
            icmode_ext = PD_WIDE_3;
        }

        if (rig->caps->rig_model == RIG_MODEL_ICR7000)
        {
            if (mode == RIG_MODE_USB || mode == RIG_MODE_LSB)
            {
                icmode = S_R7000_SSB;
                icmode_ext = 0x00;
            }
            else if (mode == RIG_MODE_AM && icmode_ext == -1)
            {
                icmode_ext = PD_WIDE_3; /* default to Wide */
            }
        }

        *pd = icmode_ext;
    }
    else
    {
        // filter should already be set elsewhere
        *pd = priv_data->filter;
    }

    *md = icmode;
    RETURNFUNC(RIG_OK);
}

/*
 * assumes rig!=NULL, mode!=NULL, width!=NULL
 */
void icom2rig_mode(RIG *rig, unsigned char md, int pd, rmode_t *mode,
                   pbwidth_t *width)
{
    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: mode=0x%02x, pd=%d\n", __func__, md, pd);
    *width = RIG_PASSBAND_NORMAL;

    switch (md)
    {
    case S_AM:  if (rig->caps->rig_model == RIG_MODEL_ICR30 && pd == 0x02)
        {
            *mode = RIG_MODE_AMN;
        }
        else
        {
            *mode = RIG_MODE_AM;
        }  break;

    case S_AMS: *mode = RIG_MODE_AMS; break;

    case S_CW:  *mode = RIG_MODE_CW; break;

    case S_CWR: *mode = RIG_MODE_CWR; break;

    case S_FM:  if (rig->caps->rig_model == RIG_MODEL_ICR7000
                        && pd == 0x00)
        {
            *mode = RIG_MODE_USB;
            *width = rig_passband_normal(rig, RIG_MODE_USB);
            return;
        }
        else if (rig->caps->rig_model == RIG_MODEL_ICR30 && pd == 0x02)
        {
            *mode = RIG_MODE_FMN;
        }
        else
        {
            *mode = RIG_MODE_FM;
        } break;

    case S_WFM:     *mode = RIG_MODE_WFM; break;

    case S_USB:     *mode = RIG_MODE_USB; break;

    case S_LSB:     *mode = RIG_MODE_LSB; break;

    case S_RTTY:    *mode = RIG_MODE_RTTY; break;

    case S_RTTYR:   *mode = RIG_MODE_RTTYR; break;

    case S_PSK:
        *mode = RIG_MODE_PSK;

        if (rig->caps->rig_model == RIG_MODEL_IC7800)
        {
            *mode = RIG_MODE_PKTUSB;
        }

        break;

    case S_PSKR:
        *mode = RIG_MODE_PSKR;

        if (rig->caps->rig_model == RIG_MODEL_IC7800)
        {
            *mode = RIG_MODE_PKTLSB;
        }

        break;

    case S_DSTAR:   *mode = RIG_MODE_DSTAR; break;

    case S_P25:     *mode = RIG_MODE_P25; break;

    case S_DPMR:    *mode = RIG_MODE_DPMR; break;

    case S_NXDNVN:  *mode = RIG_MODE_NXDNVN; break;

    case S_NXDN_N:  *mode = RIG_MODE_NXDN_N; break;

    case S_DCR: *mode = RIG_MODE_DCR; break;

    case 0xff:  *mode = RIG_MODE_NONE; break;   /* blank mem channel */

    default:
        rig_debug(RIG_DEBUG_ERR, "icom: Unsupported Icom mode %#.2x\n",
                  md);
        *mode = RIG_MODE_NONE;
    }

    /* Most rigs return 1-wide, 2-narrow; or if it has 3 filters: 1-wide, 2-middle,
           3-narrow. (Except for the 706 mkIIg 0-wide, 1-middle, 2-narrow.)  For DSP
           rigs these are presets, which can be programmed for 30 - 41 bandwidths,
           depending on mode  */

    if (pd >= 0 && (rig->caps->rig_model == RIG_MODEL_IC706MKIIG ||
                    rig->caps->rig_model == RIG_MODEL_IC706 ||
                    rig->caps->rig_model ==  RIG_MODEL_IC706MKII)) { pd++; }

    switch (pd)
    {
    case 0x01:

        /* if no wide filter defined it's the default */
        if (!(*width = rig_passband_wide(rig, *mode)))
        {
            *width = rig_passband_normal(rig, *mode);
        }

        break;

    case 0x02:
        if ((*width = rig_passband_wide(rig, *mode)))
        {
            *width = rig_passband_normal(rig, *mode);
        }
        else
            /* This really just depends on how you program the table. */
        {
            *width = rig_passband_narrow(rig, *mode);
        }

        break;

    case 0x03:
        *width = rig_passband_narrow(rig, *mode);
        break;

    case -1:
        break;        /* no passband data */

    default:
        rig_debug(RIG_DEBUG_ERR, "icom: Unsupported Icom mode width %#.2x\n", pd);
    }

}

