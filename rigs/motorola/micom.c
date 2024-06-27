/*
 *  Hamlib Motorola Micom backend - main file
 *  Copyright (c) 2024 Michael Black W9MDB
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

#include <hamlib/rig.h>
#include <misc.h>
#include <iofunc.h>

// char* to start of checksum for len bytes
unsigned int checksum(unsigned char *buf, int len)
{
    int checksum = 0;
    int i;

    // simple 1-byte checksum
    for (i = 0; i < len; ++i) { checksum += buf[i]; }

    return checksum & 0xff;
}

static int micom_open(RIG *rig)
{
    ENTERFUNC;
    RETURNFUNC(RIG_OK);
}

// returns bytes read
// format has length in byte[1] plus 5 bytes 0x24/len/cmd at start and checksum+0x03 at end
// So a data "length" of 5 is 10 bytes for example 
static int micom_read_frame(RIG *rig, unsigned char *buf, int maxlen)
{
    hamlib_port_t *rp = RIGPORT(rig);
    int bytes;
    //const char stopset[1] = {0x03};
    ENTERFUNC;
    bytes = read_block(rp, buf, 3);
    if (bytes+buf[1]+2 > maxlen)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: buffer overrun...expected max of %d, got %d\n",
            __func__, maxlen, bytes+buf[1]+2);
        dump_hex(buf,bytes);
        RETURNFUNC(-RIG_EPROTO);
    }
    bytes += read_block(rp, &buf[3], buf[1]+2);
    dump_hex(buf,bytes);
    RETURNFUNC(bytes);
}

/* Example of set of commands that works
24 06 18 05 01 00 38 ea 50 ba 03 15
24 05 18 36 fe 7b ef 01 e0 03 15
24 05 18 36 ff 7b ef 01 e1 03 15
24 05 18 36 df 7b ef 01 c1 03 15
24 05 18 36 ff 7b ef 01 e1 03
*/
static int micom_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    hamlib_port_t *rp = RIGPORT(rig);
    unsigned char rxcmd[12] = { 0x24, 0x06, 0x18, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x15 };
    unsigned char cmd2[11] = { 0x24, 0x05, 0x18, 0x36, 0xfe, 0x7b, 0xef, 0x01, 0xe0, 0x03, 0x15 };
    unsigned char cmd3[11] = { 0x24, 0x05, 0x18, 0x36, 0xfe, 0x7b, 0xef, 0x01, 0xe1, 0x03, 0x15 };
    unsigned char cmd4[11] = { 0x24, 0x05, 0x18, 0x36, 0xdf, 0x7b, 0xef, 0x01, 0xc1, 0x03, 0x15 };
    unsigned char cmd5[10] = { 0x24, 0x05, 0x18, 0x36, 0xff, 0x7b, 0xef, 0x01, 0xe1, 0x03 };
    //unsigned char txcmd[11] = { 0x24, 0x05, 0x81, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 };
    unsigned int ifreq = freq;
    unsigned char reply[11];
    int retval;

    rxcmd[5] = (ifreq >> 24) & 0xff;
    rxcmd[6] = (ifreq >> 16) & 0xff;
    rxcmd[7] = (ifreq >> 8) & 0xff;
    rxcmd[8] = ifreq & 0xff;
    rxcmd[9] = checksum(rxcmd, 9);

    set_transaction_active(rig);
    rig_flush(rp);
    retval = write_block(rp, rxcmd, sizeof(rxcmd));
    micom_read_frame(rig, reply, sizeof(reply));
    if (retval == RIG_OK) retval = write_block(rp, cmd2, sizeof(cmd2));
    micom_read_frame(rig, reply, sizeof(reply));
    if (retval == RIG_OK) retval = write_block(rp, cmd3, sizeof(cmd3));
    micom_read_frame(rig, reply, sizeof(reply));
    if (retval == RIG_OK) retval = write_block(rp, cmd4, sizeof(cmd4));
    micom_read_frame(rig, reply, sizeof(reply));
    if (retval == RIG_OK) retval = write_block(rp, cmd5, sizeof(cmd5));
    micom_read_frame(rig, reply, sizeof(reply));
    micom_read_frame(rig, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: write_block err: %s\n", __func__,
                  rigerror(retval));
        set_transaction_inactive(rig);
        return retval;
    }

    micom_read_frame(rig, reply, sizeof(reply));

