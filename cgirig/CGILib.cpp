////////////////////////////////////////////////////////////////////////////////
// 
// File:
//	CGILib.cpp
// Package:
//	CGILib
// Author:
// 	Ignacio A. Macías <mack@las.es>
// Description:
//	Body of the methods defined in "CGILib.h".
//		Tested: GCC 2.7.2
//		Tested: MS Visual C++ 4.0
// Support:
// 	Please contact <mack@las.es> if you've any problem.
// Version:
// 	v1.0 Nov  4 1996 - Main algorithms.
// 
////////////////////////////////////////////////////////////////////////////////

# include <stdlib.h>
# include <string.h>
# include <ctype.h>

# include "CGILib.h"

////////////////////////////////////////////////////////////////////////////////

void CGIParser::parse() throw (CGIException)
{
type = getEnv("REQUEST_METHOD");

if (!type.length())
	throw CGIException("Invalid request method");

if (type == "POST")
			// Must read data from cin.
	{
	string conLen(getEnv("CONTENT_LENGTH"));
			// EOF is not present. Need the length of the data.
	if (!conLen.length())
		throw CGIException("Invalid content length");

	int length = atoi(conLen.c_str());
	char *buffer = new char[length + 1];

	if (length > 0)
		{
		cin.read(buffer,length);
		if (cin.bad())
			throw CGIException("Error reading in POST");
		}
	buffer[length] = 0;
			// Mark the end of the string.
	text = buffer;
			// Preserve.
	delete buffer;
	}
else
	if (type == "GET")
		text = getEnv("QUERY_STRING");
			// The data is obtained from the environment.
	else
		throw CGIException("Unknown Request Method");

size_t ini, end, equ;

if (text.find_first_of("&=",0) == string::npos)
	type = "ISINDEX";
			// ISINDEX queries do not have pairs name-value.
else
			// Parses & decodes the string in pairs name-value.
			// ONLY standard requests are handled properly.
	for ( ini = 0, end = string::npos - 1 ; end != string::npos ; ini = end + 1)
		{
		end = text.find_first_of('&',ini);
		equ = text.find_first_of('=',ini);
		data[decode(text.substr(ini, equ - ini))]
				= decode(text.substr(equ + 1, end - equ - 1));
		}

text = decode(text);
			// Decodes the global text.
}

//------------------------------------------------------------------------------

// Translates the urlencoded string.

string CGIParser::decode(const string &cod) const
{
string dec = cod;
			// Buffer.
register unsigned int get;
			// Origin index.
register unsigned int set;
			// Target index.

for (get = 0, set = 0 ; get < dec.length() ; set++, get++)
	if (dec[get] == '+')
		dec[set] = ' ';
			// Pluses in spaces.
	else
		if (dec[get] == '%')
			{
			dec[set] = hexToChar(dec[get + 1], dec[get + 2]);
			get += 2;
			}
			// Hex values in their represented char.
		else
			dec[set] = dec[get];
dec.resize(set);
			// Cut the rest of the buffer.
return dec;
}

//------------------------------------------------------------------------------

// Translates a two-char hex representation in his corresponding value.

char CGIParser::hexToChar(char c1, char c2) const
{
c1 = tolower(c1);
c2 = tolower(c2);
			// Some browsers/servers send uppercase values.
return ((c2 < 'a')? c2 - '0' : c2 - 'a' + 10) + 
	((c1 < 'a')? c1 - '0' : c1 - 'a' + 10) * 16;
}

