/*
 *  Hamlib Interface - configuration interface
 *  Copyright (c) 2000,2001,2002 by Stephane Fillod and Frank Singleton
 *
 *	$Id: conf.c,v 1.7 2002-08-16 17:43:02 fillods Exp $
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

#include "conf.h"
#include "token.h"


/*
 * Place holder for now. Here will be defined all the configuration
 * options available in the rig->state struct.
 */
static const struct confparams frontend_cfg_params[] = {
	{ TOK_PATHNAME, "rig_pathname", "Rig path name", 
			"Path name to the device file of the rig",
			"/dev/rig", RIG_CONF_STRING, 
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
	{ TOK_ITU_REGION, "itu_region", "ITU region", 
			"ITU region this rig has been manufactured for (freq. band plan)",
			"0", RIG_CONF_NUMERIC, { .n = { 1, 3, 1 } }
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
	{ TOK_VFO_COMP, "vfo_comp", "VFO compensation", 
			"VFO compensation in ppm",
			"0", RIG_CONF_NUMERIC, { .n = { 0.0, 1000.0, .001 } }
	},

	{ RIG_CONF_END, NULL, }
};

/*
 * frontend_set_conf
 * assumes rig!=NULL, val!=NULL
 * TODO: check format of val before doing atoi().
 */
int frontend_set_conf(RIG *rig, token_t token, const char *val)
{
		const struct rig_caps *caps;
		struct rig_state *rs;

		caps = rig->caps;
		rs = &rig->state;

		switch(token) {
		case TOK_PATHNAME:
				strcpy(rs->rigport.pathname, val);
				break;
		case TOK_WRITE_DELAY:
				rs->rigport.write_delay = atoi(val);
				break;
		case TOK_POST_WRITE_DELAY:
				rs->rigport.post_write_delay = atoi(val);
				break;
		case TOK_TIMEOUT:
				rs->rigport.timeout = atoi(val);
				break;
		case TOK_RETRY:
				rs->rigport.retry = atoi(val);
				break;

		case TOK_SERIAL_SPEED:
				rs->rigport.parm.serial.rate = atoi(val);
				break;
		case TOK_DATA_BITS:
				rs->rigport.parm.serial.data_bits = atoi(val);
				break;
		case TOK_STOP_BITS:
				rs->rigport.parm.serial.stop_bits = atoi(val);
				break;
		case TOK_PARITY:
				if (!strncmp(val, "None", 8))
					rs->rigport.parm.serial.parity = RIG_PARITY_NONE;
				else if (!strncmp(val, "Odd", 8))
					rs->rigport.parm.serial.parity = RIG_PARITY_ODD;
				else if (!strncmp(val, "Even", 8))
					rs->rigport.parm.serial.parity = RIG_PARITY_EVEN;
				else 
						return -RIG_EINVAL;
				break;
		case TOK_HANDSHAKE:
				if (!strncmp(val, "None", 8))
					rs->rigport.parm.serial.handshake = RIG_HANDSHAKE_NONE;
				else if (!strncmp(val, "XONXOFF", 8))
					rs->rigport.parm.serial.handshake = RIG_HANDSHAKE_XONXOFF;
				else if (!strncmp(val, "Hardware", 8))
					rs->rigport.parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
				else 
						return -RIG_EINVAL;
				break;

		case TOK_ITU_REGION:
				rs->itu_region = atoi(val);
                switch(rs->itu_region) {
				case RIG_ITU_REGION1:
					memcpy(rs->tx_range_list, caps->tx_range_list1,
							sizeof(struct freq_range_list)*FRQRANGESIZ);
					memcpy(rs->rx_range_list, caps->rx_range_list1,
							sizeof(struct freq_range_list)*FRQRANGESIZ);
					break;
				case RIG_ITU_REGION2:
				case RIG_ITU_REGION3:
				default:
					memcpy(rs->tx_range_list, caps->tx_range_list2,
							sizeof(struct freq_range_list)*FRQRANGESIZ);
					memcpy(rs->rx_range_list, caps->rx_range_list2,
							sizeof(struct freq_range_list)*FRQRANGESIZ);
					break;
					}
				break;

		case TOK_VFO_COMP:
				rs->vfo_comp = atof(val);
				break;

		default:
				return -RIG_EINVAL;
		}
		return RIG_OK;
}

/*
 * frontend_get_conf
 * assumes rig!=NULL, val!=NULL
 */
int frontend_get_conf(RIG *rig, token_t token, char *val)
{
		const struct rig_caps *caps;
		struct rig_state *rs;
		const char *s;

		caps = rig->caps;
		rs = &rig->state;

		switch(token) {
		case TOK_PATHNAME:
				strcpy(val, rs->rigport.pathname);
				break;
		case TOK_WRITE_DELAY:
				sprintf(val, "%d", rs->rigport.write_delay);
				break;
		case TOK_POST_WRITE_DELAY:
				sprintf(val, "%d", rs->rigport.post_write_delay);
				break;
		case TOK_TIMEOUT:
				sprintf(val, "%d", rs->rigport.timeout);
				break;
		case TOK_RETRY:
				sprintf(val, "%d", rs->rigport.retry);
				break;
		case TOK_ITU_REGION:
				sprintf(val, "%d", 
					rs->itu_region == 1 ? RIG_ITU_REGION1 : RIG_ITU_REGION2);
				break;
		case TOK_SERIAL_SPEED:
				sprintf(val, "%d", rs->rigport.parm.serial.rate);
				break;
		case TOK_DATA_BITS:
				sprintf(val, "%d", rs->rigport.parm.serial.data_bits);
				break;
		case TOK_STOP_BITS:
				sprintf(val, "%d", rs->rigport.parm.serial.stop_bits);
				break;
		case TOK_PARITY:
				switch (rs->rigport.parm.serial.parity) {
				case RIG_PARITY_NONE: s = "None"; break;
				case RIG_PARITY_ODD: s = "Odd"; break;
				case RIG_PARITY_EVEN: s = "Even"; break;
				default: return -RIG_EINVAL;
				}
				strcpy(val, s);
				break;
		case TOK_HANDSHAKE:
				switch (rs->rigport.parm.serial.handshake) {
				case RIG_HANDSHAKE_NONE: s = "None"; break;
				case RIG_HANDSHAKE_XONXOFF: s = "XONXOFF"; break;
				case RIG_HANDSHAKE_HARDWARE: s = "Hardware"; break;
				default: return -RIG_EINVAL;
				}
				strcpy(val, s);
				break;

		case TOK_VFO_COMP:
				sprintf(val, "%f", rs->vfo_comp);
				break;

		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * rig_token_foreach
 * executes cfunc on all the elements stored in the conf table
 * start first with backend conf table, then finish with frontend table
 */
int rig_token_foreach(RIG *rig, int (*cfunc)(const struct confparams *, rig_ptr_t), rig_ptr_t data)
{
	const struct confparams *cfp;

	if (!rig || !rig->caps || !cfunc)
			return -RIG_EINVAL;

	for (cfp = rig->caps->cfgparams; cfp && cfp->name; cfp++)
			if ((*cfunc)(cfp, data) == 0)
					return RIG_OK;
	for (cfp = frontend_cfg_params; cfp->name; cfp++)
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
const struct confparams *rig_confparam_lookup(RIG *rig, const char *name)
{
		const struct confparams *cfp;

		if (!rig || !rig->caps)
				return NULL;
		for (cfp = rig->caps->cfgparams; cfp && cfp->name; cfp++)
				if (!strcmp(cfp->name, name))
						return cfp;
		for (cfp = frontend_cfg_params; cfp->name; cfp++)
				if (!strcmp(cfp->name, name))
						return cfp;
		return NULL;
}

/*
 * Simple lookup returning token id assicated with name
 */
token_t rig_token_lookup(RIG *rig, const char *name)
{
		const struct confparams *cfp;

		cfp = rig_confparam_lookup(rig, name);
		if (!cfp)
				return RIG_CONF_END;

		return cfp->token;
}


