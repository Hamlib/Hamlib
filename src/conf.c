/*
 *  Hamlib Interface - configuration interface
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: conf.c,v 1.2 2001-07-25 21:58:15 f4cfe Exp $
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


#define TOK_RIG_PATHNAME	RIG_TOKEN_FRONTEND(10)
#define TOK_WRITE_DELAY	RIG_TOKEN_FRONTEND(12)
#define TOK_POST_WRITE_DELAY	RIG_TOKEN_FRONTEND(13)
#define TOK_TIMEOUT	RIG_TOKEN_FRONTEND(14)
#define TOK_RETRY	RIG_TOKEN_FRONTEND(15)
#define TOK_ITU_REGION	RIG_TOKEN_FRONTEND(20)

/*
 * Place holder for now. Here will be defined all the configuration
 * options available in the rig->state struct.
 */
static const struct confparams frontend_cfg_params[] = {
	{ TOK_RIG_PATHNAME, "rig_pathname", "Rig path name", 
			"Path name to the device file of the rig",
			"/dev/rig", RIG_CONF_STRING, 
	},
	{ TOK_WRITE_DELAY, "write_delay", "Write delay", 
			"Delay in ms between each byte sent out",
			"0", RIG_CONF_NUMERIC, { n: { 0, 1000, 1 } }
	},
	{ TOK_POST_WRITE_DELAY, "post_write_delay", "Post write delay", 
			"Delay in ms between each command sent out",
			"0", RIG_CONF_NUMERIC, { n: { 0, 1000, 1 } }
	},
	{ TOK_TIMEOUT, "timeout", "Timeout", "Timeout in ms",
			"0", RIG_CONF_NUMERIC, { n: { 0, 10000, 1 } }
	},
	{ TOK_RETRY, "retry", "Retry", "Max number of retry",
			"0", RIG_CONF_NUMERIC, { n: { 0, 10, 1 } }
	},
	{ TOK_ITU_REGION, "itu_region", "ITU region", 
			"ITU region this rig has been manufactured for (freq. band plan)",
			"0", RIG_CONF_NUMERIC, { n: { 1, 3, 1 } }
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
		case TOK_RIG_PATHNAME:
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

		caps = rig->caps;
		rs = &rig->state;

		switch(token) {
		case TOK_RIG_PATHNAME:
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
		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
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

