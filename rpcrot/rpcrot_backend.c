/*
 *  Hamlib RPC backend - main file
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
 *
 *		$Id: rpcrot_backend.c,v 1.2 2002-01-22 00:48:41 fillods Exp $
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

#include <hamlib/rotator.h>
#include <serial.h>
#include <misc.h>

#include <rpc/rpc.h>
#include "rpcrot.h"
#include "rpcrot_backend.h"

struct rpcrot_priv_data {
		CLIENT *cl;
};

/* ************************************************************************* */

static int rpcrot_init(ROT *rot)
{
	if (!rot || !rot->caps)
		return -RIG_EINVAL;
	
	rot->state.priv = malloc(sizeof(struct rpcrot_priv_data));
	if (!rot->state.priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}
	rot->state.rotport.type.rig = RIG_PORT_RPC;
	strcpy(rot->state.rotport.pathname, "localhost");

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
	char *server;

	rs = &rot->state;
	priv = (struct rpcrot_priv_data *)rs->priv;
	server = rs->rotport.pathname;

	priv->cl = clnt_create(server, ROTPROG, ROTVERS, "udp");
	if (priv->cl == NULL) {
		clnt_pcreateerror(server);
		return -RIG_ECONF;
	}
	mdl_res = getmodel_1(NULL, priv->cl);
	if (mdl_res == NULL) {
		clnt_perror(priv->cl, server);
		clnt_destroy(priv->cl);
		priv->cl = NULL;
		return -RIG_EPROTO;
	}
	model = *mdl_res;
	rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ ": model %d\n", model);

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
		priv->cl = NULL;
		return -RIG_EPROTO;
	}

	if (rs_res->rotstatus != RIG_OK) {
		rig_debug(RIG_DEBUG_VERBOSE, __FUNCTION__": error from remote - %s\n", rigerror(rs_res->rotstatus));
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

/* ************************************************************************* */

struct rot_caps rpcrot_caps = {
  rot_model:     ROT_MODEL_RPC,
  model_name:    "RPC rotator",
  mfg_name:      "Hamlib",
  version:       "0.1",
  copyright:	 "LGPL",
  status:        RIG_STATUS_NEW,
  rot_type:      ROT_TYPE_OTHER,
  port_type:     RIG_PORT_NONE,
  priv: NULL,

  rot_init:    rpcrot_init,
  rot_cleanup: rpcrot_cleanup,
  rot_open:    rpcrot_open,
  rot_close:   rpcrot_close,

  set_position: rpcrot_set_position,
  get_position: rpcrot_get_position,
  stop: rpcrot_stop,
  reset: rpcrot_reset,
  park: rpcrot_park,
  move: rpcrot_move,

};

/* ************************************************************************* */

int initrots_rpcrot(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, __FUNCTION__" called\n");

	rot_register(&rpcrot_caps);

	return RIG_OK;
}

/* ************************************************************************* */
/* end of file */

