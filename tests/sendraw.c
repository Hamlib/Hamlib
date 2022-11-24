/* should work with simts2000 emulator */
#include <stdio.h>
#include <stdlib.h>
#include <hamlib/rig.h>

int
main()
{
    RIG *my_rig;
    unsigned char sendCmd[] = "FA;";
    int sendCmdLen = 3;
    unsigned char term[] = ";";
    unsigned char rcvdCmd[100];
    int rcvdCmdLen = sizeof(rcvdCmd);
    int retcode;

    my_rig = rig_init(2048);
    strcpy(my_rig->state.rigport.pathname, "/dev/pts/4");

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
