/*
 *  Hamlib Interface - sprintf toolbox
 *  Copyright (c) 2000-2009 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
 *
 *	$Id: sprintflst.c,v 1.6 2009-02-17 08:03:52 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <sys/types.h>
#include <unistd.h>

#include <hamlib/rig.h>

#include "sprintflst.h"
#include "misc.h"

/* #define DUMMY_ALL 0x7ffffffffffffffLL */
#define DUMMY_ALL ((setting_t)-1)

int sprintf_vfo(char *str, vfo_t vfo)
{
		int i, len=0;
		const char *sv;

		*str = '\0';
		if (vfo == RIG_VFO_NONE)
				return 0;
		sv = rig_strvfo(vfo & RIG_VFO_CURR);
		if (sv && sv[0]) len += sprintf(str+len, "%s ", sv);
		sv = rig_strvfo(vfo & RIG_VFO_MEM);
		if (sv && sv[0]) len += sprintf(str+len, "%s ", sv);
		sv = rig_strvfo(vfo & RIG_VFO_VFO);
		if (sv && sv[0]) len += sprintf(str+len, "%s ", sv);
		sv = rig_strvfo(vfo & RIG_VFO_MAIN);
		if (sv && sv[0]) len += sprintf(str+len, "%s ", sv);
		sv = rig_strvfo(vfo & RIG_VFO_SUB);
		if (sv && sv[0]) len += sprintf(str+len, "%s ", sv);

		for (i=0; i<16; i++) {
			sv = rig_strvfo(vfo & RIG_VFO_N(i));
			if (sv && sv[0]) len += sprintf(str+len, "%s ", sv);
		}

		return len;
}

int sprintf_mode(char *str, rmode_t mode)
{
		int i, len=0;

		*str = '\0';
		if (mode == RIG_MODE_NONE)
				return 0;

		for (i = 0; i < 30; i++) {
				const char *ms = rig_strrmode(mode & (1UL<<i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}

int sprintf_func(char *str, setting_t func)
{
		int i, len=0;

		*str = '\0';
		if (func == RIG_FUNC_NONE)
				return 0;

		for (i = 0; i < RIG_SETTING_MAX; i++) {
				const char *ms = rig_strfunc(func & rig_idx2setting(i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}


int sprintf_level(char *str, setting_t level)
{
		int i, len=0;

		*str = '\0';
		if (level == RIG_LEVEL_NONE)
				return 0;

		for (i = 0; i < RIG_SETTING_MAX; i++) {
				const char *ms = rig_strlevel(level & rig_idx2setting(i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}

int sprintf_level_gran(char *str, setting_t level, const gran_t gran[])
{
		int i, len=0;

		*str = '\0';
		if (level == RIG_LEVEL_NONE)
				return 0;

		for (i = 0; i < RIG_SETTING_MAX; i++) {
				const char *ms;

				if (!(level & rig_idx2setting(i)))
					continue;
				ms = rig_strlevel(level & rig_idx2setting(i));
				if (!ms || !ms[0]) {
					if (level != DUMMY_ALL && level != RIG_LEVEL_SET(DUMMY_ALL))
						rig_debug(RIG_DEBUG_BUG, "unknown level idx %d\n", i);
					continue;
				}
				if (RIG_LEVEL_IS_FLOAT(rig_idx2setting(i)))
					len += sprintf(str+len, "%s(%g..%g/%g) ", ms,
						gran[i].min.f,gran[i].max.f,gran[i].step.f);
				else
					len += sprintf(str+len, "%s(%d..%d/%d) ", ms,
						gran[i].min.i,gran[i].max.i,gran[i].step.i);
		}
		return len;
}


int sprintf_parm(char *str, setting_t parm)
{
		int i, len=0;

		*str = '\0';
		if (parm == RIG_PARM_NONE)
				return 0;

		for (i = 0; i < RIG_SETTING_MAX; i++) {
				const char *ms = rig_strparm(parm & rig_idx2setting(i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}

int sprintf_parm_gran(char *str, setting_t parm, const gran_t gran[])
{
		int i, len=0;

		*str = '\0';
		if (parm == RIG_PARM_NONE)
				return 0;

		for (i = 0; i < RIG_SETTING_MAX; i++) {
				const char *ms;
				if (!(parm & rig_idx2setting(i)))
					continue;
				ms = rig_strparm(parm & rig_idx2setting(i));
				if (!ms || !ms[0]) {
					if (parm != DUMMY_ALL && parm != RIG_PARM_SET(DUMMY_ALL))
						rig_debug(RIG_DEBUG_BUG, "unknown parm idx %d\n", i);
					continue;
				}
				if (RIG_PARM_IS_FLOAT(rig_idx2setting(i)))
					len += sprintf(str+len, "%s(%g..%g/%g) ", ms,
						gran[i].min.f,gran[i].max.f,gran[i].step.f);
				else
					len += sprintf(str+len, "%s(%d..%d/%d) ", ms,
						gran[i].min.i,gran[i].max.i,gran[i].step.i);
		}
		return len;
}


int sprintf_vfop(char *str, vfo_op_t op)
{
		int i, len=0;

		*str = '\0';
		if (op == RIG_OP_NONE)
				return 0;

		for (i = 0; i < 30; i++) {
				const char *ms = rig_strvfop(op & (1UL<<i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}


int sprintf_scan(char *str, scan_t rscan)
{
		int i, len=0;

		*str = '\0';
		if (rscan == RIG_SCAN_NONE)
				return 0;

		for (i = 0; i < 30; i++) {
				const char *ms = rig_strscan(rscan & (1UL<<i));
				if (!ms || !ms[0])
						continue;	/* unknown, FIXME! */
				strcat(str, ms);
				strcat(str, " ");
				len += strlen(ms) + 1;
		}
		return len;
}


