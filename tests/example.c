/*  This is a minimal program calling Hamlib to get receive frequency.
 *  
 *  To compile:
 *  	gcc -L/usr/local/lib -lhamlib -o example example.c
 *  	if hamlib is installed in /usr/local/...
 */

#include <stdio.h>
#include <string.h>
#include <hamlib/rig.h>

int main() {
    RIG *my_rig;
    char *rig_file;
    freq_t freq;
    int status, retcode;

    rig_set_debug(RIG_DEBUG_ERR);   // signal errors only
    my_rig = rig_init(1608);        // Ten-Tec Orion code
    rig_file = "/dev/ham.orion";    // communications dev.
	// you may prefer /dev/ttyS0
    strncpy(my_rig->state.rigport.pathname, rig_file, FILPATHLEN);
    my_rig->state.rigport.parm.serial.rate = 57600;
    retcode = rig_open(my_rig);     // open the rig
    status = rig_get_freq(my_rig, RIG_VFO_CURR, &freq); // get freq
    printf("%f\n", freq);
};
