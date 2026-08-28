/*
 * Hamlib IC-7760 capability tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * Expected values are taken from the IC-7760 CI-V REFERENCE GUIDE,
 * revision A7788-8EX-2 (May 2025).  Commands are cited by number rather
 * than by page, because Icom repaginates between revisions.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cal.h"
#include "icom.h"

static const struct cmdparams *find_level_cmd(const struct cmdparams *cmds,
        setting_t level)
{
    int i;

    for (i = 0; cmds != NULL && cmds[i].id.s != 0; i++)
    {
        if (cmds[i].cmdparamtype == CMD_PARAM_TYPE_LEVEL
                && cmds[i].id.s == level)
        {
            return &cmds[i];
        }
    }

    return NULL;
}

/* VOX delay is 1A 05 03 65, data 00 ~ 20 in 0.1 s steps */
static int test_vox_delay_subcommand(void)
{
    const struct icom_priv_caps *priv = (const struct icom_priv_caps *)
                                        ic7760_caps.priv;
    const struct cmdparams *cmd = find_level_cmd(priv->extcmds,
                                  RIG_LEVEL_VOXDELAY);

    if (cmd == NULL)
    {
        fprintf(stderr, "VOX delay: no extcmds entry\n");
        return 1;
    }

    if (cmd->command != 0x1a || cmd->subcmd != 0x05 || cmd->sublen != 2
            || cmd->subext[0] != 0x03 || cmd->subext[1] != 0x65)
    {
        fprintf(stderr,
                "VOX delay: expected 1A 05 03 65, got %02X %02X %02X %02X (sublen %d)\n",
                cmd->command, cmd->subcmd, cmd->subext[0], cmd->subext[1],
                cmd->sublen);
        return 1;
    }

    return 0;
}

/*
 * Capabilities the IC-7760 has no CI-V command for.  Command 14 skips
 * subcommand 10 and command 16 skips subcommand 4C, so neither the
 * dual-watch balance nor voice squelch control exists.
 */
static int test_absent_capabilities(void)
{
    static const struct
    {
        const char *name;
        setting_t level;
        setting_t func;
    } absent[] =
    {
        { "BALANCE (14 10)", RIG_LEVEL_BALANCE, 0 },
        { "VSC (16 4C)", 0, RIG_FUNC_VSC },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(absent) / sizeof(absent[0]); i++)
    {
        if ((ic7760_caps.has_get_level & absent[i].level) != 0
                || (ic7760_caps.has_set_level & absent[i].level) != 0
                || (ic7760_caps.has_get_func & absent[i].func) != 0
                || (ic7760_caps.has_set_func & absent[i].func) != 0)
        {
            fprintf(stderr, "%s is advertised but the rig has no such command\n",
                    absent[i].name);
            fail = 1;
        }
    }

    return fail;
}

/*
 * Keyer memory content (1A 02) addresses channels 01=M1 to 08=M8, and
 * the Voice TX memory (28 00) transmits 01=T1 to 08=T8.
 */
static int test_memory_channel_counts(void)
{
    static const struct
    {
        const char *name;
        chan_type_t type;
        int startc;
        int endc;
    } expected[] =
    {
        { "memory", RIG_MTYPE_MEM, 1, 99 },
        { "scan edge", RIG_MTYPE_EDGE, 100, 101 },
        { "keyer memory", RIG_MTYPE_MORSE, 1, 8 },
        { "voice memory", RIG_MTYPE_VOICE, 1, 8 },
    };
    size_t i;
    int j;
    int fail = 0;

    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        int found = 0;

        for (j = 0; j < HAMLIB_CHANLSTSIZ
                && ic7760_caps.chan_list[j].type != RIG_MTYPE_NONE; j++)
        {
            const struct chan_list *chan = &ic7760_caps.chan_list[j];

            if (chan->type != expected[i].type) { continue; }

            found = 1;

            if (chan->startc != expected[i].startc
                    || chan->endc != expected[i].endc)
            {
                fprintf(stderr, "%s channels: expected %d..%d, got %d..%d\n",
                        expected[i].name, expected[i].startc, expected[i].endc,
                        chan->startc, chan->endc);
                fail = 1;
            }
        }

        if (!found)
        {
            fprintf(stderr, "%s channels are not listed at all\n",
                    expected[i].name);
            fail = 1;
        }
    }

    return fail;
}

/*
 * Default IF filter widths, from the IC-7760 basic manual.  rig2icom_mode()
 * picks FIL1 for a width wider than the normal passband, FIL2 for the
 * normal one and FIL3 for a narrower one, so the caps list has to hold
 * each mode's FIL2, FIL3 and FIL1 width in that order.
 */
