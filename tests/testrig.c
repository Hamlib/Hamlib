/* 
 * Hamlib sample program
 */

#include <hamlib/rig.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SERIAL_PORT "/dev/ttyS0"

int main ()
{ 
	RIG *my_rig;		/* handle to rig (nstance) */
	freq_t freq;		/* frequency  */
	rmode_t rmode;		/* radio mode of operation */
	pbwidth_t width;
	vfo_t vfo;		/* vfo selection */
	int strength;		/* S-Meter level */
	int retcode;		/* generic return code from functions */


	printf("testrig:hello, I am your main() !\n");

 	/*
	 * allocate memory, setup & open port 
	 */

/*  	retcode = rig_load_backend("icom"); */
	retcode = rig_load_backend("ft747");

	if (retcode != RIG_OK ) {
		printf("rig_load_backend: error = %s \n", rigerror(retcode));
		exit(3);
	}

/*  	my_rig = rig_init(RIG_MODEL_IC706MKIIG); */
	my_rig = rig_init(RIG_MODEL_FT747);

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

/*  	retcode = rig_set_vfo(my_rig, RIG_VFO_MAIN); */
	retcode = rig_set_vfo(my_rig, RIG_VFO_B);


	if (retcode != RIG_OK ) {
	  printf("rig_set_vfo: error = %s \n", rigerror(retcode));
	}


	/*
	 * Lets try some frequencies
	 */
	
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 28350125); /* 10m */
	sleep(2);
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 21235175); /* 15m  */
	sleep(2);
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 770000); /* KAAM */
	sleep(2);
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 7250100); /* 40m  */
	sleep(2);
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 3980000); /* 80m  */
	sleep(2);
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 1875000); /* 160m  */
	sleep(2);
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 14250375); /* cq de vk3fcs */
	sleep(2);
#if 0
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 145100000); /* 2m  */
	sleep(2);
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 435125000); /* 70cm  */
	sleep(2);
#endif

	if (retcode != RIG_OK ) {
	  printf("rig_set_freq: error = %s \n", rigerror(retcode));
	} 

	retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_LSB, RIG_PASSBAND_NORMAL);

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

	retcode = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
	
	if (retcode == RIG_OK ) {
	  printf("rig_get_freq: freq = %Li \n", freq);
	} else {
	  printf("rig_get_freq: error =  %s \n", rigerror(retcode));
	}

	retcode = rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);

	if (retcode == RIG_OK ) {
	  printf("rig_get_mode: mode = %i \n", rmode);
	} else {
	  printf("rig_get_mode: error =  %s \n", rigerror(retcode));
	}

	retcode = rig_get_strength(my_rig, RIG_VFO_CURR, &strength);

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

