/*
 *  Hamlib Interface - rotator configuration interface
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: rot_conf.c,v 1.4 2004-10-02 10:32:08 fillods Exp $
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

#include <hamlib/rotator.h>

#include "rot_conf.h"
#include "token.h"


/*
 * Place holder for now. Here will be defined all the configuration
 * options available in the rot->state struct.
 */
static const struct confparams rotfrontend_cfg_params[] = {
	{ TOK_PATHNAME, "rot_pathname", "Rig path name", 
			"Path name to the device file of the rotator",
			"/dev/rotator", RIG_CONF_STRING, 
	},
	{ TOK_WRITE_DELAY, "write_delay", "Write delay", 
			"Delay in ms between each byte sent out",
			"0", RIG_CONF_NUMERIC, { .n = { 0, 1000, 1 } }
	},
	{ TOK_POST_WRITE_DELAY, "post_write_delay", "Post write delay", 
			"Delay in ms between each command sent out",
			"0", RIG_CONF_NUMERIC, { .n = { 0, 1000, 1 } }
	},
	{ TOK_TIMEOUT, "timeout", "Timeout", "Timeout in ms",
			"0", RIG_CONF_NUMERIC, { .n = { 0, 10000, 1 } }
	},
	{ TOK_RETRY, "retry", "Retry", "Max number of retry",
			"0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } }
	},

	{ TOK_SERIAL_SPEED, "serial_speed", "Serial speed", 
			"Serial port baud rate",
			"0", RIG_CONF_NUMERIC, { .n = { 300, 115200, 1 } }
	},
	{ TOK_DATA_BITS, "data_bits", "Serial data bits", 
			"Serial port data bits",
			"8", RIG_CONF_NUMERIC, { .n = { 5, 8, 1 } }
	},
	{ TOK_STOP_BITS, "stop_bits", "Serial stop bits", 
			"Serial port stop bits",
			"1", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } }
	},
	{ TOK_PARITY, "serial_parity", "Serial parity", 
			"Serial port parity",
			"None", RIG_CONF_COMBO, { .c = {{ "None", "Odd", "Even", NULL }} }
	},
	{ TOK_HANDSHAKE, "serial_handshake", "Serial handshake", 
			"Serial port handshake",
			"None", RIG_CONF_COMBO, { .c = {{ "None", "XONXOFF", "Hardware", NULL }} }
	},

	{ TOK_MIN_AZ, "min_az", "Minimum azimuth",
			"Minimum rotator azimuth in degrees",
			"-180", RIG_CONF_NUMERIC, { .n = { -360, 360, .001 } }
	},
	{ TOK_MAX_AZ, "max_az", "Maximum azimuth",
			"Maximum rotator azimuth in degrees",
			"180", RIG_CONF_NUMERIC, { .n = { -360, 360, .001 } }
	},
	{ TOK_MIN_EL, "min_el", "Minimum elevation",
			"Minimum rotator elevation in degrees",
			"0", RIG_CONF_NUMERIC, { .n = { -90, 180, .001 } }
	},
	{ TOK_MAX_EL, "max_el", "Maximum elevation",
			"Maximum rotator elevation in degrees",
			"90", RIG_CONF_NUMERIC, { .n = { -90, 180, .001 } }
	},

	{ RIG_CONF_END, NULL, }
};

/*
 * frontrot_set_conf
 * assumes rot!=NULL, val!=NULL
 * TODO: check format of val before doing atoi().
 */
int frontrot_set_conf(ROT *rot, token_t token, const char *val)
{
		const struct rot_caps *caps;
		struct rot_state *rs;

		caps = rot->caps;
		rs = &rot->state;

		switch(token) {
		case TOK_PATHNAME:
				strcpy(rs->rotport.pathname, val);
				break;
		case TOK_WRITE_DELAY:
				rs->rotport.write_delay = atoi(val);
				break;
		case TOK_POST_WRITE_DELAY:
				rs->rotport.post_write_delay = atoi(val);
				break;
		case TOK_TIMEOUT:
				rs->rotport.timeout = atoi(val);
				break;
		case TOK_RETRY:
				rs->rotport.retry = atoi(val);
				break;

		case TOK_SERIAL_SPEED:
				rs->rotport.parm.serial.rate = atoi(val);
				break;
		case TOK_DATA_BITS:
				rs->rotport.parm.serial.data_bits = atoi(val);
				break;
		case TOK_STOP_BITS:
				rs->rotport.parm.serial.stop_bits = atoi(val);
				break;
		case TOK_PARITY:
				if (!strncmp(val, "None", 8))
					rs->rotport.parm.serial.parity = RIG_PARITY_NONE;
				else if (!strncmp(val, "Odd", 8))
					rs->rotport.parm.serial.parity = RIG_PARITY_ODD;
				else if (!strncmp(val, "Even", 8))
					rs->rotport.parm.serial.parity = RIG_PARITY_EVEN;
				else 
						return -RIG_EINVAL;
				break;
		case TOK_HANDSHAKE:
				if (!strncmp(val, "None", 8))
					rs->rotport.parm.serial.handshake = RIG_HANDSHAKE_NONE;
				else if (!strncmp(val, "XONXOFF", 8))
					rs->rotport.parm.serial.handshake = RIG_HANDSHAKE_XONXOFF;
				else if (!strncmp(val, "Hardware", 8))
					rs->rotport.parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
				else 
						return -RIG_EINVAL;
				break;

		case TOK_MIN_AZ:
				rs->min_az = atof(val);
				break;
		case TOK_MAX_AZ:
				rs->max_az = atof(val);
				break;
		case TOK_MIN_EL:
				rs->min_el = atof(val);
				break;
		case TOK_MAX_EL:
				rs->max_el = atof(val);
				break;

		default:
				return -RIG_EINVAL;
		}
		return RIG_OK;
}

