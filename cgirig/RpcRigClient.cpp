#include "RpcRigClient.h"

RpcRigClient::RpcRigClient( const char* hostname ) {

	pClient = clnt_create( hostname, RIGPROG, RIGVERS, "udp" );
};


RpcRigClient::~RpcRigClient() {

	if( pClient )  clnt_destroy( pClient );
};


const rig_model_t RpcRigClient::getModel() const {

	rig_model_t result = 0;
	if( pClient ) {
		model_x* pResult = getmodel_1( pClient );
		if( pResult ) {
			result = *pResult;
		};
	};

	return result;
};


const freq_t RpcRigClient::getFrequency() const {

	freq_t result = 0;
	if( pClient ) {
		freq_res* pResult = getfreq_1( RIG_VFO1, pClient );
		if( pResult ) {
			result = pResult->freq_res_u.freq.f1;
		};
	};

	return result;
};


void RpcRigClient::setFrequency( const freq_t freq ) {

	if( pClient ) {
		freq_arg f;
		f.vfo = RIG_VFO1;
		f.freq.f1 = freq;
		f.freq.f2 = 0;
		setfreq_1( f, pClient );
	};
};
