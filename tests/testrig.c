/* 
 * Hamlib sample program
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <hamlib/rig.h>

#define SERIAL_PORT "/dev/ttyS0"

int main (int argc, char *argv[])
{ 
	RIG *my_rig;		/* handle to rig (nstance) */
	freq_t freq;		/* frequency  */
	rmode_t rmode;		/* radio mode of operation */
	pbwidth_t width;
	vfo_t vfo;		/* vfo selection */
	int strength;		/* S-Meter level */
	int retcode;		/* generic return code from functions */
	rig_model_t myrig_model;


	printf("testrig:hello, I am your main() !\n");

 	/*
	 * allocate memory, setup & open port 
	 */

	if (argc < 2) {
			hamlib_port_t myport;
			/* may be overriden by backend probe */
			myport.type.rig = RIG_PORT_SERIAL;
			myport.parm.serial.rate = 9600;
			myport.parm.serial.data_bits = 8;
			myport.parm.serial.stop_bits = 1;
			myport.parm.serial.parity = RIG_PARITY_NONE;
			myport.parm.serial.handshake = RIG_HANDSHAKE_NONE;
			strncpy(myport.pathname, SERIAL_PORT, FILPATHLEN);

			rig_load_all_backends();
			myrig_model = rig_probe(&myport);
	} else {
			myrig_model = atoi(argv[1]);
	}

	my_rig = rig_init(myrig_model);
		
	if (!my_rig) {
		fprintf(stderr,"Unknown rig num: %d\n", myrig_model);
		fprintf(stderr,"Please check riglist.h\n");
		exit(1); /* whoops! something went wrong (mem alloc?) */
	}

	strncpy(my_rig->state.rigport.pathname,SERIAL_PORT,FILPATHLEN);

	retcode = rig_open(my_rig);
	if (retcode != RIG_OK) {
		printf("rig_open: error = %s\n", rigerror(retcode));
		exit(2);
	}

	printf("Port %s opened ok\n", SERIAL_PORT);

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


	if (retcode != RIG_OK ) {
	  printf("rig_set_vfo: error = %s \n", rigerror(retcode));
	}


	/*
	 * Lets try some frequencies and modes. Return code is not checked.
	 * Examples of checking return code are further down.
	 *
	 */
	
	/* 10m FM Narrow */

	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 28350125); /* 10m */
	retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_FM, 
					rig_passband_narrow(my_rig, RIG_MODE_FM));
	sleep(3);		/* so you can see it -- FS */

	/* 15m USB */

	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 21235175); /* 15m  */
	retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_USB, RIG_PASSBAND_NORMAL);
	sleep(3);

	/* AM Broadcast band */

	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 770000); /* KAAM */
	retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_AM, RIG_PASSBAND_NORMAL);
	sleep(3);

	/* 40m LSB */

	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 7250100); /* 40m  */
	retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_LSB, RIG_PASSBAND_NORMAL);
	sleep(3);

	/* 80m AM NArrow */

	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 3980000); /* 80m  */
	retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_AM, 
					rig_passband_narrow(my_rig, RIG_MODE_FM));
	sleep(3);

	/* 160m CW Normal */

	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 1875000); /* 160m  */
	retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_CW, RIG_PASSBAND_NORMAL);
	sleep(3);

	/* 160m CW Narrow -- The band is noisy tonight -- FS*/

	retcode = rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_CW,
					rig_passband_narrow(my_rig, RIG_MODE_FM));
	sleep(3);

	/* 20m USB on VFO_A */

	retcode = rig_set_vfo(my_rig, RIG_VFO_A);
	retcode = rig_set_freq(my_rig, RIG_VFO_CURR, 14250375); /* cq de vk3fcs */
	sleep(3);

	/* 20m USB on VFO_A , with only 1 call */

	retcode = rig_set_freq(my_rig, RIG_VFO_A, 14295125); /* cq de vk3fcs */
	sleep(3);



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

	retcode = rig_set_ptt(my_rig, RIG_VFO_A, RIG_PTT_ON ); /* stand back ! */

	if (retcode != RIG_OK ) {
	  printf("rig_set_ptt: error = %s \n", rigerror(retcode));
	} 

	sleep(1);

	retcode = rig_set_ptt(my_rig, RIG_VFO_A, RIG_PTT_OFF ); /* phew ! */

	if (retcode != RIG_OK ) {
	  printf("rig_set_ptt: error = %s \n", rigerror(retcode));
	} 

	sleep(1);

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
	  printf("rig_get_freq: freq = %"PRIfreq"\n", freq);
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

