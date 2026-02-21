/**
 * dumpcaps_amp.c - Copyright (C) 2000-2012 Stephane Fillod
 *                  (C) Mikael Nousiainen OH3BHX 2026
 *
 * This program dumps the capabilities of a backend amplifier.
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

#include <stdio.h>

#include "hamlib/rig.h"

#include "sprintflst.h"
#include "ampctl_parse.h"
#include "amplifier.h"

static int print_ext(AMP *amp, const struct confparams *cfp, rig_ptr_t ptr)
{
    return print_ext_param(cfp, ptr);
}

void range_print(FILE *fout, const struct amp_freq_range_list range_list[], int rx)
{
    int i;
    char prntbuf[1024];  /* a malloc would be better.. */

    for (i = 0; i < HAMLIB_FRQRANGESIZ; i++)
    {
        if (range_list[i].startf == 0 && range_list[i].endf == 0)
        {
            break;
        }

        fprintf(fout, "\t%.0f Hz - %.0f Hz\n", range_list[i].startf,
                range_list[i].endf);

        fprintf(fout, "\t\tMode list: ");
        rig_sprintf_mode(prntbuf, sizeof(prntbuf), range_list[i].modes);
        fprintf(fout, "%s", prntbuf);
        fprintf(fout, "\n");

        // TODO: have a separate type / string identifiers for inputs?
        fprintf(fout, "\t\tInput list: ");
        rig_sprintf_ant(prntbuf, sizeof(prntbuf), range_list[i].input);
        fprintf(fout, "%s", prntbuf);
        fprintf(fout, "\n");

        fprintf(fout, "\t\tAntenna list: ");
        rig_sprintf_ant(prntbuf, sizeof(prntbuf), range_list[i].ant);
        fprintf(fout, "%s", prntbuf);
        fprintf(fout, "\n");

        if (!rx)
        {
            char *label_lo = "W";
            char *label_hi = "W";
            double low = range_list[i].low_power / 1000.0f;
            double hi = range_list[i].high_power / 1000.0f;

            if (low < 0)
            {
                label_lo = "mW";
                low *= 1000;
            }

            if (low < 0)
            {
                label_lo = "uW";
                low *= 1000;
            }

            if (hi < 0)
            {
                label_hi = "mW";
                hi *= 1000;
            }

            if (hi < 0)
            {
                label_hi = "uW";
                hi *= 1000;
            }

            fprintf(fout, "\t\tLow power: %g %s, High power: %g %s\n", low, label_lo, hi,
                    label_hi);
        }
    }
}

/*
 * check for:
 * - start_freq < end_freq  return_code = -1
 * - modes are not 0        return_code = -2
 * - if(rx), low_power, high_power set to -1        return_code = -3
 *     else, power is > 0
 * - array is ended by a {0,0,0,0,0} element (before boundary) rc = -4
 * - ranges with same modes do not overlap      rc = -5
 * ->fprintf(stderr,)!
 *
 * TODO: array is sorted in ascending freq order
 */
int range_sanity_check(const struct amp_freq_range_list range_list[], int rx)
{
    int i;

    for (i = 0; i < HAMLIB_FRQRANGESIZ; i++)
    {
        if (range_list[i].startf == 0 && range_list[i].endf == 0)
        {
            break;
        }

        if (range_list[i].startf > range_list[i].endf)
        {
            return -1;
        }

        if (range_list[i].modes == 0)
        {
            return -2;
        }

        if (rx)
        {
            if (range_list[i].low_power > 0 && range_list[i].high_power > 0)
            {
                return -3;
            }
        }
        else
        {
            if (!(range_list[i].low_power >= RIG_FREQ_NONE
                    && range_list[i].high_power >= RIG_FREQ_NONE))
            {
                return -3;
            }

            if (range_list[i].low_power > range_list[i].high_power)
            {
                return -3;
            }
        }
    }

    if (i == HAMLIB_FRQRANGESIZ)
    {
        return -4;
    }

    return 0;
}

/*
 * the amp may be in amp_init state, but not opened
 */
