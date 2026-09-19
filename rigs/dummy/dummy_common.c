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
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "dummy_common.h"
#include "rigctl_protocol.h"

static int parse_long_token(const char *token, int base, long minimum,
                            long maximum, long *value)
{
    char *end;
    long parsed;

    errno = 0;
    parsed = strtol(token, &end, base);

    if (errno == ERANGE || end == token || *end != '\0' || parsed < minimum
            || parsed > maximum)
    {
        return -RIG_EPROTO;
    }

    *value = parsed;
    return RIG_OK;
}

static int parse_hex_uint64_token(const char *token, uint64_t *value)
{
    char *end;
    uintmax_t parsed;

    if (token[0] == '-')
    {
        return -RIG_EPROTO;
    }

    errno = 0;
    parsed = strtoumax(token, &end, 16);

    if (errno == ERANGE || end == token || *end != '\0' || parsed > UINT64_MAX)
    {
        return -RIG_EPROTO;
    }

    *value = (uint64_t)parsed;
    return RIG_OK;
}

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

int dummy_parse_rigctl_range(const char *value, freq_range_t *range)
{
    char *copy;
    char *saveptr = NULL;
    char *fields[7];
    char *token;
    freq_range_t parsed = { 0 };
    uint64_t modes;
    long number;
    int count = 0;
    int status = -RIG_EPROTO;

    if (value == NULL || range == NULL)
    {
        return -RIG_EPROTO;
    }

    copy = strdup(value);

    if (copy == NULL)
    {
        return -RIG_ENOMEM;
    }

    for (token = strtok_r(copy, " \t\r\n", &saveptr); token != NULL;
            token = strtok_r(NULL, " \t\r\n", &saveptr))
    {
        if (count == 7)
        {
            goto done;
        }

        fields[count++] = token;
    }

    if (count != 7
            || rigctl_parse_double(fields[0], RIGCTL_DECIMAL_DOT_ONLY,
                                   &parsed.startf) != RIG_OK
            || rigctl_parse_double(fields[1], RIGCTL_DECIMAL_DOT_ONLY,
                                   &parsed.endf) != RIG_OK
            || parse_hex_uint64_token(fields[2], &modes) != RIG_OK
            || parse_long_token(fields[3], 10, INT_MIN, INT_MAX, &number) != RIG_OK)
    {
        goto done;
    }

    parsed.modes = (rmode_t)modes;
    parsed.low_power = (int)number;

    if (parse_long_token(fields[4], 10, INT_MIN, INT_MAX, &number) != RIG_OK)
    {
        goto done;
    }

    parsed.high_power = (int)number;

    if (parse_long_token(fields[5], 16, INT_MIN, INT_MAX, &number) != RIG_OK)
    {
        goto done;
    }

    parsed.vfo = (vfo_t)number;

    if (parse_long_token(fields[6], 16, INT_MIN, INT_MAX, &number) != RIG_OK)
    {
        goto done;
    }

    parsed.ant = (ant_t)number;
    *range = parsed;
    status = RIG_OK;

done:
    free(copy);
    return status;
}

int dummy_parse_rigctl_double_list(const char *value, const char *delimiters,
                                   double *items, int capacity)
{
    char *copy;
    char *saveptr = NULL;
    char *token;
    int count = 0;

    if (value == NULL || delimiters == NULL || items == NULL || capacity <= 0)
    {
        return -RIG_EPROTO;
    }

    copy = strdup(value);

    if (copy == NULL)
    {
        return -RIG_ENOMEM;
    }

    for (token = strtok_r(copy, delimiters, &saveptr); token != NULL;
            token = strtok_r(NULL, delimiters, &saveptr))
    {
        if (count == capacity
                || rigctl_parse_double(token, RIGCTL_DECIMAL_DOT_ONLY,
                                       &items[count]) != RIG_OK)
        {
            free(copy);
            return -RIG_EPROTO;
        }

        ++count;
    }

    free(copy);
    return count == 0 ? -RIG_EPROTO : count;
}

int dummy_parse_rigctl_gran(const char *value, int floating, int *index,
                            gran_t *gran)
{
    char *copy;
    char *equals;
    char *first_comma;
    char *second_comma;
    long parsed_index;
    gran_t parsed = { 0 };
    int status = -RIG_EPROTO;

    if (value == NULL || index == NULL || gran == NULL)
    {
        return -RIG_EPROTO;
    }

    copy = strdup(value);

    if (copy == NULL)
    {
        return -RIG_ENOMEM;
    }

    equals = strchr(copy, '=');

    if (equals == NULL || equals == copy || strchr(equals + 1, '=') != NULL)
    {
        goto done;
    }

    *equals = '\0';
    first_comma = strchr(equals + 1, ',');

    if (first_comma == NULL || first_comma == equals + 1)
    {
        goto done;
    }

    *first_comma = '\0';
    second_comma = strchr(first_comma + 1, ',');

    if (second_comma == NULL || second_comma == first_comma + 1
            || second_comma[1] == '\0' || strchr(second_comma + 1, ',') != NULL)
    {
        goto done;
    }

    *second_comma = '\0';

    if (parse_long_token(copy, 10, 0, RIG_SETTING_MAX - 1,
                         &parsed_index) != RIG_OK)
    {
        goto done;
    }

    if (floating)
    {
        if (rigctl_parse_float(equals + 1, RIGCTL_DECIMAL_DOT_ONLY,
                               &parsed.min.f) != RIG_OK
                || rigctl_parse_float(first_comma + 1,
                                      RIGCTL_DECIMAL_DOT_ONLY,
                                      &parsed.max.f) != RIG_OK
                || rigctl_parse_float(second_comma + 1,
                                      RIGCTL_DECIMAL_DOT_ONLY,
                                      &parsed.step.f) != RIG_OK)
        {
            goto done;
        }
    }
    else
    {
        long number;

        if (parse_long_token(equals + 1, 10, INT_MIN, INT_MAX,
                             &number) != RIG_OK)
        {
            goto done;
        }

        parsed.min.i = (int)number;

        if (parse_long_token(first_comma + 1, 10, INT_MIN, INT_MAX,
                             &number) != RIG_OK)
        {
            goto done;
        }

        parsed.max.i = (int)number;

        if (parse_long_token(second_comma + 1, 10, INT_MIN, INT_MAX,
                             &number) != RIG_OK)
        {
            goto done;
        }

        parsed.step.i = (int)number;
    }

    *index = (int)parsed_index;
    *gran = parsed;
    status = RIG_OK;

done:
    free(copy);
    return status;
}
