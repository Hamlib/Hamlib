/*
 *  Hamlib Interface - sprintf toolbox
 *  Copyright (c) 2000-2009 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include <hamlib/rotator.h>
#include <hamlib/amplifier.h>

#include "sprintflst.h"
#include "misc.h"

/* #define DUMMY_ALL 0x7ffffffffffffffLL */
#define DUMMY_ALL ((setting_t)-1)

int rig_sprintf_vfo(char *str, vfo_t vfo)
{
    unsigned int i, len = 0;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));
    *str = '\0';

    if (vfo == RIG_VFO_NONE)
    {
        return 0;
    }

    for (i = 0; i < 32; i++)
    {
        const char *sv;
        sv = rig_strvfo(vfo & RIG_VFO_N(i));

        if (sv && sv[0] && (strstr(sv, "None") == 0))
        {
            len += sprintf(str + len, "%s ", sv);
        }
    }

    return len;
}


int rig_sprintf_mode(char *str, rmode_t mode)
{
    uint64_t i, len = 0;

    *str = '\0';

    if (mode == RIG_MODE_NONE)
    {
        return 0;
    }

    for (i = 0; i < 63; i++)
    {
        const char *ms = rig_strrmode(mode & (1ULL << i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int rig_sprintf_ant(char *str, ant_t ant)
{
    int i, len = 0;
    char *ant_name;

    *str = '\0';

    if (ant == RIG_ANT_NONE)
    {
        sprintf(str, "ANT_NONE");
        return 0;
    }

    for (i = 0; i < RIG_ANT_MAX; i++)
    {
        if (ant & (1UL << i))
        {
            switch (i)
            {
            case 0: ant_name = "ANT1"; break;

            case 1: ant_name = "ANT2"; break;

            case 2: ant_name = "ANT3"; break;

            case 3: ant_name = "ANT4"; break;

            case 4: ant_name = "ANT5"; break;

            case 30: ant_name = "ANT_UNKNOWN"; break;

            case 31: ant_name = "ANT_CURR"; break;

            default:
                ant_name = "ANT_UNK";
                rig_debug(RIG_DEBUG_ERR, "%s: unknown ant=%d\n", __func__, i);
                break;
            }

            len += sprintf(str + len, "%s ", ant_name);
        }
    }

    return len;
}


int rig_sprintf_func(char *str, setting_t func)
{
    uint64_t i, len = 0;

    *str = '\0';

    if (func == RIG_FUNC_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms = rig_strfunc(func & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int rot_sprintf_func(char *str, setting_t func)
{
    uint64_t i, len = 0;

    *str = '\0';

    if (func == ROT_FUNC_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms = rot_strfunc(func & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int rig_sprintf_level(char *str, setting_t level)
{
    int i, len = 0;

    *str = '\0';

    if (level == RIG_LEVEL_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms = rig_strlevel(level & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int rot_sprintf_level(char *str, setting_t level)
{
    int i, len = 0;

    *str = '\0';

    if (level == ROT_LEVEL_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms = rot_strlevel(level & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int amp_sprintf_level(char *str, setting_t level)
{
    int i, len = 0;

    *str = '\0';

    if (level == AMP_LEVEL_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms = amp_strlevel(level & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int sprintf_level_ext(char *str, const struct confparams *extlevels)
{
    int len = 0;

    *str = '\0';

    if (!extlevels)
    {
        return 0;
    }

    for (; extlevels->token != RIG_CONF_END; extlevels++)
    {
        if (!extlevels->name)
        {
            continue;    /* no name */
        }

        switch (extlevels->type)
        {
        case RIG_CONF_CHECKBUTTON:
        case RIG_CONF_COMBO:
        case RIG_CONF_NUMERIC:
        case RIG_CONF_STRING:
        case RIG_CONF_BINARY:
            strcat(str, extlevels->name);
            strcat(str, " ");
            len += strlen(extlevels->name) + 1;
            break;

        case RIG_CONF_BUTTON:
            /* ignore case RIG_CONF_BUTTON */
            break;
        }
    }

    return len;
}


int rig_sprintf_level_gran(char *str, setting_t level, const gran_t *gran)
{
    int i, len = 0;

    *str = '\0';

    if (level == RIG_LEVEL_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms;

        if (!(level & rig_idx2setting(i)))
        {
            continue;
        }

        ms = rig_strlevel(level & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            if (level != DUMMY_ALL && level != RIG_LEVEL_SET(DUMMY_ALL))
            {
                rig_debug(RIG_DEBUG_BUG, "unknown level idx %d\n", i);
            }

            continue;
        }

        if (RIG_LEVEL_IS_FLOAT(rig_idx2setting(i)))
        {
            len += sprintf(str + len,
                           "%s(%g..%g/%g) ",
                           ms,
                           gran[i].min.f,
                           gran[i].max.f,
                           gran[i].step.f);
        }
        else
        {
            len += sprintf(str + len,
                           "%s(%d..%d/%d) ",
                           ms,
                           gran[i].min.i,
                           gran[i].max.i,
                           gran[i].step.i);
        }
    }

    return len;
}


int rot_sprintf_level_gran(char *str, setting_t level, const gran_t *gran)
{
    int i, len = 0;

    *str = '\0';

    if (level == ROT_LEVEL_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms;

        if (!(level & rig_idx2setting(i)))
        {
            continue;
        }

        ms = rot_strlevel(level & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            if (level != DUMMY_ALL && level != ROT_LEVEL_SET(DUMMY_ALL))
            {
                rig_debug(RIG_DEBUG_BUG, "unknown level idx %d\n", i);
            }

            continue;
        }

        if (ROT_LEVEL_IS_FLOAT(rig_idx2setting(i)))
        {
            len += sprintf(str + len,
                           "%s(%g..%g/%g) ",
                           ms,
                           gran[i].min.f,
                           gran[i].max.f,
                           gran[i].step.f);
        }
        else
        {
            len += sprintf(str + len,
                           "%s(%d..%d/%d) ",
                           ms,
                           gran[i].min.i,
                           gran[i].max.i,
                           gran[i].step.i);
        }
    }

    return len;
}


int rig_sprintf_parm(char *str, setting_t parm)
{
    int i, len = 0;

    *str = '\0';

    if (parm == RIG_PARM_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms = rig_strparm(parm & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int rot_sprintf_parm(char *str, setting_t parm)
{
    int i, len = 0;

    *str = '\0';

    if (parm == ROT_PARM_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms = rot_strparm(parm & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int rig_sprintf_parm_gran(char *str, setting_t parm, const gran_t *gran)
{
    int i, len = 0;

    *str = '\0';

    if (parm == RIG_PARM_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms;

        if (!(parm & rig_idx2setting(i)))
        {
            continue;
        }

        ms = rig_strparm(parm & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            if (parm != DUMMY_ALL && parm != RIG_PARM_SET(DUMMY_ALL))
            {
                rig_debug(RIG_DEBUG_BUG, "unknown parm idx %d\n", i);
            }

            continue;
        }

        if (RIG_PARM_IS_FLOAT(rig_idx2setting(i)))
        {
            len += sprintf(str + len,
                           "%s(%g..%g/%g) ",
                           ms,
                           gran[i].min.f,
                           gran[i].max.f,
                           gran[i].step.f);
        }
        else
        {
            len += sprintf(str + len,
                           "%s(%d..%d/%d) ",
                           ms,
                           gran[i].min.i,
                           gran[i].max.i,
                           gran[i].step.i);
        }
    }

    return len;
}


int rot_sprintf_parm_gran(char *str, setting_t parm, const gran_t *gran)
{
    int i, len = 0;

    *str = '\0';

    if (parm == ROT_PARM_NONE)
    {
        return 0;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms;

        if (!(parm & rig_idx2setting(i)))
        {
            continue;
        }

        ms = rot_strparm(parm & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            if (parm != DUMMY_ALL && parm != ROT_PARM_SET(DUMMY_ALL))
            {
                rig_debug(RIG_DEBUG_BUG, "unknown parm idx %d\n", i);
            }

            continue;
        }

        if (ROT_PARM_IS_FLOAT(rig_idx2setting(i)))
        {
            len += sprintf(str + len,
                           "%s(%g..%g/%g) ",
                           ms,
                           gran[i].min.f,
                           gran[i].max.f,
                           gran[i].step.f);
        }
        else
        {
            len += sprintf(str + len,
                           "%s(%d..%d/%d) ",
                           ms,
                           gran[i].min.i,
                           gran[i].max.i,
                           gran[i].step.i);
        }
    }

    return len;
}


int rig_sprintf_vfop(char *str, vfo_op_t op)
{
    int i, len = 0;

    *str = '\0';

    if (op == RIG_OP_NONE)
    {
        return 0;
    }

    for (i = 0; i < 30; i++)
    {
        const char *ms = rig_strvfop(op & (1UL << i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int rig_sprintf_scan(char *str, scan_t rscan)
{
    int i, len = 0;

    *str = '\0';

    if (rscan == RIG_SCAN_NONE)
    {
        return 0;
    }

    for (i = 0; i < 30; i++)
    {
        const char *ms = rig_strscan(rscan & (1UL << i));

        if (!ms || !ms[0])
        {
            continue;    /* unknown, FIXME! */
        }

        strcat(str, ms);
        strcat(str, " ");
        len += strlen(ms) + 1;
    }

    return len;
}


int rot_sprintf_status(char *str, rot_status_t status)
{
    int i, len = 0;

    rig_debug(RIG_DEBUG_TRACE, "%s: status=%08x\n", __func__, status);
    *str = '\0';

    if (status == ROT_STATUS_NONE)
    {
        return 0;
    }

    for (i = 0; i < 32; i++)
    {
        const char *sv;
        sv = rot_strstatus(status & ROT_STATUS_N(i));

        if (sv && sv[0] && (strstr(sv, "None") == 0))
        {
            len += sprintf(str + len, "%s ", sv);
        }
    }

    return len;
}


char *get_rig_conf_type(enum rig_conf_e type)
{
    switch (type)
    {
    case RIG_CONF_STRING:
        return "STRING";

    case RIG_CONF_COMBO:
        return "COMBO";

    case RIG_CONF_NUMERIC:
        return "NUMERIC";

    case RIG_CONF_CHECKBUTTON:
        return "CHECKBUTTON";

    case RIG_CONF_BUTTON:
        return "BUTTON";

    case RIG_CONF_BINARY:
        return "BINARY";
    }

    return "UNKNOWN";
}

int print_ext_param(const struct confparams *cfp, rig_ptr_t ptr)
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
