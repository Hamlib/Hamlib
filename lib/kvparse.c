/* Generic key=value argument parser for Hamlib daemon command protocols. */

#include "kvparse.h"
#include <string.h>


int hamlib_parse_kv_args(FILE *fin, struct hamlib_kv_pair *pairs,
                         int max_pairs)
{
    int count = 0;
    int ch;

    while (count < max_pairs
            && (ch = fgetc(fin)) != EOF && ch != '\n' && ch != '\r')
    {
        char buf[576];  /* 64 key + '=' + 511 value */
        char *eq;

        if (ch == ' ' || ch == '\t')
        {
            continue;
        }

        ungetc(ch, fin);

        if (fscanf(fin, "%575s", buf) < 1)
        {
            break;
        }

        eq = strchr(buf, '=');

        if (!eq)
        {
            break;  /* not a key=value token */
        }

        *eq = '\0';

        if (strlen(buf) >= sizeof(pairs[0].key)
                || strlen(eq + 1) >= sizeof(pairs[0].value))
        {
            break;  /* key or value too long */
        }

        strncpy(pairs[count].key, buf, sizeof(pairs[0].key) - 1);
        pairs[count].key[sizeof(pairs[0].key) - 1] = '\0';
        strncpy(pairs[count].value, eq + 1, sizeof(pairs[0].value) - 1);
        pairs[count].value[sizeof(pairs[0].value) - 1] = '\0';

        count++;
    }

    return count;
}
