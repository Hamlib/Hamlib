/**
 * \addtogroup rig
 * @{
 */

/**
 * \file src/conf.c
 * \brief Rig configuration interface
 * \author Stephane Fillod
 * \date 2000-2012
 */
/*
 *  Hamlib Interface - configuration interface
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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "token.h"


/*
 * Configuration options available in the rig->state struct.
 */
static const struct confparams frontend_cfg_params[] =
{
    {
        TOK_PATHNAME, "rig_pathname", "Rig path name",
        "Path name to the device file of the rig",
        "/dev/rig", RIG_CONF_STRING,
    },
    {
        TOK_WRITE_DELAY, "write_delay", "Write delay",
        "Delay in ms between each byte sent out",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 1000, 1 } }
    },
    {
        TOK_POST_WRITE_DELAY, "post_write_delay", "Post write delay",
        "Delay in ms between each command sent out",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 1000, 1 } }
    },
    {
        TOK_POST_PTT_DELAY, "post_ptt_delay", "Post ptt delay",
        "Delay in ms after PTT is asserted",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 2000, 1 } } // 2000ms should be more than enough
    },
    {
        TOK_TIMEOUT, "timeout", "Timeout", "Timeout in ms",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 10000, 1 } }
    },
    {
        TOK_RETRY, "retry", "Retry", "Max number of retry",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } }
    },
    {
        TOK_TIMEOUT_RETRY, "timeout_retry", "Number of retries for read timeouts",
        "Set the # of retries for read timeouts that may occur with some serial interfaces",
        "1", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } }
    },
    {
        TOK_RANGE_SELECTED, "Selected range list", "Range list#",
        "The tx/rx range list in use",
        "0", RIG_CONF_NUMERIC, { .n = { 1, 5, 1 } }
    },
    {
        TOK_RANGE_NAME, "Selected range list", "Range list name",
        "The tx/rx range list name",
        "Default", RIG_CONF_STRING
    },
    {
        TOK_DEVICE_ID, "device_id", "Device ID",
        "User-specified device ID for multicast state data and commands",
        "", RIG_CONF_STRING,
    },

    {
        TOK_VFO_COMP, "vfo_comp", "VFO compensation",
        "VFO compensation in ppm",
        "0", RIG_CONF_NUMERIC, { .n = { 0.0, 1000.0, .001 } }
    },
    {
        TOK_POLL_INTERVAL, "poll_interval", "Rig state poll interval in ms",
        "Polling interval in ms for transceive emulation, defaults to 1000, value of 0 disables polling",
        "1000", RIG_CONF_NUMERIC, { .n = { 0, 1000000, 1 } }
    },
    {
        TOK_PTT_TYPE, "ptt_type", "PTT type",
        "Push-To-Talk interface type override",
        "RIG", RIG_CONF_COMBO, { .c = {{ "RIG", "RIGMICDATA", "DTR", "RTS", "Parallel", "CM108", "GPIO", "GPION", "None", NULL }} }
    },
    {
        TOK_PTT_PATHNAME, "ptt_pathname", "PTT path name",
        "Path to the device of the Push-To-Talk",
        "/dev/rig", RIG_CONF_STRING,
    },
    {
        TOK_PTT_BITNUM, "ptt_bitnum", "PTT bit [0-7]",
        "Push-To-Talk GPIO bit number",
        "2", RIG_CONF_NUMERIC, { .n = { 0, 7, 1 } }
    },
    {
        TOK_DCD_TYPE, "dcd_type", "DCD type",
        "Data Carrier Detect (or squelch) interface type override",
        "RIG", RIG_CONF_COMBO, { .c = {{ "RIG", "DSR", "CTS", "CD", "Parallel", "CM108", "GPIO", "GPION", "None", NULL }} }
    },
    {
        TOK_DCD_PATHNAME, "dcd_pathname", "DCD path name",
        "Path to the device of the Data Carrier Detect (or squelch)",
        "/dev/rig", RIG_CONF_STRING,
    },
    {
        TOK_LO_FREQ, "lo_freq", "LO Frequency",
        "Frequency to add to the VFO frequency for use with a transverter",
        "0", RIG_CONF_NUMERIC, { .n = {0.0, 1e9, .1}}
    },
    {
        TOK_CACHE_TIMEOUT, "cache_timeout", "Cache timeout value in ms",
        "Cache timeout, value of 0 disables caching",
        "500", RIG_CONF_NUMERIC, { .n = {0, 5000, 1}}
    },
    {
        TOK_AUTO_POWER_ON, "auto_power_on", "Auto power on",
        "True enables compatible rigs to be powered up on open",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_AUTO_POWER_OFF, "auto_power_off", "Auto power off",
        "True enables compatible rigs to be powered down on close",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_AUTO_DISABLE_SCREENSAVER, "auto_disable_screensaver", "Auto disable screen saver",
        "True enables compatible rigs to have their screen saver disabled on open",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_DISABLE_YAESU_BANDSELECT, "disable_yaesu_bandselect", "Disable Yaesu band select logic",
        "True disables the automatic band select on band change for Yaesu rigs",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_PTT_SHARE, "ptt_share", "Share ptt port with other apps",
        "True enables ptt port to be shared with other apps",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_FLUSHX, "flushx", "Flush with read instead of TCFLUSH",
        "True enables flushing serial port with read instead of TCFLUSH -- MicroHam",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_TWIDDLE_TIMEOUT, "twiddle_timeout", "Timeout(secs) to resume VFO polling when twiddling VFO",
        "For satellite ops when VFOB is twiddled will pause VFOB commands until timeout",
        "Unset", RIG_CONF_COMBO, { .c = {{ "Unset", "ON", "OFF", NULL }} }
    },
    {
        TOK_TWIDDLE_RIT, "twiddle_rit", "RIT twiddle",
        "Suppress get_freq on VFOB for RIT tuning satellites",
        "Unset", RIG_CONF_COMBO, { .c = {{ "Unset", "ON", "OFF", NULL }} }
    },
    {
        TOK_ASYNC, "async", "Asynchronous data transfer support",
        "True enables async data for rigs that support it to allow use of transceive and spectrum data",
        "0", RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_TUNER_CONTROL_PATHNAME, "tuner_control_pathname", "Tuner script/program path name",
        "Path to a program to control a tuner with 1 argument of 0/1 for Tuner Off/On",
        "hamlib_tuner_control", RIG_CONF_STRING,
    },
    {
        TOK_OFFSET_VFOA, "offset_vfoa", "Offset value in Hz",
        "Add Hz to VFOA/Main frequency set",
        "0", RIG_CONF_NUMERIC, { .n = {0, 1e12, 1}}
    },
    {
        TOK_OFFSET_VFOB, "offset_vfob", "Offset value in Hz",
        "Add Hz to VFOB/Sub frequency set",
        "0", RIG_CONF_NUMERIC, { .n = {0, 1e12, 1}}
    },
    {
        TOK_MULTICAST_DATA_ADDR, "multicast_data_addr", "Multicast data UDP address",
        "Multicast data UDP address for publishing rig data and state, value of 0.0.0.0 disables multicast data publishing",
        "224.0.0.1", RIG_CONF_STRING,
    },
    {
        TOK_MULTICAST_DATA_PORT, "multicast_data_port", "Multicast data UDP port",
        "Multicast data UDP port for publishing rig data and state",
        "4532", RIG_CONF_NUMERIC, { .n = { 0, 1000000, 1 } }
    },
    {
        TOK_MULTICAST_CMD_ADDR, "multicast_cmd_addr", "Multicast command server UDP address",
        "Multicast command UDP address for sending commands to rig, value of 0.0.0.0 disables multicast command server",
        "224.0.0.2", RIG_CONF_STRING,
    },
    {
        TOK_MULTICAST_CMD_PORT, "multicast_cmd_port", "Multicast command server UDP port",
        "Multicast data UDP port for sending commands to rig",
        "4532", RIG_CONF_NUMERIC, { .n = { 0, 1000000, 1 } }
    },

    { RIG_CONF_END, NULL, }
};


