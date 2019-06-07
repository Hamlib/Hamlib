/*
 *  Hamlib Elecraft backend--support Elecraft extensions to Kenwood commands
 *  Copyright (C) 2010,2011 by Nate Bargmann, n0nb@n0nb.us
 *  Copyright (C) 2011 by Alexander Sack, Alexander Sack, pisymbol@gmail.com
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */

#include <string.h>
#include <stdlib.h>

#include "serial.h"
#include "elecraft.h"
#include "kenwood.h"


static const struct elec_ext_id_str elec_ext_id_str_lst[] = {
	{ K20, "K20" },
	{ K21, "K21" },
	{ K22, "K22" },
	{ K23, "K23" },
	{ K30, "K30" },
	{ K31, "K31" },
	{ EXT_LEVEL_NONE, NULL },	/* end marker */
};


/* K3 firmware revision level, will be assigned to the fw_rev pointer in
 * kenwood_priv_data structure at runtime in electraft_open().  The array is
 * declared here so that the sizeof operator can be used in the call to
 * elecraft_get_firmware_revision_level() to calculate the exact size of the
 * array for the call to strncpy().
 */
static char k3_fw_rev[KENWOOD_MAX_BUF_LEN];


/* Private Elecraft extra levels definitions
 *
 * Token definitions for .cfgparams in rig_caps
 * See enum rig_conf_e and struct confparams in rig.h
 */
const struct confparams elecraft_ext_levels[] = {
	{ TOK_IF_FREQ, "ifctr", "IF freq", "IF center frequency",
		NULL, RIG_CONF_NUMERIC, { .n = { 0, 9990, 10 } }
	},
	{ TOK_TX_STAT, "txst", "TX status", "TX status",
		NULL, RIG_CONF_CHECKBUTTON, { { } },
	},
	{ TOK_RIT_CLR, "ritclr", "RIT clear", "RIT clear",
		NULL, RIG_CONF_BUTTON, { { } },
	},
	{ RIG_CONF_END, NULL, }
};


