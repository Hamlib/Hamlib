// Example of rig_get_rig_info
// gcc -o simple simple.s -lhamlib

#include "hamlib/rig.h"

int main(int argc, char *argv[])
{
    rig_model_t model = 1;
    RIG *rig;

    rig_set_debug_level(RIG_DEBUG_WARN);
    rig = rig_init(model);

    if (rig == NULL)
    {
        fprintf(stderr, "rig_init failed\n");
        return 1;
    }

    int retcode = rig_open(rig);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "rig_open failed: %s\n", rigerror(retcode));
        return 1;
    }

    char riginfo[1024];
    retcode = rig_get_rig_info(rig, riginfo, sizeof(riginfo));

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "rig_get_rig_info failed: %s\n", rigerror(retcode));
        return 1;
    }

    char vfo[16];
    char mode[16];
    double freq;
    sscanf(riginfo, "VFO=%15s Freq=%lf Mode=%15s", vfo, &freq, mode);
    printf("VFO=%s Freq=%.0f Mode=%s\n", vfo, freq, mode);
    printf("=========================\nEntire response:\n%s", riginfo);
    rig_close(rig);
    return 0;
}
