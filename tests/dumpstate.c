/*
 * dumpstate.c - Copyright (C) 2000-2012 Stephane Fillod
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

#include <stdio.h>
#include <string.h>

#include <hamlib/rig.h>
#include "misc.h"
#include "riglist.h"
#include "sprintflst.h"
#include "rigctl_parse.h"
#include "../rigs/icom/icom.h"

void range_print(FILE *fout, const struct freq_range_list range_list[], int rx);
int range_sanity_check(const struct freq_range_list range_list[], int rx);
int ts_sanity_check(const struct tuning_step_list tuning_step[]);
static void dump_chan_caps(const channel_cap_t *chan, FILE *fout);

struct rig_type_s
{
    int type;
    char *description;
};

static struct rig_type_s rig_type[] =
{
    {RIG_TYPE_OTHER, "Other"},
    {RIG_FLAG_RECEIVER, "Receiver"},
    {RIG_FLAG_TRANSMITTER, "Transmitter"},
    {RIG_FLAG_SCANNER, "Scanner"},
    {RIG_FLAG_MOBILE, "Mobile"},
    {RIG_FLAG_HANDHELD, "Handheld"},
    {RIG_FLAG_COMPUTER, "Computer"},
    {RIG_FLAG_TRANSCEIVER, "Transceiver"},
    {RIG_FLAG_TRUNKING, "Trunking scanner"},
    {RIG_FLAG_APRS, "APRS"},
    {RIG_FLAG_TNC, "TNC"},
    {RIG_FLAG_DXCLUSTER, "DxCluster"},
    {RIG_FLAG_DXCLUSTER, "DxCluster"},
    {RIG_FLAG_TUNER, "Tuner"},
    {-1, "?\n"}
};

static int print_ext(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr)
{
    return print_ext_param(cfp, ptr);
}

/*
 * the rig may be in rig_init state, but not opened
 */