#if 0 // this method doesn't work
    txcmd[5] = (ifreq >> 16) & 0xff;
    txcmd[6] = (ifreq >> 8) & 0xff;
    txcmd[7] = ifreq & 0xff;
    txcmd[8] = checksum(txcmd, 8);
    txcmd[5] = (ifreq >> 24) & 0xff;
    txcmd[6] = (ifreq >> 16) & 0xff;
    txcmd[7] = (ifreq >> 8) & 0xff;
    txcmd[8] = ifreq & 0xff;
    txcmd[9] = checksum(rxcmd, 9);
    retval = write_block(rp, txcmd, sizeof(txcmd));
    micom_read_frame(rig, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: write_block err: %s\n", __func__,
                  rigerror(retval));
        set_transaction_inactive(rig);
        return retval;
    }

#endif
    set_transaction_inactive(rig);
    return retval;
}

static int micom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    hamlib_port_t *rp = RIGPORT(rig);
    unsigned char cmd[6] = { 0x24, 0x01, 0x18, 0x06, 0x06, 0x03 };
    unsigned char ack[6] = { 0x24, 0x01, 0x18, 0xf3, 0xff, 0x03 };
    unsigned char reply[11];
    int retval;

    cmd[4] = checksum(cmd,4);
    set_transaction_active(rig);
    rig_flush(rp);
    retval = write_block(rp, cmd, sizeof(cmd));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: write_block err: %s\n", __func__,
                  rigerror(retval));
        set_transaction_inactive(rig);
        return retval;
    }

    // expecting 24 01 80 fe 98 03 -- an ack packet?
    micom_read_frame(rig, reply, sizeof(reply));
    if (reply[3] != 0xfe)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown packet...expected byte 4 = 0xfe\n", __func__);
    }
    micom_read_frame(rig, reply, sizeof(reply));
    write_block(rp, ack, sizeof(ack));
    set_transaction_inactive(rig);
    *freq = (reply[4] << 24) | (reply[5] << 16) | (reply[6] << 8) | reply[7];
    RETURNFUNC2(RIG_OK);
}

static int micom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    hamlib_port_t *rp = RIGPORT(rig);
    unsigned char on[] =  { 0x24, 0x02, 0x81, 0x13, 0x01, 0xBB, 0x03 };
    unsigned char off[] = { 0x24, 0x02, 0x81, 0x14, 0x01, 0xBC, 0x03 };
    int retval;

    set_transaction_active(rig);
    rig_flush(rp);

    retval = write_block(rp, ptt ? on : off, sizeof(on));

    set_transaction_inactive(rig);

    return retval;
}

struct rig_caps micom_caps =
{
    RIG_MODEL(RIG_MODEL_MICOM2),
    .model_name         = "Micom 2/3",
    .mfg_name           = "Micom",
    .version            = "20240504.0",
    .copyright          = "LGPL",
    .status             = RIG_STATUS_ALPHA,
    .rig_type           = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo     = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type           = RIG_PTT_RIG,
    .port_type          = RIG_PORT_SERIAL,
    .serial_rate_min    = 9600,
    .serial_rate_max    = 9600,
    .serial_data_bits   = 8,
    .serial_stop_bits   = 2,
    .serial_parity      = RIG_PARITY_ODD,
    .serial_handshake   =  RIG_HANDSHAKE_NONE,
    .timeout            = 500,
    .rig_open           = micom_open,
    .set_freq           = micom_set_freq,
    .get_freq           = micom_get_freq,
    .set_ptt            = micom_set_ptt,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
