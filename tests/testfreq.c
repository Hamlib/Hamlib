
/* 
 * Very simple test program to check freq convertion --SF
 * This is mainly to test kHz, MHz, GHz macros and long long support.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <hamlib/rig.h>
#include "misc.h"


int main (int argc, char *argv[])
{
	freq_t f=0;

#if 0
	if (argc != 2) {
			fprintf(stderr,"Usage: %s <freq>\n",argv[0]);
			exit(1);
	}

	f = atoi(argv[1]);
#endif

	printf("%s\n", hamlib_version);
	printf("caps size: %d\n", sizeof(struct rig_caps));
	printf("state size: %d\n", sizeof(struct rig_state));
	printf("RIG size: %d\n", sizeof(struct rig));
	printf("freq_t size: %d\n", sizeof(freq_t));
	printf("shortfreq_t size: %d\n", sizeof(shortfreq_t));

	/* freq on 31bits test */
	f = GHz(2);
	printf("GHz(2) = %"PRIll"\n", (long long)f);

	/* freq on 32bits test */
	f = GHz(4);
	printf("GHz(4) = %"PRIll"\n", (long long)f);

	/* freq on >32bits test */
	f = GHz(5);
	printf("GHz(5) = %"PRIll"\n", (long long)f);

	/* floating point to freq conversion test */
	f = GHz(1.3);
	printf("GHz(1.3) = %"PRIll"\n", (long long)f);

	/* floating point to freq conversion precision test */
	f = GHz(1.234567890);
	printf("GHz(1.234567890) = %"PRIll"\n", (long long)f);

	/* floating point to freq conversion precision test, with freq >32bits */
	f = GHz(123.456789012);
	printf("GHz(123.456789012) = %"PRIll"\n", (long long)f);

	return 0;
}
