/* Internal helpers for rigctl protocol decimal values. */

#ifndef HAMLIB_RIGCTL_PROTOCOL_H
#define HAMLIB_RIGCTL_PROTOCOL_H

#include <stddef.h>

enum rigctl_decimal_policy
{
    RIGCTL_DECIMAL_DOT_ONLY,
    RIGCTL_DECIMAL_DOT_OR_COMMA
};

int rigctl_format_decimal(char *buffer, size_t buffer_size,
                          const char *format, double value);
int rigctl_parse_double(const char *token, enum rigctl_decimal_policy policy,
                        double *value);
int rigctl_parse_float(const char *token, enum rigctl_decimal_policy policy,
                       float *value);

#endif