static int test_filter_defaults(RIG *rig)
{
    static const struct
    {
        const char *name;
        rmode_t mode;
        pbwidth_t fil2;
        pbwidth_t fil3;
        pbwidth_t fil1;
    } filters[] =
    {
        { "SSB", RIG_MODE_USB, 2400, 1800, 3000 },
        { "SSB-D", RIG_MODE_PKTUSB, 1200, 500, 3000 },
        { "CW", RIG_MODE_CW, 500, 250, 1200 },
        { "PSK", RIG_MODE_PSK, 500, 250, 1200 },
        { "RTTY", RIG_MODE_RTTY, 500, 250, 2400 },
        { "AM", RIG_MODE_AM, 6000, 3000, 9000 },
        { "FM", RIG_MODE_FM, 10000, 7000, 15000 },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(filters) / sizeof(filters[0]); i++)
    {
        pbwidth_t normal = rig_passband_normal(rig, filters[i].mode);
        pbwidth_t narrow = rig_passband_narrow(rig, filters[i].mode);
        pbwidth_t wide = rig_passband_wide(rig, filters[i].mode);

        if (normal != filters[i].fil2 || narrow != filters[i].fil3
                || wide != filters[i].fil1)
        {
            fprintf(stderr,
                    "%s filters: expected FIL2/FIL3/FIL1 %ld/%ld/%ld, got %ld/%ld/%ld\n",
                    filters[i].name, (long) filters[i].fil2, (long) filters[i].fil3,
                    (long) filters[i].fil1, (long) normal, (long) narrow, (long) wide);
            fail = 1;
        }
    }

    return fail;
}

/*
 * Reading the Id meter is 15 16, and the command table gives four
 * points along its scale: 00 00 = 0 A, 00 77 = 5 A, 01 65 = 10 A and
 * 02 41 = 15 A.
 */
static int test_id_meter_calibration(void)
{
    static const struct
    {
        int raw;
        float expected;
    } cases[] =
    {
        { 0, 0.0f },
        { 77, 5.0f },
        { 121, 7.5f },  /* halfway between the 5 A and 10 A points */
        { 165, 10.0f },
        { 241, 15.0f },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        float actual = rig_raw2val_float(cases[i].raw, &ic7760_caps.id_meter_cal);

        if (fabsf(actual - cases[i].expected) > 0.0001f)
        {
            fprintf(stderr, "Id meter at raw %d: expected %.2f A, got %.2f A\n",
                    cases[i].raw, cases[i].expected, actual);
            fail = 1;
        }
    }

    return fail;
}

/*
 * Command 16 02 offers two preamplifiers, 01 and 02.  The basic manual
 * gives P.AMP 1 as approximately 12 dB and P.AMP 2 as approximately
 * 20 dB of gain.
 */
static int test_preamp_gains(void)
{
    static const int expected[] = { 12, 20, 0 };
    size_t i;

    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        if (ic7760_caps.preamp[i] != expected[i])
        {
            fprintf(stderr, "preamp %d: expected %d dB, got %d dB\n",
                    (int) i + 1, expected[i], ic7760_caps.preamp[i]);
            return 1;
        }
    }

    return 0;
}

struct name_search
{
    const char *wanted;
    int found;
};

static int match_name(RIG *rig, const struct confparams *cfp, rig_ptr_t data)
{
    struct name_search *search = (struct name_search *) data;

    if (cfp != NULL && cfp->name != NULL
            && strcmp(cfp->name, search->wanted) == 0)
    {
        search->found = 1;
    }

    return 1;
}

/*
 * ext_tokens is a whitelist over caps->extlevels and caps->extfuncs, so
 * both tables have to be present for the declared tokens to be
 * enumerable.  DRIVE gain is 14 14 and DIGI-SEL is 16 4E with its level
 * on 14 13; the spectrum scope tokens must stay filtered out, because
 * the backend implements none of the 27 commands.
 */
static int test_ext_tokens_are_enumerable(RIG *rig)
{
    static const struct
    {
        const char *name;
        int is_func;
        int expected;
    } tokens[] =
    {
        { "drive_gain", 0, 1 },
        { "digi_sel_level", 0, 1 },
        { "digi_sel", 1, 1 },
        { "SPECTRUM_SELECT", 0, 0 },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
    {
        struct name_search search = { tokens[i].name, 0 };

        if (tokens[i].is_func)
        {
            rig_ext_func_foreach(rig, match_name, &search);
        }
        else
        {
            rig_ext_level_foreach(rig, match_name, &search);
        }

        if (search.found != tokens[i].expected)
        {
            fprintf(stderr, "ext token %s: expected %s, but it is %s\n",
                    tokens[i].name,
                    tokens[i].expected ? "enumerable" : "filtered out",
                    search.found ? "enumerable" : "missing");
            fail = 1;
        }
    }

    /*
     * Listing an ext func is only half of it: rig_set_ext_func() and
     * rig_get_ext_func() answer ENAVAIL unless the backend implements
     * them, which would leave digi_sel visible but unreachable.
     */
    if (ic7760_caps.set_ext_func == NULL || ic7760_caps.get_ext_func == NULL)
    {
        fprintf(stderr, "the ext funcs are listed but cannot be set or read,"
                " so digi_sel cannot be reached\n");
        fail = 1;
    }

    return fail;
}

int main(void)
{
    RIG *rig;
    int fail = 0;

    fail |= test_vox_delay_subcommand();
    fail |= test_absent_capabilities();
    fail |= test_memory_channel_counts();
    fail |= test_preamp_gains();
    fail |= test_id_meter_calibration();

    rig_register(&ic7760_caps);
    rig = rig_init(RIG_MODEL_IC7760);

    if (rig == NULL)
    {
        fprintf(stderr, "rig_init failed\n");
        return 1;
    }

    fail |= test_filter_defaults(rig);
    fail |= test_ext_tokens_are_enumerable(rig);

    rig_cleanup(rig);

    return fail;
}
