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
 *
 *	$Id: locator.c,v 1.7 2003-08-19 23:41:08 n0nb Exp $
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
double dms2dec(int degrees, int minutes, int seconds)
{
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
void dec2dms(double dec, int *degrees, int *minutes, int *seconds)
{
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


/**
 * \brief Convert Maidenhead grid locator to longitude/latitude
 * \param longitude	The location where to store longitude, decimal
 * \param latitude	The location where to store latitude, decimal
 * \param locator	The locator--four or six char nul terminated string
 *
 *  Convert Maidenhead grid locator to longitude/latitude (decimal).
 *  The locator be either in 4 or 6 chars long format. locator2longlat
 *  is case insensitive, however it checks for locator validity.
 *
 *  Decimal long/lat is computed to center of grid square, i.e. given
 *  EM19 will return coordinates equivalent to the southwest corner
 *  of EM19mm.  Given a six character locator, computed values will
 *  in the center of the given subsquare, i.e. 2' 30" from west boundary
 *  and 1' 15" from south boundary.
 *
 * \todo Support for greater accuracy as proposed by Dave Hines to
 *  six pairs of grid designators.
 *
 * \return RIG_OK to indicate conversion went ok, -RIG_EINVAL if locator
 *  exceeds RR99xx or is malformed (not of 4 or 6 character format).
 *
 * \sa longlat2locator()
 */
int locator2longlat(double *longitude, double *latitude, const char *locator)
{
	char loc[6];
        int length;

	if (locator[4] != '\0' && locator[6] != '\0')
		return -RIG_EINVAL;

	loc[0] = toupper(locator[0]);
	loc[1] = toupper(locator[1]);
	loc[2] = locator[2];
	loc[3] = locator[3];
	if (locator[4] != '\0') {
		loc[4] = toupper(locator[4]);
		loc[5] = toupper(locator[5]);
                length = 6;
	} else {
                /* center of 4 character grid */
		loc[4] = 'M';
		loc[5] = 'M';
                length = 4;
	}
	if (loc[0] < 'A' || loc[0] > 'R' ||
		loc[1] < 'A' || loc[1] > 'R' ||
		loc[2] < '0' || loc[2] > '9' ||
		loc[3] < '0' || loc[3] > '9' ||
		loc[4] < 'A' || loc[4] > 'X' ||
		loc[5] < 'A' || loc[5] > 'X' ) {
			return -RIG_EINVAL;
	}

	*longitude = 20.0 * (loc[0]-'A') - 180.0 + 2.0 * (loc[2]-'0') + 
		(loc[4]-'A')/12.0;

        /* move east to center of subsquare */
	if (length == 6)
		*longitude += 0.04166666;

	*latitude = 10.0 * (loc[1]-'A') - 90.0 + (loc[3]-'0') +
							(loc[5]-'A')/24.0;
        /* move north to center of subsquare */
	if (length == 6)
		*latitude += 0.020833333;

	return RIG_OK;
}

/**
 * \brief Convert longitude/latitude to Maidenhead grid locator
 * \param longitude	The longitude, decimal
 * \param latitude	The latitude, decimal
 * \param locator	The location where to store the locator
 *
 *  Convert longitude/latitude (decimal) to Maidenhead grid locator.
 *  \a locator must point to an array at least 6 char long.
 *
 * \todo Support for greater accuracy as proposed by Dave Hines to
 *  six pairs of grid designators.
 *
 * \sa locator2longlat()
 */
void longlat2locator(double longitude, double latitude, char *locator)
{
	double tmp, min_sec;

	tmp = longitude;

	/* Ideally, the input should be constrained to
	 * >= -180. && < 179.9999999999999
         */
	if (tmp == 180.)
		tmp = -tmp;

	/* with input of -180 to 179 this expression will evaluate
	 * to 1 to 359 or degrees east of -180 degrees longitude.
         */
	tmp = fmod(tmp, 360) + 180.;

	/* determine west side of the field.  Fields always start at
	 * a longitude that is a multiple of 20.  Fields advance
         * eastward from -180 deg West longitude.
	 */
	locator[0] = 'A' + (int)floor(tmp/20.);
	tmp = fmod(tmp, 20.);

	/* at this point tmp = degrees east of west boundary
	 * of the field.
         */
	locator[2] = '0' + (int)floor(tmp/2.);

	min_sec = 12. * fabs(floor(longitude)-longitude);

	/* When tmp is an odd value, then we must be sure that
	 * the subsquare is referenced to 'm' as the longitude
	 * subsquare range spans 2 degrees.
	 */
	if ((int)tmp % 2)
		locator[4] = 'm' + (int)floor(min_sec);
	else
		locator[4] = 'a' + (int)floor(min_sec);

        /* input should be constrained to >= -90. && < 90. */
	tmp = fmod(latitude, 360) + 90.;

	locator[1] = 'A' + (int)floor(tmp/10.);
	tmp = fmod(tmp, 10.);
	locator[3] = '0' + (int)floor(tmp);
	tmp = 24. * fabs(floor(latitude)-latitude);
	locator[5] = 'a' + (int)floor(tmp);
}


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
				double *distance, double *azimuth)
{
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

	if (tmp > .999999)  {
		/* Station points coincide, use an Omni! */
		*distance = 0.0;
		*azimuth = 0.0;
		return 0;
	}

	if (tmp < -.999999)  {
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
double distance_long_path(double distance)
{
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
double azimuth_long_path(double azimuth)
{
	return 360.0 - azimuth;
}

