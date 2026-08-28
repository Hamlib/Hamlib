/* Generic key=value argument parser for Hamlib daemon command protocols. */

#ifndef HAMLIB_KVPARSE_H
#define HAMLIB_KVPARSE_H

#include <stdio.h>

/* Key=value pair parsed from command arguments. */
struct hamlib_kv_pair
{
    char key[64];
    char value[512];
};

/* Parse key=value arguments from remaining input on the current line.
 * Reads whitespace-separated key=value tokens from fin until newline/EOF.
 * Stops at the first token that doesn't contain '='.
 * Returns number of pairs parsed (0 if none). */
int hamlib_parse_kv_args(FILE *fin, struct hamlib_kv_pair *pairs,
                         int max_pairs);

#endif /* HAMLIB_KVPARSE_H */
