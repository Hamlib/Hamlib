/**
 * \file src/locator.c
 * \ingroup hamlib
 * \brief locator and bearing conversion interface
 * \author Stephane Fillod and the Hamlib Group
 * \date 2000-2003
 *
 *  Hamlib Interface - locator, bearing, and conversion calls
 */

/*
 *  Hamlib Interface - locator and bearing conversion calls
 *  Copyright (c) 2001-2002 by Stephane Fillod
 *  Copyright (c) 2003 by Nate Bargmann
 *  Copyright (c) 2003 by Dave Hines
 *
 *	$Id: locator.c,v 1.13 2003-11-03 04:26:37 n0nb Exp $
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

/* Approximate radius of the earth in km */
#define RADIUS_IN_KM       6378.8

/* arc length for 1 degree, 60 Nautical Miles */
#define ARC_IN_KM 111.2

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
 *
 * MAX_LOCATOR_PAIRS now sets the limit locator2longlat() will convert and
 * sets the maximum length longlat2locator() will generate.  Each function
 * properly handles any value from 1 to 6 so MAX_LOCATOR_PAIRS should be
 * left at 6.  MIN_LOCATOR_PAIRS sets a floor on the shortest locator that
 * should be handled.  -N0NB
 *
 */
const static double loc_char_weight[] = { 600.0, 60.0, 2.5, 0.25, 0.01, 0.001 };
const static int loc_char_range[] = { 18, 10, 24, 10, 25, 10 };
#define MAX_LOCATOR_PAIRS       6
#define MIN_LOCATOR_PAIRS       1

/* end dph */

#endif	/* !DOC_HIDDEN */

/**
 * \brief Convert DMS to decimal degrees
 * \param degrees	Degrees
 * \param minutes	Minutes
 * \param seconds	Seconds
 * \param sw            South or West
 *
 *  Convert degree/minute/second angle to decimal degrees angle.
 *  \a degrees >360, \a minutes > 60, and \a seconds > 60 are allowed,
 *  but resulting angle won't be normalized.
 *
 *  When the variable sw is passed a value of 1, the returned decimal
 *  degrees value will be negative (south or west).  When passed a
 *  value of 0 the returned decimal degrees value will be positive
 *  (north or east).
 *
 * \return The angle in decimal degrees.
 *
 * \sa dec2dms()
 */

double dms2dec(int degrees, int minutes, double seconds, int sw) {
	double st;

//	s = copysign(1.0, (double)degrees);
//	st = fabs((double)degrees);

	if (degrees < 0)
		degrees = abs(degrees);
	if (minutes < 0)
		minutes = abs(minutes);
	if (seconds < 0)
		seconds = fabs(seconds);

	st = (double)degrees + (double)minutes / 60. + seconds / 3600.;

	if (sw == 1)
		return -st;
	else
                return st;
}

/**
 * \brief Convert D M.MMM notation to decimal degrees
 * \param degrees	Degrees
 * \param minutes	Minutes
 * \param sw            South or West
 *
 *  Convert a degrees, decimal minutes notation common on
 *  many GPS units to its decimal degrees value.
 *
 *  \a degrees > 360, \a minutes > 60 are allowed, but
 *  resulting angle won't be normalized.
 *
 *  When the variable sw is passed a value of 1, the returned decimal
 *  degrees value will be negative (south or west).  When passed a
 *  value of 0 the returned decimal degrees value will be positive
 *  (north or east).
 *
 * \return The angle in decimal degrees.
 *
 * \sa dec2dmmm()
 */

double dmmm2dec(int degrees, double minutes, int sw) {
	double st;

	if (degrees < 0)
		degrees = abs(degrees);
	if (minutes < 0)
		minutes = fabs(minutes);

	st = (double)degrees + minutes / 60.;

	if (sw == 1)
		return -st;
	else
                return st;
}

