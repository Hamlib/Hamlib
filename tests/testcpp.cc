
/* 
 * Hamlib sample C++ program
 */

#include <hamlib/rigclass.h>

int main(int argc, char* argv[])
{
	Rig myRig = Rig(RIG_MODEL_DUMMY);

	myRig.open();
	myRig.setFreq(MHz(144));
	myRig.close();

	return 0;
}
