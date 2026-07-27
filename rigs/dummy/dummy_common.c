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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "dummy_common.h"

struct ext_list *alloc_init_ext(const struct confparams *cfp)
{
    struct ext_list *elp;
    int nb_ext;

    if (cfp == NULL)
    {
        return NULL;
    }

    for (nb_ext = 0; !RIG_IS_EXT_END(cfp[nb_ext]); nb_ext++)
        ;

    elp = calloc((nb_ext + 1), sizeof(struct ext_list));

    if (!elp)
    {
        return NULL;
    }

    for (int i = 0; !RIG_IS_EXT_END(cfp[i]); i++)
    {
        elp[i].token = cfp[i].token;
        /* value reset already by calloc */
    }

    /* last token in array is set to 0 by calloc */

    return elp;
}

struct ext_list *find_ext(struct ext_list *elp, hamlib_token_t token)
{
    if (elp == NULL)
    {
        return NULL;
    }

    for (int i = 0; elp[i].token != 0; i++)
    {
        if (elp[i].token == token)
        {
            return &elp[i];
        }
    }

    return NULL;
}

void dummy_reset_agc_levels(
    enum agc_level_e agc_levels[HAMLIB_MAX_AGC_LEVELS],
    int *agc_level_count)
{
    for (int i = 0; i < HAMLIB_MAX_AGC_LEVELS; i++)
    {
        agc_levels[i] = RIG_AGC_NONE;
    }

    *agc_level_count = 0;
}

int dummy_parse_agc_levels(char *value,
                           enum agc_level_e agc_levels[HAMLIB_MAX_AGC_LEVELS],
                           int *agc_level_count)
{
    enum agc_level_e parsed_levels[HAMLIB_MAX_AGC_LEVELS];
    char *saveptr = NULL;
    char *token;
    int count = 0;

    if (value == NULL || value[0] == '\0' || agc_levels == NULL
            || agc_level_count == NULL)
    {
        return -RIG_EPROTO;
    }

    for (int i = 0; i < HAMLIB_MAX_AGC_LEVELS; i++)
    {
        parsed_levels[i] = RIG_AGC_NONE;
    }

    for (token = strtok_r(value, " ", &saveptr); token != NULL;
            token = strtok_r(NULL, " ", &saveptr))
    {
        char *separator;
        char *endptr;
        long code;

        if (count == HAMLIB_MAX_AGC_LEVELS)
        {
            // Truncate a longer peer list to the array capacity
            break;
        }

        separator = strchr(token, '=');

        if (separator == NULL || separator == token || separator[1] == '\0'
                || strchr(separator + 1, '=') != NULL)
        {
            return -RIG_EPROTO;
        }

        errno = 0;
        code = strtol(token, &endptr, 10);

        if (errno == ERANGE || code < INT_MIN || code > INT_MAX
                || endptr != separator)
        {
            return -RIG_EPROTO;
        }

        parsed_levels[count++] = (enum agc_level_e)code;
    }

    if (count == 0)
    {
        return -RIG_EPROTO;
    }

    memcpy(agc_levels, parsed_levels, sizeof(parsed_levels));
    *agc_level_count = count;

    return RIG_OK;
}
