/* should work with simts2000 emulator */
#include <stdio.h>
#include <stdlib.h>
#include <hamlib/rig.h>
#define PATH "/dev/pts/4"

int
main()
{
    RIG *my_rig;
    const unsigned char sendCmd[] = "FA;";
    int sendCmdLen = 3;
    unsigned char term[] = ";";
    unsigned char rcvdCmd[100];
    int rcvdCmdLen = sizeof(rcvdCmd);
    int retcode;

    my_rig = rig_init(2048);
    rig_set_conf(my_rig, rig_token_lookup(my_rig, "rig_pathname"), PATH);

    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
        printf("rig_open: error = %s\n", rigerror(retcode));
        exit(2);
    }

    freq_t f;
    rig_get_freq(my_rig, RIG_VFO_A, &f);

    int nbytes = rig_send_raw(my_rig, sendCmd, sendCmdLen, rcvdCmd, rcvdCmdLen,
                              term);

    if (nbytes >= 0) { printf("Response(%d bytes): %s\n", nbytes, rcvdCmd); }
    else { printf("Error occurred = %s\n", rigerror(retcode)); }

    return retcode;
}
