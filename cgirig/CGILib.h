////////////////////////////////////////////////////////////////////////////////
// 
// File:
//	CGILib.h
// Package:
//	CGILib
// Author:
// 	Ignacio A. Macías <mack@las.es>
// Description:
//	File header of the CGILib utils. For a description of the use
//	of the CGILib library, please browse the "CGILib.html" file.
//		Tested: GCC 2.7.2
//		Tested: MS Visual C++ 4.0
// Support:
// 	Please contact <mack@las.es> if you've any problem.
// Version:
// 	v1.0 Nov  4 1996 - Main algorithms.
// 
////////////////////////////////////////////////////////////////////////////////

# ifndef CGILIB_H
#  define CGILIB_H

#   include <string>

#  include <map.h>

typedef less<string> order;
			// MSVC++ 4.0 can't handle a direct mapping.
typedef map<string, string, order> CGIData;
			// CGIData is a STL map which associates the names
			// of the form inputs with the user selected values.

////////////////////////////////////////////////////////////////////////////////

// This exception is throwed by the CGILib on any error condition. The text
// contains the description of the problem.

class CGIException
	{
		string text;
	public:
		CGIException(const char *cstr);
		string getText() const;
	};

////////////////////////////////////////////////////////////////////////////////

// This is the main class. When called the "parse()" method, the cgi data
// received from the http server is analyzed and is ready for handling with the
// observer functions.

class CGIParser
	{
	private:
		string text;
			// Source text obtained from the server.
		string type;
			// Type of transaction: "GET", "POST" or "ISINDEX".
		CGIData data;
			// Map which associates names and values.
	public:
		void parse() throw (CGIException);
			//  Parses and decodes the CGI transaction.
		string getText() const;
			// Returns the transaction's complete text.
		string getType() const;
			// Returns the type of transaction. See above.
		CGIData getData() const;
			// Returns the parsed data;
		string getEnv(const char *var) const;
			// Receives the name of an environment variable and
			// returns her value as a string. If the variable is
			// not defined, an empty string is defined.
	private:
		string decode(const string &str) const;
			// Decodes the form text.
		char hexToChar(char c1, char c2) const;
			// Obtains hte char represented by the hex. value c1c2.
	};

////////////////////////////////////////////////////////////////////////////////

// Constructor of the global exception. Receives the text describing the error.

inline CGIException::CGIException(const char *cstr)
	: text(cstr)
{
}

//------------------------------------------------------------------------------

// Obtains the text of the exception catched.

inline string CGIException::getText() const
{
return text;
}

////////////////////////////////////////////////////////////////////////////////

inline string CGIParser::getText() const
{
return text;
}

//------------------------------------------------------------------------------

inline string CGIParser::getType() const
{
return type;
}

//------------------------------------------------------------------------------

inline CGIData CGIParser::getData() const
{
return data;
}

//------------------------------------------------------------------------------

inline string CGIParser::getEnv(const char *var) const
{
char *aux = getenv(var);
return (aux)? string(aux) : string();
}

////////////////////////////////////////////////////////////////////////////////

# endif