/* Private function declarations */
int verify_kenwood_id(RIG *rig, char *id);
int elecraft_get_extension_level(RIG *rig, const char *cmd, int *ext_level);
int elecraft_get_firmware_revision_level(RIG *rig, const char *cmd, char *fw_rev, size_t fw_rev_sz);

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
	rig_debug(RIG_DEBUG_VERBOSE, "%s called, rig version=%s\n", __func__, rig->caps->version);

	if (!rig)
		return -RIG_EINVAL;

	int err;
	char id[KENWOOD_MAX_BUF_LEN];

	/* Actual read extension levels from radio.
	 *
	 * The value stored in the k?_ext_lvl variables map to
	 * elec_ext_id_str_lst.level and is only written to by the
	 * elecraft_get_extension_level() private function during elecraft_open()
	 * and thereafter shall be treated as READ ONLY!
	 */
	struct kenwood_priv_data *priv = rig->state.priv;

	/* As k3_fw_rev is declared static, it is persistent so the structure
	 * can point to it.  This way was chosen to allow the compiler to
	 * calculate the size of the array to resolve a bug found by gcc 4.8.x
	 */
	priv->fw_rev = k3_fw_rev;

	/* Use check for "ID017;" to verify rig is reachable */
	rig_debug(RIG_DEBUG_TRACE, "%s: rig_model=%d,%d\n", __func__,rig->caps->rig_model,RIG_MODEL_XG3);
	if (rig->caps->rig_model == RIG_MODEL_XG3) { // XG3 doesn't have ID
		struct rig_state *rs = &rig->state;
		char *cmd = "V;";
		char data[32];

		strcpy(data,"EMPTY");
		// Not going to get carried away with retries and such
		err = write_block(&rs->rigport, cmd, strlen(cmd));
		if (err != RIG_OK) {
			rig_debug(RIG_DEBUG_TRACE, "%s: XG3 cannot request identification\n", __func__);
			return err;
		}
		err = read_string(&rs->rigport, id, sizeof(id), ";", 1);
		if (err < 0) {
			rig_debug(RIG_DEBUG_TRACE, "%s: XG3 cannot get identification\n", __func__);
			return err;
		}

		rig_debug(RIG_DEBUG_VERBOSE, "%s: id=%s\n", __func__,id);
#if 0
		if (err != RIG_OK) {
			rig_debug(RIG_DEBUG_TRACE, "%s: cannot get identification\n", __func__);
			return err;
		}
#endif
	}
	else { // Standard Kenwood
		err = verify_kenwood_id(rig, id);
		if (err != RIG_OK)
			return err;
	}
	switch(rig->caps->rig_model) {
	case RIG_MODEL_K2:
		err = elecraft_get_extension_level(rig, "K2", &priv->k2_ext_lvl);
		if (err != RIG_OK)
			return err;

		rig_debug(RIG_DEBUG_VERBOSE, "%s: K2 level is %d, %s\n", __func__,
			priv->k2_ext_lvl, elec_ext_id_str_lst[priv->k2_ext_lvl].id);

		break;
	case RIG_MODEL_K3:
	case RIG_MODEL_K3S:
	case RIG_MODEL_KX2:
	case RIG_MODEL_KX3:
		err = elecraft_get_extension_level(rig, "K2", &priv->k2_ext_lvl);
		if (err != RIG_OK)
			return err;

		rig_debug(RIG_DEBUG_VERBOSE, "%s: K2 level is %d, %s\n", __func__,
			priv->k2_ext_lvl, elec_ext_id_str_lst[priv->k2_ext_lvl].id);

		err = elecraft_get_extension_level(rig, "K3", &priv->k3_ext_lvl);
		if (err != RIG_OK)
			return err;

		rig_debug(RIG_DEBUG_VERBOSE, "%s: K3 level is %d, %s\n", __func__,
			priv->k3_ext_lvl, elec_ext_id_str_lst[priv->k3_ext_lvl].id);

		err = elecraft_get_firmware_revision_level(rig, "RVM", priv->fw_rev,
				(sizeof(k3_fw_rev) / sizeof(k3_fw_rev[0])));

		if (err != RIG_OK)
			return err;

		break;
	case RIG_MODEL_XG3:
		rig_debug(RIG_DEBUG_VERBOSE, "%s: XG3 level is %d, %s\n", __func__,
			priv->k3_ext_lvl, elec_ext_id_str_lst[priv->k3_ext_lvl].id);
		break;
	default:
		rig_debug(RIG_DEBUG_WARN, "%s: unrecognized rig model %d\n",
			__func__, rig->caps->rig_model);
		return -RIG_EINVAL;
	}

	if (RIG_MODEL_XG3 != rig->caps->rig_model)
		{
			/* get current AI state so it can be restored */
			priv->trn_state = -1;
			kenwood_get_trn (rig, &priv->trn_state); /* ignore errors */
			/* Currently we cannot cope with AI mode so turn it off in
				 case last client left it on */
			kenwood_set_trn(rig, RIG_TRN_OFF); /* ignore status in case
																						it's not supported */
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
		rig_debug(RIG_DEBUG_VERBOSE, "%s: cannot get identification\n", __func__);
		return err;
	}

	/* ID is 'ID017;' */
	if (strlen(id) < 5) {
		rig_debug(RIG_DEBUG_VERBOSE, "%s: unknown ID type (%s)\n", __func__, id);
		return -RIG_EPROTO;
	}

	/* check for any white space and skip it */
	idptr = &id[2];
	if (*idptr == ' ')
		idptr++;

	if (strcmp("017", idptr) != 0) {
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig (%s) is not a K2 or K3\n", __func__, id);
		return -RIG_EPROTO;
	} else
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig ID is %s\n", __func__, id);

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

	err = kenwood_safe_transaction(rig, cmd, buf, KENWOOD_MAX_BUF_LEN, 3);
	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot get K2|K3 ID\n", __func__);
		return err;
	}

	/* Get extension level string */
	bufptr = &buf[0];

	for (i = 0; elec_ext_id_str_lst[i].level != EXT_LEVEL_NONE; i++) {
		if (strcmp(elec_ext_id_str_lst[i].id, bufptr) != 0)
			continue;

		if (strcmp(elec_ext_id_str_lst[i].id, bufptr) == 0) {
			*ext_level = elec_ext_id_str_lst[i].level;
			rig_debug(RIG_DEBUG_VERBOSE, "%s: %s extension level is %d, %s\n",
			__func__, cmd, *ext_level, elec_ext_id_str_lst[i].id);
		}
	}

	return RIG_OK;
}

/* Determine firmware revision level */

int elecraft_get_firmware_revision_level(RIG *rig, const char *cmd,
                                         char *fw_rev, size_t fw_rev_sz)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !fw_rev)
		return -RIG_EINVAL;

	int err;
	char *bufptr;
	char buf[KENWOOD_MAX_BUF_LEN];

	/* Get the actual firmware revision number. */
	err = kenwood_transaction(rig, cmd, buf, sizeof (buf));

	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot get firmware revision level\n", __func__);
		return err;
	}

	/* Now buf[] contains the string from the K3 which includes the command
	 * and the firmware revision number as:  "RVM04.67".
	 */

	bufptr = &buf[0];

	/* Skip the command string */
	bufptr += strlen(cmd);

	/* Skip leading zero(s) as the revision number has the format of: "04.67" */
	while (bufptr && *bufptr == '0') { bufptr++; }

	/* Copy out */
	strncpy(fw_rev, bufptr, fw_rev_sz - 1);

	rig_debug(RIG_DEBUG_VERBOSE, "%s: Elecraft firmware revision is %s\n", __func__, fw_rev);

	return RIG_OK;
}