/**
 * \brief Convert decimal degrees angle into DMS notation
 * \param dec		Decimal angle
 * \param degrees	The address of the degrees
 * \param minutes	The address of the minutes
 * \param seconds	The address of the seconds
 * \param sw            The address of the sw flag
 *
 *  Convert decimal degrees angle into its degree/minute/second
 *  notation.
 *
 *  When \a dec < -180 or \a dec > 180, the angle will be normalized
 *  within these limits and the sign set appropriately.
 *
 *  When \a dec is < 0 \a sw will be set to 1.  When \a dec is
 *  >= 0 \a sw will be set to 0.
 *
 *  Upon return dec2dms guarantees -180 <= \a degrees < 180,
 *  0 <= \a minutes < 60, and 0 <= \a seconds < 60.
 *
 * \retval -RIG_EINVAL if any of the pointers are NULL.
 * \retval RIG_OK if conversion went OK.
 *
 * \sa dms2dec()
 */

int dec2dms(double dec, int *degrees, int *minutes, double *seconds, int *sw) {
//	int s = 0;
	int deg, min;
	double st;

	/* bail if NULL pointers passed */
	if (!degrees || !minutes || !seconds || !sw)
		return -RIG_EINVAL;

	/* reverse the sign if dec has a magnitude greater
	 * than 180 and factor out multiples of 360.
	 * e.g. when passed 270 st will be set to -90
	 * and when passed -270 st will be set to 90.  If
	 * passed 361 st will be set to 1, etc.  If passed
	 * a value > -180 || < 180, value will be unchanged.
	 */
	if (dec >= 0.0)
		st = fmod(dec + 180, 360) - 180;
	else
		st = fmod(dec - 180, 360) + 180;

	/* if after all of that st is negative, we want deg
	 * to be negative as well except for 180 which we want
	 * to be positive.
	 */
	if (st < 0.0 && st != -180)
		*sw = 1;
	else
		*sw = 0;

	/* work on st as a positive value to remove a
	 * bug introduced by the effect of floor() when
	 * passed a negative value.  e.g. when passed
	 * -96.8333 floor() returns -95!  Also avoids
	 * a rounding error introduced on negative values.
	 */
	st = fabs(st);

	deg = (int)floor(st);
	st  = 60. * (st - (double)deg);
	min = (int)floor(st);
	st  = 60. * (st - (double)min);

	/* set *degrees to sign determined by fmod() */
//	(s == 1) ? (*degrees = -deg) : (*degrees = deg);

	*degrees = deg;
	*minutes = min;
	*seconds = st;

	return RIG_OK;
}

/**
 * \brief Convert a decimal angle into D M.MMM notation
 * \param dec		Decimal angle
 * \param degrees	The address of the degrees
 * \param minutes	The address of the minutes
 * \param sw            The address of the sw flag
 *
 *  Convert a decimal angle into its degree, decimal minute
 *  notation common on many GPS units.
 *
 *  When passed a value < -180 or > 180, the value will be normalized
 *  within these limits and the sign set apropriately.
 *
 *  When \a dec is < 0 \a sw will be set to 1.  When \a dec is
 *  >= 0 \a sw will be set to 0.
 *
 *  Upon return dec2dmmm guarantees -180 <= \a degrees < 180,
 *  0 <= \a minutes < 60.
 *
 * \retval -RIG_EINVAL if any of the pointers are NULL.
 * \retval RIG_OK if conversion went OK.
 *
 * \sa dmmm2dec()
 */

int dec2dmmm(double dec, int *degrees, double *minutes, int *sw) {
	int r, min;
	double sec;

	/* bail if NULL pointers passed */
	if (!degrees || !minutes || !sw)
		return -RIG_EINVAL;

	r = dec2dms(dec, degrees, &min, &sec, sw);
	if (r != RIG_OK)
		return r;

	*minutes = (double)min + sec / 60;

	return RIG_OK;
}

