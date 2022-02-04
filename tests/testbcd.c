
/*
 * Very simple test program to check BCD conversion against some other --SF
 * This is mainly to test freq2bcd and bcd2freq functions.
 */

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <hamlib/rig.h>
#include "misc.h"

#define MAXDIGITS 32

int main(int argc, char *argv[])
{
    unsigned char b[(MAXDIGITS + 1) / 2];
    freq_t f = 0;
    int digits = 10;
    int i;

    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: %s <freq> [digits]\n", argv[0]);
        exit(1);
    }

    f = (freq_t)atoll(argv[1]);

    if (argc > 2)
    {
        digits = atoi(argv[2]);

        if (digits > MAXDIGITS)
        {
            exit(1);
        }
    }

    printf("Little Endian mode\n");
    printf("Frequency: %"PRIfreq"\n", f);
    to_bcd(b, f, digits);
    printf("BCD: %2.2x", b[0]);

    for (i = 1; i < (digits + 1) / 2; i++)
    {
        printf(",%2.2x", b[i]);
    }

    // cppcheck-suppress *
    printf("\nResult after recoding: %"PRIll"\n", (int64_t)from_bcd(b, digits));

    printf("\nBig Endian mode\n");
    printf("Frequency: %"PRIfreq"\n", f);
    to_bcd_be(b, f, digits);
    printf("BCD: %2.2x", b[0]);

    for (i = 1; i < (digits + 1) / 2; i++)
    {
        printf(",%2.2x", b[i]);
    }

    printf("\nResult after recoding: %"PRIll"\n",
           (int64_t)from_bcd_be(b, digits));

    return 0;
}
