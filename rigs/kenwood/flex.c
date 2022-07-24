/*
 *  Hamlib Flex 6K series support using supported Kenwood commands
 *  Copyright (C) 2010,2011 by Nate Bargmann, n0nb@n0nb.us
 *  Copyright (C) 2011 by Alexander Sack, Alexander Sack, pisymbol@gmail.com
 *  Copyright (C) 2013 by Steve Conklin AI4QR, steve@conklinhouse.com
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */

#include <string.h>

#include "flex.h"
#include "kenwood.h"

/* Private helper functions */

int verify_flexradio_id(RIG *rig, char *id)
{
    int err;
    char *idptr;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!id)
    {
        return -RIG_EINVAL;
    }

    /* Check for a Flex 6700 which returns "904" */
    err = kenwood_get_id(rig, id);

    if (err != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: cannot get identification\n", __func__);
        return err;
    }

    /* ID is 'ID904;' */
    if (strlen(id) < 5)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: unknown ID type (%s)\n", __func__, id);
        return -RIG_EPROTO;
    }

    /* check for any white space and skip it */
    idptr = &id[2];

    if (*idptr == ' ')
    {
        idptr++;
    }

    if (strcmp("900", idptr) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig ID is %.5s (PowerSDR compatible)\n",
                  __func__, id);
    }
    else if (strcmp("904", idptr) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig ID is %.5s (Flex 6700)\n", __func__, id);
    }
    else if (strcmp("905", idptr) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig ID is %.5s (Flex 6500)\n", __func__, id);
    }
    else if (strcmp("906", idptr) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig ID is %.5s (Flex 6500R)\n", __func__, id);
    }
    else if (strcmp("907", idptr) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig ID is %.5s (Flex 6300)\n", __func__, id);
    }
    else if (strcmp("908", idptr) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig ID is %.5s (Flex 6400)\n", __func__, id);
    }
    else if (strcmp("909", idptr) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Rig ID is %.5s (Flex 6600)\n", __func__, id);
    }
    else
    {
        rig_debug(RIG_DEBUG_WARN, "%s: Rig (%.5s) is not a Flex 6000 Series\n",
                  __func__, id);
    }

    return RIG_OK;
}

/* Shared backend function definitions */

/* flexradio_open()
 *
 */

int flexradio_open(RIG *rig)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    int err;
    char id[FLEXRADIO_MAX_BUF_LEN];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    //struct flexradio_priv_data *priv = rig->state.priv;

    /* Use check for "ID017;" to verify rig is reachable */
    err = verify_flexradio_id(rig, id);

    if (err != RIG_OK)
    {
        return err;
    }

    switch (rig->caps->rig_model)
    {
    case RIG_MODEL_F6K:
        break;

    case RIG_MODEL_POWERSDR:
        break;

    default:
        rig_debug(RIG_DEBUG_WARN, "%s: unrecognized rig model %u\n",
                  __func__, rig->caps->rig_model);
        return -RIG_EINVAL;
    }

    priv->has_rit2 = 1;
    /* get current AI state so it can be restored */
    priv->trn_state = -1;
    kenwood_get_trn(rig, &priv->trn_state);  /* ignore errors */
    /* Currently we cannot cope with AI mode so turn it off in
         case last client left it on */
    kenwood_set_trn(rig, RIG_TRN_OFF); /* ignore status in case
                                                                                it's not supported */

    return RIG_OK;
}

//stopped


