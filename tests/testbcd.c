
/* 
 * Very simple test program to check BCD convertion against some other --SF
 * This is mainly to test freq2bcd and bcd2freq functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <rig.h>
#include "../src/misc.h"


int main (int argc, char *argv[])
{
	char b[5];
	freq_t f=0;

	if (argc != 2) {
			fprintf(stderr,"%s <freq>\n",argv[0]);
			exit(1);
	}

	f = atoi(argv[1]);

	printf("Frequency: %lld\n",f);
	to_bcd(b, f, 10);
	printf("BCD: %2.2x,%2.2x,%2.2x,%2.2x,%2.2x\n",b[0],b[1],b[2],b[3],b[4]);
	printf("Result after recoding: %lld\n", from_bcd(b, 10));

	return 0;
}
