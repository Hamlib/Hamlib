
/* 
 * Hamlib sample program
 */

#include <rig.h>
#include <stdio.h>
#include <string.h>

#define SERIAL_PORT "/dev/ttyS1"

int main ()
{ 
	RIG *my_rig;

 	/*
	 * allocate memory, setup & open port 
	 */
	my_rig = rig_init(RIG_MODEL_FT847);
	if (!my_rig)
			exit(1); /* whoops! something went wrong (mem alloc?) */

	strncpy(my_rig->state.rig_path,SERIAL_PORT,MAXRIGPATHLEN);

	if (rig_open(my_rig))
			exit(2);

	printf("Port %s opened ok\n", SERIAL_PORT);

	cmd_set_freq(my_rig, 439700000, RIG_MODE_FM, RIG_VFO_MAIN);

	rig_close(my_rig); /* close port */
	rig_cleanup(my_rig); /* if you care about memory */

	printf("port %s closed ok \n",SERIAL_PORT);

	return 0;
}

