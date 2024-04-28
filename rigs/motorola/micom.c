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

static freq_t freqA = 14074000;

static int micom_open(RIG *rig)
{
    ENTERFUNC;
    RETURNFUNC(RIG_OK);
}

static int micom_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    freqA = freq;
    RETURNFUNC(RIG_OK);
}

static int micom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    *freq = freqA;
    RETURNFUNC(RIG_OK);
}

static int micom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    hamlib_port_t *rp = RIGPORT(rig);
    unsigned char on[] =  { 0x24, 0x02, 0x81, 0x13, 0x01, 0xBB, 0x03 };
    unsigned char off[] = { 0x24, 0x02, 0x81, 0x14, 0x01, 0xBC, 0x03 };

    rig_flush(rp);
    int retval = write_block(rp, ptt ? on : off, sizeof(on));

    if (retval != RIG_OK)
    {
        set_transaction_inactive(rig);
        RETURNFUNC(retval);
    }

    return RIG_OK;
}

struct rig_caps micom_caps =
{
    RIG_MODEL(RIG_MODEL_MICOM2),
    .model_name         = "Micom 2/3",
    .mfg_name           = "Micom",
    .version            = "20240428.0",
    .copyright          = "LGPL",
    .status             = RIG_STATUS_ALPHA,
    .rig_type           = RIG_TYPE_OTHER,
    .targetable_vfo     = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type           = RIG_PTT_RIG,
    .port_type          = RIG_PORT_SERIAL,
    .serial_rate_min    = 9600,
    .serial_rate_max    = 9600,
    .serial_data_bits   = 8,
    .serial_stop_bits   = 2,
    .serial_parity      = RIG_PARITY_EVEN,
    .rig_open           = micom_open,
    .set_freq           = micom_set_freq,
    .get_freq           = micom_get_freq,
    .set_ptt            = micom_set_ptt
};
