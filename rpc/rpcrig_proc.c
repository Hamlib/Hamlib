/*
 *  Hamlib RPC server - procedures
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: rpcrig_proc.c,v 1.1 2001-10-07 21:42:13 f4cfe Exp $
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
#include <sys/ioctl.h>

#include <rpc/rpc.h>
#include "rpcrig.h"
#include <hamlib/rig.h>

extern RIG *the_rpc_rig;

void freq_t2freq_s(freq_t freqt, freq_s *freqs)
{
	freqs->f1 = freqt & 0xffffffff;
	freqs->f2 = (freqt>>32) & 0xffffffff;
}
freq_t freq_s2freq_t(freq_s *freqs)
{
	return freqs->f1 | ((freq_t)freqs->f2 << 32);
}

model_x *getmodel_1_svc(void *arg, struct svc_req *svc)
{
	static model_x res;

	/* free previous result */
	//xdr_free(xdr_model, &res);

	res = the_rpc_rig->caps->rig_model;

	return &res;
}

int *setfreq_1_svc(freq_arg *farg, struct svc_req *svc)
{
	static int res;

	res = rig_set_freq(the_rpc_rig, farg->vfo, freq_s2freq_t(&farg->freq));

	return &res;
}

freq_res *getfreq_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static freq_res res;
	freq_t f;

	res.rigstatus = rig_get_freq(the_rpc_rig, *vfo, &f);
	freq_t2freq_s(f, &res.freq_res_u.freq);

	return &res;
}


