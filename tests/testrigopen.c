/*
 * Hamlib sample program
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#include <hamlib/rig.h>

#include "misc.h"

#include <hamlib/config.h>

#define SERIAL_PORT "/dev/ttyUSB0"

int callback(const struct rig_caps *caps, rig_ptr_t rigp)
{
    RIG *rig = (RIG *) rigp;

    rig = rig_init(caps->rig_model);

    if (!rig)
    {
        fprintf(stderr, "Unknown rig num: %u\n", caps->rig_model);
        fprintf(stderr, "Please check riglist.h\n");
        exit(1); /* whoops! something went wrong (mem alloc?) */
    }

    char *port = "/dev/pts/3";
    strcpy(rig->state.rigport.pathname, port);

    printf("%20s:", caps->model_name);
    fflush(stdout);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    rig_open(rig);
    gettimeofday(&end, NULL);
    double dstart = start.tv_sec + start.tv_usec / 1e6;
    double dend = end.tv_sec + end.tv_usec / (double)1e6;
    printf(" %.1f\n",  dend - dstart);

    rig_close(rig); /* close port */
    rig_cleanup(rig); /* if you care about memory */
    return 1;
}

int main(int argc, char *argv[])
{
    RIG rig;
    printf("testing rig timeouts when rig powered off\n");

    /* Turn off backend debugging output */
    rig_set_debug_level(RIG_DEBUG_NONE);
    rig_load_all_backends();
    rig_list_foreach(callback, &rig);
    return 0;
}
