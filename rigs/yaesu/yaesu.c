/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * yaesu.c - (C) Stephane Fillod 2001-2010
 *
 * This shared library provides an API for communicating
 * via serial interface to a Yaesu rig
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

#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "yaesu.h"

struct yaesu_id
{
    rig_model_t model;
    int id1, id2;
};

#define UNKNOWN_ID -1

/*
 * Identification number as returned by 0xFA opcode.
 * Note: On the FT736R, the 0xFA opcode sets CTCSS tone code
 *
 * Please, if the model number of your rig is listed as UNKNOWN_ID,
 * send the value to <fillods@users.sourceforge.net> for inclusion. Thanks --SF
 */
static const struct yaesu_id yaesu_id_list[] =
{
    { RIG_MODEL_FT1000D, 0x10, 0x21 }, /* or 0x10, 0x00 ? */
    { RIG_MODEL_FT990, 0x09, 0x90 },
    { RIG_MODEL_FT990UNI, 0x09, 0x90 },
    { RIG_MODEL_FT890, 0x08, 0x41 },
    { RIG_MODEL_FRG100, 0x03, 0x92 }, /* TBC, inconsistency in manual */
    { RIG_MODEL_FT1000MP, 0x03, 0x93 },
    { RIG_MODEL_FT1000MPMKV, 0x03, 0x93 }, /* or 0x10, 0x00 ? */
    { RIG_MODEL_FT1000MPMKVFLD, 0x03, 0x93 },
    { RIG_MODEL_VX1700, 0x06, 0x04 },
    { RIG_MODEL_NONE, UNKNOWN_ID, UNKNOWN_ID }, /* end marker */
};


/*
 * initrigs_yaesu is called by rig_backend_load
 */

DECLARE_INITRIG_BACKEND(yaesu)
{
    rig_debug(RIG_DEBUG_VERBOSE, "yaesu: %s called\n", __func__);

    ft450d_caps = ft450_caps;
    ft450d_caps.rig_model = RIG_MODEL_FT450D;
    ft450d_caps.model_name = "FT-450D";
    rig_register(&ft100_caps);
    rig_register(&ft450_caps);
    rig_register(&ft450d_caps);
    rig_register(&ft736_caps);
    rig_register(&ft747_caps);
    rig_register(&ft757gx_caps);
    rig_register(&ft757gx2_caps);
    rig_register(&ft600_caps);
    rig_register(&ft767gx_caps);
    rig_register(&ft817_caps);
    rig_register(&ft847_caps);
    rig_register(&ft857_caps);
    rig_register(&ft897_caps);
    rig_register(&ft840_caps);
    rig_register(&ft890_caps);
    rig_register(&ft900_caps);
    rig_register(&ft920_caps);
    rig_register(&ft950_caps);
    rig_register(&ft980_caps);
    rig_register(&ft990_caps);
    rig_register(&ft990uni_caps);
    rig_register(&ft1000d_caps);
    rig_register(&ft1000mp_caps);
    rig_register(&ft1000mpmkv_caps);
    rig_register(&ft1000mpmkvfld_caps);
    rig_register(&ft2000_caps);
    rig_register(&ftdx3000_caps);
    rig_register(&ftdx5000_caps);
    rig_register(&ft9000_caps);
    rig_register(&frg100_caps);
    rig_register(&frg8800_caps);
    rig_register(&frg9600_caps);
    rig_register(&vr5000_caps);
    rig_register(&vx1700_caps);
    rig_register(&ftdx1200_caps);
    rig_register(&ft991_caps);
    rig_register(&ft891_caps);
    rig_register(&ft847uni_caps);
    rig_register(&ftdx101d_caps);
    rig_register(&ft818_caps);
    rig_register(&ftdx10_caps);
    rig_register(&ft897d_caps);
    rig_register(&ftdx101mp_caps);
    rig_register(&mchfqrp_caps);
    rig_register(&ft650_caps);
    rig_register(&ft710_caps);

    return RIG_OK;
}

/*
 * proberigs_yaesu
 *
 * Notes:
 * There's only one rig possible per port.
 *
 * rig_model_t probeallrigs_yaesu(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(yaesu)
{
    unsigned char idbuf[YAESU_CMD_LENGTH + 1];
    static const unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xfa};
    int id_len = -1, i, id1, id2;
    int retval = -1;
    static const int rates[] = { 4800, 57600, 9600, 38400, 0 };  /* possible baud rates */
    int rates_idx;

    if (!port)
    {
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
    }

    port->write_delay = port->post_write_delay = 20;
    port->parm.serial.stop_bits = 2;
    port->retry = 1;

    /*
     * try for all different baud rates
     */
    for (rates_idx = 0; rates[rates_idx]; rates_idx++)
    {
        port->parm.serial.rate = rates[rates_idx];
        port->timeout = 2 * 1000 / rates[rates_idx] + 50;

        retval = serial_open(port);

        if (retval != RIG_OK)
        {
            return RIG_MODEL_NONE;
        }

        /* send READ STATUS cmd to rig  */
        retval = write_block(port, cmd, YAESU_CMD_LENGTH);
        id_len = read_block(port, idbuf, YAESU_CMD_LENGTH);

        close(port->fd);

        if (retval == RIG_OK && id_len > 0)
        {
            break;
        }
    }

    if (retval != RIG_OK || id_len < 0)
    {
        return RIG_MODEL_NONE;
    }

    /*
     * reply should be [Flag1,Flag2,Flag3,ID1,ID2]
     */
    if (id_len != 5 && id_len != 6)
    {
        idbuf[YAESU_CMD_LENGTH] = '\0';
        rig_debug(RIG_DEBUG_WARN, "probe_yaesu: protocol error,"
                  " expected %d, received %d: %s\n", 6, id_len, idbuf);
        return RIG_MODEL_NONE;
    }

    id1 = idbuf[3];
    id2 = idbuf[4];


    for (i = 0; yaesu_id_list[i].model != RIG_MODEL_NONE; i++)
    {
        if (id1 == yaesu_id_list[i].id1 && id2 == yaesu_id_list[i].id2)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "probe_yaesu: "
                      "found ID %02xH %02xH\n", id1, id2);

            if (cfunc)
            {
                (*cfunc)(port, yaesu_id_list[i].model, data);
            }

            return yaesu_id_list[i].model;
        }
    }

    /*
     * not found in known table....
     * update yaesu_id_list[]!
     */
    rig_debug(RIG_DEBUG_WARN, "probe_yaesu: found unknown device "
              "with ID %02xH %02xH, please report to Hamlib "
              "developers.\n", id1, id2);

    return RIG_MODEL_NONE;
}
