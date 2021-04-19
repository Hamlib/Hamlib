
/*
 * Very simple test program to check locator conversion against some other --SF
 * This is mainly to test longlat2locator and locator2longlat functions.
 *
 * Takes at least two arguments, which is a locator and desired locater
 * precision in pairs, e.g. EM19ov is three pairs.  precision is limited
 * to >= 1 or <= 6.  If two locators are given, then the qrb is also
 * calculated.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include <hamlib/rotator.h>


int main(int argc, char *argv[])
{
    char recodedloc[13], *loc1, *loc2, sign;
    double lon1, lat1, lon2, lat2;
    double distance, az, mmm, sec;
    int  deg, min, retcode, loc_len, nesw = 0;

    if (argc < 2)
    {
        fprintf(stderr,
                "Usage: %s <locator1> <precision> [<locator2>]\n",
                argv[0]);
        exit(1);
    }

    loc1 = argv[1];
    loc_len = argc > 2 ? atoi(argv[2]) : strlen(loc1) / 2;
    loc2 = argc > 3 ? argv[3] : NULL;

    printf("Locator1:\t%s\n", loc1);

    /* hamlib function to convert maidenhead to decimal degrees */
    retcode = locator2longlat(&lon1, &lat1, loc1);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "locator2longlat() failed with malformed input.\n");
        exit(2);
    }

    /* hamlib function to convert decimal degrees to deg, min, sec */
    retcode = dec2dms(lon1, &deg, &min, &sec, &nesw);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "dec2dms() failed, invalid parameter address.\n");
        exit(2);
    }

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    printf("  Longitude:\t%f\t%c%d %d' %.2f\"\n", lon1, sign, deg, min, sec);

    /* hamlib function to convert deg, min, sec to decimal degrees */
    lon1 = dms2dec(deg, min, sec, nesw);
    printf("  Recoded lon:\t%f\n", lon1);

    /* hamlib function to convert decimal degrees to deg decimal minutes */
    retcode = dec2dmmm(lon1, &deg, &mmm, &nesw);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "dec2dmmm() failed, invalid parameter address.\n");
        exit(2);
    }

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    printf("  GPS lon:\t%f\t%c%d %.3f'\n", lon1, sign, deg, mmm);

    /* hamlib function to convert deg, decimal min to decimal degrees */
    lon1 = dmmm2dec(deg, mmm, nesw, 0.0);
    printf("  Recoded GPS:\t%f\n", lon1);

    /* hamlib function to convert decimal degrees to deg, min, sec */
    retcode = dec2dms(lat1, &deg, &min, &sec, &nesw);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "dec2dms() failed, invalid parameter address.\n");
        exit(2);
    }

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    printf("  Latitude:\t%f\t%c%d %d' %.2f\"\n", lat1, sign, deg, min, sec);

    /* hamlib function to convert deg, min, sec to decimal degrees */
    lat1 = dms2dec(deg, min, sec, nesw);
    printf("  Recoded lat:\t%f\n", lat1);

    /* hamlib function to convert decimal degrees to deg decimal minutes */
    retcode = dec2dmmm(lat1, &deg, &mmm, &nesw);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "dec2dmmm() failed, invalid parameter address.\n");
        exit(2);
    }

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    printf("  GPS lat:\t%f\t%c%d %.3f'\n", lat1, sign, deg, mmm);

    /* hamlib function to convert deg, decimal min to decimal degrees */
    lat1 = dmmm2dec(deg, mmm, nesw, 0.0);
    printf("  Recoded GPS:\t%f\n", lat1);

    /* hamlib function to convert decimal degrees to maidenhead */
    retcode = longlat2locator(lon1, lat1, recodedloc, loc_len);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "longlat2locator() failed, precision out of range.\n");
        exit(2);
    }

    printf("  Recoded:\t%s\n", recodedloc);

    if (loc2 == NULL)
    {
        exit(0);
    }

    /* Now work on the second locator */
    printf("\nLocator2:\t%s\n", loc2);

    retcode = locator2longlat(&lon2, &lat2, loc2);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "locator2longlat() failed with malformed input.\n");
        exit(2);
    }

    /* hamlib function to convert decimal degrees to deg, min, sec */
    retcode = dec2dms(lon2, &deg, &min, &sec, &nesw);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "dec2dms() failed, invalid parameter address.\n");
        exit(2);
    }

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    printf("  Longitude:\t%f\t%c%d %d' %.2f\"\n", lon2, sign, deg, min, sec);

    /* hamlib function to convert deg, min, sec to decimal degrees */
    lon2 = dms2dec(deg, min, sec, nesw);
    printf("  Recoded lon:\t%f\n", lon2);

    /* hamlib function to convert decimal degrees to deg decimal minutes */
    retcode = dec2dmmm(lon2, &deg, &mmm, &nesw);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "dec2dmmm() failed, invalid parameter address.\n");
        exit(2);
    }

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    printf("  GPS lon:\t%f\t%c%d %.3f'\n", lon2, sign, deg, mmm);

    /* hamlib function to convert deg, decimal min to decimal degrees */
    lon2 = dmmm2dec(deg, mmm, nesw, 0.0);
    printf("  Recoded GPS:\t%f\n", lon2);

    /* hamlib function to convert decimal degrees to deg, min, sec */
    retcode = dec2dms(lat2, &deg, &min, &sec, &nesw);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "dec2dms() failed, invalid parameter address.\n");
        exit(2);
    }

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    printf("  Latitude:\t%f\t%c%d %d' %.2f\"\n", lat2, sign, deg, min, sec);

    /* hamlib function to convert deg, min, sec to decimal degrees */
    lat2 = dms2dec(deg, min, sec, nesw);
    printf("  Recoded lat:\t%f\n", lat2);

    /* hamlib function to convert decimal degrees to deg decimal minutes */
    retcode = dec2dmmm(lat2, &deg, &mmm, &nesw);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "dec2dmmm() failed, invalid parameter address.\n");
        exit(2);
    }

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    printf("  GPS lat:\t%f\t%c%d %.3f'\n", lat2, sign, deg, mmm);

    /* hamlib function to convert deg, decimal min to decimal degrees */
    lat2 = dmmm2dec(deg, mmm, nesw, 0.0);
    printf("  Recoded GPS:\t%f\n", lat2);

    /* hamlib function to convert decimal degrees to maidenhead */
    retcode = longlat2locator(lon2, lat2, recodedloc, loc_len);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "longlat2locator() failed, precision out of range.\n");
        exit(2);
    }

    printf("  Recoded:\t%s\n", recodedloc);

    retcode = qrb(lon1, lat1, lon2, lat2, &distance, &az);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "QRB error: %d\n", retcode);
        exit(2);
    }

    dec2dms(az, &deg, &min, &sec, &nesw);
    printf("\nDistance: %.6fkm\n", distance);

    if (nesw == 1)
    {
        sign = '-';
    }
    else
    {
        sign = '\0';
    }

    /* Beware printf() rounding error! */
    printf("Bearing: %.2f, %c%d %d' %.2f\"\n", az, sign, deg, min, sec);

    exit(0);
}
