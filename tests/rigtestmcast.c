#include <stdlib.h>
#include <hamlib/rig.h>

#define TEST
#ifdef TEST
int main(int argc, char *argv[])
{
    RIG *rig;
    rig_model_t myrig_model;
    rig_set_debug_level(RIG_DEBUG_WARN);

    if (argc > 1) { myrig_model = atoi(argv[1]); }
    else
    {
        myrig_model = 1035;
    }

    rig = rig_init(myrig_model);

    if (rig == NULL)
    {
        printf("Error in rig_init\n");
        return 1;

    }

#ifdef _WIN32
    strncpy(rig->state.rigport.pathname, "COM37", HAMLIB_FILPATHLEN - 1);
#else
    strncpy(rig->state.rigport.pathname, "/dev/ttyUSB0", HAMLIB_FILPATHLEN - 1);
#endif
    rig->state.rigport.parm.serial.rate = 38400;
    rig_open(rig);
    multicast_init(rig, "224.0.0.1", 4532);
    printf("threadid=%lld\n", (long long)rig->state.multicast->threadid);
    pthread_join(rig->state.multicast->threadid, NULL);
    pthread_exit(NULL);
    return 0;
}
#endif

