#include "hamlib/config.h"

#include <errno.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "rigctl_protocol.h"

static int is_protocol_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
           || c == '\v';
}

static int is_ascii_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int validate_decimal(const char *start, const char *end,
                            enum rigctl_decimal_policy policy,
                            const char **separator)
{
    const char *p = start;
    int digits = 0;

    *separator = NULL;

    if (p < end && (*p == '+' || *p == '-'))
    {
        ++p;
    }

    while (p < end && is_ascii_digit(*p))
    {
        ++digits;
        ++p;
    }

    if (p < end && (*p == '.' || *p == ','))
    {
        if (*p == ',' && policy != RIGCTL_DECIMAL_DOT_OR_COMMA)
        {
            return -RIG_EINVAL;
        }

        *separator = p++;

        while (p < end && is_ascii_digit(*p))
        {
            ++digits;
            ++p;
        }
    }

    if (digits == 0)
    {
        return -RIG_EINVAL;
    }

    if (p < end && (*p == 'e' || *p == 'E'))
    {
        int exponent_digits = 0;

        ++p;

        if (p < end && (*p == '+' || *p == '-'))
        {
            ++p;
        }

        while (p < end && is_ascii_digit(*p))
        {
            ++exponent_digits;
            ++p;
        }

        if (exponent_digits == 0)
        {
            return -RIG_EINVAL;
        }
    }

    return p == end ? RIG_OK : -RIG_EINVAL;
}

int rigctl_format_decimal(char *buffer, size_t buffer_size,
                          const char *format, double value)
{
    const struct lconv *numeric_locale;
    const char *decimal_point;
    size_t decimal_point_length;
    char *position;
    int length;

    if (buffer == NULL || buffer_size == 0 || format == NULL || !isfinite(value))
    {
        return -RIG_EINVAL;
    }

    length = snprintf(buffer, buffer_size, format, value);

    if (length < 0)
    {
        return -RIG_EPROTO;
    }

    if ((size_t)length >= buffer_size)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: formatted value requires %d bytes, buffer holds %zu\n",
                  __func__, length + 1, buffer_size);
        return -RIG_ETRUNC;
    }

    numeric_locale = localeconv();
    decimal_point = numeric_locale == NULL ? NULL : numeric_locale->decimal_point;

    if (decimal_point == NULL || decimal_point[0] == '\0')
    {
        return -RIG_EPROTO;
    }

    decimal_point_length = strlen(decimal_point);
    position = buffer;

    while (strcmp(decimal_point, ".") != 0
            && (position = strstr(position, decimal_point)) != NULL)
    {
        *position = '.';

        if (decimal_point_length > 1)
        {
            memmove(position + 1, position + decimal_point_length,
                    strlen(position + decimal_point_length) + 1);
        }

        ++position;
    }

    return RIG_OK;
}

int rigctl_parse_double(const char *token, enum rigctl_decimal_policy policy,
                        double *value)
{
    const struct lconv *numeric_locale;
    const char *decimal_point;
    const char *start;
    const char *end;
    const char *separator;
    size_t decimal_point_length;
    size_t token_length;
    size_t normalized_length;
    char *normalized;
    char *output;
    char *parse_end;
    double parsed;
    int status;

    if (token == NULL || value == NULL)
    {
        return -RIG_EINVAL;
    }

    start = token;

    while (*start != '\0' && is_protocol_space(*start))
    {
        ++start;
    }

    end = start + strlen(start);

    while (end > start && is_protocol_space(end[-1]))
    {
        --end;
    }

    status = validate_decimal(start, end, policy, &separator);

    if (status != RIG_OK)
    {
        return status;
    }

    numeric_locale = localeconv();
    decimal_point = numeric_locale == NULL ? NULL : numeric_locale->decimal_point;

    if (decimal_point == NULL || decimal_point[0] == '\0')
    {
        return -RIG_EPROTO;
    }

    decimal_point_length = strlen(decimal_point);
    token_length = (size_t)(end - start);
    normalized_length = token_length;

    if (separator != NULL)
    {
        normalized_length += decimal_point_length - 1;
    }

    normalized = malloc(normalized_length + 1);

    if (normalized == NULL)
    {
        return -RIG_ENOMEM;
    }

    output = normalized;

    if (separator != NULL)
    {
        size_t prefix_length = (size_t)(separator - start);

        memcpy(output, start, prefix_length);
        output += prefix_length;
        memcpy(output, decimal_point, decimal_point_length);
        output += decimal_point_length;
        memcpy(output, separator + 1, (size_t)(end - separator - 1));
        output += end - separator - 1;
    }
    else
    {
        memcpy(output, start, token_length);
        output += token_length;
    }

    *output = '\0';
    errno = 0;
    parsed = strtod(normalized, &parse_end);

    if (errno == ERANGE || parse_end != output || !isfinite(parsed))
    {
        free(normalized);
        return -RIG_EINVAL;
    }

    free(normalized);
    *value = parsed;
    return RIG_OK;
}

int rigctl_parse_float(const char *token, enum rigctl_decimal_policy policy,
                       float *value)
{
    double parsed;
    int status;

    if (value == NULL)
    {
        return -RIG_EINVAL;
    }

    status = rigctl_parse_double(token, policy, &parsed);

    if (status != RIG_OK || parsed > FLT_MAX || parsed < -FLT_MAX)
    {
        return status == RIG_OK ? -RIG_EINVAL : status;
    }

    *value = (float)parsed;
    return RIG_OK;
}
