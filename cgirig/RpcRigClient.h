#ifndef __RPCRIGCLIENT_H__
#define __RPCRIGCLIENT_H__

#include "hamlib/rig.h"
#include "rpcrig.h"

class RpcRigClient {
public:
	RpcRigClient( const char* hostname );
	virtual ~RpcRigClient();
	const rig_model_t getModel() const;
	const freq_t getFrequency() const;
	void setFrequency( const freq_t freq );

private:
	CLIENT* pClient;
};

#endif // __RPCRIGCLIENT_H__
