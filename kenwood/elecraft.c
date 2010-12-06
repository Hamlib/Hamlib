/*
 *  Hamlib Elecraft backend--support extensions to Kenwood commands
 *  Copyright (C) 2010 by Nate Bargmann, n0nb@n0nb.us
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */

#include <string.h>
#include <stdlib.h>
#include "token.h"

#include "elecraft.h"
#include "kenwood.h"


/* Actual read extension levels from radio.
 * 
 * Values stored in these variables map to elecraft_ext_id_string_list.level
 * and are only written to by the elecraft_get_extension_level() private
 * function during elecraft_open() and thereafter shall be treated as 
 * READ ONLY!
 */
int k2_ext_lvl;	/* Initial K2 extension level */
int k3_ext_lvl;	/* Initial K3 extension level */


static const struct elecraft_ext_id_string elecraft_ext_id_string_list[] = {
	{ K20, "K20" },
	{ K21, "K21" },
	{ K22, "K22" },
	{ K23, "K23" },
	{ K30, "K30" },
	{ K31, "K31" },
	{ EXT_LEVEL_NONE, NULL },		/* end marker */
};

/* Private function declarations */
int verify_kenwood_id(RIG *rig, char *id);
int elecraft_get_extension_level(RIG *rig, const char *cmd, int *ext_level);


/* Shared backend function definitions */

/* elecraft_open()
 * 
 * First checks for ID of '017' then tests for an Elecraft radio/backend using
 * the K2; command.  Here we also test for a K3 and if that fails, assume a K2.
 * Finally, save the value for later reading.
 * 
 */
 
int elecraft_open(RIG *rig)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err;
	char id[KENWOOD_MAX_BUF_LEN];
	
	/* Use check for "ID017;" to verify rig is reachable */
	err = verify_kenwood_id(rig, id);
	if (err != RIG_OK)
		return err;

	switch(rig->caps->rig_model) {
		case RIG_MODEL_K2:
			err = elecraft_get_extension_level(rig, "K2", &k2_ext_lvl);
			if (err != RIG_OK)
				return err;

			rig_debug(RIG_DEBUG_ERR, "%s: K2 level is %d, %s\n", __func__, 
				k2_ext_lvl, elecraft_ext_id_string_list[k2_ext_lvl].id);
			break;
		case RIG_MODEL_K3:
			err = elecraft_get_extension_level(rig, "K2", &k2_ext_lvl);
			if (err != RIG_OK)
				return err;

			rig_debug(RIG_DEBUG_ERR, "%s: K2 level is %d, %s\n", __func__, 
				k2_ext_lvl, elecraft_ext_id_string_list[k2_ext_lvl].id);

			err = elecraft_get_extension_level(rig, "K3", &k3_ext_lvl);
			if (err != RIG_OK)
				return err;

			rig_debug(RIG_DEBUG_ERR, "%s: K3 level is %d, %s\n", __func__, 
				k3_ext_lvl, elecraft_ext_id_string_list[k3_ext_lvl].id);
			break;
		default:
			rig_debug(RIG_DEBUG_ERR, "%s: unrecognized rig model %d\n", __func__, rig->caps->rig_model);
			return -RIG_EINVAL;
	}

	return RIG_OK;
}



/* Private helper functions */

/* Tests for Kenwood ID string of "017" */

int verify_kenwood_id(RIG *rig, char *id)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !id)
		return -RIG_EINVAL;

	int err;
	char *idptr;

	/* Check for an Elecraft K2|K3 which returns "017" */
	err = kenwood_get_id(rig, id);
	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_TRACE, "%s: cannot get identification\n", __func__);
		return err;
	}

	/* ID is 'ID017;' */
	if (strlen(id) < 5) {
		rig_debug(RIG_DEBUG_TRACE, "%s: unknown ID type (%s)\n", __func__, id);
		return -RIG_EPROTO;
	}

	/* check for any white space and skip it */
	idptr = &id[2];
	if (*idptr == ' ')
		idptr++;

	if (strcmp("017", idptr) != 0) {
		rig_debug(RIG_DEBUG_TRACE, "%s: Rig (%s) is not a K2 or K3\n", __func__, id);
		return -RIG_EPROTO;
	} else
		rig_debug(RIG_DEBUG_TRACE, "%s: Rig ID is %s\n", __func__, id);

	return RIG_OK;
}


/* Determines K2 and K3 extension level */

int elecraft_get_extension_level(RIG *rig, const char *cmd, int *ext_level)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !ext_level)
		return -RIG_EINVAL;

	int err, i;
	char buf[KENWOOD_MAX_BUF_LEN];
	char *bufptr;

	err = kenwood_safe_transaction(rig, cmd, buf, KENWOOD_MAX_BUF_LEN, 4);
	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_ERR, "%s: Cannot get K2|K3 ID\n", __func__);
		return err;
	}

	/* Get extension level string */
	bufptr = &buf[0];

	for (i = 0; elecraft_ext_id_string_list[i].level != EXT_LEVEL_NONE; i++) {
		if (strcmp(elecraft_ext_id_string_list[i].id, bufptr) != 0)
			continue;

		if (strcmp(elecraft_ext_id_string_list[i].id, bufptr) == 0) {
			*ext_level = elecraft_ext_id_string_list[i].level;
			rig_debug(RIG_DEBUG_TRACE, "%s: Extension level is %d, %s\n",
			__func__, *ext_level, elecraft_ext_id_string_list[i].id);
		}
	}

	return RIG_OK;
}
