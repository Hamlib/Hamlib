/*
 *  Hamlib RPC backend - main file
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
 *
 *	$Id: rpcrot_backend.c,v 1.5 2002-12-16 22:07:00 fillods Exp $
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
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>
#include <netdb.h>
#include <limits.h>

#include <hamlib/rotator.h>
#include <serial.h>
#include <misc.h>
#include <token.h>

#include <rpc/rpc.h>
#ifdef HAVE_RPC_RPCENT_H
#include <rpc/rpcent.h>
#endif

#include "rpcrot.h"
#include "rpcrot_backend.h"

struct rpcrot_priv_data {
		CLIENT *cl;
		unsigned long prognum;
};

#define ROTPROTO "udp"

/*
 * Borrowed from stringify.h
 * Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */

#define r_stringify_1(x)	#x
#define r_stringify(x)		r_stringify_1(x)


/* ************************************************************************* */

static unsigned long extract_prognum(const char val[])
{
	if (val[0] == '+') {
		return ROTPROG + atol(val+1);
	} else
		if (val[0] < '0' || val[0] > '9') {
			struct rpcent *ent;
			ent = getrpcbyname (val);
			if (ent)
				return ent->r_number;
			else
				return 0;
	} else
		return atol(val);
}


static int rpcrot_init(ROT *rot)
{
	struct rpcrot_priv_data *priv;

	if (!rot || !rot->caps)
		return -RIG_EINVAL;
	
	rot->state.priv = malloc(sizeof(struct rpcrot_priv_data));
	if (!rot->state.priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}
	priv = (struct rpcrot_priv_data*)rot->state.priv;

	rot->state.rotport.type.rig = RIG_PORT_RPC;
	strcpy(rot->state.rotport.pathname, "localhost");
	priv->prognum = ROTPROG;

	return RIG_OK;
}

static int rpcrot_cleanup(ROT *rot)
{
	if (!rot)
		return -RIG_EINVAL;

	if (rot->state.priv)
		free(rot->state.priv);

	rot->state.priv = NULL;

	return RIG_OK;
}

/* ************************************************************************* */

/*
 * assumes rot!=NULL, rs->priv != NULL
 */
static int rpcrot_open(ROT *rot)
{
	struct rpcrot_priv_data *priv;
	struct rot_state *rs;
	model_x *mdl_res;
	rotstate_res *rs_res;
	rot_model_t model;
	const struct rot_caps *caps;
	char *server, *s;

	rs = &rot->state;
	priv = (struct rpcrot_priv_data *)rs->priv;
	server = strdup(rs->rotport.pathname);
	s = strchr(server, ':');
	if (s) {
		unsigned long prognum;
		*s = '\0';
		prognum = extract_prognum(s+1);
		if (prognum == 0) {
			free(server);
			return -RIG_ECONF;
		}
		priv->prognum = prognum;
	}

	priv->cl = clnt_create(server, priv->prognum, ROTVERS, ROTPROTO);
	if (priv->cl == NULL) {
		clnt_pcreateerror(server);
		free(server);
		return -RIG_ECONF;
	}
	mdl_res = getmodel_1(NULL, priv->cl);
	if (mdl_res == NULL) {
		clnt_perror(priv->cl, server);
		clnt_destroy(priv->cl);
		free(server);
		priv->cl = NULL;
		return -RIG_EPROTO;
	}
	model = *mdl_res;
	rig_debug(RIG_DEBUG_TRACE,"%s: model %d\n", __FUNCTION__, model);

	/*
	 * autoload if necessary
	 */
	rot_check_backend(model);
	caps = rot_get_caps(model);

	/*
	 * Load state values from remote rotator.
	 */
	rs_res = getrotstate_1(NULL, priv->cl);
	if (rs_res == NULL) {
		clnt_perror(priv->cl, server);
		clnt_destroy(priv->cl);
		free(server);
		priv->cl = NULL;
		return -RIG_EPROTO;
	}

	free(server);

	if (rs_res->rotstatus != RIG_OK) {
		rig_debug(RIG_DEBUG_VERBOSE, "%s: error from remote - %s\n", __FUNCTION__, rigerror(rs_res->rotstatus));
		return rs_res->rotstatus;
	}

	rs->min_az = rs_res->rotstate_res_u.state.az_min;
	rs->max_az = rs_res->rotstate_res_u.state.az_max;
	rs->min_el = rs_res->rotstate_res_u.state.el_min;
	rs->max_el = rs_res->rotstate_res_u.state.el_max;

	return RIG_OK;
}

static int rpcrot_close(ROT *rot)
{
	struct rpcrot_priv_data *priv;

	priv = (struct rpcrot_priv_data*)rot->state.priv;

	if (priv->cl)
		clnt_destroy(priv->cl);

	return RIG_OK;
}

/* ************************************************************************* */

static int rpcrot_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
	struct rpcrot_priv_data *priv;
	int *result;
	position_s parg;

	priv = (struct rpcrot_priv_data *)rot->state.priv;

	parg.az = az;
	parg.el = el;
	result = setposition_1(&parg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setposition_1");
		return -RIG_EPROTO;
	}

	return *result;
}

