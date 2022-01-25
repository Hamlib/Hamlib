/*
 * Hamlib rig_bench program
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <hamlib/rig.h>
#include <sys/time.h>
#include "misc.h"

#define LOOP_COUNT 100

#define SERIAL_PORT "/dev/ttyUSB0"


int main(int argc, char *argv[])
{
    RIG *my_rig;        /* handle to rig (nstance) */
    int retcode;        /* generic return code from functions */
    rig_model_t myrig_model;
    unsigned i;
    struct timeval tv1, tv2;
    float elapsed;

    rig_set_debug(RIG_DEBUG_ERR);

    /*
     * allocate memory, setup & open port
     */

    if (argc < 2)
    {
        hamlib_port_t myport;
        /* may be overridden by backend probe */
        myport.type.rig = RIG_PORT_SERIAL;
        myport.parm.serial.rate = 19200;
        myport.parm.serial.data_bits = 8;
        myport.parm.serial.stop_bits = 1;
        myport.parm.serial.parity = RIG_PARITY_NONE;
        myport.parm.serial.handshake = RIG_HANDSHAKE_NONE;
        strncpy(myport.pathname, SERIAL_PORT, HAMLIB_FILPATHLEN - 1);

        rig_load_all_backends();
        myrig_model = rig_probe(&myport);
    }
    else
    {
        myrig_model = atoi(argv[1]);
    }

    my_rig = rig_init(myrig_model);

    if (!my_rig)
    {
        fprintf(stderr, "Unknown rig num: %u\n", myrig_model);
        fprintf(stderr, "Please check riglist.h\n");
        exit(1);    /* whoops! something went wrong (mem alloc?) */
    }

    printf("Opened rig model %u, '%s'\n",
           my_rig->caps->rig_model,
           my_rig->caps->model_name);

    printf("Backend version: %s, Status: %s\n",
           my_rig->caps->version,
           rig_strstatus(my_rig->caps->status));

    printf("Serial speed: %d baud\n", my_rig->state.rigport.parm.serial.rate);
#if 0 // if we want to bench serial or network I/O use this time
    rig_set_cache_timeout_ms(my_rig, HAMLIB_CACHE_ALL, 0);
#endif

    strncpy(my_rig->state.rigport.pathname, SERIAL_PORT, HAMLIB_FILPATHLEN - 1);

    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
        printf("rig_open: error = %s\n", rigerror(retcode));
        exit(2);
    }

    printf("Port %s opened ok\n", SERIAL_PORT);
    printf("Perform %d loops...\n", LOOP_COUNT);

    /*
     * we're not using getrusage here because we want effective time
     */
    gettimeofday(&tv1, NULL);

    for (i = 0; i < LOOP_COUNT; i++)
    {
        freq_t freq;
        rmode_t rmode;
        pbwidth_t width;

        retcode = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);

        if (retcode != RIG_OK)
        {
            printf("rig_get_freq: error =  %s \n", rigerror(retcode));
            exit(1);
        }

        retcode = rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

        if (retcode != RIG_OK)
        {
            printf("rig_get_mode: error =  %s \n", rigerror(retcode));
            exit(1);
        }
    }

    gettimeofday(&tv2, NULL);

    elapsed = tv2.tv_sec - tv1.tv_sec + (tv2.tv_usec - tv1.tv_usec) / 1000000.0;
    printf("Elapsed: %.3fs, Avg: %f loops/s, %f s/loop\n",
           elapsed,
           LOOP_COUNT / elapsed,
           elapsed / LOOP_COUNT);

    rig_close(my_rig);      /* close port */
    rig_cleanup(my_rig);    /* if you care about memory */

    printf("port %s closed ok \n", SERIAL_PORT);

    return 0;
}
