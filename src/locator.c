/**
 * \file src/locator.c
 * \brief Ham Radio Control Libraries interface
 * \author Stephane Fillod
 * \date 2000-2001
 *
 * Hamlib interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib Interface - locator and bearing conversion calls
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: locator.c,v 1.2 2001-12-28 20:33:27 fillods Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


#include <hamlib/rotator.h>


#define RADIAN  (180.0 / M_PI)

/* arc length for 1 degree, 60 Nautical Miles */
#define ARC_IN_KM 111.2

/**
 * \brief Convert DMS angle to decimal representation
 * \param degrees	Degrees
 * \param minutes	Minutes
 * \param degrees	Seconds
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
		return (double)degrees + (double)minutes/60. + (double)seconds/3600.;
}

/**
 * \brief Convert decimal angle into DMS representation
 * \param dec	Decimal angle
 * \param degrees	The location where to store the degrees
 * \param minutes	The location where to store the minutes
 * \param degrees	The location where to store the seconds
 *
 *  Convert decimal angle into its degree/minute/second representation.
 *  Upon return dec2dms guarantees -180<=degrees<180,
 *  0<=minutes<60, and 0<=seconds<60.
 *
 * \sa dms2dec()
 */
void dec2dms(double dec, int *degrees, int *minutes, int *seconds)
{
		int deg, min, sec;
		double st;

		if (!degrees || !minutes || !seconds)
				return;

		st = fmod(dec+180, 360)-180;

		deg = (int)floor(st);
		st  = 60. * (st-(double)deg);
		min = (int)floor(st);
		st  = 60. * (st-(double)min);
		if (deg < 0 && min != 0) 
				min = 60 - min;
		sec = (int)floor(st);
		if (deg < 0 && sec != 0) 
				sec = 60 - sec;

		*degrees = deg;
		*minutes = min;
		*seconds = sec;
}


/**
 * \brief Convert Maidenhead grid locator to longitude/latitude
 * \param longitude	The location where to store longitude, decimal
 * \param latitude	The location where to store latitude, decimal
 * \param locator	The locator
 *
 *  Convert Maidenhead grid locator to longitude/latitude (decimal).
 *  The locator be either in 4 or 6 chars long format. locator2longlat
 *  is case insensitive, however it checks for locator validity.
 *
 * \todo: give center coordinate of locator
 *
 * \return 0 to indicate conversion went ok, otherwise a negative value.
 *
 * \sa longlat2locator()
 */
int locator2longlat(double *longitude, double *latitude, const char *locator)
{
	char loc[6];

	if (locator[4] != '\0' && locator[6] != '\0')
			return -1;

	loc[0] = toupper(locator[0]);
	loc[1] = toupper(locator[1]);
	loc[2] = locator[2];
	loc[3] = locator[3];
	if (locator[4] != '\0') {
		loc[4] = toupper(locator[4]);
		loc[5] = toupper(locator[5]);
	} else {
		loc[4] = 'A';
		loc[5] = 'A';
	}
	if (loc[0] < 'A' || loc[0] > 'R' ||
		loc[1] < 'A' || loc[1] > 'R' ||
		loc[2] < '0' || loc[2] > '9' ||
		loc[3] < '0' || loc[3] > '9' ||
		loc[4] < 'A' || loc[4] > 'X' ||
		loc[5] < 'A' || loc[5] > 'X' ) {
			return -1;
	}

	*longitude = 20.0 * (loc[0]-'A') - 180.0 + 2.0 * (loc[2]-'0') + 
										(loc[4]-'A')/12.0;
	if (loc[0] <= 'I' && (loc[2] != '0' || loc[4] != 'A'))
			*longitude += 1;

	*latitude = 10.0 * (loc[1]-'A') - 90.0 + (loc[3]-'0') + 
										(loc[5]-'A')/24.0;

	return 0;
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
 * \todo: give center coordinate of locator
 *
 * \sa locator2longlat()
 */
void longlat2locator(double longitude, double latitude, char *locator)
{
	double tmp;

	tmp = longitude + 180.;
	locator[0] = 'A' + (int)floor(tmp/20.);
	tmp = fmod(tmp, 20.);
	locator[2] = '0' + (int)floor(tmp/2.);
	tmp = 12.5*fmod(tmp, 2.);
	locator[4] = 'A' + (int)floor(tmp);

	tmp = latitude + 90.;
	locator[1] = 'A' + (int)(tmp/10.);
	tmp = fmod(tmp, 10.);
	locator[3] = '0' + (int)tmp;
	tmp = 25. * (tmp - trunc(tmp));
	locator[5] = 'A' + (int)tmp;
}


/**
 * \brief Calculate the bearing and azimuth between two points.
 * \param lon1	The local longitude, decimal degrees
 * \param lat1	The local latitude, decimal degrees
 * \param lon2	The remote longitude, decimal degrees
 * \param lat2	The remote latitude, decimal degrees
 * \param bearing	The location where to store the bearing
 * \param azimuth	The location where to store the azimuth
 *
 *  Calculate the QRB between \a lat1,\a lat1 and
 *  \a lon2,\a lat2, and return the bearing in kilometers and
 *  azimuth in decimal degrees for the short path.
 *
 *	This version also takes into consideration the two points 
 *	being close enough to be in the near-field, and the antipodal points, 
 *	which are easily calculated.
 *
 * \sa bearing_long_path(), azimuth_long_path()
 */
int qrb(double lon1, double lat1, double lon2, double lat2,
				double *bearing, double *azimuth)
{
	double delta_long, tmp, arc, cosaz, az;

	if (!bearing || !azimuth)
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

   	tmp = sin(lat1) * sin(lat2)  +  cos(lat1) * cos(lat2) * cos(delta_long);

	if (tmp > .999999)  {
		/* Station points coincide, use an Omni! */
			*bearing = 0.0;
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

		*bearing = 180.0*ARC_IN_KM;
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

	*bearing = ARC_IN_KM * RADIAN * arc;

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
	 */

    if (sin(delta_long) < 0.0)  {
		*azimuth = az;
	} else  {
		*azimuth = 360.0 - az;
	}

	return 0;
}

double bearing_long_path(double bearing)
{
	 return (ARC_IN_KM * 360.0) - bearing;
}

double azimuth_long_path(double azimuth)
{
		return 360.0-azimuth;
}


