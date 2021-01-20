#include <stdio.h>
#include <hamlib/rig.h>

int main()
{
    printf("Check rig_caps offsets\n");
    printf("If changed can affect shared library API\n");
    RIG *rig;
    int retcode = 0;
    rig_set_debug_level(RIG_DEBUG_NONE);
    rig = rig_init(1);
    void *p1 = &rig->state.rigport;
    void *p2 = &rig->state.vfo_list;
    unsigned long offset = p2 - p1;
    printf("offset vfo_list=%ld\n", offset);
    int expected = 13280;
    if (offset != expected)
    {
        printf("offset of vfo_list has changed!!!\n");
        printf("was %d, now %lu\n", expected, offset );
        retcode = 1;
    }

    p2 = &rig->state.power_max;
    offset = p2 - p1;
    printf("offset power_max=%ld\n", offset);

    expected = 13696;
    if (offset != expected)
    {
        printf("offset of power_max has changed!!!\n");
        printf("was %d, now %lu\n", expected, offset );
        retcode = 1;
    }

    return retcode;
}
