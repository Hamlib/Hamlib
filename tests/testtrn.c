/* 
 * Hamlib sample program to test transceive mode (async event)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <hamlib/rig.h>

#define SERIAL_PORT "/dev/ttyS0"

int myfreq_event(RIG *rig, vfo_t vfo, freq_t freq)
{
		printf("Rig changed freq to %LiHz\n",freq);
		return 0;
}


int main ()
{ 
	RIG *my_rig;		/* handle to rig (nstance) */
	int retcode;		/* generic return code from functions */
	int i;


	printf("testrig:hello, I am your main() !\n");

 	/*
	 * allocate memory, setup & open port 
	 */

	retcode = rig_load_backend("icom");
	if (retcode != RIG_OK ) {
		printf("rig_load_backend: error = %s \n", rigerror(retcode));
		exit(3);
	}

	my_rig = rig_init(RIG_MODEL_IC706MKIIG);
	if (!my_rig)
			exit(1); /* whoops! something went wrong (mem alloc?) */

	strncpy(my_rig->state.rigport.path, SERIAL_PORT, FILPATHLEN);

	if (rig_open(my_rig))
			exit(2);

	printf("Port %s opened ok\n", SERIAL_PORT);

	/*
	 * Below are examples of set/get routines.
	 * Must add checking of functionality map prior to command execution -- FS
	 *
	 */
	

	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 439700000);

	if (retcode != RIG_OK ) {
	  printf("rig_set_freq: error = %s \n", rigerror(retcode));
	} 

	my_rig->callbacks.freq_event = myfreq_event;

	retcode = rig_set_trn(my_rig, RIG_VFO_CURR, RIG_TRN_RIG);

	if (retcode != RIG_OK ) {
	  printf("rig_set_trn: error = %s \n", rigerror(retcode));
	} 


	for (i=0;i<12;i++)
			sleep(60);	/* or anything smarter */


	rig_close(my_rig); /* close port */
	rig_cleanup(my_rig); /* if you care about memory */

	printf("port %s closed ok \n",SERIAL_PORT);

	return 0;
}


