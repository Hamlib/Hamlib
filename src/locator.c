/**
 * \file src/locator.c
 * \ingroup hamlib
 * \brief locator and bearing conversion interface
 * \author Stephane Fillod
 * \date 2000-2002
 *
 *  Hamlib Interface - locator and bearing conversion calls
 */

/*
 *  Hamlib Interface - locator and bearing conversion calls
 *  Copyright (c) 2001-2002 by Stephane Fillod
 *  Copyright (c) 2003 by Nate Bargmann
 *  Copyright (c) 2003 by Dave Hines
 *
 *	$Id: locator.c,v 1.8 2003-08-21 03:11:27 n0nb Exp $
 *
 *	Code to determine bearing and range was taken from the Great Circle,
 *	by S. R. Sampson, N5OWK.
 *	Ref: "Air Navigation", Air Force Manual 51-40, 1 February 1987
 *	Ref: "ARRL Satellite Experimenters Handbook", August 1990
 *
 *  Code to calculate distance and azimuth between two Maidenhead locators,
 *  taken from wwl, by IK0ZSN Mirko Caserta.
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*! \page hamlib Hamlib general purpose API
 *
 *  Here are grouped some often used functions, like locator conversion
 *  routines.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


#include <hamlib/rotator.h>


#ifndef DOC_HIDDEN

#define RADIAN  (180.0 / M_PI)

/* arc length for 1 degree, 60 Nautical Miles */
#define ARC_IN_KM 111.2

#endif	/* !DOC_HIDDEN */

/**
 * \brief Convert DMS angle to decimal representation
 * \param degrees	Degrees
 * \param minutes	Minutes
 * \param seconds	Seconds
 *
 *  Convert degree/minute/second angle to a decimal representation.
 *  Degrees >360, minutes > 60, and seconds > 60 are allowed, but
 *  resulting angle won't be normalized.
 *
 * \return the decimal representation.
 *
 * \sa dec2dms()
 */
double dms2dec(int degrees, int minutes, int seconds) {
	if (degrees >= 0)
		return (double)degrees + (double)minutes/60. + (double)seconds/3600.;
	else
		return (double)degrees - (double)minutes/60. - (double)seconds/3600.;
}

/**
 * \brief Convert decimal angle into DMS representation
 * \param dec	Decimal angle
 * \param degrees	The location where to store the degrees
 * \param minutes	The location where to store the minutes
 * \param seconds	The location where to store the seconds
 *
 *  Convert decimal angle into its degree/minute/second representation.
 *
 *  When passed a value < -180 or > 180, the sign will be reversed
 *  and the value constrained to => -180 and <= 180 before conversion.
 *
 *  Upon return dec2dms guarantees -180<=degrees<180,
 *  0<=minutes<60, and 0<=seconds<60.
 *
 * \sa dms2dec()
 */
void dec2dms(double dec, int *degrees, int *minutes, int *seconds) {
	int deg, min, sec, is_neg = 0;
	double st;

	if (!degrees || !minutes || !seconds)
		return;

	/* reverse the sign if dec has a magnitude greater
	 * than 180 and factor out multiples of 360.
	 * e.g. when passed 270 st will be set to -90
	 * and when passed -270 st will be set to 90.  If
	 * passed 361 st will be set to -1, etc.  If passed
         * a value > -180 || < 180, value will be unchanged.
	 */
	if (dec >= 0.0)
		st = fmod(dec + 180, 360) - 180;
        else
		st = fmod(dec - 180, 360) + 180;

	/* if after all of that st is negative, we want deg
	 * to be negative as well.  Treat -180 as a special
	 * case, not returning its sign so longitudes will
         * be returned from -179.999 to 180.0.
         */
	if (st < 0.0 && st != -180.)
		is_neg = 1;

	/* work on st as a positive value to remove a
	 * bug introduced by the effect of floor() when
	 * passed a negative value.  e.g. when passed
	 * -96.8333 floor() returns -95!  Also avoids
         * a rounding error introduced on negative values.
	 */
	st = fabs(st);

	deg = (int)floor(st);
	st  = 60. * (st-(double)deg);
	min = (int)floor(st);
	st  = 60. * (st-(double)min);
	sec = (int)floor(st);

	/* round fractional seconds up if greater than sec.5
	 * round up min and deg if warranted.
         */
	if (fmod(st, sec) >= 0.5) {
		sec++;
		if (sec == 60) {
			sec = 0;
			min++;
			if (min == 60) {
				min = 0;
				deg++;
			}
		}
	}

	/* set *degrees to original sign passed to dec */
	(is_neg == 1) ? (*degrees = deg * -1) : (*degrees = deg);

	*minutes = min;
	*seconds = sec;
}