static const struct confparams frontend_serial_cfg_params[] =
{
#include "serial_cfg_params.h"
};


/*
 * frontend_set_conf
 * assumes rig!=NULL, val!=NULL
 */
static int frontend_set_conf(RIG *rig, token_t token, const char *val)
{
    struct rig_caps *caps;
    struct rig_state *rs;
    hamlib_port_t *rp = RIGPORT(rig);
    hamlib_port_t *pttp = PTTPORT(rig);
    hamlib_port_t *dcdp = DCDPORT(rig);
    long val_i;

    caps = rig->caps;
    rs = &rig->state;

    switch (token)
    {
    case TOK_PATHNAME:
        strncpy(rp->pathname, val, HAMLIB_FILPATHLEN - 1);
        strncpy(rs->rigport_deprecated.pathname, val, HAMLIB_FILPATHLEN - 1);
        break;

    case TOK_WRITE_DELAY:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        rp->write_delay = val_i;
        rs->rigport_deprecated.write_delay = val_i;
        break;

    case TOK_POST_WRITE_DELAY:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        rp->post_write_delay = val_i;
        rs->rigport_deprecated.post_write_delay = val_i;
        break;

    case TOK_POST_PTT_DELAY:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->post_ptt_delay = val_i;
        break;

    case TOK_TIMEOUT:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        rp->timeout = val_i;
        rs->rigport_deprecated.timeout = val_i;
        break;

    case TOK_RETRY:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        rp->retry = val_i;
        rs->rigport_deprecated.retry = val_i;
        break;

    case TOK_SERIAL_SPEED:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        rp->parm.serial.rate = val_i;
        rs->rigport_deprecated.parm.serial.rate = val_i;
        break;

    case TOK_DATA_BITS:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        rp->parm.serial.data_bits = val_i;
        rs->rigport_deprecated.parm.serial.data_bits = val_i;
        break;

    case TOK_STOP_BITS:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        rp->parm.serial.stop_bits = val_i;
        rs->rigport_deprecated.parm.serial.stop_bits = val_i;
        break;

    case TOK_PARITY:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "None"))
        {
            rp->parm.serial.parity = RIG_PARITY_NONE;
            rs->rigport_deprecated.parm.serial.parity = RIG_PARITY_SPACE;
        }
        else if (!strcmp(val, "Odd"))
        {
            rp->parm.serial.parity = RIG_PARITY_ODD;
            rs->rigport_deprecated.parm.serial.parity = RIG_PARITY_SPACE;
        }
        else if (!strcmp(val, "Even"))
        {
            rp->parm.serial.parity = RIG_PARITY_EVEN;
            rs->rigport_deprecated.parm.serial.parity = RIG_PARITY_SPACE;
        }
        else if (!strcmp(val, "Mark"))
        {
            rp->parm.serial.parity = RIG_PARITY_MARK;
            rs->rigport_deprecated.parm.serial.parity = RIG_PARITY_SPACE;
        }
        else if (!strcmp(val, "Space"))
        {
            rp->parm.serial.parity = RIG_PARITY_SPACE;
            rs->rigport_deprecated.parm.serial.parity = RIG_PARITY_SPACE;
        }
        else
        {
            return -RIG_EINVAL;
        }

        break;

    case TOK_HANDSHAKE:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: setting handshake is invalid for non-serial port rig type\n",
                      __func__);
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "None"))
        {
            rp->parm.serial.handshake = RIG_HANDSHAKE_NONE;
            rs->rigport_deprecated.parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
        }
        else if (!strcmp(val, "XONXOFF"))
        {
            rp->parm.serial.handshake = RIG_HANDSHAKE_XONXOFF;
            rs->rigport_deprecated.parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
        }
        else if (!strcmp(val, "Hardware"))
        {
            rp->parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
            rs->rigport_deprecated.parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
        }
        else
        {
            return -RIG_EINVAL;
        }

        break;

    case TOK_RTS_STATE:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "Unset"))
        {
            rp->parm.serial.rts_state = RIG_SIGNAL_UNSET;
            rs->rigport_deprecated.parm.serial.rts_state = RIG_SIGNAL_UNSET;
        }
        else if (!strcmp(val, "ON"))
        {
            rp->parm.serial.rts_state = RIG_SIGNAL_ON;
            rs->rigport_deprecated.parm.serial.rts_state = RIG_SIGNAL_ON;
        }
        else if (!strcmp(val, "OFF"))
        {
            rp->parm.serial.rts_state = RIG_SIGNAL_OFF;
            rs->rigport_deprecated.parm.serial.rts_state = RIG_SIGNAL_OFF;
        }
        else
        {
            return -RIG_EINVAL;
        }

        break;

    case TOK_DTR_STATE:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        if (!strcmp(val, "Unset"))
        {
            rp->parm.serial.dtr_state = RIG_SIGNAL_UNSET;
            rs->rigport_deprecated.parm.serial.dtr_state = RIG_SIGNAL_UNSET;
        }
        else if (!strcmp(val, "ON"))
        {
            rp->parm.serial.dtr_state = RIG_SIGNAL_ON;
            rs->rigport_deprecated.parm.serial.dtr_state = RIG_SIGNAL_ON;
        }
        else if (!strcmp(val, "OFF"))
        {
            rp->parm.serial.dtr_state = RIG_SIGNAL_OFF;
            rs->rigport_deprecated.parm.serial.dtr_state = RIG_SIGNAL_OFF;
        }
        else
        {
            return -RIG_EINVAL;
        }

        break;

    case TOK_RANGE_SELECTED:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        switch (val_i)
        {
        case 1:
            memcpy(rs->tx_range_list, caps->tx_range_list1,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            memcpy(rs->rx_range_list, caps->rx_range_list1,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            break;

        case 2:
            memcpy(rs->tx_range_list, caps->tx_range_list2,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            memcpy(rs->rx_range_list, caps->rx_range_list2,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            break;

        case 3:
            memcpy(rs->tx_range_list, caps->tx_range_list3,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            memcpy(rs->rx_range_list, caps->rx_range_list3,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            break;

        case 4:
            memcpy(rs->tx_range_list, caps->tx_range_list4,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            memcpy(rs->rx_range_list, caps->rx_range_list4,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            break;

        case 5:
            memcpy(rs->tx_range_list, caps->tx_range_list5,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            memcpy(rs->rx_range_list, caps->rx_range_list5,
                   sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
            break;

        default:
            return -RIG_EINVAL;
        }

        break;

    case TOK_PTT_TYPE:
        if (!strcmp(val, "RIG"))
        {
            pttp->type.ptt = RIG_PTT_RIG;
            caps->ptt_type = RIG_PTT_RIG;
        }
        else if (!strcmp(val, "RIGMICDATA"))
        {
            pttp->type.ptt = RIG_PTT_RIG_MICDATA;
            caps->ptt_type = RIG_PTT_RIG_MICDATA;
        }
        else if (!strcmp(val, "DTR"))
        {
            pttp->type.ptt = RIG_PTT_SERIAL_DTR;
            caps->ptt_type = RIG_PTT_SERIAL_DTR;
        }
        else if (!strcmp(val, "RTS"))
        {
            pttp->type.ptt = RIG_PTT_SERIAL_RTS;
            caps->ptt_type = RIG_PTT_SERIAL_RTS;
        }
        else if (!strcmp(val, "Parallel"))
        {
            pttp->type.ptt = RIG_PTT_PARALLEL;
            caps->ptt_type = RIG_PTT_PARALLEL;
        }
        else if (!strcmp(val, "CM108"))
        {
            pttp->type.ptt = RIG_PTT_CM108;
            caps->ptt_type = RIG_PTT_CM108;
        }
        else if (!strcmp(val, "GPIO"))
        {
            pttp->type.ptt = RIG_PTT_GPIO;
        }
        else if (!strcmp(val, "GPION"))
        {
            pttp->type.ptt = RIG_PTT_GPION;
            caps->ptt_type = RIG_PTT_GPION;
        }
        else if (!strcmp(val, "None"))
        {
            pttp->type.ptt = RIG_PTT_NONE;
            caps->ptt_type = RIG_PTT_NONE;
        }
        else
        {
            return -RIG_EINVAL;
        }

        // JTDX and WSJTX currently use state.pttport to check for PTT_NONE
//        rig->state.pttport.type.ptt = pttp->type.ptt;
        rs->pttport_deprecated.type.ptt = pttp->type.ptt;

        break;

    case TOK_PTT_PATHNAME:
        strncpy(pttp->pathname, val, HAMLIB_FILPATHLEN - 1);
        strncpy(rs->pttport_deprecated.pathname, val, HAMLIB_FILPATHLEN - 1);
        break;

    case TOK_PTT_BITNUM:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;//value format error
        }

        pttp->parm.cm108.ptt_bitnum = val_i;
        rp->parm.cm108.ptt_bitnum = val_i;
        rs->pttport_deprecated.parm.cm108.ptt_bitnum = val_i;
        break;

    case TOK_DCD_TYPE:
        if (!strcmp(val, "RIG"))
        {
            dcdp->type.dcd = RIG_DCD_RIG;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_RIG;
        }
        else if (!strcmp(val, "DSR"))
        {
            dcdp->type.dcd = RIG_DCD_SERIAL_DSR;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_SERIAL_DSR;
        }
        else if (!strcmp(val, "CTS"))
        {
            dcdp->type.dcd = RIG_DCD_SERIAL_CTS;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_SERIAL_CTS;
        }
        else if (!strcmp(val, "CD"))
        {
            dcdp->type.dcd = RIG_DCD_SERIAL_CAR;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_SERIAL_CAR;
        }
        else if (!strcmp(val, "Parallel"))
        {
            dcdp->type.dcd = RIG_DCD_PARALLEL;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_PARALLEL;
        }
        else if (!strcmp(val, "CM108"))
        {
            dcdp->type.dcd = RIG_DCD_CM108;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_CM108;
        }
        else if (!strcmp(val, "GPIO"))
        {
            dcdp->type.dcd = RIG_DCD_GPIO;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_GPIO;
        }
        else if (!strcmp(val, "GPION"))
        {
            dcdp->type.dcd = RIG_DCD_GPION;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_GPION;
        }
        else if (!strcmp(val, "None"))
        {
            dcdp->type.dcd = RIG_DCD_NONE;
            rs->dcdport_deprecated.type.dcd = RIG_DCD_NONE;
        }
        else
        {
            return -RIG_EINVAL;
        }

        break;

    case TOK_DCD_PATHNAME:
        strncpy(dcdp->pathname, val, HAMLIB_FILPATHLEN - 1);
        strncpy(rs->dcdport_deprecated.pathname, val, HAMLIB_FILPATHLEN - 1);
        break;

    case TOK_DEVICE_ID:
        strncpy(rs->device_id, val, HAMLIB_RIGNAMSIZ - 1);
        break;


    case TOK_VFO_COMP:
        rs->vfo_comp = atof(val);
        break;

    case TOK_POLL_INTERVAL:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->poll_interval = val_i;
        // Make sure cache times out before next poll cycle
        rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, atol(val));
        break;

    case TOK_LO_FREQ:
        rs->lo_freq = atof(val);
        break;

    case TOK_CACHE_TIMEOUT:
        rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, atol(val));
        break;

    case TOK_AUTO_POWER_ON:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->auto_power_on = val_i ? 1 : 0;
        break;

    case TOK_AUTO_POWER_OFF:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->auto_power_off = val_i ? 1 : 0;
        break;

    case TOK_AUTO_DISABLE_SCREENSAVER:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->auto_disable_screensaver = val_i ? 1 : 0;
        break;

    case TOK_DISABLE_YAESU_BANDSELECT:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->disable_yaesu_bandselect = val_i ? 1 : 0;
        break;

    case TOK_PTT_SHARE:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->ptt_share = val_i ? 1 : 0;
        break;

    case TOK_FLUSHX:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rp->flushx = val_i ? 1 : 0;
        break;

    case TOK_TWIDDLE_TIMEOUT:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->twiddle_timeout = val_i;
        break;

    case TOK_TWIDDLE_RIT:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->twiddle_rit = val_i ? 1 : 0;
        break;

    case TOK_ASYNC:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->async_data_enabled = val_i ? 1 : 0;
        break;

    case TOK_TUNER_CONTROL_PATHNAME:
        rs->tuner_control_pathname = strdup(val); // yeah -- need to free it
        break;

    case TOK_TIMEOUT_RETRY:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;
        }

        rp->timeout_retry = val_i;
        break;

    case TOK_OFFSET_VFOA:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->offset_vfoa = val_i;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: offset_vfoa=%ld\n", __func__, val_i);
        break;

    case TOK_OFFSET_VFOB:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL; //value format error
        }

        rs->offset_vfob = val_i;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: offset_vfob=%ld\n", __func__, val_i);
        break;

    case TOK_MULTICAST_DATA_ADDR:
        rs->multicast_data_addr = strdup(val);
        break;

    case TOK_MULTICAST_DATA_PORT:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->multicast_data_port = val_i;
        break;

    case TOK_MULTICAST_CMD_ADDR:
        rs->multicast_cmd_addr = strdup(val);
        break;

    case TOK_MULTICAST_CMD_PORT:
        if (1 != sscanf(val, "%ld", &val_i))
        {
            return -RIG_EINVAL;
        }

        rs->multicast_cmd_port = val_i;
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * frontend_get_conf
 * assumes rig!=NULL, val!=NULL
 */
static int frontend_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct rig_state *rs;
    const char *s = "";
    hamlib_port_t *rp = RIGPORT(rig);
    hamlib_port_t *pttp = PTTPORT(rig);
    hamlib_port_t *dcdp = DCDPORT(rig);

    rs = &rig->state;

    switch (token)
    {
    case TOK_PATHNAME:
        SNPRINTF(val, val_len, "%s", rp->pathname);
        break;

    case TOK_WRITE_DELAY:
        SNPRINTF(val, val_len, "%d", rp->write_delay);
        break;

    case TOK_POST_WRITE_DELAY:
        SNPRINTF(val, val_len, "%d", rp->post_write_delay);
        break;

    case TOK_POST_PTT_DELAY:
        SNPRINTF(val, val_len, "%d", rs->post_ptt_delay);
        break;

    case TOK_TIMEOUT:
        SNPRINTF(val, val_len, "%d", rp->timeout);
        break;

    case TOK_RETRY:
        SNPRINTF(val, val_len, "%d", rp->retry);
        break;

#if 0 // needs to be replace?

    case TOK_ITU_REGION:
        SNPRINTF(val, val_len, "%d",
                 rs->itu_region == 1 ? RIG_ITU_REGION1 : RIG_ITU_REGION2);
        break;
#endif

    case TOK_SERIAL_SPEED:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(val, val_len, "%d", rp->parm.serial.rate);
        break;

    case TOK_DATA_BITS:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(val, val_len, "%d", rp->parm.serial.data_bits);
        break;

    case TOK_STOP_BITS:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(val, val_len, "%d", rp->parm.serial.stop_bits);
        break;

    case TOK_PARITY:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        switch (rp->parm.serial.parity)
        {
        case RIG_PARITY_NONE:
            s = "None";
            break;

        case RIG_PARITY_ODD:
            s = "Odd";
            break;

        case RIG_PARITY_EVEN:
            s = "Even";
            break;

        case RIG_PARITY_MARK:
            s = "Mark";
            break;

        case RIG_PARITY_SPACE:
            s = "Space";
            break;

        default:
            return -RIG_EINVAL;
        }

        strcpy(val, s);
        break;

    case TOK_HANDSHAKE:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: getting handshake is invalid for non-serial port rig type\n",
                      __func__);
            return -RIG_EINVAL;
        }

        switch (rp->parm.serial.handshake)
        {
        case RIG_HANDSHAKE_NONE:
            s = "None";
            break;

        case RIG_HANDSHAKE_XONXOFF:
            s = "XONXOFF";
            break;

        case RIG_HANDSHAKE_HARDWARE:
            s = "Hardware";
            break;

        default:
            return -RIG_EINVAL;
        }

        strcpy(val, s);
        break;

    case TOK_RTS_STATE:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        switch (rp->parm.serial.rts_state)
        {
        case RIG_SIGNAL_UNSET:
            s = "Unset";
            break;

        case RIG_SIGNAL_ON:
            s = "ON";
            break;

        case RIG_SIGNAL_OFF:
            s = "OFF";
            break;

        default:
            return -RIG_EINVAL;
        }

        strcpy(val, s);
        break;

    case TOK_DTR_STATE:
        if (rp->type.rig != RIG_PORT_SERIAL)
        {
            return -RIG_EINVAL;
        }

        switch (rp->parm.serial.dtr_state)
        {
        case RIG_SIGNAL_UNSET:
            s = "Unset";
            break;

        case RIG_SIGNAL_ON:
            s = "ON";
            break;

        case RIG_SIGNAL_OFF:
            s = "OFF";
            break;

        default:
            return -RIG_EINVAL;
        }

        strcpy(val, s);
        break;

    case TOK_DEVICE_ID:
        SNPRINTF(val, val_len, "%s", rs->device_id);
        break;

    case TOK_VFO_COMP:
        SNPRINTF(val, val_len, "%f", rs->vfo_comp);
        break;

    case TOK_POLL_INTERVAL:
        SNPRINTF(val, val_len, "%d", rs->poll_interval);
        break;

    case TOK_PTT_TYPE:
        switch (pttp->type.ptt)
        {
        case RIG_PTT_RIG:
            s = "RIG";
            break;

        case RIG_PTT_RIG_MICDATA:
            s = "RIGMICDATA";
            break;

        case RIG_PTT_SERIAL_DTR:
            s = "DTR";
            break;

        case RIG_PTT_SERIAL_RTS:
            s = "RTS";
            break;

        case RIG_PTT_PARALLEL:
            s = "Parallel";
            break;

        case RIG_PTT_CM108:
            s = "CM108";
            break;

        case RIG_PTT_GPIO:
            s = "GPIO";
            break;

        case RIG_PTT_GPION:
            s = "GPION";
            break;

        case RIG_PTT_NONE:
            s = "None";
            break;

        default:
            return -RIG_EINVAL;
        }

        strcpy(val, s);
        break;

    case TOK_PTT_PATHNAME:
        strcpy(val, pttp->pathname);
        break;

    case TOK_PTT_BITNUM:
        SNPRINTF(val, val_len, "%d", pttp->parm.cm108.ptt_bitnum);
        break;

    case TOK_DCD_TYPE:
        switch (dcdp->type.dcd)
        {
        case RIG_DCD_RIG:
            s = "RIG";
            break;

        case RIG_DCD_SERIAL_DSR:
            s = "DSR";
            break;

        case RIG_DCD_SERIAL_CTS:
            s = "CTS";
            break;

        case RIG_DCD_SERIAL_CAR:
            s = "CD";
            break;

        case RIG_DCD_PARALLEL:
            s = "Parallel";
            break;

        case RIG_DCD_CM108:
            s = "CM108";
            break;

        case RIG_DCD_GPIO:
            s = "GPIO";
            break;

        case RIG_DCD_GPION:
            s = "GPION";
            break;

        case RIG_DCD_NONE:
            s = "None";
            break;

        default:
            return -RIG_EINVAL;
        }

        strcpy(val, s);
        break;

    case TOK_DCD_PATHNAME:
        strcpy(val, dcdp->pathname);
        break;

    case TOK_LO_FREQ:
        SNPRINTF(val, val_len, "%g", rs->lo_freq);
        break;

    case TOK_CACHE_TIMEOUT:
        SNPRINTF(val, val_len, "%d", rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL));
        break;

    case TOK_AUTO_POWER_ON:
        SNPRINTF(val, val_len, "%d", rs->auto_power_on);
        break;

    case TOK_AUTO_POWER_OFF:
        SNPRINTF(val, val_len, "%d", rs->auto_power_off);
        break;

    case TOK_AUTO_DISABLE_SCREENSAVER:
        SNPRINTF(val, val_len, "%d", rs->auto_disable_screensaver);
        break;

    case TOK_PTT_SHARE:
        SNPRINTF(val, val_len, "%d", rs->ptt_share);
        break;

    case TOK_FLUSHX:
        SNPRINTF(val, val_len, "%d", rp->flushx);
        break;

    case TOK_DISABLE_YAESU_BANDSELECT:
        SNPRINTF(val, val_len, "%d", rs->disable_yaesu_bandselect);
        break;

    case TOK_TWIDDLE_TIMEOUT:
        SNPRINTF(val, val_len, "%d", rs->twiddle_timeout);
        break;

    case TOK_TWIDDLE_RIT:
        SNPRINTF(val, val_len, "%d", rs->twiddle_rit);
        break;

    case TOK_ASYNC:
        SNPRINTF(val, val_len, "%d", rs->async_data_enabled);
        break;

    case TOK_TIMEOUT_RETRY:
        SNPRINTF(val, val_len, "%d", rp->timeout_retry);
        break;

    case TOK_MULTICAST_DATA_ADDR:
        SNPRINTF(val, val_len, "%s", rs->multicast_data_addr);
        break;

    case TOK_MULTICAST_DATA_PORT:
        SNPRINTF(val, val_len, "%d", rs->multicast_data_port);
        break;

    case TOK_MULTICAST_CMD_ADDR:
        SNPRINTF(val, val_len, "%s", rs->multicast_cmd_addr);
        break;

    case TOK_MULTICAST_CMD_PORT:
        SNPRINTF(val, val_len, "%d", rs->multicast_cmd_port);
        break;

    default:
        return -RIG_EINVAL;
    }

    memcpy(&rs->rigport_deprecated, rp, sizeof(hamlib_port_t_deprecated));

    return RIG_OK;
}


/**
 * \brief call a function against each configuration token of a rig
 * \param rig   The rig handle
 * \param cfunc The function to perform on each token
 * \param data  Any data to be passed to cfunc()
 *
 * Executes \a cfunc() on all the elements stored in the conf table.
 * rig_token_foreach starts first with backend conf table, then finish
 * with frontend table.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_token_foreach(RIG *rig,
                                 int (*cfunc)(const struct confparams *,
                                         rig_ptr_t),
                                 rig_ptr_t data)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps || !cfunc)
    {
        return -RIG_EINVAL;
    }

    for (cfp = frontend_cfg_params; cfp->name; cfp++)
    {
        if ((*cfunc)(cfp, data) == 0)
        {
            return RIG_OK;
        }
    }

    if (rig->caps->port_type == RIG_PORT_SERIAL)
    {
        for (cfp = frontend_serial_cfg_params; cfp->name; cfp++)
        {
            if ((*cfunc)(cfp, data) == 0)
            {
                return RIG_OK;
            }
        }
    }

    for (cfp = rig->caps->cfgparams; cfp && cfp->name; cfp++)
    {
        if ((*cfunc)(cfp, data) == 0)
        {
            return RIG_OK;
        }
    }

    for (cfp = rig->caps->extlevels; cfp && cfp->name; cfp ++)
    {
        if ((*cfunc)(cfp, data) == 0)
        {
            return RIG_OK;
        }
    }

    return RIG_OK;
}


/**
 * \brief lookup a confparam struct
 * \param rig   The rig handle
 * \param name  The name of the configuration parameter
 *
 * Lookup conf token by its name.
 *
 * \return a pointer to the confparams struct if found, otherwise NULL.
 */
const struct confparams *HAMLIB_API rig_confparam_lookup(RIG *rig,
        const char *name)
{
    const struct confparams *cfp;
    token_t token;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called for %s\n", __func__, name);

    if (!rig || !rig->caps)
    {
        return NULL;
    }

    /* 0 returned for invalid format */
    token = strtol(name, NULL, 0);

    for (cfp = rig->caps->cfgparams; cfp && cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name) || token == cfp->token)
        {
            return cfp;
        }
    }

    for (cfp = frontend_cfg_params; cfp->name; cfp++)
    {
        if (!strcmp(cfp->name, name) || token == cfp->token)
        {
            return cfp;
        }
    }

    if (rig->caps->port_type == RIG_PORT_SERIAL)
    {
        for (cfp = frontend_serial_cfg_params; cfp->name; cfp++)
        {
            if (!strcmp(cfp->name, name) || token == cfp->token)
            {
                return cfp;
            }
        }
    }

    return NULL;
}


