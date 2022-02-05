#include <stdio.h>
#include <hamlib/rig.h>

// we have a const *char HAMLIB_CHECK_RIG_CAPS defined in most backends
// if the structure ever changes it will produce an error during rig_init
int main()
{
    printf("Check rig_caps offsets\n");
    printf("If structure changes will break shared library API\n");
    RIG *rig;
    rig_set_debug_level(RIG_DEBUG_ERR);
    rig = rig_init(1);

    if (rig == NULL) { return 1; }

    printf("Offsets are OK (i.e. have not changed)\n");
    return 0;
}
