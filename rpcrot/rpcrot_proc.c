/*
 *  Hamlib RPC server - procedures
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
 *
 *		$Id: rpcrot_proc.c,v 1.1 2002-01-16 16:45:11 fgretief Exp $
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


#include <rpc/rpc.h>
#include "rpcrot.h"
#include <hamlib/rotator.h>

extern ROT *the_rpc_rot;

model_x *
getmodel_1_svc(void *argp, struct svc_req *rqstp)
{
	static model_x  result;

	rig_debug(RIG_DEBUG_VERBOSE, __FUNCTION__" called\n");

	result = the_rpc_rot->caps->rot_model;

	return &result;
}

rotstate_res *
getrotstate_1_svc(void *argp, struct svc_req *rqstp)
{
	static rotstate_res res;
	struct rot_state *rs;

	rig_debug(RIG_DEBUG_VERBOSE, __FUNCTION__" called\n");

	if (!the_rpc_rot->state.comm_state) {
		res.rotstatus = -RIG_ERJCTED;
		return &res;
	}
	rs = &the_rpc_rot->state;

	/* Copy the current state of the rotator into the transfer variable */
	res.rotstate_res_u.state.az_min = rs->min_az;
	res.rotstate_res_u.state.az_max = rs->max_az;
	res.rotstate_res_u.state.el_min = rs->min_el;
	res.rotstate_res_u.state.el_max = rs->max_el;

	res.rotstatus = RIG_OK;
	return &res;
}

int *
setposition_1_svc(position_s *argp, struct svc_req *rqstp)
{
	static int  result;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

	rig_debug(RIG_DEBUG_VERBOSE, "Setting position to %.1f deg Az, %.1f deg El\n", argp->az, argp->el);

	result = rot_set_position(the_rpc_rot, argp->az, argp->el);

	return &result;
}

position_res *
getposition_1_svc(void *argp, struct svc_req *rqstp)
{
	static position_res  res;
	azimuth_t az;
	elevation_t el;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");
	memset(&res, 0, sizeof(res));

	res.rotstatus = rot_get_position(the_rpc_rot, &az, &el);
	if (res.rotstatus == RIG_OK) {
		rig_debug(RIG_DEBUG_VERBOSE, "Getting position of %.1f deg Az, %.1f deg El\n", az, el);
		res.position_res_u.pos.az = az;
		res.position_res_u.pos.el = el;
	}
	return &res;
}

int *
stop_1_svc(void *argp, struct svc_req *rqstp)
{
	static int  result;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

	result = rot_stop(the_rpc_rot);

	return &result;
}

int *
reset_1_svc(rot_reset_x *argp, struct svc_req *rqstp)
{
	static int  result;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

	result = rot_reset(the_rpc_rot, *argp);

	return &result;
}

int *
park_1_svc(void *argp, struct svc_req *rqstp)
{
	static int  result;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

	result = rot_park(the_rpc_rot);

	return &result;
}

int *
move_1_svc(move_s *argp, struct svc_req *rqstp)
{
    static int result;
    rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

    result = rot_move(the_rpc_rot, argp->direction, argp->speed);
    return &result;
}

