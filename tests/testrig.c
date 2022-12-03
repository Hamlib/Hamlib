/*
 * Hamlib sample program
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <hamlib/rig.h>

#include <hamlib/config.h>

#define SERIAL_PORT "/dev/pts/4"


int main(int argc, char *argv[])
{
    RIG *my_rig;        /* handle to rig (nstance) */
    freq_t freq;        /* frequency  */
    rmode_t rmode;      /* radio mode of operation */
    pbwidth_t width;
    vfo_t vfo;          /* vfo selection */
    int strength;       /* S-Meter level */
    int rit = 0;        /* RIT status */
    int xit = 0;        /* XIT status */
    int retcode;        /* generic return code from functions */

    rig_model_t myrig_model;


    printf("testrig: Hello, I am your main() !\n");

    /* Turn off backend debugging output */
    rig_set_debug_level(RIG_DEBUG_NONE);

    /*
     * allocate memory, setup & open port
     */

    if (argc < 2)
    {
        hamlib_port_t myport;
        /* may be overridden by backend probe */
        myport.type.rig = RIG_PORT_SERIAL;
        myport.parm.serial.rate = 9600;
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
        fprintf(stderr, "Unknown rig num: %d\n", myrig_model);
        fprintf(stderr, "Please check riglist.h\n");
        exit(1); /* whoops! something went wrong (mem alloc?) */
    }

    //strncpy(my_rig->state.rigport.pathname, SERIAL_PORT, HAMLIB_FILPATHLEN - 1);

    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
        printf("rig_open: error = %s\n", rigerror(retcode));
        exit(2);
    }

    char val[256];
    retcode = rig_get_conf2(my_rig, rig_token_lookup(my_rig, "write_delay"), val, sizeof(val));
    printf("write_delay=%s\n", val);