int dumpstate(RIG *rig, FILE *fout)
{
    const struct rig_caps *caps;
    struct rig_state *rs = STATE(rig);
    int status, i;
    int can_esplit, can_echannel;
    char freqbuf[20];
    int backend_warnings = 0;
    char warnbuf[4096];
    char prntbuf[2048];  /* a malloc would be better.. */
    char *label1, *label2, *label3, *label4, *label5;
    char *labelrx1; // , *labelrx2, *labelrx3, *labelrx4, *labelrx5;

    warnbuf[0] = 0;
    prntbuf[0] = 0;

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    fprintf(fout, "Caps dump for model: %u\n", rs->rig_model);
    fprintf(fout, "Model name:\t%s\n", rs->model_name);
    fprintf(fout, "Mfg name:\t%s\n", rs->mfg_name);
    fprintf(fout, "Hamlib version:\t%s\n", hamlib_version2);
    fprintf(fout, "Backend version:\t%s\n", rs->version);
    fprintf(fout, "Backend copyright:\t%s\n", rs->copyright);
    fprintf(fout, "Backend status:\t%s\n", rig_strstatus(rs->status));
    fprintf(fout, "Rig type:\t");

    char *unknown = "Unknown";

    for (i = 0; rig_type[i].type != -1; ++i)
    {
        if ((rig_type[i].type & rs->rig_type) == rig_type[i].type)
        {
            fprintf(fout, "%s ", rig_type[i].description);
            unknown = "";
        }
    }

    fprintf(fout, "%s\n", unknown);

    if (strlen(unknown) > 0)
    {
        strcat(warnbuf, " RIG_TYPE");
        backend_warnings++;
    }

    fprintf(fout, "PTT type:\t");

    switch (rs->ptt_type)
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
        strcat(warnbuf, " PTT_TYPE");
        backend_warnings++;
    }

    fprintf(fout, "DCD type:\t");

    switch (rs->dcd_type)
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
        strcat(warnbuf, " DCD_TYPE");
        backend_warnings++;
    }

    fprintf(fout, "Port type:\t");

    switch (rs->port_type)
    {
    case RIG_PORT_SERIAL:
        fprintf(fout, "RS-232\n");
        fprintf(fout,
                "Serial speed: %d..%d baud, %d%c%d, ctrl=%s\n",
                rs->serial_rate_min,
                rs->serial_rate_max,
                rs->serial_data_bits,
                rs->serial_parity == RIG_PARITY_NONE ? 'N' :
                rs->serial_parity == RIG_PARITY_ODD ? 'O' :
                rs->serial_parity == RIG_PARITY_EVEN ? 'E' :
                rs->serial_parity == RIG_PARITY_MARK ? 'M' : 'S',
                rs->serial_stop_bits,
                rs->serial_handshake == RIG_HANDSHAKE_NONE ? "NONE" :
                (rs->serial_handshake == RIG_HANDSHAKE_XONXOFF ? "XONXOFF" : "CTS/RTS")
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
        strcat(warnbuf, " PORT_TYPE");
        backend_warnings++;
    }

    fprintf(fout,
            "Write delay: %dms, timeout %dms, %d retry\n",
            rs->write_delay, rs->timeout, rs->retry);
    fprintf(fout,
            "Post write delay: %dms\n",
            rs->post_write_delay);

    fprintf(fout,
            "Has targetable VFO: %s\n",
            rs->targetable_vfo ? "Y" : "N");

    fprintf(fout,
            "Has async data support: %s\n",
            rs->async_data_supported ? "Y" : "N");

    fprintf(fout, "Announce: 0x%x\n", rs->announces);
    fprintf(fout,
            "Max RIT: -%ld.%ldkHz/+%ld.%ldkHz\n",
            rs->max_rit / 1000, rs->max_rit % 1000,
            rs->max_rit / 1000, rs->max_rit % 1000);

    fprintf(fout,
            "Max XIT: -%ld.%ldkHz/+%ld.%ldkHz\n",
            rs->max_xit / 1000, rs->max_xit % 1000,
            rs->max_xit / 1000, rs->max_xit % 1000);

    fprintf(fout,
            "Max IF-SHIFT: -%ld.%ldkHz/+%ld.%ldkHz\n",
            rs->max_ifshift / 1000, rs->max_ifshift % 1000,
            rs->max_ifshift / 1000, rs->max_ifshift % 1000);

    fprintf(fout, "Preamp:");

    for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rs->preamp[i] != 0; i++)
    {
        fprintf(fout, " %ddB", rs->preamp[i]);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }

    fprintf(fout, "\n");
    fprintf(fout, "Attenuator:");

    for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rs->attenuator[i] != 0; i++)
    {
        fprintf(fout, " %ddB", rs->attenuator[i]);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }

    fprintf(fout, "\n");


    fprintf(fout, "AGC levels:");
    const struct icom_priv_caps *priv_caps =
        (const struct icom_priv_caps *) rs->priv;

    if (priv_caps && RIG_BACKEND_NUM(rs->rig_model) == RIG_ICOM
            && priv_caps->agc_levels_present)
    {
        for (i = 0; i < HAMLIB_MAX_AGC_LEVELS
                && priv_caps->agc_levels[i].level != RIG_AGC_LAST; i++)
        {
            fprintf(fout, " %d=%s", priv_caps->agc_levels[i].level,
                    rig_stragclevel(priv_caps->agc_levels[i].level));
        }
    }
    else
    {
        for (i = 0; i < HAMLIB_MAX_AGC_LEVELS && i < rs->agc_level_count; i++)
        {
            fprintf(fout, " %d=%s", rs->agc_levels[i],
                    rig_stragclevel(rs->agc_levels[i]));
        }
    }

    if (i == 0)
    {
        fprintf(fout, " %d=%s", RIG_AGC_NONE, rig_stragclevel(RIG_AGC_NONE));
    }

    fprintf(fout, "\n");

    fprintf(fout, "CTCSS:");

    for (i = 0; rs->ctcss_list && i < 60
            && rs->ctcss_list[i] != 0; i++)
    {
        fprintf(fout,
                " %u.%1u",
                rs->ctcss_list[i] / 10, rs->ctcss_list[i] % 10);
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

    for (i = 0; rs->dcs_list && i < 128 && rs->dcs_list[i] != 0; i++)
    {
        fprintf(fout, " %u", rs->dcs_list[i]);
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

    rig_sprintf_func(prntbuf, sizeof(prntbuf), rs->has_get_func);
    fprintf(fout, "Get functions: %s\n", prntbuf);

    rig_sprintf_func(prntbuf, sizeof(prntbuf), rs->has_set_func);
    fprintf(fout, "Set functions: %s\n", prntbuf);

    fprintf(fout, "Extra functions:\n");
    rig_ext_func_foreach(rig, print_ext, fout);

    rig_sprintf_level_gran(prntbuf, sizeof(prntbuf), rs->has_get_level,
                           rs->level_gran);
    fprintf(fout, "Get level: %s\n", prntbuf);

    if ((rs->has_get_level & RIG_LEVEL_RAWSTR)
            && rs->str_cal.size == 0
            && !(rs->has_get_level & RIG_LEVEL_STRENGTH))
    {

        fprintf(fout,
                "Warning--backend has get RAWSTR, but not calibration data\n");
        strcat(warnbuf, " RAWSTR_level");
        backend_warnings++;
    }

    rig_sprintf_level_gran(prntbuf, sizeof(prntbuf), rs->has_set_level,
                           rs->level_gran);
    fprintf(fout, "Set level: %s\n", prntbuf);

    if (rs->has_set_level & RIG_LEVEL_READONLY_LIST)
    {

        //fprintf(fout, "Warning--backend can set readonly levels=0x%0llx\n", rs->has_set_level & RIG_LEVEL_READONLY_LIST);
        fprintf(fout, "Warning--backend can set readonly levels\n");
        strcat(warnbuf, " READONLY_LEVEL");
        backend_warnings++;
    }

    fprintf(fout, "Extra levels:\n");
    rig_ext_level_foreach(rig, print_ext, fout);

    rig_sprintf_parm_gran(prntbuf, sizeof(prntbuf), rs->has_get_parm,
                          rs->parm_gran);
    fprintf(fout, "Get parameters: %s\n", prntbuf);

    rig_sprintf_parm_gran(prntbuf, sizeof(prntbuf), rs->has_set_parm,
                          rs->parm_gran);
    fprintf(fout, "Set parameters: %s\n", prntbuf);

    if (rs->has_set_parm & RIG_PARM_READONLY_LIST)
    {
        fprintf(fout, "Warning--backend can set readonly parms!\n");
        strcat(warnbuf, " READONLY_PARM");
        backend_warnings++;
    }

    fprintf(fout, "Extra parameters:\n");
    rig_ext_parm_foreach(rig, print_ext, fout);


    if (rs->mode_list != 0)
    {
        rig_sprintf_mode(prntbuf, sizeof(prntbuf), rs->mode_list);
    }
    else
    {
        strcpy(prntbuf, "None. This backend might be bogus!\n");
        strcat(warnbuf, " MODE_LIST");
        backend_warnings++;
    }

    fprintf(fout, "Mode list: %s\n", prntbuf);

    if (rs->vfo_list != 0)
    {
        rig_sprintf_vfo(prntbuf, sizeof(prntbuf), rs->vfo_list);
    }
    else
    {
        strcpy(prntbuf, "None. This backend might be bogus!\n");
        strcat(warnbuf, " VFO_LIST");
        backend_warnings++;
    }

    fprintf(fout, "VFO list: %s\n", prntbuf);

    rig_sprintf_vfop(prntbuf, sizeof(prntbuf), rs->vfo_ops);
    fprintf(fout, "VFO Ops: %s\n", prntbuf);

    rig_sprintf_scan(prntbuf, sizeof(prntbuf), rs->scan_ops);
    fprintf(fout, "Scan Ops: %s\n", prntbuf);

    fprintf(fout, "Number of banks:\t%d\n", rs->bank_qty);
    fprintf(fout, "Memory name desc size:\t%d\n", rs->chan_desc_sz);

    fprintf(fout, "Memories:");

    for (i = 0; i < HAMLIB_CHANLSTSIZ && rs->chan_list[i].type; i++)
    {
        fprintf(fout,
                "\n\t%d..%d:   \t%s",
                rs->chan_list[i].startc,
                rs->chan_list[i].endc,
                rig_strmtype(rs->chan_list[i].type));
        fprintf(fout, "\n\t  Mem caps: ");
        dump_chan_caps(&rs->chan_list[i].mem_caps, fout);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }

    fprintf(fout, "\n");

    label1 = rs->tx_range_list1->label;
    label1 = label1 == NULL ? "TBD" : label1;
    fprintf(fout, "TX ranges #1 for %s:\n", label1);
    range_print(fout, rs->tx_range_list1, 0);

    labelrx1 = rs->rx_range_list1->label;
    labelrx1 = labelrx1 == NULL ? "TBD" : labelrx1;
    fprintf(fout, "RX ranges #1 for %s:\n", labelrx1);
    range_print(fout, rs->rx_range_list1, 1);

    label2 = rs->rx_range_list2->label;
    label2 = label2 == NULL ? "TBD" : label2;
    fprintf(fout, "TX ranges #2 for %s:\n", label2);
    range_print(fout, rs->tx_range_list2, 0);

    label2 = rs->rx_range_list2->label;
    label2 = label2 == NULL ? "TBD" : label2;
    fprintf(fout, "RX ranges #2 for %s:\n", label2);
    range_print(fout, rs->rx_range_list2, 1);

    label3 = rs->rx_range_list3->label;
    label3 = label3 == NULL ? "TBD" : label3;
    fprintf(fout, "TX ranges #3 for %s:\n", label3);
    range_print(fout, rs->tx_range_list3, 0);

    label3 = rs->rx_range_list3->label;
    label3 = label3 == NULL ? "TBD" : label3;
    fprintf(fout, "RX ranges #3 for %s:\n", label3);
    range_print(fout, rs->rx_range_list3, 1);

    label4 = rs->rx_range_list4->label;
    label4 = label4 == NULL ? "TBD" : label4;
    fprintf(fout, "TX ranges #4 for %s:\n", label4);
    range_print(fout, rs->tx_range_list5, 0);

    label4 = rs->rx_range_list4->label;
    label4 = label4 == NULL ? "TBD" : label4;
    fprintf(fout, "RX ranges #4 for %s:\n", label4);
    range_print(fout, rs->rx_range_list5, 1);

    label5 = rs->rx_range_list5->label;
    label5 = label5 == NULL ? "TBD" : label5;
    fprintf(fout, "TX ranges #5 for %s:\n", label5);
    range_print(fout, rs->tx_range_list5, 0);

    label5 = rs->rx_range_list5->label;
    label5 = label5 == NULL ? "TBD" : label5;
    fprintf(fout, "RX ranges #5 for %s:\n", label5);
    range_print(fout, rs->rx_range_list5, 1);

    status = range_sanity_check(rs->tx_range_list1, 0);
    fprintf(fout,
            "TX ranges #1 status for %s:\t%s (%d)\n", label1,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#1");
        backend_warnings++;
    }

    status = range_sanity_check(rs->rx_range_list1, 1);
    fprintf(fout,
            "RX ranges #1 status for %s:\t%s (%d)\n", labelrx1,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " RX#1");
        backend_warnings++;
    }

    status = range_sanity_check(rs->tx_range_list2, 0);
    fprintf(fout,
            "TX ranges #2 status for %s:\t%s (%d)\n", label2,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#2");
        backend_warnings++;
    }

    status = range_sanity_check(rs->rx_range_list2, 1);
    fprintf(fout,
            "RX ranges #2 status for %s:\t%s (%d)\n", label2,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " RX#2");
        backend_warnings++;
    }

    status = range_sanity_check(rs->tx_range_list3, 0);
    fprintf(fout,
            "TX ranges #3 status for %s:\t%s (%d)\n", label3,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#3");
        backend_warnings++;
    }

    status = range_sanity_check(rs->rx_range_list3, 1);
    fprintf(fout,
            "RX ranges #3 status for %s:\t%s (%d)\n", label3,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " RX#3");
        backend_warnings++;
    }

    status = range_sanity_check(rs->tx_range_list4, 0);
    fprintf(fout,
            "TX ranges #4 status for %s:\t%s (%d)\n", label4,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#4");
        backend_warnings++;
    }

    status = range_sanity_check(rs->rx_range_list4, 1);
    fprintf(fout,
            "RX ranges #4 status for %s:\t%s (%d)\n", label4,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " RX#4");
        backend_warnings++;
    }

    status = range_sanity_check(rs->tx_range_list5, 0);
    fprintf(fout,
            "TX ranges #5 status for %s:\t%s (%d)\n", label5,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " TX#5");
        backend_warnings++;
    }

    status = range_sanity_check(rs->rx_range_list5, 1);
    fprintf(fout,
            "RX ranges #5 status for %s:\t%s (%d)\n", label5,
            status ? "Bad" : "OK",
            status);

    if (status)
    {
        strcat(warnbuf, " RX#5");
        backend_warnings++;
    }

    fprintf(fout, "Tuning steps:");

    for (i = 0; i < HAMLIB_TSLSTSIZ
            && !RIG_IS_TS_END(rs->tuning_steps[i]); i++)
    {
        if (rs->tuning_steps[i].ts == RIG_TS_ANY)
        {
            strcpy(freqbuf, "ANY");    /* strcpy!  Looks safe for now */
        }
        else
        {
            sprintf_freq(freqbuf, sizeof(freqbuf), rs->tuning_steps[i].ts);
        }

        rig_sprintf_mode(prntbuf, sizeof(prntbuf), rs->tuning_steps[i].modes);
        fprintf(fout, "\n\t%s:   \t%s", freqbuf, prntbuf);
    }

    if (i == 0)
    {
        fprintf(fout, " None! This backend might be bogus!");
        strcat(warnbuf, " TUNING_STEPS");
        backend_warnings++;
    }

    fprintf(fout, "\n");
    status = ts_sanity_check(rs->tuning_steps);
    fprintf(fout, "Tuning steps status:\t%s (%d)\n", status ? "Bad" : "OK", status);

    if (status)
    {
        strcat(warnbuf, " TUNING_SANE");
        backend_warnings++;
    }

    fprintf(fout, "Filters:");

    for (i = 0; i < HAMLIB_FLTLSTSIZ && !RIG_IS_FLT_END(rs->filters[i]); i++)
    {
        if (rs->filters[i].width == RIG_FLT_ANY)
        {
            strcpy(freqbuf, "ANY");
        }
        else
        {
            sprintf_freq(freqbuf, sizeof(freqbuf), rs->filters[i].width);
        }

        rig_sprintf_mode(prntbuf, sizeof(prntbuf), rs->filters[i].modes);
        fprintf(fout, "\n\t%s:   \t%s", freqbuf, prntbuf);
    }

    if (i == 0)
    {
        fprintf(fout, " None. This backend might be bogus!");
        strcat(warnbuf, " FILTERS");
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

        sprintf_freq(freqbuf, sizeof(freqbuf), pbnorm);
        fprintf(fout, "\n\t%s\tNormal: %s,\t", rig_strrmode(i), freqbuf);

        sprintf_freq(freqbuf, sizeof(freqbuf), rig_passband_narrow(rig, i));
        fprintf(fout, "Narrow: %s,\t", freqbuf);

        sprintf_freq(freqbuf, sizeof(freqbuf), rig_passband_wide(rig, i));
        fprintf(fout, "Wide: %s", freqbuf);
    }

    fprintf(fout, "\n");

    fprintf(fout, "Spectrum scopes:");

    for (i = 0; i < HAMLIB_MAX_SPECTRUM_SCOPES
            && rs->spectrum_scopes[i].name != NULL; i++)
    {
        fprintf(fout, " %d=\"%s\"", rs->spectrum_scopes[i].id,
                rs->spectrum_scopes[i].name);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }

    fprintf(fout, "\n");

    rig_sprintf_spectrum_modes(prntbuf, sizeof(prntbuf), rs->spectrum_modes);
    fprintf(fout, "Spectrum modes: %s\n", prntbuf);

    rig_sprintf_spectrum_spans(prntbuf, sizeof(prntbuf), rs->spectrum_spans);
    fprintf(fout, "Spectrum spans: %s\n", prntbuf);

    rig_sprintf_spectrum_avg_modes(prntbuf, sizeof(prntbuf),
                                   rs->spectrum_avg_modes);
    fprintf(fout, "Spectrum averaging modes: %s\n", prntbuf);

    fprintf(fout, "Spectrum attenuator:");

    for (i = 0; i < HAMLIB_MAXDBLSTSIZ
            && rs->spectrum_attenuator[i] != 0; i++)
    {
        fprintf(fout, " %ddB", rs->spectrum_attenuator[i]);
    }

    if (i == 0)
    {
        fprintf(fout, " None");
    }

    fprintf(fout, "\n");

    fprintf(fout, "Has priv data:\t%c\n", rs->priv != NULL ? 'Y' : 'N');
    /*
     * Status is either 'Y'es, 'E'mulated, 'N'o
     *
     * TODO: keep me up-to-date with API call list!
     */
    fprintf(fout, "Has Init:\t%c\n", rig->caps->rig_init != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Cleanup:\t%c\n", rig->caps->rig_cleanup != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Open:\t%c\n", rig->caps->rig_open != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Close:\t%c\n", rig->caps->rig_close != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Conf:\t%c\n", rig->caps->set_conf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Conf:\t%c\n", rig->caps->get_conf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Frequency:\t%c\n",
            rig->caps->set_freq != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Frequency:\t%c\n",
            rig->caps->get_freq != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Mode:\t%c\n", rig->caps->set_mode != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Mode:\t%c\n", rig->caps->get_mode != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set VFO:\t%c\n", rig->caps->set_vfo != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get VFO:\t%c\n", rig->caps->get_vfo != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set PTT:\t%c\n", rig->caps->set_ptt != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get PTT:\t%c\n", rig->caps->get_ptt != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get DCD:\t%c\n", rig->caps->get_dcd != NULL ? 'Y' : 'N');
    fprintf(fout,
            "Can set Repeater Duplex:\t%c\n",
            rig->caps->set_rptr_shift != NULL ? 'Y' : 'N');
    fprintf(fout,
            "Can get Repeater Duplex:\t%c\n",
            rig->caps->get_rptr_shift != NULL ? 'Y' : 'N');
    fprintf(fout,
            "Can set Repeater Offset:\t%c\n",
            rig->caps->set_rptr_offs != NULL ? 'Y' : 'N');
    fprintf(fout,
            "Can get Repeater Offset:\t%c\n",
            rig->caps->get_rptr_offs != NULL ? 'Y' : 'N');

    can_esplit = rig->caps->set_split_vfo
                 && (rig->caps->set_vfo
                     || (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && rig->caps->vfo_op));

    fprintf(fout,
            "Can set Split Freq:\t%c\n",
            rig->caps->set_split_freq != NULL ? 'Y' : (can_esplit
                    && rig->caps->set_freq ? 'E' : 'N'));

    fprintf(fout,
            "Can get Split Freq:\t%c\n",
            rig->caps->get_split_freq != NULL ? 'Y' : (can_esplit
                    && rig->caps->get_freq ? 'E' : 'N'));

    fprintf(fout,
            "Can set Split Mode:\t%c\n",
            rig->caps->set_split_mode != NULL ? 'Y' : (can_esplit
                    && rig->caps->set_mode ? 'E' : 'N'));

    fprintf(fout,
            "Can get Split Mode:\t%c\n",
            rig->caps->get_split_mode != NULL ? 'Y' : (can_esplit
                    && rig->caps->get_mode ? 'E' : 'N'));

    fprintf(fout,
            "Can set Split VFO:\t%c\n",
            rig->caps->set_split_vfo != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get Split VFO:\t%c\n",
            rig->caps->get_split_vfo != NULL ? 'Y' : 'N');

    fprintf(fout, "Can set Tuning Step:\t%c\n",
            rig->caps->set_ts != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Tuning Step:\t%c\n",
            rig->caps->get_ts != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set RIT:\t%c\n", rig->caps->set_rit != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get RIT:\t%c\n", rig->caps->get_rit != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set XIT:\t%c\n", rig->caps->set_xit != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get XIT:\t%c\n", rig->caps->get_xit != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set CTCSS:\t%c\n",
            rig->caps->set_ctcss_tone != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get CTCSS:\t%c\n",
            rig->caps->get_ctcss_tone != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set DCS:\t%c\n",
            rig->caps->set_dcs_code != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get DCS:\t%c\n",
            rig->caps->get_dcs_code != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set CTCSS Squelch:\t%c\n",
            rig->caps->set_ctcss_sql != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get CTCSS Squelch:\t%c\n",
            rig->caps->get_ctcss_sql != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set DCS Squelch:\t%c\n",
            rig->caps->set_dcs_sql != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get DCS Squelch:\t%c\n",
            rig->caps->get_dcs_sql != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set Power Stat:\t%c\n",
            rig->caps->set_powerstat != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get Power Stat:\t%c\n",
            rig->caps->get_powerstat != NULL ? 'Y' : 'N');

    fprintf(fout, "Can Reset:\t%c\n", rig->caps->reset != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Ant:\t%c\n", rig->caps->get_ant != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Ant:\t%c\n", rig->caps->set_ant != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set Transceive:\t%c\n",
            rig->caps->set_trn != NULL ? 'Y' : rig->caps->transceive == RIG_TRN_RIG ? 'E' :
            'N');

    fprintf(fout, "Can get Transceive:\t%c\n",
            rig->caps->get_trn != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Func:\t%c\n", rig->caps->set_func != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Func:\t%c\n", rig->caps->get_func != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Level:\t%c\n", rig->caps->set_level != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Level:\t%c\n", rig->caps->get_level != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Param:\t%c\n", rig->caps->set_parm != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Param:\t%c\n", rig->caps->get_parm != NULL ? 'Y' : 'N');
    fprintf(fout, "Can send DTMF:\t%c\n", rig->caps->send_dtmf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can recv DTMF:\t%c\n", rig->caps->recv_dtmf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can send Morse:\t%c\n",
            rig->caps->send_morse != NULL ? 'Y' : 'N');
    fprintf(fout, "Can stop Morse:\t%c\n",
            rig->caps->stop_morse != NULL ? 'Y' : 'N');
    fprintf(fout, "Can wait Morse:\t%c\n",
            rig->caps->wait_morse != NULL ? 'Y' : 'N');
    fprintf(fout, "Can send Voice:\t%c\n",
            rig->caps->send_voice_mem != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can decode Events:\t%c\n",
            rig->caps->decode_event != NULL ? 'Y' : 'N');

    fprintf(fout, "Can set Bank:\t%c\n", rig->caps->set_bank != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Mem:\t%c\n", rig->caps->set_mem != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Mem:\t%c\n", rig->caps->get_mem != NULL ? 'Y' : 'N');

    can_echannel = rig->caps->set_mem
                   && ((rig->caps->set_vfo
                        && ((rs->vfo_list & RIG_VFO_MEM) == RIG_VFO_MEM))
                       || (rig->caps->vfo_op
                           && rig_has_vfo_op(rig, RIG_OP_TO_VFO | RIG_OP_FROM_VFO)));

    fprintf(fout,
            "Can set Channel:\t%c\n",
            rig->caps->set_channel != NULL ? 'Y' : (can_echannel ? 'E' : 'N'));

    fprintf(fout,
            "Can get Channel:\t%c\n",
            rig->caps->get_channel != NULL ? 'Y' : (can_echannel ? 'E' : 'N'));

    fprintf(fout, "Can ctl Mem/VFO:\t%c\n", rig->caps->vfo_op != NULL ? 'Y' : 'N');
    fprintf(fout, "Can Scan:\t%c\n", rig->caps->scan != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Info:\t%c\n", rig->caps->get_info != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get power2mW:\t%c\n", caps->power2mW != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get mW2power:\t%c\n", caps->mW2power != NULL ? 'Y' : 'N');

    fprintf(fout, "\nOverall backend warnings: %d %c %s\n", backend_warnings,
            warnbuf[0] != 0 ? '=' : ' ', warnbuf);

    return backend_warnings;
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
