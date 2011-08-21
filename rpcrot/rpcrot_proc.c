/*
 *  Hamlib RPC server - procedures
 *  Copyright (c) 2000-2002 by Stephane Fillod and Frank Singleton
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
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

	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

	result = the_rpc_rot->caps->rot_model;

	return &result;
}

rotstate_res *
getrotstate_1_svc(void *argp, struct svc_req *rqstp)
{
	static rotstate_res res;
	struct rot_state *rs;

	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

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

	rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

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

	rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);
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

	rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

	result = rot_stop(the_rpc_rot);

	return &result;
}

int *
reset_1_svc(rot_reset_x *argp, struct svc_req *rqstp)
{
	static int  result;

	rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

	result = rot_reset(the_rpc_rot, *argp);

	return &result;
}

int *
park_1_svc(void *argp, struct svc_req *rqstp)
{
	static int  result;

	rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

	result = rot_park(the_rpc_rot);

	return &result;
}

int *
move_1_svc(move_s *argp, struct svc_req *rqstp)
{
    static int result;
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    result = rot_move(the_rpc_rot, argp->direction, argp->speed);
    return &result;
}

