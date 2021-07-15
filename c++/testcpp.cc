
/* 
 * Hamlib sample C++ program
 */

#include <iostream>
#include <hamlib/rigclass.h>

int main(int argc, char* argv[])
{
	Rig myRig {RIG_MODEL_DUMMY};

	try {
		myRig.setConf("rig_pathname", "/dev/ttyS1");
		myRig.open();
		myRig.setFreq(MHz(144));
		std::cout << myRig.getLevelI(RIG_LEVEL_STRENGTH) << "dB" << std::endl;
		std::cout << "Modes for freq 14.332: " << myRig.RngRxModes(MHz(14.332)) << std::endl;
		myRig.close();
	}
	catch (const RigException &Ex) {
		Ex.print();
		return 1;
	}

	return 0;
}
