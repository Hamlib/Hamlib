/*
 *  Hamlib TAPR backend - main file
 *  Copyright (c) 2003 by Stephane Fillod
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

#include <hamlib/config.h>

#include "hamlib/rig.h"
#include "register.h"
#include "serial.h"

#include "tapr.h"

#define ESC '$'
#define CMD_LEN 6

#define CMD7 7

#define NU1 1
#define NU2 2
#define NU3 3
#define NU4 4

/*
 * tapr_cmd
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
static int tapr_cmd(RIG *rig, unsigned char cmd, unsigned char c1,
                    unsigned char c2, unsigned char c3, unsigned char c4)
{
    int retval;
    struct rig_state *rs;
    unsigned char cmdbuf[CMD_LEN];

    rs = &rig->state;

    rig_flush(&rs->rigport);

    cmdbuf[0] = ESC;
    cmdbuf[1] = cmd;
    cmdbuf[2] = c1;
    cmdbuf[3] = c2;
    cmdbuf[4] = c3;
    cmdbuf[5] = c4;

    retval = write_block(&rs->rigport, cmdbuf, 6);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}



/*
 * tapr_set_freq
 * Assumes rig!=NULL
 */
int tapr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned int dsp_dph, dsp_deltap_lo, dsp_deltap_hi;
    int retval;

    return -RIG_ENIMPL; /* FIXME! */

    dsp_dph = (unsigned int)(1.365333333 * (double)(freq - MHz(144) + 15000UL));
    dsp_deltap_lo = 0xff & dsp_dph;
    dsp_deltap_hi = 0xff & (dsp_dph >> 8);
    retval =  tapr_cmd(rig, CMD7, 0, NU1, dsp_deltap_lo, dsp_deltap_hi);

    return retval;
}


/*
 * tapr_set_mode
 * Assumes rig!=NULL
 */
int tapr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    return -RIG_ENIMPL;
}


/*
 * initrigs_tapr is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(tapr)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&dsp10_caps);

    return RIG_OK;
}


