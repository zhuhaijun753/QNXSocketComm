/*
 * CSocketBase.h
 *
 *  Created on: 2017-10-31
 *      Author: win7
 */

#ifndef CSOCKETBASE_H_
#define CSOCKETBASE_H_

#include <string>
#include <functional>
#include "SMTypes.h"

#define MAX_PACKET_DATA_LEN (1024*4 - 16)    //包最大数据将长度

typedef struct _SM_PACKET_DATA
{
	int appId; //源AppID
	int toAppId;  //目标AppID,如果该ID为0 表示给所有已注册App

	struct _MSG_DATA
	{
		int msgType; //消息类型，必须和预定义消息区分
		int wParam;
		char lParam[MAX_PACKET_DATA_LEN];
	}msgData;
}T_SM_PACKET_DATA;

enum
{
	SMMsgType_CONNECT =100000,
};

typedef std::function<void(int appId, char* data, int len)> SocketDataChangedCallBackProc;

class CIPCBase {
public:
	virtual ~CIPCBase();
public:
	virtual int Create(const char *path, int nAppId) = 0;
	virtual int Destroy() = 0;
	virtual int Send(const void *pData, unsigned int nLen, int nAppID) = 0;
	virtual int Receive(void *buf, unsigned int nLen) = 0;
	virtual std::string GetImplementVersion() = 0;
	virtual void RegisterReceiveDataCallBack(SocketDataChangedCallBackProc cb) = 0;
};


extern "C" SMAPI_PORT CIPCBase* SMCALL GetIPCSingelObject();

#endif /* CSOCKETBASE_H_ */
