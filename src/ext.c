/*
 *  Hamlib Interface - extrq parameter interface
 *  Copyright (c) 2000,2001,2002 by Stephane Fillod
 *
 *	$Id: ext.c,v 1.2 2002-09-18 21:19:39 fillods Exp $
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

#include <hamlib/rig.h>

#include "token.h"



/*
 * rig_ext_level_foreach
 * executes cfunc on all the elements stored in the extlevels table
 */
int rig_ext_level_foreach(RIG *rig, int (*cfunc)(RIG *, const struct confparams *, rig_ptr_t), rig_ptr_t data)
{
	const struct confparams *cfp;

	if (!rig || !rig->caps || !cfunc)
		return -RIG_EINVAL;

	for (cfp = rig->caps->extlevels; cfp && cfp->name; cfp++)
		if ((*cfunc)(rig, cfp, data) == 0)
			return RIG_OK;

	return RIG_OK;
}

/*
 * rig_ext_parm_foreach
 * executes cfunc on all the elements stored in the extparms table
 */
int rig_ext_parm_foreach(RIG *rig, int (*cfunc)(RIG *, const struct confparams *, rig_ptr_t), rig_ptr_t data)
{
	const struct confparams *cfp;

	if (!rig || !rig->caps || !cfunc)
		return -RIG_EINVAL;

	for (cfp = rig->caps->extparms; cfp && cfp->name; cfp++)
		if ((*cfunc)(rig, cfp, data) == 0)
			return RIG_OK;

	return RIG_OK;
}



/*
 * lookup ext token by its name, return pointer to confparams struct.
 *
 * lookup extlevels table first, then fall back to extparms.
 *
 * Returns NULL if nothing found
 * TODO: should use Lex to speed it up, strcmp hurts!
 */
const struct confparams *rig_ext_lookup(RIG *rig, const char *name)
{
	const struct confparams *cfp;

	if (!rig || !rig->caps)
		return NULL;

	for (cfp = rig->caps->extlevels; cfp && cfp->name; cfp++)
		if (!strcmp(cfp->name, name))
			return cfp;
	for (cfp = rig->caps->extparms; cfp && cfp->name; cfp++)
		if (!strcmp(cfp->name, name))
			return cfp;
	return NULL;
}

/*
 * Simple lookup returning token id assicated with name
 */
token_t rig_ext_token_lookup(RIG *rig, const char *name)
{
	const struct confparams *cfp;

	cfp = rig_ext_lookup(rig, name);
	if (!cfp)
		return RIG_CONF_END;

	return cfp->token;
}


