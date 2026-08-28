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
 * Expected values are taken from the IC-7760 CI-V REFERENCE GUIDE
 * (A7788-8EX, Oct. 2024); page numbers below refer to that document.
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

/* p. 14: VOX delay is 1A 05 03 65, data 00 ~ 20 */
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

int main(void)
{
    int fail = 0;

    fail |= test_vox_delay_subcommand();

    return fail;
}
