#include <stdio.h>
#include <hamlib/rig.h>

int main()
{
    printf("Check rig_caps offsets\n");
    printf("If changed will break shared library API\n");
    RIG *rig;
    int retcode = 0;
    rig_set_debug_level(RIG_DEBUG_NONE);
    rig = rig_init(1);
    void *p1 = &rig->state.rigport;
    void *p2 = &rig->state.vfo_list;
    unsigned long offset = p2 - p1;
    printf("offset vfo_list=%lu -- this is the important one\n", offset);
#if defined(WIN64) || defined (_WIN64) || defined (__WIN64__)
    printf("Testing WIN64\n");
    int expected = 13312; // mingw64
#elif defined(WIN32) || defined (_WIN32) || defined(__WIN32__)
    printf("Testing WIN32\n");
    int expected = 10168; // mingw32
#else
    printf("Testing Other\n");
    int expected = 13328; // should be most 64-bit compilers
#endif

    if (offset == 9384) { expected = 9384; }  // 32-bit Intel

    if (offset == 10144) { expected = 10144; } // 32-bit Arm

    if (offset != expected)
    {
        printf("offset of vfo_list has changed!!!\n");
        printf("was %d, now %lu\n", expected, offset);
        retcode = 1;
    }

    p2 = &rig->state.power_max;
    offset = p2 - p1;
    printf("offset power_max=%lu\n", offset);

#if defined(WIN64) || defined (_WIN64) || defined (__WIN64__)
    printf("Testing WIN64\n");
    expected = 14124; // mingw64
#elif defined(WIN32) || defined (_WIN32) || defined(__WIN32__)
    printf("Testing WIN32\n");
    expected = 10734; // mingw32
#else
    printf("Testing Other\n");
    expected = 14188;
#endif

    if (offset == 9676) { expected = 9676; } // 32-bit Intel

    if (offset == 10448) { expected = 10448; } // 32-bit Arm

    if (offset != expected)
    {
        printf("Warning...offset of power_max has changed!!!\n");
        printf("was %d, now %lu\n", expected, offset);
        retcode = 2;
    }
    if (retcode == 0)
    {
        printf("Offsets are OK (i.e. have not changed)\n");
    }

    return retcode;
}
