/* 
 * Hamlib sample program
 */

#include <hamlib/rig.h>
#include <stdio.h>
#include <string.h>

#define SERIAL_PORT "/dev/ttyS0"

int main ()
{ 
	RIG *my_rig;		/* handle to rig (nstance) */
	freq_t freq;		/* frequency  */
	rmode_t rmode;		/* radio mode of operation */
	vfo_t vfo;		/* vfo selection */
	int strength;		/* S-Meter level */
	int retcode;		/* generic return code from functions */


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

	strncpy(my_rig->state.rig_path,SERIAL_PORT,FILPATHLEN);

	if (rig_open(my_rig))
			exit(2);

	printf("Port %s opened ok\n", SERIAL_PORT);

	/*
	 * Below are examples of set/get routines.
	 * Must add checking of functionality map prior to command execution -- FS
	 *
	 */
	

	/*
	 * Example of setting rig Main VFO to 439.700 Mhz FM -- FS
	 * and some error checking on the return code.
	 */

	retcode = rig_set_vfo(my_rig, RIG_VFO_MAIN);

	if (retcode != RIG_OK ) {
	  printf("rig_set_vfo: error = %s \n", rigerror(retcode));
	}

	retcode = rig_set_freq(my_rig, 439700000); /* cq de vk3fcs */

	if (retcode != RIG_OK ) {
	  printf("rig_set_freq: error = %s \n", rigerror(retcode));
	} 

	retcode = rig_set_mode(my_rig, RIG_MODE_FM);

	if (retcode != RIG_OK ) {
	  printf("rig_set_mode: error = %s \n", rigerror(retcode));
	} 

	/*
	 * Simple examples of getting rig information -- FS
	 *
	 */

	retcode = rig_get_vfo(my_rig, &vfo); /* try to get vfo info */

	if (retcode == RIG_OK ) {
	  printf("rig_get_vfo: vfo = %i \n", vfo);
	} else {
	  printf("rig_get_vfo: error =  %s \n", rigerror(retcode));
	}

	retcode = rig_get_freq(my_rig, &freq);
	
	if (retcode == RIG_OK ) {
	  printf("rig_get_freq: freq = %Li \n", freq);
	} else {
	  printf("rig_get_freq: error =  %s \n", rigerror(retcode));
	}

	retcode = rig_get_mode(my_rig, &rmode);

	if (retcode == RIG_OK ) {
	  printf("rig_get_mode: mode = %i \n", rmode);
	} else {
	  printf("rig_get_mode: error =  %s \n", rigerror(retcode));
	}

	retcode = rig_get_strength(my_rig, &strength);

	if (retcode == RIG_OK ) {
	  printf("rig_get_strength: strength = %i \n", strength);
	} else {
	  printf("rig_get_strength: error =  %s \n", rigerror(retcode));
	}


	rig_close(my_rig); /* close port */
	rig_cleanup(my_rig); /* if you care about memory */

	printf("port %s closed ok \n",SERIAL_PORT);

	return 0;
}


