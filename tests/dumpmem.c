/*
 * dumpmem.c - Copyright (C) 2001 Stephane Fillod
 * This programs dumps the mmeory contents of a rig.
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
#include <stdlib.h>

#include <hamlib/rig.h>

#include <hamlib/config.h>

#include "misc.h"

#define SERIAL_PORT "/dev/ttyS0"


static char *decode_modes(rmode_t modes);
static int dump_chan(RIG *rig, int chan_num);


int main(int argc, char *argv[])
{
    RIG *my_rig;
    int status, i, j;

    if (argc != 2)
    {
        fprintf(stderr, "%s <rig_num>\n", argv[0]);
        exit(1);
    }

    my_rig = rig_init(atoi(argv[1]));

    if (!my_rig)
    {
        fprintf(stderr, "Unknown rig num: %d\n", atoi(argv[1]));
        fprintf(stderr, "Please check riglist.h\n");
        exit(1); /* whoops! something went wrong (mem alloc?) */
    }

    strncpy(my_rig->state.rigport.pathname, SERIAL_PORT, HAMLIB_FILPATHLEN - 1);

    if (rig_open(my_rig))
    {
        exit(2);
    }

    status = rig_set_vfo(my_rig, RIG_VFO_B);


    if (status != RIG_OK)
    {
        printf("rig_set_vfo: error = %s \n", rigerror(status));
    }

    /*
     * chan_t is used to describe what memory your rig is equipped with
     * cf. chan_list field in caps
     * Example for the Ic706MkIIG (99 memory channels, 2 scan edges, 2 call chans):
     *  chan_t chan_list[] = {
     *      { 1, 99, RIG_MTYPE_MEM, 0 },
     *      { 100, 103, RIG_MTYPE_EDGE, 0 },
     *      { 104, 105, RIG_MTYPE_CALL, 0 },
     *      RIG_CHAN_END
     *  }
     */

    for (i = 0; my_rig->state.chan_list[i].type; i++)
    {
        for (j = my_rig->state.chan_list[i].startc;
                j <= my_rig->state.chan_list[i].endc; j++)
        {
            dump_chan(my_rig, j);
        }
    }

    rig_close(my_rig); /* close port */
    rig_cleanup(my_rig); /* if you care about memory */

    printf("port %s closed ok \n", SERIAL_PORT);

    return 0;
}


/*
 * NB: this function is not reentrant, because of the static buf.
 *      but who cares?  --SF
 */
static char *decode_modes(rmode_t modes)
{
    static char buf[80];

    buf[0] = '\0';

    if (modes & RIG_MODE_AM)
    {
        strcat(buf, "AM ");
    }

    if (modes & RIG_MODE_CW)
    {
        strcat(buf, "CW ");
    }

    if (modes & RIG_MODE_USB)
    {
        strcat(buf, "USB ");
    }

    if (modes & RIG_MODE_LSB)
    {
        strcat(buf, "LSB ");
    }

    if (modes & RIG_MODE_RTTY)
    {
        strcat(buf, "RTTY ");
    }

    if (modes & RIG_MODE_FM)
    {
        strcat(buf, "FM ");
    }

#ifdef RIG_MODE_WFM

    if (modes & RIG_MODE_WFM)
    {
        strcat(buf, "WFM ");
    }

#endif

    return buf;
}