int dumpcaps_amp(AMP *amp, FILE *fout)
{
    const struct amp_caps *caps;
    int backend_warnings = 0;
    static char prntbuf[1024];
    char warnbuf[4096] = "";
    char *label1, *label2, *label3, *label4, *label5;
    int status;

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
                "Serial speed:\t\t%d..%d baud, %d%c%d, ctrl=%s\n",
                caps->serial_rate_min,
                caps->serial_rate_max,
                caps->serial_data_bits,
                caps->serial_parity == RIG_PARITY_NONE ? 'N' :
                caps->serial_parity == RIG_PARITY_ODD ? 'O' :
                caps->serial_parity == RIG_PARITY_EVEN ? 'E' :
                caps->serial_parity == RIG_PARITY_MARK ? 'M' : 'S',
                caps->serial_stop_bits,
                caps->serial_handshake == RIG_HANDSHAKE_NONE ? "NONE" :
                (caps->serial_handshake == RIG_HANDSHAKE_XONXOFF ? "XONXOFF" : "CTS/RTS")
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
            "Write delay:\t\t%dms, timeout %dms, %d retr%s\n",
            caps->write_delay,
            caps->timeout, caps->retry,
            (caps->retry == 1) ? "y" : "ies");

    fprintf(fout,
            "Post write delay:\t%dms\n",
            caps->post_write_delay);

    fprintf(fout, "Has priv data:\t\t%c\n", caps->priv != NULL ? 'Y' : 'N');

    if (amp->state.has_status != 0)
    {
        amp_sprintf_status(prntbuf, sizeof(prntbuf), amp->state.has_status);
    }
    else
    {
        strcpy(prntbuf, "None");
    }

    fprintf(fout, "Status flags: %s\n", prntbuf);

    amp_sprintf_func(prntbuf, sizeof(prntbuf), caps->has_get_func);
    fprintf(fout, "Get functions: %s\n", prntbuf);

    amp_sprintf_func(prntbuf, sizeof(prntbuf), caps->has_set_func);
    fprintf(fout, "Set functions: %s\n", prntbuf);

    fprintf(fout, "Extra functions:\n");
    amp_ext_func_foreach(amp, print_ext, fout);

    amp_sprintf_level_gran(prntbuf, sizeof(prntbuf), caps->has_get_level,
                           caps->level_gran);
    fprintf(fout, "Get level: %s\n", prntbuf);

    amp_sprintf_level_gran(prntbuf, sizeof(prntbuf), caps->has_set_level,
                           caps->level_gran);
    fprintf(fout, "Set level: %s\n", prntbuf);

    if (caps->has_set_level & AMP_LEVEL_READONLY_LIST)
    {
        fprintf(fout, "Warning--backend can set readonly levels!\n");
        backend_warnings++;
    }

    fprintf(fout, "Extra levels:\n");
    amp_ext_level_foreach(amp, print_ext, fout);

    amp_sprintf_parm_gran(prntbuf, sizeof(prntbuf), caps->has_get_parm,
                          caps->parm_gran);
    fprintf(fout, "Get parameters: %s\n", prntbuf);

    amp_sprintf_parm_gran(prntbuf, sizeof(prntbuf), caps->has_set_parm,
                          caps->parm_gran);
    fprintf(fout, "Set parameters: %s\n", prntbuf);

    if (caps->has_set_parm & AMP_PARM_READONLY_LIST)
    {
        fprintf(fout, "Warning--backend can set readonly parms!\n");
        backend_warnings++;
    }

    fprintf(fout, "Extra parameters:\n");
    amp_ext_parm_foreach(amp, print_ext, fout);

    amp_sprintf_amp_op(prntbuf, sizeof(prntbuf), caps->amp_ops);
    fprintf(fout, "Amp Ops: %s\n", prntbuf);

    label1 = caps->range_list1->label;
    label1 = label1 == NULL ? "TBD" : label1;
    fprintf(fout, "TX ranges #1 for %s:\n", label1);
    range_print(fout, caps->range_list1, 0);

    label2 = caps->range_list2->label;
    label2 = label2 == NULL ? "TBD" : label2;
    fprintf(fout, "TX ranges #2 for %s:\n", label2);
    range_print(fout, caps->range_list2, 0);

    label3 = caps->range_list3->label;
    label3 = label3 == NULL ? "TBD" : label3;
    fprintf(fout, "TX ranges #3 for %s:\n", label3);
    range_print(fout, caps->range_list3, 0);

    label4 = caps->range_list4->label;
    label4 = label4 == NULL ? "TBD" : label4;
    fprintf(fout, "TX ranges #4 for %s:\n", label4);
    range_print(fout, caps->range_list4, 0);

    label5 = caps->range_list5->label;
    label5 = label5 == NULL ? "TBD" : label5;
    fprintf(fout, "TX ranges #5 for %s:\n", label5);
    range_print(fout, caps->range_list5, 0);

    status = range_sanity_check(caps->range_list1, 0);
    fprintf(fout,
            "TX ranges #1 status for %s:\t%s (%d)\n", label1,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#1");
        backend_warnings++;
    }

    status = range_sanity_check(caps->range_list2, 0);
    fprintf(fout,
            "TX ranges #2 status for %s:\t%s (%d)\n", label2,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#2");
        backend_warnings++;
    }

    status = range_sanity_check(caps->range_list3, 0);
    fprintf(fout,
            "TX ranges #3 status for %s:\t%s (%d)\n", label3,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#3");
        backend_warnings++;
    }

    status = range_sanity_check(caps->range_list4, 0);
    fprintf(fout,
            "TX ranges #4 status for %s:\t%s (%d)\n", label4,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#4");
        backend_warnings++;
    }

    status = range_sanity_check(caps->range_list5, 0);
    fprintf(fout,
            "TX ranges #5 status for %s:\t%s (%d)\n", label5,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#5");
        backend_warnings++;
    }

    /*
     * Status is either 'Y'es, 'E'mulated, 'N'o
     */
    fprintf(fout, "Has Init:\t\t%c\n", caps->amp_init != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Cleanup:\t\t%c\n", caps->amp_cleanup != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Open:\t\t%c\n", caps->amp_open != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Close:\t\t%c\n", caps->amp_close != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Conf:\t\t%c\n", caps->set_conf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Conf:\t\t%c\n", caps->get_conf != NULL ? 'Y' : 'N');

    fprintf(fout, "Can Reset:\t\t%c\n", caps->reset != NULL ? 'Y' : 'N');

    fprintf(fout, "Can get Info:\t\t%c\n", caps->get_info != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Status:\t\t%c\n", caps->get_status != NULL ? 'Y' : 'N');

    fprintf(fout, "Can set Func:\t%c\n", caps->set_func != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Func:\t%c\n", caps->get_func != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Level:\t%c\n", caps->set_level != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Level:\t%c\n", caps->get_level != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Param:\t%c\n", caps->set_parm != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Param:\t%c\n", caps->get_parm != NULL ? 'Y' : 'N');

    fprintf(fout, "Can perform Amp Ops:\t%c\n", caps->amp_op != NULL ? 'Y' : 'N');

    fprintf(fout, "Can set Frequency:\t%c\n", caps->set_freq != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Frequency:\t%c\n", caps->get_freq != NULL ? 'Y' : 'N');

    fprintf(fout, "Can set Ant:\t%c\n", caps->set_ant != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Ant:\t%c\n", caps->get_ant != NULL ? 'Y' : 'N');

    fprintf(fout, "Can set Input:\t%c\n", caps->set_input != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Input:\t%c\n", caps->get_input != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set Power Stat:\t%c\n",
            caps->set_powerstat != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get Power Stat:\t%c\n",
            caps->get_powerstat != NULL ? 'Y' : 'N');

    fprintf(fout, "\nOverall backend warnings: %d %c %s\n", backend_warnings,
            warnbuf[0] != 0 ? '=' : ' ', warnbuf);

    return backend_warnings;
}
