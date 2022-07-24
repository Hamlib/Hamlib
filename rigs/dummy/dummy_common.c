/*
 *  Hamlib Dummy backend - shared routines
 *  Copyright (c) 2001-2010 by Stephane Fillod
 *  Copyright (c) 2010 by Nate Bargmann
 *  Copyright (c) 2020 by Mikael Nousiainen
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

// cppcheck-suppress *
#include <stdlib.h>

#include "hamlib/rig.h"

struct ext_list *alloc_init_ext(const struct confparams *cfp)
{
    struct ext_list *elp;
    int i, nb_ext;

    for (nb_ext = 0; !RIG_IS_EXT_END(cfp[nb_ext]); nb_ext++)
        ;

    elp = calloc((nb_ext + 1), sizeof(struct ext_list));

    if (!elp)
    {
        return NULL;
    }

    for (i = 0; !RIG_IS_EXT_END(cfp[i]); i++)
    {
        elp[i].token = cfp[i].token;
        /* value reset already by calloc */
    }

    /* last token in array is set to 0 by calloc */

    return elp;
}

struct ext_list *find_ext(struct ext_list *elp, token_t token)
{
    int i;

    for (i = 0; elp[i].token != 0; i++)
    {
        if (elp[i].token == token)
        {
            return &elp[i];
        }
    }

    return NULL;
}
