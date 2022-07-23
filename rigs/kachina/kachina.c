/*
 *  Hamlib Kachina backend - main file
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

#include <hamlib/config.h>

#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"

#include "kachina.h"


/*
 * protocol format
 */
#define STX     0x02
#define ETX     0x03
#define GDCMD   0xff
#define ERRCMD  0xfe

/*
 * modes in use by the "M" command
 */
#define M_AM    0x01
#define M_CW    0x02
#define M_FM    0x03
#define M_USB   0x04
#define M_LSB   0x05

#define DDS_CONST 2.2369621333
#define DDS_BASE 75000000

/* uppermost 2 bits of the high byte
 * designating the antenna port in DDS calculation
 */
#define PORT_AB 0x00
#define PORT_A  0x40
#define PORT_B  0x80
#define PORT_BA 0xc0

/*
 * kachina_transaction
 * We assume that rig!=NULL, rig->state!= NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
static int kachina_transaction(RIG *rig, unsigned char cmd1, unsigned char cmd2)
{
    int count, retval;
    struct rig_state *rs;
    unsigned char buf4[4];

    rs = &rig->state;

    buf4[0] = STX;
    buf4[1] = cmd1;
    buf4[2] = cmd2;
    buf4[3] = ETX;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, buf4, 4);

    if (retval != RIG_OK)
    {
        return retval;
    }

    count = read_string(&rs->rigport, buf4, 1, "", 0, 0, 1);

    if (count != 1)
    {
        return count;
    }

    return (buf4[0] == GDCMD) ? RIG_OK : -RIG_EPROTO;
}

static int kachina_trans_n(RIG *rig, unsigned char cmd1, const char *data,
                           int data_len)
{
    int cmd_len, count, retval;
    struct rig_state *rs;
    unsigned char buf[16];

    rs = &rig->state;

    buf[0] = STX;
    buf[1] = cmd1;
    memcpy(buf + 2, data, data_len);
    buf[data_len + 2] = ETX;

    cmd_len = data_len + 3;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, buf, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    count = read_string(&rs->rigport, buf, 1, "", 0, 0, 1);

    if (count != 1)
    {
        return count;
    }

    return (buf[0] == GDCMD) ? RIG_OK : -RIG_EPROTO;
}

/*
 * convert a frequency in Hz in the range of 30kHz to 30MHz
 * to DDS value, as expected by the Kachina.
 */
static void freq2dds(freq_t freq, int ant_port, unsigned char fbuf[4])
{
    double dds;
    unsigned long dds_ulong;

    dds = DDS_CONST * (DDS_BASE + freq);
    dds_ulong = (unsigned long)dds;

    /*
     * byte 0 transferred first,
     * dds is big endian format
     */
    fbuf[0] = ant_port | ((dds_ulong >> 24) & 0x3f);
    fbuf[1] = (dds_ulong >> 16) & 0xff;
    fbuf[2] = (dds_ulong >> 8) & 0xff;
    fbuf[3] = dds_ulong & 0xff;
}

/*
 * kachina_set_freq
 * Assumes rig!=NULL
 */
int kachina_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    unsigned char freqbuf[4];

    freq2dds(freq, PORT_A, freqbuf);

    /* receive frequency */
    retval = kachina_trans_n(rig, 'R', (char *) freqbuf, 4);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* transmit frequency */
    retval = kachina_trans_n(rig, 'T', (char *) freqbuf, 4);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * kachina_set_mode
 * Assumes rig!=NULL
 *
 * FIXME: pbwidth handling
 */
int kachina_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    unsigned char k_mode;

    switch (mode)
    {
    case RIG_MODE_CW:       k_mode = M_CW; break;

    case RIG_MODE_USB:      k_mode = M_USB; break;

    case RIG_MODE_LSB:      k_mode = M_LSB; break;

    case RIG_MODE_FM:       k_mode = M_FM; break;

    case RIG_MODE_AM:       k_mode = M_AM; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    retval = kachina_transaction(rig, 'M', k_mode);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return retval;
}

/*
 * kachina_get_level
 * Assumes rig!=NULL
 */
int kachina_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int i, count;
    unsigned char buf[32];

    static const char rcv_signal_range[128] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
    };



    /* so far, only RAWSTR supported! */
    if (level != RIG_LEVEL_RAWSTR)
    {
        return -RIG_ENIMPL;
    }


    /* telemetry sent to the PC automatically at a 50msec rate */

    rig_flush(&rig->state.rigport);

    count = read_string(&rig->state.rigport, buf, 31, rcv_signal_range,
                        128, 0, 1);

    if (count < 1)
    {
        return count;
    }

    for (i = 0; i < count; i++)
    {
        if (buf[i] <= 0x7f)
        {
            break;
        }
    }

    val->i = buf[i];

    return RIG_OK;
}


/*
 * initrigs_kachina is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(kachina)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&k505dsp_caps);

    return RIG_OK;
}

