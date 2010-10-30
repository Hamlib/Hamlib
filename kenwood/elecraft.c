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
 * Values stored in these variables maps to elecraft_ext_id_string_list.level
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

	err = elecraft_get_extension_level(rig, "K2", &k2_ext_lvl);
	if (err != RIG_OK)
		return err;

	/* This command will likely fail on a K2.  Needs testing
	 * to determine proper rig response
	 */
	err = elecraft_get_extension_level(rig, "K3", &k3_ext_lvl);
	if (err != RIG_OK)
		rig_debug(RIG_DEBUG_WARN, "%s: K3 probe failed\n", __func__);

	rig_debug(RIG_DEBUG_ERR, "%s: K2 level is %d, %s\n", __func__, 
		k2_ext_lvl, elecraft_ext_id_string_list[k2_ext_lvl].id);
	rig_debug(RIG_DEBUG_ERR, "%s: K3 level is %d, %s\n", __func__, 
		k3_ext_lvl, elecraft_ext_id_string_list[k3_ext_lvl].id);

	return RIG_OK;
}


/* k3_get_mode()
 * 
 * The K3 supports a new command, DT, to query the data submode so 
 * RIG_MODE_PKTUSB and RIG_MODE_PKTLSB can be supported.
 */
 
int k3_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !mode || !width)
		return -RIG_EINVAL;

	char buf[KENWOOD_MAX_BUF_LEN];
	int err;
	rmode_t temp_m;
	pbwidth_t temp_w;
	
	err = kenwood_get_mode(rig, vfo, &temp_m, &temp_w);
	if (err != RIG_OK)
		return err;

	if (temp_m == RIG_MODE_RTTY) {
		err = kenwood_safe_transaction(rig, "DT", buf, KENWOOD_MAX_BUF_LEN, 4);
		if (err != RIG_OK) {
			rig_debug(RIG_DEBUG_ERR, "%s: Cannot read K3 DT value\n", __func__);
			return err;
		}
		switch(atoi(&buf[2])) {
			case K3_MODE_DATA_A:
				*mode = RIG_MODE_PKTUSB;
				break;
			case K3_MODE_AFSK_A:
				*mode = RIG_MODE_RTTY;
				break;
			default:
				rig_debug(RIG_DEBUG_ERR, "%s: unsupported data sub-mode %c\n", __func__, buf[2]);
				return -RIG_EINVAL;
		}
	} else if (temp_m == RIG_MODE_RTTYR) {
		err = kenwood_safe_transaction(rig, "DT", buf, KENWOOD_MAX_BUF_LEN, 4);
		if (err != RIG_OK) {
			rig_debug(RIG_DEBUG_ERR, "%s: Cannot read K3 DT value\n", __func__);
			return err;
		}
		switch(atoi(&buf[2])) {
			case K3_MODE_DATA_A:
				*mode = RIG_MODE_PKTLSB;
				break;
			case K3_MODE_AFSK_A:
				*mode = RIG_MODE_RTTYR;
				break;
			default:
				rig_debug(RIG_DEBUG_ERR, "%s: unsupported data sub-mode %c\n", __func__, buf[2]);
				return -RIG_EINVAL;
		}
	} else
		*mode = temp_m;

	/* The K3 is not limited to specific filter widths so we can query
	 * the actual bandwidth using the BW command
	 */
	err = kenwood_safe_transaction(rig, "BW", buf, KENWOOD_MAX_BUF_LEN, 7);
	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_ERR, "%s: Cannot read K3 BW value\n", __func__);
		return err;
	}
	*width = atoi(&buf[2]) * 10;

	return RIG_OK;
}


/* k3_set_mode()
 * 
 * As with k3_get_mode(), the K3 can also set the data submodes which allows
 * use of RIG_MODE_PKTUSB and RIG_MODE_PKLSB.
 */
 
int k3_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err;
	char cmd_s[16];
	
	switch (mode) {
		case RIG_MODE_PKTLSB:
			mode = RIG_MODE_RTTYR;
			strncpy(cmd_s, "DT0", 5);
			break;
		case RIG_MODE_PKTUSB:
			mode = RIG_MODE_RTTY;
			strncpy(cmd_s, "DT0", 5);
			break;
		case RIG_MODE_RTTY:
		case RIG_MODE_RTTYR:
			strncpy(cmd_s, "DT1", 5);
			break;
		default:
			break;
	}

	/* kenwood_set_mode() ignores width value for K2/K3/TS-570 */
	err = kenwood_set_mode(rig, vfo, mode, width);
	if (err != RIG_OK)
		return err;

	/* Now set data sub-mode.  K3 needs to be in a DATA mode before setting
	 * the sub-mode.
	 */
	if (mode == RIG_MODE_PKTLSB || mode == RIG_MODE_PKTUSB
		|| mode == RIG_MODE_RTTY || mode == RIG_MODE_RTTYR) {
		err = kenwood_simple_cmd(rig, cmd_s);
		if (err != RIG_OK)
			return err;
	}

	/* and set the requested bandwidth.  On my K3, the bandwidth is rounded
	 * down to the nearest 50 Hz, i.e. sending BW0239; will cause the bandwidth
	 * to be set to 2.350 kHz.  As the width must be divided by 10, 10 Hz values
	 * between 0 and 4 round down to the nearest 100 Hz and values between 5
	 * and 9 round down to the nearest 50 Hz.
	 * 
	 * width string value must be padded with leading '0' to equal four 
	 * characters.
	 */
	sprintf(cmd_s, "BW%04d", width / 10);
	err = kenwood_simple_cmd(rig, cmd_s);
	if (err != RIG_OK)
		return err;

	return RIG_OK;
}


/* The K3 changes "VFOs" by swapping the contents of
 * the upper display with the lower display.  This function
 * accomplishes this by sending the emulation command, SWT11;
 * to the K3 to emulate a tap of the A/B button.
 */
 
int k3_set_vfo(RIG *rig, vfo_t vfo)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err;

	switch (vfo) {
		case RIG_VFO_B:
			err = kenwood_simple_cmd(rig, "SWT11");
			if (err != RIG_OK)
				return err;
			break;
		default:
			break;
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
