
/*
 * Very simple test program to check freq conversion --SF
 * This is mainly to test kHz, MHz, GHz macros and int64_t support.
 */

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <hamlib/rig.h>
#include "misc.h"


int main(int argc, char *argv[])
{
    freq_t f = 0;

#if 0

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <freq>\n", argv[0]);
        exit(1);
    }

    f = atoi(argv[1]);
#endif

    printf("%s\n", hamlib_version);
    printf("caps size: %lu\n", (long unsigned) sizeof(struct rig_caps));
    printf("state size: %lu\n", (long unsigned) sizeof(struct rig_state));
    printf("RIG size: %lu\n", (long unsigned) sizeof(RIG));
    printf("freq_t size: %lu\n", (long unsigned) sizeof(freq_t));
    printf("shortfreq_t size: %lu\n", (long unsigned) sizeof(shortfreq_t));

    /* freq on 31bits test */
    f = GHz(2);
    // cppcheck-suppress *
    printf("GHz(2) = %"PRIll"\n", (int64_t)f);

    /* freq on 32bits test */
    f = GHz(4);
    printf("GHz(4) = %"PRIll"\n", (int64_t)f);

    /* freq on >32bits test */
    f = GHz(5);
    printf("GHz(5) = %"PRIll"\n", (int64_t)f);

    /* floating point to freq conversion test */
    f = GHz(1.3);
    printf("GHz(1.3) = %"PRIll"\n", (int64_t)f);

    /* floating point to freq conversion precision test */
    f = GHz(1.234567890);
    printf("GHz(1.234567890) = %"PRIll"\n", (int64_t)f);

    /* floating point to freq conversion precision test, with freq >32bits */
    f = GHz(123.456789012);
    printf("GHz(123.456789012) = %"PRIll"\n", (int64_t)f);

    return 0;
}
