
#include <rig.h>
#include <stdio.h>

#define SERIAL_PORT "/dev/ttyS1"

int main ()
{ 
	RIG *my_rig;

 	/* allocate memory, setup & open port */
	my_rig = rig_open(SERIAL_PORT, RIG_MODEL_FT847);
	if (!my_rig) exit(1); /* whoops! smth went wrong (mem alloc?) */

	/* at this point, the RIG* struct is automatically populated */

	printf("Port %s opened ok\n",SERIAL_PORT);

	cmd_set_freq_main_vfo_hz(my_rig, 439700000, RIG_MODE_FM);

	rig_close(my_rig); /* close port and release memory */

	printf("port %s closed ok \n",SERIAL_PORT);

	return 0;
}

