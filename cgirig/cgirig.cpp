#include "RpcRigClient.h"
#include "stdio.h"
#include "CGILib.h"

main( int argv, char** argc )
{
	freq_t freq = 0;
	RpcRigClient rig( "localhost" );
	
	CGIParser parser;
	parser.parse();
	CGIData data = parser.getData();
	
	if( data["freq"] != string( "" ) )
	{
		freq = atoi( data["freq"].c_str() );
		rig.setFrequency( freq );
	};

	freq = rig.getFrequency();

	printf( "Content-type: text/html\r\n\r\n" );
	printf( "<html><head><title>hamlib cgirig</title></head><body>" );
	printf( "<form method=\"get\" action=\"cgirig\">\n" );
	printf( "The frequency is " );
	printf( "<input type=\"text\" name=\"freq\" size=\"15\" value=\"%d\"> Hz \n", freq );
	printf( "<input type=\"submit\" value=\"Tune\">\n" );
	printf( "</form>\n" );
	printf( "</body></html>" );
}