static int rpcrot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
	struct rpcrot_priv_data *priv;
	position_res *pres;

	priv = (struct rpcrot_priv_data *)rot->state.priv;
	pres = getposition_1(NULL, priv->cl);
	if (pres == NULL) {
		clnt_perror(priv->cl, "getposition_1");
		return -RIG_EPROTO;
	}

	if (pres->rotstatus == RIG_OK) {
		*az = pres->position_res_u.pos.az;
		*el = pres->position_res_u.pos.el;
	}
	return pres->rotstatus;
}

static int rpcrot_stop(ROT *rot)
{
	struct rpcrot_priv_data *priv;
	int *result;

	priv = (struct rpcrot_priv_data *)rot->state.priv;
	result = stop_1(NULL, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "stop_1");
		return -RIG_EPROTO;
	}
	return *result;
}

static int rpcrot_reset(ROT *rot, rot_reset_t reset)
{
	struct rpcrot_priv_data *priv;
	int *result;
	rot_reset_x rstx;

	rstx = reset;  /* FIXME: is this correct? */
	priv = (struct rpcrot_priv_data *)rot->state.priv;
	result = reset_1(&rstx, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "reset_1");
		return -RIG_EPROTO;
	}
	return *result;
}

static int rpcrot_park(ROT *rot)
{
	struct rpcrot_priv_data *priv;
	int *result;

	priv = (struct rpcrot_priv_data *)rot->state.priv;
	result = park_1(NULL, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "park_1");
		return -RIG_EPROTO;
	}
	return *result;
}

static int rpcrot_move(ROT *rot, int direction, int speed)
{
    struct rpcrot_priv_data *priv;
    move_s arg;
    int *result;
    arg.direction = direction;
    arg.speed = speed;
    priv = (struct rpcrot_priv_data *)rot->state.priv;
    result = move_1(&arg, priv->cl);
    if (result == NULL) {
        clnt_perror(priv->cl, "move_1");
        return -RIG_EPROTO;
    }
    return *result;
}

#define TOK_PROGNUM TOKEN_BACKEND(1)

static const struct confparams rpcrot_cfg_params[] = {
	{ TOK_PROGNUM, "prognum", "Program number", "RPC program number",
			r_stringify(ROTPROG), RIG_CONF_NUMERIC, { .n = { 100000, ULONG_MAX, 1 } }
	},
	{ RIG_CONF_END, NULL, }
};

/*
 * Assumes rot!=NULL, rot->state.priv!=NULL
 */
int rpcrot_set_conf(ROT *rot, token_t token, const char *val)
{
	struct rpcrot_priv_data *priv;
	struct rot_state *rs;

	rs = &rot->state;
	priv = (struct rpcrot_priv_data*)rs->priv;

	switch(token) {
	case TOK_PROGNUM:
		{
			unsigned long prognum;
			prognum = extract_prognum(val);
			if (prognum == 0)
				return -RIG_EINVAL;
			priv->prognum = prognum;
			break;
		}
	default:
		return -RIG_EINVAL;
	}
	return RIG_OK;
}

/*
 * assumes rot!=NULL,
 * Assumes rot!=NULL, rot->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int rpcrot_get_conf(ROT *rot, token_t token, char *val)
{
	struct rpcrot_priv_data *priv;
	struct rot_state *rs;

	rs = &rot->state;
	priv = (struct rpcrot_priv_data*)rs->priv;

	switch(token) {
	case TOK_PROGNUM:
		sprintf(val, "%ld", priv->prognum);
		break;
	default:
		return -RIG_EINVAL;
	}
	return RIG_OK;
}

/* ************************************************************************* */

struct rot_caps rpcrot_caps = {
  .rot_model =      ROT_MODEL_RPC,
  .model_name =     "RPC rotator",
  .mfg_name =       "Hamlib",
  .version =        "0.1",
  .copyright = 	 "LGPL",
  .status =         RIG_STATUS_NEW,
  .rot_type =       ROT_TYPE_OTHER,
  .port_type =      RIG_PORT_NONE,
  .priv =  NULL,

  .rot_init =     rpcrot_init,
  .rot_cleanup =  rpcrot_cleanup,
  .rot_open =     rpcrot_open,
  .rot_close =    rpcrot_close,

  .cfgparams =  rpcrot_cfg_params,
  .set_conf =   rpcrot_set_conf,
  .get_conf =   rpcrot_get_conf,

  .set_position =  rpcrot_set_position,
  .get_position =  rpcrot_get_position,
  .stop =  rpcrot_stop,
  .reset =  rpcrot_reset,
  .park =  rpcrot_park,
  .move =  rpcrot_move,

};

/* ************************************************************************* */

int initrots_rpcrot(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

	rot_register(&rpcrot_caps);

	return RIG_OK;
}

/* ************************************************************************* */
/* end of file */