/**
 * \brief lookup a token id
 * \param rig   The rig handle
 * \param name  The name of the configuration parameter
 *
 * Simple lookup returning token id associated with name.
 *
 * \return the token id if found, otherwise RIG_CONF_END
 */
token_t HAMLIB_API rig_token_lookup(RIG *rig, const char *name)
{
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called for %s\n", __func__, name);

    cfp = rig_confparam_lookup(rig, name);

    if (!cfp)
    {
        return RIG_CONF_END;
    }

    return cfp->token;
}


/**
 * \brief set a radio configuration parameter
 * \param rig   The rig handle
 * \param token The parameter
 * \param val   The value to set the parameter to
 *
 *  Sets a configuration parameter.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_conf()
 */
int HAMLIB_API rig_set_conf(RIG *rig, token_t token, const char *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    // Some parameters can be ignored
    if (token == TOK_HANDSHAKE && (RIGPORT(rig)->type.rig != RIG_PORT_SERIAL))
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: handshake is not valid for non-serial port rig\n", __func__);
        return RIG_OK; // this allows rigctld to continue and just print a warning instead of error
    }

    if (rig_need_debug(RIG_DEBUG_VERBOSE))
    {
        const struct confparams *cfp;
        char tokenstr[12];
        SNPRINTF(tokenstr, sizeof(tokenstr), "%ld", token);
        cfp = rig_confparam_lookup(rig, tokenstr);

        if (!cfp)
        {
            return -RIG_EINVAL;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: %s='%s'\n", __func__, cfp->name, val);
    }

    if (IS_TOKEN_FRONTEND(token))
    {
        return frontend_set_conf(rig, token, val);
    }

    if (rig->caps->set_conf == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return rig->caps->set_conf(rig, token, val);
}


/**
 * \brief get the value of a configuration parameter
 * \param rig   The rig handle
 * \param token The parameter
 * \param val   The location where to store the value of config \a token
 *
 *  Retrieves the value of a configuration parameter associated with \a token.
 *  The location pointed to by val must be large enough to hold the value of the config.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_conf()
 */
int HAMLIB_API rig_get_conf(RIG *rig, token_t token, char *val)
{
    return rig_get_conf2(rig, token, val, 128);
}

int HAMLIB_API rig_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rig->caps || !val)
    {
        return -RIG_EINVAL;
    }

    if (IS_TOKEN_FRONTEND(token))
    {
        return frontend_get_conf2(rig, token, val, val_len);
    }

    if (rig->caps->get_conf2)
    {
        return rig->caps->get_conf2(rig, token, val, val_len);
    }

    if (rig->caps->get_conf == NULL)
    {
        return -RIG_ENAVAIL;
    }

    return rig->caps->get_conf(rig, token, val);
}

/*! @} */
