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

#include <stdio.h>

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

int main(void)
{
    int fail = 0;

    fail |= test_vox_delay_subcommand();
    fail |= test_absent_capabilities();
    fail |= test_memory_channel_counts();

    return fail;
}
