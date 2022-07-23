/*
 *  Hamlib RFT backend - main file
 *  Copyright (c) 2003 by Thomas B. Ruecker
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

#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "rft.h"


#define BUFSZ 64

#define CR "\x0d"
#define EOM CR


/*
 * rft_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 */
int rft_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                    int *data_len)
{
    int retval;
    struct rig_state *rs;

    rs = &rig->state;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }


    /* no data expected, TODO: flush input? */
    if (!data || !data_len)
    {
        return 0;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, CR, 1, 0, 1);

    if (retval == -RIG_ETIMEOUT)
    {
        retval = 0;
    }

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}

/*
 * rft_set_freq
 * Assumes rig!=NULL
 */
int rft_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[16], ackbuf[16];
    int ack_len, retval;

    /*
     */
    SNPRINTF(freqbuf, sizeof(freqbuf), "FRQ%f" EOM, (float)freq / 1000);
    retval = rft_transaction(rig, freqbuf, strlen(freqbuf), ackbuf, &ack_len);

    return retval;
}

/*
 * initrigs_rft is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(rft)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&ekd500_caps);

    return RIG_OK;
}

