
/* 
 * Hamlib sample C++ program
 */

#include <iostream.h>
#include <hamlib/rigclass.h>

int main(int argc, char* argv[])
{
	Rig myRig = Rig(RIG_MODEL_DUMMY);

	try {
		myRig.open();
		myRig.setFreq(MHz(144));
		cout << myRig.getLevelI(RIG_LEVEL_STRENGTH) << "dB" << endl;
		cout << "Modes for freq 14.332: " << myRig.RngRxModes(MHz(14.332)) << endl;
		myRig.close();
	}
	catch (const RigException &Ex) {
		Ex.print();
		return 1;
	}

	return 0;
}