/**
 * \brief Convert Maidenhead grid locator to longitude/latitude
 * \param longitude	The location where to store longitude, decimal degrees
 * \param latitude	The location where to store latitude, decimal degrees
 * \param locator	The locator--2 through 12 char + nul string
 *
 *  Convert Maidenhead grid locator to longitude/latitude (decimal).
 *  The locator should be in 2 through 12 chars long format.
 *  \a locator2longlat is case insensitive, however it checks for
 *  locator validity.
 *
 *  Decimal long/lat is computed to center of grid square, i.e. given
 *  EM19 will return coordinates equivalent to the southwest corner
 *  of EM19mm.  Given a six character locator, computed values will
 *  in the center of the given subsquare, i.e. 2' 30" from west boundary
 *  and 1' 15" from south boundary.
 *
 * \retval -RIG_EINVAL if locator exceeds RR99xx99yy99 or exceeds length
 *  limit--currently 1 to 6 lon/lat pairs.
 * \retval RIG_OK if conversion went OK.
 *
 * \bug The fifth pair ranges from aa to yy, there is another convention
 *  that ranges from aa to xx.  At some point both conventions should be
 *  supported.
 *
 * \sa longlat2locator()
 */

/* begin dph */

int locator2longlat(double *longitude, double *latitude, const char *locator) {
	int x_or_y, paircount;
        int locvalue, pair;
	double xy[2], minutes;

	/* bail if NULL pointers passed */
	if (!longitude || !latitude)
		return -RIG_EINVAL;

	paircount = strlen(locator) / 2;

	/* verify paircount is within limits */
	if (paircount > MAX_LOCATOR_PAIRS)
		paircount = MAX_LOCATOR_PAIRS;
	else if (paircount < MIN_LOCATOR_PAIRS)
		return -RIG_EINVAL;

	/* For x(=lon) and y(=lat) */
	for (x_or_y = 0;  x_or_y < 2;  ++x_or_y) {
		minutes = 0.0;

		for (pair = 0;  pair < paircount;  ++pair) {
			locvalue = locator[pair*2 + x_or_y];

			/* Value of digit or letter */
			locvalue -= (loc_char_range[pair] == 10) ? '0' :
				(isupper(locvalue)) ? 'A' : 'a';

			/* Check range for non-letter/digit or out of range */
			if (((unsigned) locvalue) >= loc_char_range[pair])
				return -RIG_EINVAL;

			minutes += locvalue * loc_char_weight[pair];
		}
		/* Center coordinate */
		minutes += loc_char_weight[paircount - 1] / 2.0;

		xy[x_or_y] = minutes / 60.0 - 90.0;
	}

	*longitude = xy[0] * 2;
	*latitude = xy[1];

	return RIG_OK;
}
/* end dph */

/**
 * \brief Convert longitude/latitude to Maidenhead grid locator
 * \param longitude	The longitude, decimal degrees
 * \param latitude	The latitude, decimal degrees
 * \param locator	The location where to store the locator
 * \param pair_count	The desired precision expressed as lon/lat pairs in the locator
 *
 *  Convert longitude/latitude (decimal degrees) to Maidenhead grid locator.
 *  \a locator must point to an array at least \a pair_count * 2 char + '\0'.
 *
 * \retval -RIG_EINVAL if \a locator is NULL or \a pair_count exceeds
 *  length limit.  Currently 1 to 6 lon/lat pairs.
 * \retval RIG_OK if conversion went OK.
 *
 * \bug \a locator is not tested for overflow.
 * \bug The fifth pair ranges from aa to yy, there is another convention
 *  that ranges from aa to xx.  At some point both conventions should be
 *  supported.
 *
 * \sa locator2longlat()
 */

/* begin dph */

int longlat2locator(double longitude, double latitude,
		    char *locator, int pair_count) {
	int x_or_y, pair, locvalue;
	double tmp;

	if (!locator)
                return -RIG_EINVAL;

	if (pair_count < MIN_LOCATOR_PAIRS || pair_count > MAX_LOCATOR_PAIRS)
                return -RIG_EINVAL;

	for (x_or_y = 0;  x_or_y < 2;  ++x_or_y) {
		tmp = ((x_or_y == 0) ? longitude / 2. : latitude);

		/* The 1e-6 here guards against floating point rounding errors */
		tmp = fmod(tmp + 270., 180.) * 60. + 1e-6;
		for (pair = 0;  pair < pair_count;  ++pair) {
			locvalue = (int) (tmp / loc_char_weight[pair]);

			/* assert(locvalue < loc_char_range[pair]); */
			tmp -= loc_char_weight[pair] * locvalue;
			locvalue += (loc_char_range[pair] == 10) ? '0':'A';
			locator[pair * 2 + x_or_y] = locvalue;
		}
	}
        locator[pair_count * 2] = '\0';

	return RIG_OK;
}