/*
 * frontrot_get_conf
 * assumes rot!=NULL, val!=NULL
 */
int frontrot_get_conf(ROT *rot, token_t token, char *val)
{
		const struct rot_caps *caps;
		struct rot_state *rs;
		const char *s;

		caps = rot->caps;
		rs = &rot->state;

		switch(token) {
		case TOK_PATHNAME:
				strcpy(val, rs->rotport.pathname);
				break;
		case TOK_WRITE_DELAY:
				sprintf(val, "%d", rs->rotport.write_delay);
				break;
		case TOK_POST_WRITE_DELAY:
				sprintf(val, "%d", rs->rotport.post_write_delay);
				break;
		case TOK_TIMEOUT:
				sprintf(val, "%d", rs->rotport.timeout);
				break;
		case TOK_RETRY:
				sprintf(val, "%d", rs->rotport.retry);
				break;
		case TOK_SERIAL_SPEED:
				sprintf(val, "%d", rs->rotport.parm.serial.rate);
				break;
		case TOK_DATA_BITS:
				sprintf(val, "%d", rs->rotport.parm.serial.data_bits);
				break;
		case TOK_STOP_BITS:
				sprintf(val, "%d", rs->rotport.parm.serial.stop_bits);
				break;
		case TOK_PARITY:
				switch (rs->rotport.parm.serial.parity) {
				case RIG_PARITY_NONE: s = "None"; break;
				case RIG_PARITY_ODD: s = "Odd"; break;
				case RIG_PARITY_EVEN: s = "Even"; break;
				default: return -RIG_EINVAL;
				}
				strcpy(val, s);
				break;
		case TOK_HANDSHAKE:
				switch (rs->rotport.parm.serial.handshake) {
				case RIG_HANDSHAKE_NONE: s = "None"; break;
				case RIG_HANDSHAKE_XONXOFF: s = "XONXOFF"; break;
				case RIG_HANDSHAKE_HARDWARE: s = "Hardware"; break;
				default: return -RIG_EINVAL;
				}
				strcpy(val, s);
				break;

		case TOK_MIN_AZ:
				sprintf(val, "%f", rs->min_az);
				break;
		case TOK_MAX_AZ:
				sprintf(val, "%f", rs->max_az);
				break;
		case TOK_MIN_EL:
				sprintf(val, "%f", rs->min_el);
				break;
		case TOK_MAX_EL:
				sprintf(val, "%f", rs->max_el);
				break;

		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * rot_token_foreach
 * executes cfunc on all the elements stored in the conf table
 * start first with backend conf table, then finish with frontend table
 */
int HAMLIB_API rot_token_foreach(ROT *rot, int (*cfunc)(const struct confparams *, rig_ptr_t), rig_ptr_t data)
{
	const struct confparams *cfp;

	if (!rot || !rot->caps || !cfunc)
			return -RIG_EINVAL;

	for (cfp = rot->caps->cfgparams; cfp && cfp->name; cfp++)
			if ((*cfunc)(cfp, data) == 0)
					return RIG_OK;
	for (cfp = rotfrontend_cfg_params; cfp->name; cfp++)
			if ((*cfunc)(cfp, data) == 0)
					return RIG_OK;
	return RIG_OK;
}


/*
 * lookup conf token by its name, return pointer to confparams struct.
 *
 * lookup backend config table first, then fall back to frontend.
 * TODO: should use Lex to speed it up, strcmp hurts!
 */
const struct confparams * HAMLIB_API rot_confparam_lookup(ROT *rot, const char *name)
{
		const struct confparams *cfp;

		if (!rot || !rot->caps)
				return NULL;
		for (cfp = rot->caps->cfgparams; cfp && cfp->name; cfp++)
				if (!strcmp(cfp->name, name))
						return cfp;
		for (cfp = rotfrontend_cfg_params; cfp->name; cfp++)
				if (!strcmp(cfp->name, name))
						return cfp;
		return NULL;
}

/*
 * Simple lookup returning token id assicated with name
 */
token_t HAMLIB_API rot_token_lookup(ROT *rot, const char *name)
{
		const struct confparams *cfp;

		cfp = rot_confparam_lookup(rot, name);
		if (!cfp)
				return RIG_CONF_END;

		return cfp->token;
}


