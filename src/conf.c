/*
 *  Hamlib Interface - configuration interface
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: conf.c,v 1.1 2001-07-21 12:55:04 f4cfe Exp $
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

#define HAMLIB_DLL
#include <hamlib/rig.h>

#include "conf.h"

#define TOK_EXAMPLE RIG_TOKEN_FRONTEND(1)

/*
 * Place holder for now. Here will be defined all the configuration
 * options available in the rig->state struct.
 */
static const struct confparams frontend_cfg_params[] = {
	{ TOK_EXAMPLE, "example", "Example", "Frontend conf param example",
	"0", RIG_CONF_NUMERIC, { n: { 0, 10, 1 } }
	},
	{ RIG_CONF_END, NULL, }
};

int frontend_set_conf(RIG *rig, token_t token, const char *val)
{
		return -RIG_ENIMPL;
}

int frontend_get_conf(RIG *rig, token_t token, char *val)
{
		return -RIG_ENIMPL;
}

/*
 * lookup only in backend config table
 * should use Lex to speed it up, strcmp hurts!
 */
const struct confparams *rig_confparam_lookup(RIG *rig, const char *name)
{
		const struct confparams *cfp;

		if (!rig || !rig->caps || !rig->caps->cfgparams)
				return NULL;
		for (cfp = rig->caps->cfgparams; cfp->name; cfp++)
				if (!strcmp(cfp->name, name))
						return cfp;
		return NULL;
}

token_t rig_token_lookup(RIG *rig, const char *name)
{
		const struct confparams *cfp;

		cfp = rig_confparam_lookup(rig, name);
		if (!cfp)
				return RIG_CONF_END;

		return cfp->token;
}