/* The following is contributed by Dave Hines
 *
 * begin dph
 */
/*
 * These are the constants used when converting between Maidenhead grid
 * locators and longitude/latitude values. MAX_LOCATOR_PAIRS is the maximum
 * number of locator character pairs to convert. This number MUST NOT exceed
 * the number of pairs of values in range[] & weight[].
 * Setting MAX_LOCATOR_PAIRS to 3 will convert the currently defined 6
 * character locators. A value of 4 will convert the extended 8 character
 * locators described in section 3L of "The IARU region 1 VHF managers
 * handbook". Values of 5 and 6 will extent the format even more, to the
 * longest definition I have seen for locators. Beware that there seems to be
 * no universally accepted standard for 10 & 12 character locators.
 * Note that the loc_char_weight values are in minutes of arc, to avoid
 * constants which can't be represented precisely in either binary or decimal.
 */
const static double loc_char_weight[] = { 600.0, 60.0, 2.5, 0.25, 0.01, 0.001 };
const static int loc_char_range[] = { 18, 10, 24, 10, 25, 10 };
#define MAX_LOCATOR_PAIRS       6

/* end dph */

/**
 * \brief Convert Maidenhead grid locator to longitude/latitude
 * \param longitude	The location where to store longitude, decimal
 * \param latitude	The location where to store latitude, decimal
 * \param locator	The locator--2 through 12 char nul terminated string
 *
 *  Convert Maidenhead grid locator to longitude/latitude (decimal).
 *  The locator should be in 2 through 12 chars long format. locator2longlat
 *  is case insensitive, however it checks for locator validity.
 *
 *  Decimal long/lat is computed to center of grid square, i.e. given
 *  EM19 will return coordinates equivalent to the southwest corner
 *  of EM19mm.  Given a six character locator, computed values will
 *  in the center of the given subsquare, i.e. 2' 30" from west boundary
 *  and 1' 15" from south boundary.
 *
 * \return RIG_OK to indicate conversion went ok, -RIG_EINVAL if locator
 *  exceeds RR99xx or is malformed (not of 2 through 12 character format).
 *
 * \sa longlat2locator()
 */

/* begin dph */

int locator2longlat(double *longitude, double *latitude, const char *locator) {
	int x_or_y, paircount = strlen(locator) / 2;
        int locvalue, pair;
	double xy[2], minutes;

	if (paircount > MAX_LOCATOR_PAIRS)	/* Max. locator length to allow */
		paircount = MAX_LOCATOR_PAIRS;

	for (x_or_y = 0;  x_or_y < 2;  ++x_or_y) { /* For x(=long) and y(=lat) */
		minutes = 0.0;

		for (pair = 0;  pair < paircount;  ++pair) {
			locvalue = locator[pair*2 + x_or_y];

			locvalue -= (loc_char_range[pair] == 10) ? '0' : /* Value of digit */
				(isupper(locvalue)) ? 'A' : 'a';	 /*  or letter. */

			if (((unsigned) locvalue) >= loc_char_range[pair]) /* Check range */
				return -RIG_EINVAL;	/* Non-letter/digit or out of range */

			minutes += locvalue * loc_char_weight[pair];
		}
		minutes += loc_char_weight[paircount-1] / 2.0; /* Center coordinate */

		xy[x_or_y] = minutes / 60.0 - 90.0;
	}

	/* Don't seg. fault if longitude or latitude pointers are null */
	if (longitude != NULL)	*longitude = xy[0] * 2;
	if (latitude != NULL)	*latitude = xy[1];

	return RIG_OK;
}
/* end dph */

/**
 * \brief Convert longitude/latitude to Maidenhead grid locator
 * \param longitude	The longitude, decimal
 * \param latitude	The latitude, decimal
 * \param locator	The location where to store the locator
 *
 *  Convert longitude/latitude (decimal) to Maidenhead grid locator.
 *  \a locator must point to an array at least MAX_LOCATOR_PAIRS*2 char plus nul long.
 *
 * \sa locator2longlat()
 */

/* begin dph */

void longlat2locator(double longitude, double latitude, char *locator) {
	int x_or_y, pair, locvalue;
        double tmp;

	for (x_or_y = 0;  x_or_y < 2;  ++x_or_y) {
		tmp = ((x_or_y == 0) ? longitude / 2. : latitude);

		/* The 1e-6 here guards against floating point rounding errors */
		tmp = fmod(tmp + 270., 180.) * 60. + 1e-6;
		for (pair = 0;  pair < MAX_LOCATOR_PAIRS;  ++pair) {
			locvalue = (int) (tmp / loc_char_weight[pair]);

			/* assert(locvalue < loc_char_range[pair]); */
			tmp -= loc_char_weight[pair] * locvalue;
			locvalue += (loc_char_range[pair] == 10) ? '0':'A';
			locator[pair*2 + x_or_y] = locvalue;
		}
	}
        locator[MAX_LOCATOR_PAIRS * 2] = '\0';
}