/* end dph */

/**
 * \brief Calculate the distance and bearing between two points.
 * \param lon1		The local longitude, decimal degrees
 * \param lat1		The local latitude, decimal degrees
 * \param lon2		The remote longitude, decimal degrees
 * \param lat2		The remote latitude, decimal degrees
 * \param distance	The location where to store the distance
 * \param azimuth	The location where to store the bearing
 *
 *  Calculate the QRB between \a lon1, \a lat1 and \a lon2, \a lat2.
 *
 *	This version also takes into consideration the two points
 *	being close enough to be in the near-field, and the antipodal points,
 *	which are easily calculated.
 *
 * \retval -RIG_EINVAL if NULL pointer passed or lat and lon values
 * exceed -90 to 90 or -180 to 180.
 * \retval RIG_OK if calculations are successful.
 *
 * \return The distance in kilometers and azimuth in decimal degrees
 *  for the short path are stored in \a distance and \a azimuth.
 *
 * \sa distance_long_path(), azimuth_long_path()
 */

int qrb(double lon1, double lat1, double lon2, double lat2,
				double *distance, double *azimuth) {
	double delta_long, tmp, arc, az;
//        double cosaz, a, c, dlon, dlat;

	/* bail if NULL pointers passed */
	if (!distance || !azimuth)
		return -RIG_EINVAL;

	if ((lat1 > 90.0 || lat1 < -90.0) || (lat2 > 90.0 || lat2 < -90.0))
		return -RIG_EINVAL;

	if ((lon1 > 180.0 || lon1 < -180.0) || (lon2 > 180.0 || lon2 < -180.0))
		return -RIG_EINVAL;

	/* Prevent ACOS() Domain Error */

	if (lat1 == 90.0)
		lat1 = 89.999999999;
	else if (lat1 == -90.0)
		lat1 = -89.999999999;

	if (lat2 == 90.0)
		lat2 = 89.999999999;
	else if (lat2 == -90.0)
		lat2 = -89.999999999;

	/* Convert variables to Radians */
	lat1	/= RADIAN;
	lon1	/= RADIAN;
   	lat2	/= RADIAN;
   	lon2	/= RADIAN;

   	delta_long = lon2 - lon1;

   	tmp = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(delta_long);

	if (tmp > .999999999999999) {
		/* Station points coincide, use an Omni! */
		*distance = 0.0;
		*azimuth = 0.0;
		return RIG_OK;
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
		return RIG_OK;
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

/*	cosaz = (sin(lat2) - (sin(lat1) * cos(arc))) /
                (sin(arc) * cos(lat1));

	if (cosaz > .999999)
		az = 0.0;
	else if (cosaz < -.999999)
		az = 180.0;
	else
		az = acos(cosaz) * RADIAN;
*/
	/*
	 * Handbook had the test ">= 0.0" which looks backwards??
	 * must've been frontwards since the numbers seem to make sense
	 * now.  ;-)  -N0NB
	 */
//	if (sin(delta_long) < 0.0)  {
/*	if (sin(delta_long) >= 0.0)  {
		*azimuth = az;
	} else {
		*azimuth = 360.0 - az;
	}

	if (*azimuth == 360.0)
		*azimuth = 0;

*/

	/* This formula seems to work with very small distances
	 *
	 * I found it on the Web at:
	 * http://williams.best.vwh.net/avform.htm#Crs
	 *
	 * Strangely, all the computed values were negative thus the
         * sign reversal below.
	 * - N0NB
         */
	az = RADIAN * fmod(atan2(sin(lon1 - lon2) * cos(lat2),
				 cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(lon1 - lon2)), 2 * M_PI);
	if (lon1 > lon2) {
		az -= 360.;
		*azimuth = -az;
	} else {
		if (az >= 0.0)
			*azimuth = az;
		else
			*azimuth = -az;
	}
	return RIG_OK;
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