//    printf("Port %s opened ok\n", SERIAL_PORT);

    /*
     * Below are examples of set/get routines.
     * Must add checking of functionality map prior to command execution -- FS
     *
     */

    /*
     * Example of setting rig paameters
     * and some error checking on the return code.
     */

    retcode = rig_set_vfo(my_rig, RIG_VFO_B);


    if (retcode != RIG_OK)
    {
        printf("rig_set_vfo: error = %s \n", rigerror(retcode));
    }


    /*
     * Lets try some frequencies and modes. Return code is not checked.
     * Examples of checking return code are further down.
     *
     */

    /* 10m FM Narrow */

    printf("\nSetting 10m FM Narrow...\n");

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 29620000); /* 10m */

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_FM,
                           rig_passband_narrow(my_rig, RIG_MODE_FM));

    if (retcode != RIG_OK)
    {
        printf("rig_set_mode: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

    printf(" Freq: %.6f MHz, Mode: %s, Passband: %.3f kHz\n\n",
           freq / 1000000,
           rig_strrmode(rmode),
           width / 1000.0);

    if (freq != 29620000)
    {
        printf("rig_set_freq: error exptect %.0f got %.0f\n", 296290000.0, freq);
    }

    if (rmode != RIG_MODE_FM || width != rig_passband_narrow(my_rig, RIG_MODE_FM))
    {
        printf("rig_set_mode: error expected FM/%d, got %s/%d\n",
               (int)rig_passband_narrow(my_rig, RIG_MODE_FM), rig_strrmode(rmode), (int)width);
    }

    sleep(1);       /* so you can see it -- FS */

    /* 15m USB */

    printf("Setting 15m USB...\n");

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 21235175); /* 15m  */

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    retcode = rig_set_mode(my_rig,
                           RIG_VFO_CURR,
                           RIG_MODE_USB,
                           rig_passband_normal(my_rig, RIG_MODE_USB));

    if (retcode != RIG_OK)
    {
        printf("rig_set_mode: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

    printf(" Freq: %.6f MHz, Mode: %s, Passband: %.3f kHz\n\n",
           freq / 1000000, rig_strrmode(rmode), width / 1000.0);
    sleep(1);

    /* 40m LSB */

    printf("Setting 40m LSB...\n");

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 7250100); /* 40m  */

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    retcode = rig_set_mode(my_rig,
                           RIG_VFO_CURR,
                           RIG_MODE_LSB,
                           RIG_PASSBAND_NORMAL);

    if (retcode != RIG_OK)
    {
        printf("rig_set_mode: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

    printf(" Freq: %.6f MHz, Mode: %s, Passband: %.3f kHz\n\n",
           freq / 1000000,
           rig_strrmode(rmode),
           width / 1000.0);
    sleep(1);

    /* 80m AM Narrow */

    printf("Setting 80m AM Narrow...\n");

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 3885000); /* 80m  */

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    retcode = rig_set_mode(my_rig,
                           RIG_VFO_CURR,
                           RIG_MODE_AM,
                           rig_passband_narrow(my_rig, RIG_MODE_AM));

    if (retcode != RIG_OK)
    {
        printf("rig_set_mode: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

    printf(" Freq: %.6f MHz, Mode: %s, Passband: %.3f kHz\n\n",
           freq / 1000000,
           rig_strrmode(rmode),
           width / 1000.0);

    sleep(1);

    /* 160m CW Normal */

    printf("Setting 160m CW...\n");

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 1875000); /* 160m  */

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    retcode = rig_set_mode(my_rig,
                           RIG_VFO_CURR,
                           RIG_MODE_CW,
                           RIG_PASSBAND_NORMAL);

    if (retcode != RIG_OK)
    {
        printf("rig_set_mode: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

    printf(" Freq: %.3f kHz, Mode: %s, Passband: %li Hz\n\n",
           freq / 1000,
           rig_strrmode(rmode),
           width);

    sleep(1);

    /* 160m CW Narrow -- The band is noisy tonight -- FS*/

    printf("Setting 160m CW Narrow...\n");

    retcode = rig_set_mode(my_rig,
                           RIG_VFO_CURR,
                           RIG_MODE_CW,
                           rig_passband_narrow(my_rig, RIG_MODE_CW));

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

    printf(" Freq: %.3f kHz, Mode: %s, Passband: %li Hz\n\n",
           freq / 1000,
           rig_strrmode(rmode),
           width);

    sleep(1);

    /* AM Broadcast band */

    printf("Setting Medium Wave AM...\n");

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 770000); /* KAAM */

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    retcode = rig_set_mode(my_rig,
                           RIG_VFO_CURR,
                           RIG_MODE_AM,
                           RIG_PASSBAND_NORMAL);

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

    printf(" Freq: %.3f kHz, Mode: %s, Passband: %.3f kHz\n\n",
           freq / 1000,
           rig_strrmode(rmode),
           width / 1000.0);

    sleep(1);

    /* 20m USB on VFO_A */

    printf("Setting 20m on VFO A with two functions...\n");

    retcode = rig_set_vfo(my_rig, RIG_VFO_A);

    if (retcode != RIG_OK)
    {
        printf("rig_set_vfo: error = %s \n", rigerror(retcode));
    }

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 14250375); /* cq de vk3fcs */

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_vfo(my_rig, &vfo);

    printf(" Freq: %.6f MHz, VFO: %s\n\n", freq / 1000000, rig_strvfo(vfo));

    sleep(1);

    /* 20m USB on VFO_A , with only 1 call */

    printf("Setting  20m on VFO A with one function...\n");
    retcode = rig_set_freq(my_rig, RIG_VFO_A, 14295125); /* cq de vk3fcs */

    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s \n", rigerror(retcode));
    }

    rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    rig_get_vfo(my_rig, &vfo);

    printf(" Freq: %.6f MHz, VFO: %s\n\n", freq / 1000000, rig_strvfo(vfo));

    sleep(1);


#if 0
    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 145100000); /* 2m  */
    sleep(2);
    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 435125000); /* 70cm  */
    sleep(2);
#endif

    printf("Setting rig Mode to LSB.\n");
    retcode = rig_set_mode(my_rig,
                           RIG_VFO_CURR,
                           RIG_MODE_LSB,
                           RIG_PASSBAND_NORMAL);

    if (retcode != RIG_OK)
    {
        printf("rig_set_mode: error = %s \n", rigerror(retcode));
    }

    sleep(1);

    printf("Setting rig PTT ON.\n");
    retcode = rig_set_ptt(my_rig, RIG_VFO_A, RIG_PTT_ON);  /* stand back ! */

    if (retcode != RIG_OK)
    {
        printf("rig_set_ptt: error = %s \n", rigerror(retcode));
    }

    sleep(1);

    printf("Setting rig PTT OFF.\n");
    retcode = rig_set_ptt(my_rig, RIG_VFO_A, RIG_PTT_OFF);  /* phew ! */

    if (retcode != RIG_OK)
    {
        printf("rig_set_ptt: error = %s \n", rigerror(retcode));
    }

    sleep(1);

    /*
     * Simple examples of getting rig information -- FS
     *
     */

    printf("\nGet various raw rig values:\n");
    retcode = rig_get_vfo(my_rig, &vfo); /* try to get vfo info */

    if (retcode == RIG_OK)
    {
        printf("rig_get_vfo: vfo = %i \n", vfo);
    }
    else
    {
        printf("rig_get_vfo: error =  %s \n", rigerror(retcode));
    }

    retcode = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);

    if (retcode == RIG_OK)
    {
        printf("rig_get_freq: freq = %"PRIfreq"\n", freq);
    }
    else
    {
        printf("rig_get_freq: error =  %s \n", rigerror(retcode));
    }

    retcode = rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

    if (retcode == RIG_OK)
    {
        // cppcheck-suppress *
        printf("rig_get_mode: mode = %s\n", rig_strrmode(rmode));
    }
    else
    {
        printf("rig_get_mode: error =  %s \n", rigerror(retcode));
    }

    retcode = rig_get_strength(my_rig, RIG_VFO_CURR, &strength);

    if (retcode == RIG_OK)
    {
        printf("rig_get_strength: strength = %i \n", strength);
    }
    else
    {
        printf("rig_get_strength: error =  %s \n", rigerror(retcode));
    }

    if (rig_has_set_func(my_rig, RIG_FUNC_RIT))
    {
        retcode = rig_set_func(my_rig, RIG_VFO_CURR, RIG_FUNC_RIT, 1);

        if (retcode != RIG_OK) { printf("rig_set_func RIT error: %s\n", rigerror(retcode)); }

        printf("rig_set_func: Setting RIT ON\n");
    }

    if (rig_has_get_func(my_rig, RIG_FUNC_RIT))
    {
        retcode = rig_get_func(my_rig, RIG_VFO_CURR, RIG_FUNC_RIT, &rit);

        if (retcode != RIG_OK) { printf("rig_get_func RIT error: %s\n", rigerror(retcode)); }

        printf("rig_get_func: RIT: %d\n", rit);
    }

    if (rig_has_set_func(my_rig, RIG_FUNC_XIT))
    {
        retcode = rig_set_func(my_rig, RIG_VFO_CURR, RIG_FUNC_XIT, 1);

        if (retcode != RIG_OK) { printf("rig_set_func XIT error: %s\n", rigerror(retcode)); }

        printf("rig_set_func: Setting XIT ON\n");
    }

    if (rig_has_get_func(my_rig, RIG_FUNC_XIT))
    {
        retcode = rig_get_func(my_rig, RIG_VFO_CURR, RIG_FUNC_XIT, &xit);

        if (retcode != RIG_OK) { printf("rig_get_func XIT error: %s\n", rigerror(retcode)); }

        printf("rig_get_func: XIT: %d\n", xit);
    }

    rig_close(my_rig); /* close port */
    rig_cleanup(my_rig); /* if you care about memory */

    printf("port %s closed ok \n", SERIAL_PORT);

    for (unsigned long i = 1; i < 0x80000000; i = i << 1)
    {
        const char *vfostr = rig_strvfo(i);

        if (strlen(vfostr) > 0) { printf("0x%08lx=%s\n", i, vfostr); }
    }

    return 0;
}