/* end dph */

/**
 * \brief Calculate the distance and bearing between two points.
 * \param lon1	The local longitude, decimal degrees
 * \param lat1	The local latitude, decimal degrees
 * \param lon2	The remote longitude, decimal degrees
 * \param lat2	The remote latitude, decimal degrees
 * \param distance	The location where to store the distance
 * \param azimuth	The location where to store the bearing
 *
 *  Calculate the QRB between \a lat1,\a lat1 and \a lon2,\a lat2.
 *
 *	This version also takes into consideration the two points
 *	being close enough to be in the near-field, and the antipodal points,
 *	which are easily calculated.
 *
 * \return the distance in kilometers and azimuth in decimal degrees
 * for the short path.
*
 * \sa distance_long_path(), azimuth_long_path()
 */
int qrb(double lon1, double lat1, double lon2, double lat2,
				double *distance, double *azimuth) {
	double delta_long, tmp, arc, cosaz, az;

	if (!distance || !azimuth)
		return -1;

	if ((lat1 > 90.0 || lat1 < -90.0) || (lat2 > 90.0 || lat2 < -90.0))
		return -1;

	if ((lon1 > 180.0 || lon1 < -180.0) || (lon2 > 180.0 || lon2 < -180.0))
		return -1;

	/* Prevent ACOS() Domain Error */

	if (lat1 == 90.0)
		lat1 = 89.99;
	else if (lat1 == -90.0)
		lat1 = -89.99;

	if (lat2 == 90.0)
		lat2 = 89.99;
	else if (lat2 == -90.0)
		lat2 = -89.99;

	/*
	 * Convert variables to Radians
	 */
	lat1	/= RADIAN;
	lon1	/= RADIAN;
   	lat2	/= RADIAN;
   	lon2	/= RADIAN;

   	delta_long = lon2 - lon1;

   	tmp = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(delta_long);

	if (tmp > .999999) {
		/* Station points coincide, use an Omni! */
		*distance = 0.0;
		*azimuth = 0.0;
		return 0;
	}

	if (tmp < -.999999) {
		/*
		 * points are antipodal, it's straight down.
		 * Station is equal distance in all Azimuths.
		 * So take 180 Degrees of arc times 60 nm,
		 * and you get 10800 nm, or whatever units...
		 */

		*distance = 180.0 * ARC_IN_KM;
		*azimuth = 0.0;
		return 0;
	}

	arc = acos(tmp);

	/*
	 * One degree of arc is 60 Nautical miles
	 * at the surface of the earth, 111.2 km, or 69.1 sm
	 * This method is easier than the one in the handbook
	 */

	/* Short Path */

	*distance = ARC_IN_KM * RADIAN * arc;

	/*
	 * Long Path
	 *
	 * distlp = (ARC_IN_KM * 360.0) - distsp;
	 */

	cosaz = (sin(lat2) - (sin(lat1) * cos(arc))) /
                (sin(arc) * cos(lat1));

	if (cosaz > .999999)
		az = 0.0;
	else if (cosaz < -.999999)
		az = 180.0;
	else
		az = acos(cosaz) * RADIAN;

	/*
	 * Handbook had the test ">= 0.0" which looks backwards??
	 * must've been frontwards since the numbers seem to make sense
	 * now.  ;-)  -N0NB
	 */

//	if (sin(delta_long) < 0.0)  {
	if (sin(delta_long) >= 0.0)  {
		*azimuth = az;
	} else {
		*azimuth = 360.0 - az;
	}

	if (*azimuth == 360.0)
		*azimuth = 0;

	return 0;
}

/**
 * \brief Calculate the long path distance between two points.
 * \param distance	The distance
 *
 *  Calculate the long path (resp. short path) of a given distance.
 *
 * \return the distance in kilometers for the opposite path.
 *
 * \sa qrb()
 */
double distance_long_path(double distance) {
	 return (ARC_IN_KM * 360.0) - distance;
}

/**
 * \brief Calculate the long path bearing between two points.
 * \param azimuth	The bearing
 *
 *  Calculate the long path (resp. short path) of a given bearing.
 *
 * \return the azimuth in decimal degrees for the opposite path.
 *
 * \sa qrb()
 */
double azimuth_long_path(double azimuth) {
	return 360.0 - azimuth;
}