int dump_chan(RIG *rig, int chan_num)
{
    channel_t chan;
    int status;
    char freqbuf[20];

    chan.vfo = RIG_VFO_MEM;
    chan.channel_num = chan_num;
    status = rig_get_channel(rig, RIG_VFO_NONE, &chan, 1);

    if (status != RIG_OK)
    {
        printf("rig_get_channel: error = %s \n", rigerror(status));
        return status;
    }

    printf("Channel: %d\n", chan.channel_num);

    sprintf_freq(freqbuf, sizeof(freqbuf), chan.freq);
    printf("Frequency: %s\n", freqbuf);
    printf("Mode: %s\n", decode_modes(chan.mode));

    sprintf_freq(freqbuf, sizeof(freqbuf), chan.width);
    printf("Width: %s\n", freqbuf);
    printf("VFO: %s\n", rig_strvfo(chan.vfo));

    printf("Split: %d\n", chan.split);
    sprintf_freq(freqbuf, sizeof(freqbuf), chan.tx_freq);
    printf("TXFrequency: %s\n", freqbuf);
    printf("TXMode: %s\n", decode_modes(chan.tx_mode));

    sprintf_freq(freqbuf, sizeof(freqbuf), chan.tx_width);
    printf("TXWidth: %s\n", freqbuf);

    printf("Shift: %s\n", rig_strptrshift(chan.rptr_shift));
    sprintf_freq(freqbuf, sizeof(freqbuf), chan.rptr_offs);
    printf("Offset: %s%s\n", chan.rptr_offs > 0 ? "+" : "", freqbuf);

    printf("Antenna: %u\n", chan.ant);

    sprintf_freq(freqbuf, sizeof(freqbuf), chan.tuning_step);
    printf("Step: %s\n", freqbuf);

    sprintf_freq(freqbuf, sizeof(freqbuf), chan.rit);
    printf("RIT: %s%s\n", chan.rit > 0 ? "+" : "", freqbuf);

    sprintf_freq(freqbuf, sizeof(freqbuf), chan.xit);
    printf("XIT: %s%s\n", chan.xit > 0 ? "+" : "", freqbuf);
    printf("CTCSS: %u.%uHz\n", chan.ctcss_tone / 10, chan.ctcss_tone % 10);
    printf("CTCSSsql: %u.%uHz\n", chan.ctcss_sql / 10, chan.ctcss_sql % 10);
    printf("DCS: %u.%u\n", chan.dcs_code / 10, chan.dcs_code % 10);
    printf("DCSsql: %u.%u\n", chan.dcs_sql / 10, chan.dcs_sql % 10);
    printf("Name: %s\n", chan.channel_desc);

    printf("Functions: ");

    if (chan.funcs != 0)
    {
        if (chan.funcs & RIG_FUNC_FAGC)
        {
            printf("FAGC ");
        }

        if (chan.funcs & RIG_FUNC_NB)
        {
            printf("NB ");
        }

        if (chan.funcs & RIG_FUNC_COMP)
        {
            printf("COMP ");
        }

        if (chan.funcs & RIG_FUNC_VOX)
        {
            printf("VOX ");
        }

        if (chan.funcs & RIG_FUNC_TONE)
        {
            printf("TONE ");
        }

        if (chan.funcs & RIG_FUNC_TSQL)
        {
            printf("TSQL ");
        }

        if (chan.funcs & RIG_FUNC_SBKIN)
        {
            printf("SBKIN ");
        }

        if (chan.funcs & RIG_FUNC_FBKIN)
        {
            printf("FBKIN ");
        }

        if (chan.funcs & RIG_FUNC_ANF)
        {
            printf("ANF ");
        }

        if (chan.funcs & RIG_FUNC_NR)
        {
            printf("NR ");
        }

        if (chan.funcs & RIG_FUNC_AIP)
        {
            printf("AIP ");
        }

        if (chan.funcs & RIG_FUNC_APF)
        {
            printf("APF ");
        }

        if (chan.funcs & RIG_FUNC_MON)
        {
            printf("MON ");
        }

        if (chan.funcs & RIG_FUNC_MN)
        {
            printf("MN ");
        }

        if (chan.funcs & RIG_FUNC_RF)
        {
            printf("RF ");
        }

        printf("\n");
    }
    else
    {
        printf("none\n");
    }

    if (rig_has_set_level(rig, RIG_LEVEL_PREAMP))
    {
        printf("PREAMP: %ddB\n", chan.levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_ATT))
    {
        printf("ATT: %ddB\n", chan.levels[rig_setting2idx(RIG_LEVEL_ATT)].i);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_AF))
    {
        printf("AF: %g%%\n", 100 * chan.levels[rig_setting2idx(RIG_LEVEL_AF)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_RF))
    {
        printf("RF: %g%%\n", 100 * chan.levels[rig_setting2idx(RIG_LEVEL_RF)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_SQL))
    {
        printf("SQL: %g%%\n", 100 * chan.levels[rig_setting2idx(RIG_LEVEL_SQL)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_IF))
    {
        printf("IF: %dHz\n", chan.levels[rig_setting2idx(RIG_LEVEL_IF)].i);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_APF))
    {
        printf("APF: %g%%\n", 100 * chan.levels[rig_setting2idx(RIG_LEVEL_APF)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_NR))
    {
        printf("NR: %g%%\n", 100 * chan.levels[rig_setting2idx(RIG_LEVEL_NR)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_PBT_IN))
    {
        printf("PBT_IN: %g%%\n",
               100 * chan.levels[rig_setting2idx(RIG_LEVEL_PBT_IN)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_PBT_OUT))
    {
        printf("PBT_OUT: %g%%\n",
               100 * chan.levels[rig_setting2idx(RIG_LEVEL_PBT_OUT)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_CWPITCH))
    {
        printf("CWPITCH: %dHz\n", chan.levels[rig_setting2idx(RIG_LEVEL_CWPITCH)].i);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_RFPOWER))
    {
        printf("RFPOWER: %g%%\n",
               100 * chan.levels[rig_setting2idx(RIG_LEVEL_RFPOWER)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_MICGAIN))
    {
        printf("MICGAIN: %g%%\n",
               100 * chan.levels[rig_setting2idx(RIG_LEVEL_MICGAIN)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_COMP))
    {
        printf("COMP: %g%%\n", 100 * chan.levels[rig_setting2idx(RIG_LEVEL_COMP)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_BALANCE))
    {
        printf("BALANCE: %g%%\n",
               100 * chan.levels[rig_setting2idx(RIG_LEVEL_BALANCE)].f);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_KEYSPD))
    {
        printf("KEYSPD: %d\n", chan.levels[rig_setting2idx(RIG_LEVEL_KEYSPD)].i);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_NOTCHF))
    {
        printf("NOTCHF: %d\n", chan.levels[rig_setting2idx(RIG_LEVEL_NOTCHF)].i);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_AGC))
    {
        printf("AGC: %d\n", chan.levels[rig_setting2idx(RIG_LEVEL_AGC)].i);
    }

    if (rig_has_set_level(rig, RIG_LEVEL_BKINDL))
    {
        printf("BKINDL: %d\n", chan.levels[rig_setting2idx(RIG_LEVEL_BKINDL)].i);
    }

    return 0;
}
