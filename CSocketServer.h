/*
 * CSocketServer.h
 *
 *  Created on: 2017-10-31
 *      Author: win7
 */

#ifndef CSOCKETSERVER_H_
#define CSOCKETSERVER_H_


#include <map>
#include <vector>
#include "CSocketBase.h"
#include "singleton.h"

class CIPCServer : public CIPCBase
{
public:
	CIPCServer();
	virtual ~CIPCServer();

	virtual int Create(const char *path, int nAppId);
	virtual int Destroy();
	virtual int Send(const void *pData, unsigned int nLen, int nAppID);
	virtual int Receive(void *buf, unsigned int nLen);
	virtual std::string GetImplementVersion();
	virtual void RegisterReceiveDataCallBack(SocketDataChangedCallBackProc cb);
private:
	void Contructor();
	int Init(const char *path, int nAppId);
	int GetErrorCode(int nError); //将系统errno转换为内部error code
private:
	int m_nSocketFd;
	std::map<int, int> m_Client2AppMap;
	std::vector<int> m_ClientSocketFd;
	std::string m_strFilePath;
};

extern "C" SMAPI_PORT CIPCBase* SMCALL GetIPCServerSingelObject();

#endif /* CSOCKETSERVER_H_ */
