/*
 * Hamlib sample program to test transceive mode (async event)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <hamlib/rig.h>

#include <hamlib/config.h>

#define SERIAL_PORT "/dev/ttyS0"


int myfreq_event(RIG *rig, vfo_t vfo, freq_t freq, rig_ptr_t arg)
{
    int *count_ptr = (int *) arg;

    printf("Rig changed freq to %"PRIfreq"Hz\n", freq);
    *count_ptr += 1;

    return 0;
}


int main(int argc, char *argv[])
{
    RIG *my_rig;        /* handle to rig (nstance) */
    int retcode;        /* generic return code from functions */
    int i, count = 0;

    if (argc != 2)
    {
        fprintf(stderr, "%s <rig_num>\n", argv[0]);
        exit(1);
    }

    printf("testrig: Hello, I am your main() !\n");

    /*
     * allocate memory, setup & open port
     */

    my_rig = rig_init(atoi(argv[1]));

    if (!my_rig)
    {
        fprintf(stderr, "Unknown rig num: %d\n", atoi(argv[1]));
        fprintf(stderr, "Please check riglist.h\n");
        exit(1);    /* whoops! something went wrong (mem alloc?) */
    }

    strncpy(my_rig->state.rigport.pathname, SERIAL_PORT, HAMLIB_FILPATHLEN - 1);

    if (rig_open(my_rig))
    {
        exit(2);
    }

    printf("Port %s opened ok\n", SERIAL_PORT);

    /*
     * Below are examples of set/get routines.
     * Must add checking of functionality map prior to command execution -- FS
     *
     */

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 439700000);

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    rig_set_freq_callback(my_rig, myfreq_event, (rig_ptr_t)&count);

    retcode = rig_set_trn(my_rig, RIG_TRN_RIG);

    if (retcode != RIG_OK)
    {
        printf("rig_set_trn: error = %s \n", rigerror(retcode));
    }


    for (i = 0; i < 12; i++)
    {
        printf("Loop count: %d\n", i);
        sleep(10);  /* or anything smarter */
    }

    printf("Frequency changed %d times\n", count);

    rig_close(my_rig);      /* close port */
    rig_cleanup(my_rig);    /* if you care about memory */

    printf("port %s closed ok \n", SERIAL_PORT);

    return 0;
}
