/*
 * Hamlib sample program
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include <hamlib/rig.h>

#include <hamlib/config.h>

#define HISTORYSIZE 10
double history[HISTORYSIZE];
int nhistory;
int historyinit = 1;

double compute_mean(double arr[], int length) 
{
    double sum = 0.0;
    for (int i = 0; i < length; i++) {
        sum += arr[i];
    }
    return sum / length;
}

double sigma(double arr[], int length) {
    double mean = compute_mean(arr, length);
    double sum_of_squares = 0.0;
    
    for (int i = 0; i < length; i++) {
        sum_of_squares += pow(arr[i] - mean, 2);
    }
    
    return sqrt(sum_of_squares / length);
}

int main(int argc, char *argv[])
{
    RIG *my_rig;        /* handle to rig (nstance) */
    int strength;       /* S-Meter level */
    int retcode;        /* generic return code from functions */

    rig_model_t myrig_model;



    if (argc != 8)
    {
        fprintf(stderr,"%s: version 1.0\n", argv[0]);
        fprintf(stderr,"Usage: %s [model#] [comport] [baud] [start freq] [stop_freq] [stepsize] [seconds/step]\n",
               argv[0]);
        return 1;
    }

    /* Turn off backend debugging output */
    rig_set_debug_level(RIG_DEBUG_NONE);

    /*
     * allocate memory, setup & open port
     */

    hamlib_port_t myport;
    myrig_model = atoi(argv[1]);
    strncpy(myport.pathname, argv[2], HAMLIB_FILPATHLEN - 1);
    myport.parm.serial.rate = atoi(argv[3]);

    my_rig = rig_init(myrig_model);

    if (!my_rig)
    {
        fprintf(stderr, "Unknown rig num: %d\n", myrig_model);
        fprintf(stderr, "Please check riglist.h\n");
        exit(1); /* whoops! something went wrong (mem alloc?) */
    }

    strncpy(my_rig->state.rigport.pathname, argv[2], HAMLIB_FILPATHLEN - 1);

    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
        printf("rig_open: error = %s\n", rigerror(retcode));
        exit(2);
    }

//    printf("Port %s opened ok\n", SERIAL_PORT);

    long freq1, freq2;
    int stepsize, seconds;
    int n = sscanf(argv[4], "%ld", &freq1);
    n += sscanf(argv[5], "%ld", &freq2);
    n += sscanf(argv[6], "%d", &stepsize);
    n += sscanf(argv[7], "%d", &seconds);

    if (n != 4)
    {
        fprintf(stderr, "Error parsing %s/%s/%s/%s as start/stop/step/seconds\n",
                argv[4], argv[5], argv[6], argv[7]);
        return 1;
    }

    printf("Start:%ld Stop:%ld Step:%d Wait:%d\n", freq1, freq2, stepsize, seconds);

    while (1)
    {
        for (long f = freq1; f <= freq2; f += stepsize)
        {
            retcode = rig_set_freq(my_rig, RIG_VFO_CURR, (freq_t)f);
            if (retcode != RIG_OK)
            {
                fprintf(stderr, "%s: Error rig_set_freq: %s\n", __func__, rigerror(retcode));
                return 1;
            }
            sleep(seconds);
            retcode = rig_get_strength(my_rig, RIG_VFO_CURR, &strength);

            if (retcode != RIG_OK)
            {
                int static  once=1;
                if (once)
                {
                    once = 0;
                    fprintf(stderr,"rig_get_strength error: %s\n", rigerror(retcode));
                }
                strength = 1;
            }
            history[nhistory++] = strength;
            if (historyinit)
            {
                for(int i=0;i<HISTORYSIZE;++i) history[i] = strength;
                historyinit = 0;
            }
            nhistory %= HISTORYSIZE;
            double s = sigma(history, HISTORYSIZE);
            char timebuf[64];
            rig_date_strget(timebuf, sizeof(timebuf), 1);
            printf("%s\t%ld\t%d\t%f\n", timebuf, f, strength, s);
            fflush(stdout);

        }
    }

    rig_close(my_rig); /* close port */
    rig_cleanup(my_rig); /* if you care about memory */

    printf("port %s closed ok \n", argv[2]);

    return 0;
}
