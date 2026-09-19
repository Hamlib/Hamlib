/*
 * Hamlib rigctl protocol numeric helper tests
 * Copyright (c) 2026 by Hamlib Team
 */

#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "rigctl_protocol.h"

struct parse_case
{
    const char *token;
    enum rigctl_decimal_policy policy;
    int expected_status;
    double expected_value;
};

static int check_parse_cases(void)
{
    static const struct parse_case cases[] = {
        { "7177000.125", RIGCTL_DECIMAL_DOT_ONLY, RIG_OK, 7177000.125 },
        { " .5 ", RIGCTL_DECIMAL_DOT_ONLY, RIG_OK, 0.5 },
        { "-1.25e+2", RIGCTL_DECIMAL_DOT_ONLY, RIG_OK, -125.0 },
        { "7177000,125", RIGCTL_DECIMAL_DOT_OR_COMMA, RIG_OK, 7177000.125 },
        { "7177000,125", RIGCTL_DECIMAL_DOT_ONLY, -RIG_EINVAL, 0.0 },
        { "1,2.3", RIGCTL_DECIMAL_DOT_OR_COMMA, -RIG_EINVAL, 0.0 },
        { "1.2junk", RIGCTL_DECIMAL_DOT_ONLY, -RIG_EINVAL, 0.0 },
        { "nan", RIGCTL_DECIMAL_DOT_ONLY, -RIG_EINVAL, 0.0 },
        { "inf", RIGCTL_DECIMAL_DOT_ONLY, -RIG_EINVAL, 0.0 },
        { "0x1p2", RIGCTL_DECIMAL_DOT_ONLY, -RIG_EINVAL, 0.0 },
        { "1e9999", RIGCTL_DECIMAL_DOT_ONLY, -RIG_EINVAL, 0.0 },
        { "", RIGCTL_DECIMAL_DOT_ONLY, -RIG_EINVAL, 0.0 }
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        double value = 42.0;
        int status = rigctl_parse_double(cases[i].token, cases[i].policy,
                                         &value);

        if (status != cases[i].expected_status
                || (status == RIG_OK && value != cases[i].expected_value))
        {
            fprintf(stderr, "parse '%s': expected %d/%g, got %d/%g\n",
                    cases[i].token, cases[i].expected_status,
                    cases[i].expected_value, status, value);
            return 1;
        }
    }

    return 0;
}

static int check_float_range(void)
{
    char token[64];
    float value;

    snprintf(token, sizeof(token), "%.0e", (double)FLT_MAX * 2.0);

    if (rigctl_parse_float(token, RIGCTL_DECIMAL_DOT_ONLY, &value)
            != -RIG_EINVAL)
    {
        fprintf(stderr, "out-of-range float was accepted\n");
        return 1;
    }

    return 0;
}

static int check_format(void)
{
    char buffer[32];

    if (rigctl_format_decimal(buffer, sizeof(buffer), "%.3f", 0.5) != RIG_OK
            || strcmp(buffer, "0.500") != 0)
    {
        fprintf(stderr, "expected canonical 0.500, got '%s'\n", buffer);
        return 1;
    }

    if (rigctl_format_decimal(buffer, 4, "%.3f", 0.5) != -RIG_ETRUNC)
    {
        fprintf(stderr, "format truncation was not reported\n");
        return 1;
    }

    if (rigctl_format_decimal(buffer, sizeof(buffer), "%f", INFINITY)
            != -RIG_EINVAL)
    {
        fprintf(stderr, "non-finite output was accepted\n");
        return 1;
    }

    return 0;
}

static int check_comma_locale(void)
{
    static const char *locales[] = {
        "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8",
        "German_Germany.1252", "French_France.1252"
    };
    const char *current = setlocale(LC_NUMERIC, NULL);
    char *saved = current == NULL ? NULL : strdup(current);
    char buffer[32];
    double value;
    int found = 0;
    int result = 0;

    if (current != NULL && saved == NULL)
    {
        return 1;
    }

    for (size_t i = 0; i < sizeof(locales) / sizeof(locales[0]); ++i)
    {
        if (setlocale(LC_NUMERIC, locales[i]) != NULL
                && strcmp(localeconv()->decimal_point, ",") == 0)
        {
            found = 1;
            break;
        }
    }

    if (found
            && (rigctl_format_decimal(buffer, sizeof(buffer), "%.3f", 0.5)
                != RIG_OK
                || strcmp(buffer, "0.500") != 0
                || rigctl_parse_double("0.500", RIGCTL_DECIMAL_DOT_ONLY,
                                       &value) != RIG_OK
                || value != 0.5
                || rigctl_parse_double("0,500", RIGCTL_DECIMAL_DOT_OR_COMMA,
                                       &value) != RIG_OK
                || value != 0.5
                || strcmp(localeconv()->decimal_point, ",") != 0))
    {
        fprintf(stderr, "comma-locale protocol conversion failed\n");
        result = 1;
    }

    if (!found)
    {
        fprintf(stderr, "testrigctlprotocol: comma-decimal locale unavailable; "
                "locale-specific cases not run\n");
    }

    if (saved != NULL)
    {
        setlocale(LC_NUMERIC, saved);
        free(saved);
    }

    return result;
}

int main(void)
{
    return check_parse_cases() || check_float_range() || check_format()
           || check_comma_locale();
}
