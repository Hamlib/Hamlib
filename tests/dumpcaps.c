/*
 * dumpcaps.c - Copyright (C) 2000-2012 Stephane Fillod
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>
#include "misc.h"

#include "sprintflst.h"
#include "rigctl_parse.h"

static int print_ext(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr);
void range_print(FILE *fout, const struct freq_range_list range_list[], int rx);
int range_sanity_check(const struct freq_range_list range_list[], int rx);
int ts_sanity_check(const struct tuning_step_list tuning_step[]);
static void dump_chan_caps(const channel_cap_t *chan, FILE *fout);


/*
 * the rig may be in rig_init state, but not opened
 */
int dumpcaps(RIG *rig, FILE *fout)
{
    const struct rig_caps *caps;
    int status, i;
    int can_esplit, can_echannel;
    char freqbuf[20];
    int backend_warnings = 0;
    static char prntbuf[1024];  /* a malloc would be better.. */
    char *label1, *label2, *label3, *label4, *label5;
    char *labelrx1; // , *labelrx2, *labelrx3, *labelrx4, *labelrx5;

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    fprintf(fout, "Caps dump for model: %u\n", caps->rig_model);
    fprintf(fout, "Model name:\t%s\n", caps->model_name);
    fprintf(fout, "Mfg name:\t%s\n", caps->mfg_name);
    fprintf(fout, "Backend version:\t%s\n", caps->version);
    fprintf(fout, "Backend copyright:\t%s\n", caps->copyright);
    fprintf(fout, "Backend status:\t%s\n", rig_strstatus(caps->status));
    fprintf(fout, "Rig type:\t");

    switch (caps->rig_type & RIG_TYPE_MASK)
    {
    case RIG_TYPE_TRANSCEIVER:
        fprintf(fout, "Transceiver\n");
        break;

    case RIG_TYPE_HANDHELD:
        fprintf(fout, "Handheld\n");
        break;

    case RIG_TYPE_MOBILE:
        fprintf(fout, "Mobile\n");
        break;

    case RIG_TYPE_RECEIVER:
        fprintf(fout, "Receiver\n");
        break;

    case RIG_TYPE_PCRECEIVER:
        fprintf(fout, "PC Receiver\n");
        break;

    case RIG_TYPE_SCANNER:
        fprintf(fout, "Scanner\n");
        break;

    case RIG_TYPE_TRUNKSCANNER:
        fprintf(fout, "Trunking scanner\n");
        break;

    case RIG_TYPE_COMPUTER:
        fprintf(fout, "Computer\n");
        break;

    case RIG_TYPE_TUNER:
        fprintf(fout, "Tuner\n");
        break;

    case RIG_TYPE_OTHER:
        fprintf(fout, "Other\n");
        break;

    default:
        fprintf(fout, "Unknown\n");
        backend_warnings++;
    }

    fprintf(fout, "PTT type:\t");

    switch (caps->ptt_type)
    {
    case RIG_PTT_RIG:
        fprintf(fout, "Rig capable\n");
        break;

    case RIG_PTT_RIG_MICDATA:
        fprintf(fout, "Rig capable (Mic/Data)\n");
        break;

    case RIG_PTT_PARALLEL:
        fprintf(fout, "Parallel port (DATA0)\n");
        break;

    case RIG_PTT_SERIAL_RTS:
        fprintf(fout, "Serial port (CTS/RTS)\n");
        break;

    case RIG_PTT_SERIAL_DTR:
        fprintf(fout, "Serial port (DTR/DSR)\n");
        break;

    case RIG_PTT_NONE:
        fprintf(fout, "None\n");
        break;

    default:
        fprintf(fout, "Unknown\n");
        backend_warnings++;
    }

    fprintf(fout, "DCD type:\t");

    switch (caps->dcd_type)
    {
    case RIG_DCD_RIG:
        fprintf(fout, "Rig capable\n");
        break;

    case RIG_DCD_PARALLEL:
        fprintf(fout, "Parallel port (/STROBE)\n");
        break;

    case RIG_DCD_SERIAL_CTS:
        fprintf(fout, "Serial port (CTS/RTS)\n");
        break;

    case RIG_DCD_SERIAL_DSR:
        fprintf(fout, "Serial port (DTR/DSR)\n");
        break;

    case RIG_DCD_SERIAL_CAR:
        fprintf(fout, "Serial port (CD)\n");
        break;

    case RIG_DCD_NONE:
        fprintf(fout, "None\n");
        break;

    default:
        fprintf(fout, "Unknown\n");
        backend_warnings++;
    }

    fprintf(fout, "Port type:\t");

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        fprintf(fout, "RS-232\n");
        fprintf(fout,
                "Serial speed: %d..%d bauds, %d%c%d %s\n",
                caps->serial_rate_min,
                caps->serial_rate_max,
                caps->serial_data_bits,
                caps->serial_parity == RIG_PARITY_NONE ? 'N' :
                caps->serial_parity == RIG_PARITY_ODD ? 'O' :
                caps->serial_parity == RIG_PARITY_EVEN ? 'E' :
                caps->serial_parity == RIG_PARITY_MARK ? 'M' : 'S',
                caps->serial_stop_bits,
                caps->serial_handshake == RIG_HANDSHAKE_NONE ? "" :
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
            "Write delay: %dmS, timeout %dmS, %d retry\n",
            caps->write_delay, caps->timeout, caps->retry);
    fprintf(fout,
            "Post Write delay: %dmS\n",
            caps->post_write_delay);

    fprintf(fout,
            "Has targetable VFO: %s\n",
            caps->targetable_vfo ? "Y" : "N");

    fprintf(fout,
            "Has transceive: %s\n",
            caps->transceive ? "Y" : "N");

    fprintf(fout, "Announce: 0x%x\n", caps->announces);
    fprintf(fout,
            "Max RIT: -%ld.%ldkHz/+%ld.%ldkHz\n",
            caps->max_rit / 1000, caps->max_rit % 1000,
            caps->max_rit / 1000, caps->max_rit % 1000);

    fprintf(fout,
            "Max XIT: -%ld.%ldkHz/+%ld.%ldkHz\n",
            caps->max_xit / 1000, caps->max_xit % 1000,
            caps->max_xit / 1000, caps->max_xit % 1000);

    fprintf(fout,
            "Max IF-SHIFT: -%ld.%ldkHz/+%ld.%ldkHz\n",
            caps->max_ifshift / 1000, caps->max_ifshift % 1000,
            caps->max_ifshift / 1000, caps->max_ifshift % 1000);

    fprintf(fout, "Preamp:");

    for (i = 0; i < MAXDBLSTSIZ && caps->preamp[i] != 0; i++)
    {
        fprintf(fout, " %ddB", caps->preamp[i]);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }

    fprintf(fout, "\n");
    fprintf(fout, "Attenuator:");

    for (i = 0; i < MAXDBLSTSIZ && caps->attenuator[i] != 0; i++)
    {
        fprintf(fout, " %ddB", caps->attenuator[i]);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }

    fprintf(fout, "\n");

    fprintf(fout, "CTCSS:");

    for (i = 0; caps->ctcss_list && i < 60 && caps->ctcss_list[i] != 0; i++)
    {
        fprintf(fout,
                " %u.%1u",
                caps->ctcss_list[i] / 10, caps->ctcss_list[i] % 10);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }
    else
    {
        fprintf(fout, " Hz, %d tones", i);
    }

    fprintf(fout, "\n");

    fprintf(fout, "DCS:");

    for (i = 0; caps->dcs_list && i < 128 && caps->dcs_list[i] != 0; i++)
    {
        fprintf(fout, " %u", caps->dcs_list[i]);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }
    else
    {
        fprintf(fout, ", %d codes", i);
    }

    fprintf(fout, "\n");

    sprintf_func(prntbuf, caps->has_get_func);
    fprintf(fout, "Get functions: %s\n", prntbuf);

    sprintf_func(prntbuf, caps->has_set_func);
    fprintf(fout, "Set functions: %s\n", prntbuf);

    fprintf(fout, "Extra functions:\n");
    rig_ext_func_foreach(rig, print_ext, fout);

    sprintf_level_gran(prntbuf, caps->has_get_level, caps->level_gran);
    fprintf(fout, "Get level: %s\n", prntbuf);

    if ((caps->has_get_level & RIG_LEVEL_SQLSTAT))
    {
        fprintf(fout, "Warning--backend uses deprecated SQLSTAT level!\n");
        backend_warnings++;
    }

    if ((caps->has_get_level & RIG_LEVEL_RAWSTR)
            && caps->str_cal.size == 0
            && !(caps->has_get_level & RIG_LEVEL_STRENGTH))
    {

        fprintf(fout,
                "Warning--backend has get RAWSTR, but not calibration data\n");
        backend_warnings++;
    }

    sprintf_level_gran(prntbuf, caps->has_set_level, caps->level_gran);
    fprintf(fout, "Set level: %s\n", prntbuf);

    if (caps->has_set_level & RIG_LEVEL_READONLY_LIST)
    {
        fprintf(fout, "Warning--backend can set readonly levels!\n");
        backend_warnings++;
    }

    fprintf(fout, "Extra levels:\n");
    rig_ext_level_foreach(rig, print_ext, fout);

    sprintf_parm_gran(prntbuf, caps->has_get_parm, caps->parm_gran);
    fprintf(fout, "Get parameters: %s\n", prntbuf);

    sprintf_parm_gran(prntbuf, caps->has_set_parm, caps->parm_gran);
    fprintf(fout, "Set parameters: %s\n", prntbuf);

    if (caps->has_set_parm & RIG_PARM_READONLY_LIST)
    {
        fprintf(fout, "Warning--backend can set readonly parms!\n");
        backend_warnings++;
    }

    fprintf(fout, "Extra parameters:\n");
    rig_ext_parm_foreach(rig, print_ext, fout);


    if (rig->state.mode_list != 0)
    {
        sprintf_mode(prntbuf, rig->state.mode_list);
    }
    else
    {
        strcpy(prntbuf, "None. This backend might be bogus!\n");
        backend_warnings++;
    }

    fprintf(fout, "Mode list: %s\n", prntbuf);

    if (rig->state.vfo_list != 0)
    {
        sprintf_vfo(prntbuf, rig->state.vfo_list);
    }
    else
    {
        strcpy(prntbuf, "None. This backend might be bogus!\n");
        backend_warnings++;
    }

    fprintf(fout, "VFO list: %s\n", prntbuf);

    sprintf_vfop(prntbuf, caps->vfo_ops);
    fprintf(fout, "VFO Ops: %s\n", prntbuf);

    sprintf_scan(prntbuf, caps->scan_ops);
    fprintf(fout, "Scan Ops: %s\n", prntbuf);

    fprintf(fout, "Number of banks:\t%d\n", caps->bank_qty);
    fprintf(fout, "Memory name desc size:\t%d\n", caps->chan_desc_sz);

    fprintf(fout, "Memories:");

    for (i = 0; i < CHANLSTSIZ && caps->chan_list[i].type; i++)
    {
        fprintf(fout,
                "\n\t%d..%d:   \t%s",
                caps->chan_list[i].startc,
                caps->chan_list[i].endc,
                rig_strmtype(caps->chan_list[i].type));
        fprintf(fout, "\n\t  Mem caps: ");
        dump_chan_caps(&caps->chan_list[i].mem_caps, fout);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }

    fprintf(fout, "\n");

    label1 = caps->tx_range_list1->label;
    label1 = label1 == NULL ? "TBD" : label1;
    fprintf(fout, "TX ranges #1 for %s:\n", label1);
    range_print(fout, caps->tx_range_list1, 0);

    labelrx1 = caps->rx_range_list1->label;
    labelrx1 = labelrx1 == NULL ? "TBD" : labelrx1;
    fprintf(fout, "RX ranges #1 for %s:\n", labelrx1);
    range_print(fout, caps->rx_range_list1, 1);

    label2 = caps->rx_range_list2->label;
    label2 = label2 == NULL ? "TBD" : label2;
    fprintf(fout, "TX ranges #2 for %s:\n", label2);
    range_print(fout, caps->tx_range_list2, 0);

    label2 = caps->rx_range_list2->label;
    label2 = label2 == NULL ? "TBD" : label2;
    fprintf(fout, "RX ranges #2 for %s:\n", label2);
    range_print(fout, caps->rx_range_list2, 1);

    label3 = caps->rx_range_list3->label;
    label3 = label3 == NULL ? "TBD" : label3;
    fprintf(fout, "TX ranges #3 for %s:\n", label3);
    range_print(fout, caps->tx_range_list3, 0);

    label3 = caps->rx_range_list3->label;
    label3 = label3 == NULL ? "TBD" : label3;
    fprintf(fout, "RX ranges #3 for %s:\n", label3);
    range_print(fout, caps->rx_range_list3, 1);

    label4 = caps->rx_range_list4->label;
    label4 = label4 == NULL ? "TBD" : label4;
    fprintf(fout, "TX ranges #4 for %s:\n", label4);
    range_print(fout, caps->tx_range_list5, 0);

    label4 = caps->rx_range_list4->label;
    label4 = label4 == NULL ? "TBD" : label4;
    fprintf(fout, "RX ranges #4 for %s:\n", label4);
    range_print(fout, caps->rx_range_list5, 1);

    label5 = caps->rx_range_list5->label;
    label5 = label5 == NULL ? "TBD" : label5;
    fprintf(fout, "TX ranges #5 for %s:\n", label5);
    range_print(fout, caps->tx_range_list5, 0);

    label5 = caps->rx_range_list5->label;
    label5 = label5 == NULL ? "TBD" : label5;
    fprintf(fout, "RX ranges #5 for %s:\n", label5);
    range_print(fout, caps->rx_range_list5, 1);

    status = range_sanity_check(caps->tx_range_list1, 0);
    fprintf(fout,
            "TX ranges #1 status for %s:\t%s (%d)\n", label1,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->rx_range_list1, 1);
    fprintf(fout,
            "RX ranges #1 status for %s:\t%s (%d)\n", labelrx1,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->tx_range_list2, 0);
    fprintf(fout,
            "TX ranges #2 status for %s:\t%s (%d)\n", label2,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->rx_range_list2, 1);
    fprintf(fout,
            "RX ranges #2 status for %s:\t%s (%d)\n", label2,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->tx_range_list3, 0);
    fprintf(fout,
            "TX ranges #3 status for %s:\t%s (%d)\n", label3,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->rx_range_list3, 1);
    fprintf(fout,
            "RX ranges #3 status for %s:\t%s (%d)\n", label3,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->tx_range_list4, 0);
    fprintf(fout,
            "TX ranges #4 status for %s:\t%s (%d)\n", label4,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->rx_range_list4, 1);
    fprintf(fout,
            "RX ranges #4 status for %s:\t%s (%d)\n", label4,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->tx_range_list5, 0);
    fprintf(fout,
            "TX ranges #5 status for %s:\t%s (%d)\n", label5,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    status = range_sanity_check(caps->rx_range_list5, 1);
    fprintf(fout,
            "RX ranges #5 status for %s:\t%s (%d)\n", label5,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        backend_warnings++;
    }

    fprintf(fout, "Tuning steps:");

    for (i = 0; i < TSLSTSIZ && !RIG_IS_TS_END(caps->tuning_steps[i]); i++)
    {
        if (caps->tuning_steps[i].ts == RIG_TS_ANY)
        {
            strcpy(freqbuf, "ANY");    /* strcpy!  Looks safe for now */
        }
        else
        {
            sprintf_freq(freqbuf, caps->tuning_steps[i].ts);
        }

        sprintf_mode(prntbuf, caps->tuning_steps[i].modes);
        fprintf(fout, "\n\t%s:   \t%s", freqbuf, prntbuf);
    }

    if (i == 0)
    {
        fprintf(fout, " None! This backend might be bogus!");
        backend_warnings++;
    }

    fprintf(fout, "\n");
    status = ts_sanity_check(caps->tuning_steps);
    fprintf(fout, "Tuning steps status:\t%s (%d)\n", status ? "Bad" : "OK", status);

    if (status)
    {
        backend_warnings++;
    }

    fprintf(fout, "Filters:");

    for (i = 0; i < FLTLSTSIZ && !RIG_IS_FLT_END(caps->filters[i]); i++)
    {
        if (caps->filters[i].width == RIG_FLT_ANY)
        {
            strcpy(freqbuf, "ANY");
        }
        else
        {
            sprintf_freq(freqbuf, caps->filters[i].width);
        }

        sprintf_mode(prntbuf, caps->filters[i].modes);
        fprintf(fout, "\n\t%s:   \t%s", freqbuf, prntbuf);
    }

    if (i == 0)
    {
        fprintf(fout, " None. This backend might be bogus!");
        backend_warnings++;
    }

    fprintf(fout, "\n");

    fprintf(fout, "Bandwidths:");

    for (i = 1; i < RIG_MODE_TESTS_MAX; i <<= 1)
    {
        pbwidth_t pbnorm = rig_passband_normal(rig, i);

        if (pbnorm == 0)
        {
            continue;
        }

        sprintf_freq(freqbuf, pbnorm);
        fprintf(fout, "\n\t%s\tNormal: %s,\t", rig_strrmode(i), freqbuf);

        sprintf_freq(freqbuf, rig_passband_narrow(rig, i));
        fprintf(fout, "Narrow: %s,\t", freqbuf);

        sprintf_freq(freqbuf, rig_passband_wide(rig, i));
        fprintf(fout, "Wide: %s", freqbuf);
    }

    fprintf(fout, "\n");

    fprintf(fout, "Has priv data:\t%c\n", caps->priv != NULL ? 'Y' : 'N');
    /*
     * Status is either 'Y'es, 'E'mulated, 'N'o
     *
     * TODO: keep me up-to-date with API call list!
     */
    fprintf(fout, "Has Init:\t%c\n", caps->rig_init != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Cleanup:\t%c\n", caps->rig_cleanup != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Open:\t%c\n", caps->rig_open != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Close:\t%c\n", caps->rig_close != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Conf:\t%c\n", caps->set_conf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Conf:\t%c\n", caps->get_conf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Frequency:\t%c\n", caps->set_freq != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Frequency:\t%c\n", caps->get_freq != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Mode:\t%c\n", caps->set_mode != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Mode:\t%c\n", caps->get_mode != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set VFO:\t%c\n", caps->set_vfo != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get VFO:\t%c\n", caps->get_vfo != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set PTT:\t%c\n", caps->set_ptt != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get PTT:\t%c\n", caps->get_ptt != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get DCD:\t%c\n", caps->get_dcd != NULL ? 'Y' : 'N');
    fprintf(fout,
            "Can set Repeater Duplex:\t%c\n",
            caps->set_rptr_shift != NULL ? 'Y' : 'N');
    fprintf(fout,
            "Can get Repeater Duplex:\t%c\n",
            caps->get_rptr_shift != NULL ? 'Y' : 'N');
    fprintf(fout,
            "Can set Repeater Offset:\t%c\n",
            caps->set_rptr_offs != NULL ? 'Y' : 'N');
    fprintf(fout,
            "Can get Repeater Offset:\t%c\n",
            caps->get_rptr_offs != NULL ? 'Y' : 'N');

    can_esplit = caps->set_split_vfo
                 && (caps->set_vfo
                     || (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && caps->vfo_op));

    fprintf(fout,
            "Can set Split Freq:\t%c\n",
            caps->set_split_freq != NULL ? 'Y' : (can_esplit
                    && caps->set_freq ? 'E' : 'N'));

    fprintf(fout,
            "Can get Split Freq:\t%c\n",
            caps->get_split_freq != NULL ? 'Y' : (can_esplit
                    && caps->get_freq ? 'E' : 'N'));

    fprintf(fout,
            "Can set Split Mode:\t%c\n",
            caps->set_split_mode != NULL ? 'Y' : (can_esplit
                    && caps->set_mode ? 'E' : 'N'));

    fprintf(fout,
            "Can get Split Mode:\t%c\n",
            caps->get_split_mode != NULL ? 'Y' : (can_esplit
                    && caps->get_mode ? 'E' : 'N'));

    fprintf(fout,
            "Can set Split VFO:\t%c\n",
            caps->set_split_vfo != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get Split VFO:\t%c\n",
            caps->get_split_vfo != NULL ? 'Y' : 'N');

    fprintf(fout, "Can set Tuning Step:\t%c\n", caps->set_ts != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Tuning Step:\t%c\n", caps->get_ts != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set RIT:\t%c\n", caps->set_rit != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get RIT:\t%c\n", caps->get_rit != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set XIT:\t%c\n", caps->set_xit != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get XIT:\t%c\n", caps->get_xit != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set CTCSS:\t%c\n", caps->set_ctcss_tone != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get CTCSS:\t%c\n", caps->get_ctcss_tone != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set DCS:\t%c\n", caps->set_dcs_code != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get DCS:\t%c\n", caps->get_dcs_code != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set CTCSS Squelch:\t%c\n",
            caps->set_ctcss_sql != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get CTCSS Squelch:\t%c\n",
            caps->get_ctcss_sql != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set DCS Squelch:\t%c\n",
            caps->set_dcs_sql != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get DCS Squelch:\t%c\n",
            caps->get_dcs_sql != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set Power Stat:\t%c\n",
            caps->set_powerstat != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get Power Stat:\t%c\n",
            caps->get_powerstat != NULL ? 'Y' : 'N');

    fprintf(fout, "Can Reset:\t%c\n", caps->reset != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Ant:\t%c\n", caps->get_ant != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Ant:\t%c\n", caps->set_ant != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set Transceive:\t%c\n",
            caps->set_trn != NULL ? 'Y' : caps->transceive == RIG_TRN_RIG ? 'E' : 'N');

    fprintf(fout, "Can get Transceive:\t%c\n", caps->get_trn != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Func:\t%c\n", caps->set_func != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Func:\t%c\n", caps->get_func != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Level:\t%c\n", caps->set_level != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Level:\t%c\n", caps->get_level != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Param:\t%c\n", caps->set_parm != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Param:\t%c\n", caps->get_parm != NULL ? 'Y' : 'N');
    fprintf(fout, "Can send DTMF:\t%c\n", caps->send_dtmf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can recv DTMF:\t%c\n", caps->recv_dtmf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can send Morse:\t%c\n", caps->send_morse != NULL ? 'Y' : 'N');
    fprintf(fout, "Can send Voice:\t%c\n",
            caps->send_voice_mem != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can decode Events:\t%c\n",
            caps->decode_event != NULL ? 'Y' : 'N');

    fprintf(fout, "Can set Bank:\t%c\n", caps->set_bank != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Mem:\t%c\n", caps->set_mem != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Mem:\t%c\n", caps->get_mem != NULL ? 'Y' : 'N');

    can_echannel = caps->set_mem
                   && ((caps->set_vfo
                        && ((rig->state.vfo_list & RIG_VFO_MEM) == RIG_VFO_MEM))
                       || (caps->vfo_op
                           && rig_has_vfo_op(rig, RIG_OP_TO_VFO | RIG_OP_FROM_VFO)));

    fprintf(fout,
            "Can set Channel:\t%c\n",
            caps->set_channel != NULL ? 'Y' : (can_echannel ? 'E' : 'N'));

    fprintf(fout,
            "Can get Channel:\t%c\n",
            caps->get_channel != NULL ? 'Y' : (can_echannel ? 'E' : 'N'));

    fprintf(fout, "Can ctl Mem/VFO:\t%c\n", caps->vfo_op != NULL ? 'Y' : 'N');
    fprintf(fout, "Can Scan:\t%c\n", caps->scan != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Info:\t%c\n", caps->get_info != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get power2mW:\t%c\n", caps->power2mW != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get mW2power:\t%c\n", caps->mW2power != NULL ? 'Y' : 'N');

    fprintf(fout, "\nOverall backend warnings: %d\n", backend_warnings);

    return backend_warnings;
}

static int print_ext(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr)
{
    int i;
    fprintf((FILE *)ptr, "\t%s\n", cfp->name);
    fprintf((FILE *)ptr, "\t\tType: %s\n", get_rig_conf_type(cfp->type));
    fprintf((FILE *)ptr, "\t\tDefault: %s\n", cfp->dflt != NULL ? cfp->dflt : "");
    fprintf((FILE *)ptr, "\t\tLabel: %s\n", cfp->label != NULL ? cfp->label : "");
    fprintf((FILE *)ptr, "\t\tTooltip: %s\n",
            cfp->tooltip != NULL ? cfp->tooltip : "");

    switch (cfp->type)
    {
    case RIG_CONF_NUMERIC:
        fprintf((FILE *)ptr, "\t\tRange: %g..%g/%g\n", cfp->u.n.min, cfp->u.n.max,
                cfp->u.n.step);
        break;

    case RIG_CONF_COMBO:
        fprintf((FILE *)ptr, "\t\tValues:");

        for (i = 0; i < RIG_COMBO_MAX && cfp->u.c.combostr[i] != NULL; i++)
        {
            fprintf((FILE *)ptr, " %d=\"%s\"", i, cfp->u.c.combostr[i]);
        }

        fprintf((FILE *)ptr, "\n");
        break;

    default:
        break;
    }

    return 1;       /* process them all */
}

void range_print(FILE *fout, const struct freq_range_list range_list[], int rx)
{
    int i;
    char prntbuf[1024];  /* a malloc would be better.. */

    for (i = 0; i < FRQRANGESIZ; i++)
    {
        if (range_list[i].startf == 0 && range_list[i].endf == 0)
        {
            break;
        }

        fprintf(fout, "\t%.0f Hz - %.0f Hz\n", range_list[i].startf,
                range_list[i].endf);

        fprintf(fout, "\t\tVFO list: ");
        sprintf_vfo(prntbuf, range_list[i].vfo);
        fprintf(fout, "%s", prntbuf);
        fprintf(fout, "\n");

        fprintf(fout, "\t\tMode list: ");
        sprintf_mode(prntbuf, range_list[i].modes);
        fprintf(fout, "%s", prntbuf);
        fprintf(fout, "\n");

        fprintf(fout, "\t\tAntenna list: ");
        sprintf_ant(prntbuf, range_list[i].ant);
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
int range_sanity_check(const struct freq_range_list range_list[], int rx)
{
    int i;

    for (i = 0; i < FRQRANGESIZ; i++)
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
            if (!(range_list[i].low_power > 0 && range_list[i].high_power > 0))
            {
                return -3;
            }

            if (range_list[i].low_power > range_list[i].high_power)
            {
                return -3;
            }
        }
    }

    if (i == FRQRANGESIZ)
    {
        return -4;
    }

    return 0;
}

/*
 * check for:
 * - steps sorted in ascending order return_code=-1
 * - modes are not 0        return_code=-2
 * - array is ended by a {0,0,0,0,0} element (before boundary) rc=-4
 *
 * TODO: array is sorted in ascending freq order
 */
int ts_sanity_check(const struct tuning_step_list tuning_step[])
{
    int i;
    shortfreq_t last_ts;
    rmode_t last_modes;

    last_ts = 0;
    last_modes = RIG_MODE_NONE;

    for (i = 0; i < TSLSTSIZ; i++)
    {
        if (RIG_IS_TS_END(tuning_step[i]))
        {
            break;
        }

        if (tuning_step[i].ts != RIG_TS_ANY
                && tuning_step[i].ts < last_ts
                && last_modes == tuning_step[i].modes)
        {

            return -1;
        }

        if (tuning_step[i].modes == 0)
        {
            return -2;
        }

        last_ts = tuning_step[i].ts;
        last_modes = tuning_step[i].modes;
    }

    if (i == TSLSTSIZ)
    {
        return -4;
    }

    return 0;
}


static void dump_chan_caps(const channel_cap_t *chan, FILE *fout)
{
    if (chan->bank_num)
    {
        fprintf(fout, "BANK ");
    }

    if (chan->ant)
    {
        fprintf(fout, "ANT ");
    }

    if (chan->freq)
    {
        fprintf(fout, "FREQ ");
    }

    if (chan->mode)
    {
        fprintf(fout, "MODE ");
    }

    if (chan->width)
    {
        fprintf(fout, "WIDTH ");
    }

    if (chan->tx_freq)
    {
        fprintf(fout, "TXFREQ ");
    }

    if (chan->tx_mode)
    {
        fprintf(fout, "TXMODE ");
    }

    if (chan->tx_width)
    {
        fprintf(fout, "TXWIDTH ");
    }

    if (chan->split)
    {
        fprintf(fout, "SPLIT ");
    }

    if (chan->rptr_shift)
    {
        fprintf(fout, "RPTRSHIFT ");
    }

    if (chan->rptr_offs)
    {
        fprintf(fout, "RPTROFS ");
    }

    if (chan->tuning_step)
    {
        fprintf(fout, "TS ");
    }

    if (chan->rit)
    {
        fprintf(fout, "RIT ");
    }

    if (chan->xit)
    {
        fprintf(fout, "XIT ");
    }

    if (chan->funcs)
    {
        fprintf(fout, "FUNC ");    /* TODO: iterate over the list */
    }

    if (chan->levels)
    {
        fprintf(fout, "LEVEL ");    /* TODO: iterate over the list */
    }

    if (chan->ctcss_tone)
    {
        fprintf(fout, "TONE ");
    }

    if (chan->ctcss_sql)
    {
        fprintf(fout, "CTCSS ");
    }

    if (chan->dcs_code)
    {
        fprintf(fout, "DCSCODE ");
    }

    if (chan->dcs_sql)
    {
        fprintf(fout, "DCSSQL ");
    }

    if (chan->scan_group)
    {
        fprintf(fout, "SCANGRP ");
    }

    if (chan->flags)
    {
        fprintf(fout, "FLAG ");    /* TODO: iterate over the RIG_CHFLAG's */
    }

    if (chan->channel_desc)
    {
        fprintf(fout, "NAME ");
    }

    if (chan->ext_levels)
    {
        fprintf(fout, "EXTLVL ");
    }
}


int dumpconf(RIG *rig, FILE *fout)
{
    rig_token_foreach(rig, print_conf_list, (rig_ptr_t)rig);

    return 0;
}
