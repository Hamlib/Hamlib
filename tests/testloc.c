
/*
 * Very simple test program to check locator convertion against some other --SF
 * This is mainly to test longlat2locator and locator2longlat functions.
 *
 * Takes at least two arguments, which is a locator and desired locater
 * precision in pairs, e.g. EM19ov is three pairs.  precision is limited
 * to >= 1 or <= 6.  If two locators are given, then the qrb is also
 * calculated.
 *
 *	$Id: testloc.c,v 1.10 2003-10-28 01:01:06 n0nb Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <hamlib/rotator.h>


int main (int argc, char *argv[]) {
	char recodedloc[13], *loc1, *loc2;
	double lon1, lat1, lon2, lat2;
	double distance, az, min, sec;
	float deg;
	int retcode, loc_len;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <locator1> <precision> [<locator2>]\n", argv[0]);
		exit(1);
	}

	loc1 = argv[1];
        loc_len = atoi(argv[2]);
	loc2 = argc > 3 ? argv[3] : NULL;

	printf("Locator1:\t%s\n", loc1);

        /* hamlib function to convert maidenhead to decimal degrees */
	retcode = locator2longlat(&lon1, &lat1, loc1);
	if (retcode != RIG_OK) {
		fprintf(stderr, "locator2longlat() failed with malformed input.\n");
		exit(2);
	}

        /* hamlib function to convert decimal degrees to deg, min, sec */
	retcode = dec2dms(lon1, &deg, &min, &sec);
	if (retcode != RIG_OK) {
		fprintf(stderr, "dec2dms() failed, invalid paramter address.\n");
		exit(2);
	}
	printf("  Longitude:\t%f\t%.0f° %.0f' %.2f\"\n", lon1, deg, min, sec);

        /* hamlib function to convert deg, min, sec to decimal degrees */
	lon1 = dms2dec(deg, min, sec);
	printf("  Recoded lon:\t%f\n", lon1);

        /* hamlib function to convert decimal degrees to deg decimal minutes */
	retcode = dec2dmmm(lon1, &deg, &min);
	if (retcode != RIG_OK) {
		fprintf(stderr, "dec2dmmm() failed, invalid paramter address.\n");
		exit(2);
	}
	printf("  GPS lon:\t%f\t%.0f° %.3f'\n", lon1, deg, min);

        /* hamlib function to convert deg, decimal min to decimal degrees */
	lon1 = dmmm2dec(deg, min);
	printf("  Recoded GPS:\t%f\n", lon1);

        /* hamlib function to convert decimal degrees to deg, min, sec */
	retcode = dec2dms(lat1, &deg, &min, &sec);
	if (retcode != RIG_OK) {
		fprintf(stderr, "dec2dms() failed, invalid paramter address.\n");
		exit(2);
	}
	printf("  Latitude:\t%f\t%.0f° %.0f' %.2f\"\n", lat1, deg, min, sec);

        /* hamlib function to convert deg, min, sec to decimal degrees */
	lat1 = dms2dec(deg, min, sec);
	printf("  Recoded lat:\t%f\n", lat1);

        /* hamlib function to convert decimal degrees to deg decimal minutes */
	retcode = dec2dmmm(lat1, &deg, &min);
	if (retcode != RIG_OK) {
		fprintf(stderr, "dec2dmmm() failed, invalid paramter address.\n");
		exit(2);
	}
	printf("  GPS lat:\t%f\t%.0f° %.3f'\n", lat1, deg, min);

        /* hamlib function to convert deg, decimal min to decimal degrees */
	lat1 = dmmm2dec(deg, min);
	printf("  Recoded GPS:\t%f\n", lat1);

        /* hamlib function to convert decimal degrees to maidenhead */
	retcode = longlat2locator(lon1, lat1, recodedloc, loc_len);
	if (retcode != RIG_OK) {
		fprintf(stderr, "longlat2locator() failed, precision out of range.\n");
		exit(2);
	}
	printf("  Recoded:\t%s\n", recodedloc);

	if (loc2 == NULL)
		exit(0);

	/* Now work on the second locator */
	printf("\nLocator2:\t%s\n", loc2);

	retcode = locator2longlat(&lon2, &lat2, loc2);
	if (retcode != RIG_OK) {
		fprintf(stderr, "locator2longlat() failed with malformed input.\n");
		exit(2);
	}

	/* hamlib function to convert decimal degrees to deg, min, sec */
	retcode = dec2dms(lon2, &deg, &min, &sec);
	if (retcode != RIG_OK) {
		fprintf(stderr, "dec2dms() failed, invalid paramter address.\n");
		exit(2);
	}
	printf("  Longitude:\t%f\t%.0f° %.0f' %.2f\"\n", lon2, deg, min, sec);

	/* hamlib function to convert deg, min, sec to decimal degrees */
	lon2 = dms2dec(deg, min, sec);
	printf("  Recoded lon:\t%f\n", lon2);

	/* hamlib function to convert decimal degrees to deg decimal minutes */
	retcode = dec2dmmm(lon2, &deg, &min);
	if (retcode != RIG_OK) {
		fprintf(stderr, "dec2dmmm() failed, invalid paramter address.\n");
		exit(2);
	}
	printf("  GPS lon:\t%f\t%.0f° %.3f'\n", lon2, deg, min);

	/* hamlib function to convert deg, decimal min to decimal degrees */
	lon2 = dmmm2dec(deg, min);
	printf("  Recoded GPS:\t%f\n", lon2);

	/* hamlib function to convert decimal degrees to deg, min, sec */
	retcode = dec2dms(lat2, &deg, &min, &sec);
	if (retcode != RIG_OK) {
		fprintf(stderr, "dec2dms() failed, invalid paramter address.\n");
		exit(2);
	}
	printf("  Latitude:\t%f\t%.0f° %.0f' %.2f\"\n", lat2, deg, min, sec);

	/* hamlib function to convert deg, min, sec to decimal degrees */
	lat2 = dms2dec(deg, min, sec);
	printf("  Recoded lat:\t%f\n", lat2);

	/* hamlib function to convert decimal degrees to deg decimal minutes */
	retcode = dec2dmmm(lat2, &deg, &min);
	if (retcode != RIG_OK) {
		fprintf(stderr, "dec2dmmm() failed, invalid paramter address.\n");
		exit(2);
	}
	printf("  GPS lat:\t%f\t%.0f° %.3f'\n", lat2, deg, min);

        /* hamlib function to convert deg, decimal min to decimal degrees */
	lat2 = dmmm2dec(deg, min);
	printf("  Recoded GPS:\t%f\n", lat2);

        /* hamlib function to convert decimal degrees to maidenhead */
	retcode = longlat2locator(lon2, lat2, recodedloc, loc_len);
	if (retcode != RIG_OK) {
		fprintf(stderr, "longlat2locator() failed, precision out of range.\n");
		exit(2);
	}
	printf("  Recoded:\t%s\n", recodedloc);

	retcode = qrb(lon1, lat1, lon2, lat2, &distance, &az);
	if (retcode != 0) {
		fprintf(stderr, "QRB error: %d\n", retcode);
		exit(2);
	}

	dec2dms(az, &deg, &min, &sec);
	printf("\nDistance: %.6fkm\n", distance);
	printf("Bearing: %f, %.0f° %.0f' %.2f\"\n", az, deg, min, sec);

	exit(0);
}
