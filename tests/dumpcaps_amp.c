/*
 * dumpcaps_amp.c - Copyright (C) 2000-2012 Stephane Fillod
 * This programs dumps the capabilities of a backend rig.
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <hamlib/config.h>

#include <stdio.h>

#include <hamlib/rig.h>
#include "misc.h"

#include "sprintflst.h"
#include "ampctl_parse.h"


/*
 * the amp may be in amp_init state, but not opened
 */
int dumpcaps_amp(AMP *amp, FILE *fout)
{
    const struct amp_caps *caps;
    int backend_warnings = 0;
    static char prntbuf[1024];

    if (!amp || !amp->caps)
    {
        return -RIG_EINVAL;
    }

    caps = amp->caps;

    fprintf(fout, "Caps dump for model:\t%d\n", caps->amp_model);
    fprintf(fout, "Model name:\t\t%s\n", caps->model_name);
    fprintf(fout, "Mfg name:\t\t%s\n", caps->mfg_name);
    fprintf(fout, "Backend version:\t%s\n", caps->version);
    fprintf(fout, "Backend copyright:\t%s\n", caps->copyright);
    fprintf(fout, "Backend status:\t\t%s\n", rig_strstatus(caps->status));
    fprintf(fout, "Amp type:\t\t");

    switch (caps->amp_type & AMP_TYPE_MASK)
    {
    case AMP_TYPE_OTHER:
        fprintf(fout, "Other\n");
        break;

    default:
        fprintf(fout, "Unknown type=%d\n", caps->amp_type);
        backend_warnings++;
    }

    fprintf(fout, "Port type:\t\t");

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        fprintf(fout, "RS-232\n");
        fprintf(fout,
                "Serial speed:\t\t%d..%d bauds, %d%c%d%s\n",
                caps->serial_rate_min,
                caps->serial_rate_max,
                caps->serial_data_bits,
                caps->serial_parity == RIG_PARITY_NONE ? 'N' :
                caps->serial_parity == RIG_PARITY_ODD ? 'O' :
                caps->serial_parity == RIG_PARITY_EVEN ? 'E' :
                caps->serial_parity == RIG_PARITY_MARK ? 'M' : 'S',
                caps->serial_stop_bits,
                caps->serial_handshake == RIG_HANDSHAKE_NONE ? "" :
                (caps->serial_handshake == RIG_HANDSHAKE_XONXOFF ? " XONXOFF" : " CTS/RTS")
               );
        break;

    case RIG_PORT_PARALLEL:
        fprintf(fout, "Parallel\n");
        break;

    case RIG_PORT_DEVICE:
        fprintf(fout, "Device driver\n");
        break;

    case RIG_PORT_USB:
        fprintf(fout, "USB\n");
        break;

    case RIG_PORT_NETWORK:
        fprintf(fout, "Network link\n");
        break;

    case RIG_PORT_UDP_NETWORK:
        fprintf(fout, "UDP Network link\n");
        break;

    case RIG_PORT_NONE:
        fprintf(fout, "None\n");
        break;

    default:
        fprintf(fout, "Unknown\n");
        backend_warnings++;
    }

    fprintf(fout,
            "Write delay:\t\t%dmS, timeout %dmS, %d retr%s\n",
            caps->write_delay,
            caps->timeout, caps->retry,
            (caps->retry == 1) ? "y" : "ies");

    fprintf(fout,
            "Post Write delay:\t%dmS\n",
            caps->post_write_delay);

    fprintf(fout, "Has priv data:\t\t%c\n", caps->priv != NULL ? 'Y' : 'N');

    amp_sprintf_level(prntbuf, sizeof(prntbuf), caps->has_get_level);
    fprintf(fout, "Get level: %s\n", prntbuf);

    /*
     * Status is either 'Y'es, 'E'mulated, 'N'o
     *
     * TODO: keep me up-to-date with API call list!
     */
    fprintf(fout, "Has Init:\t\t%c\n", caps->amp_init != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Cleanup:\t\t%c\n", caps->amp_cleanup != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Open:\t\t%c\n", caps->amp_open != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Close:\t\t%c\n", caps->amp_close != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Conf:\t\t%c\n", caps->set_conf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Conf:\t\t%c\n", caps->get_conf != NULL ? 'Y' : 'N');

    fprintf(fout, "Can Reset:\t\t%c\n", caps->reset != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Info:\t\t%c\n", caps->get_info != NULL ? 'Y' : 'N');

    fprintf(fout, "\nOverall backend warnings: %d\n", backend_warnings);

    return backend_warnings;
}
