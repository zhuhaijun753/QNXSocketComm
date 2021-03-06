/*
 * CSocketServer.cpp
 *
 *  Created on: 2017-10-31
 *      Author: win7
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mutex>
#include <thread>
#include <sys/select.h>

#include "CSocketServer.h"
#include "SMErrorCode.h"


static std::mutex g_socketserver_mutex;
static const int g_max_listen_client_count = 20;

//监听超时时间单位us
static const int SELECT_SELECT_TIMEOUT = 500*1000;


CIPCServer::CIPCServer()
{
	// TODO Auto-generated constructor stub
	Contructor();
}

CIPCServer::~CIPCServer()
{
	// TODO Auto-generated destructor stub
}

int CIPCServer::Create(const char* path, int nAppId)
{
	return Init(path, nAppId);
}

int CIPCServer::Destroy()
{
	std::lock_guard<std::mutex> auto_lock(g_socketserver_mutex);
	if (-1 != m_nSocketFd)
	{
		close(m_nSocketFd);
		m_nSocketFd = -1;
		unlink(m_strFilePath.c_str());
	}
}

int CIPCServer::Send(const void* pData, unsigned int nLen, int nAppID)
{
}

int CIPCServer::Receive(void* buf, unsigned int nLen)
{
}

std::string CIPCServer::GetImplementVersion()
{
	return "1.0";
}

void CIPCServer::RegisterReceiveDataCallBack(
		SocketDataChangedCallBackProc cb)
{
}

void CIPCServer::Contructor()
{
	m_nSocketFd = -1;
	m_Client2AppMap.clear();
}

int CIPCServer::Init(const char* path, int nAppId)
{
	if (-1 != m_nSocketFd || NULL == path)
	{
		printf("socket has been inited\n");
		return SMErrorCode_InvalidCall;
	}

	std::lock_guard<std::mutex> auto_lock(g_socketserver_mutex);
	if (-1 != m_nSocketFd)
	{
		printf("socket has been inited\n");
		return SMErrorCode_InvalidCall;
	}

	/* delete the socket file */
	unlink(path);

	/* create a socket */
	int server_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == server_sockfd)
	{
		printf("Create socket failed.\n");
		return GetErrorCode(errno);
	}

	struct sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, path);

	/* bind with the local file */
	int nRet = bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (-1 == nRet)
	{
		printf("Bind failed.\n");
		int nRet = GetErrorCode(errno);
		Destroy();
		return nRet;
	}

	/* listen */
	nRet = listen(server_sockfd, g_max_listen_client_count);
	if (-1 == nRet)
	{
		printf("listen failed.\n");
		int nRet = GetErrorCode(errno);
		Destroy();
		return nRet;
	}

	//监听客户端线程
	std::thread([this, server_sockfd](){
		fd_set server_fd_set;
		int max_fd = -1;
		struct timeval tv;
		while(server_sockfd != -1)
		{
			tv.tv_sec = 0;
			tv.tv_usec = SELECT_SELECT_TIMEOUT;
			FD_ZERO(&server_fd_set);

			//服务器socket
			FD_SET(server_sockfd, &server_fd_set);
			if (max_fd < server_sockfd)
			{
				max_fd = server_sockfd;
			}

			//客户端链接
			for(auto it = m_ClientSocketFd.begin(); it != m_ClientSocketFd.end(); it++)
			{
				FD_SET(*it, &server_fd_set);
				if (max_fd < *it)
				{
					max_fd = *it;
				}
			}

			int nRet = select(max_fd + 1, &server_fd_set, NULL, NULL, &tv);
			if (nRet <= 0)
			{
				continue;
			}
			else
			{
				std::lock_guard<std::mutex> auto_lock(g_socketserver_mutex);
				if (FD_ISSET(server_sockfd, &server_fd_set))
				{
					//新的链接请求
					int client_sockfd = -1;
					struct sockaddr_un client_addr;
					socklen_t len = sizeof(client_addr);
					/* accept a connection */
					client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &len);
					if (-1 == client_sockfd)
					{
						printf("accept client failed[%s]\n", strerror(errno));
						continue;
					}
					else
					{
						printf("accept client.fd[%d]\n", client_sockfd);
						m_ClientSocketFd.push_back(client_sockfd);
					}
				}

				for (auto it = m_ClientSocketFd.begin(); it != m_ClientSocketFd.end();)
				{
					T_SM_PACKET_DATA data = { 0 };
					if (*it != 0 && FD_ISSET(*it, &server_fd_set))
					{
						//客户端发送过来的消息
						int nRead = read(*it, &data, sizeof(data));
						//断开消息
						if (nRead <= 0)
						{
							int nTempSock = *it;
							//printf("Read socketd failed.[%d]:[%s]", errno, strerror(errno));
							it = m_ClientSocketFd.erase(it); //需要记录删除后的迭代器作为新的迭代器
							for (auto cit = m_Client2AppMap.begin(); cit != m_Client2AppMap.end(); cit++)
							{
								if (cit->second == nTempSock)
								{
									printf("socekt[%d] disconnect\n", nTempSock);
									cit = m_Client2AppMap.erase(cit);
									break;
								}
							}
						}
						else
						{

							if (data.msgData.msgType == SMMsgType_CONNECT)
							{
								m_Client2AppMap[data.appId] = *it;
								printf("SMMsgType_CONNECT\n");
							}
							else
							{
								//如果带有目标Appid，直接发送给他
								if (data.toAppId > 0 && m_Client2AppMap.find(data.toAppId) != m_Client2AppMap.end())
								{
									write(m_Client2AppMap[data.toAppId], &data, sizeof(data));
								}
								else
								{
									//转发给除当前App之外的其他App
									for (auto it = m_ClientSocketFd.begin(); it != m_ClientSocketFd.end(); it++)
									{
										//printf("send to client fd[%d]\n", *it);
										if (*it > 0 && *it != m_Client2AppMap[data.appId])
										{
											write(*it, &data, sizeof(data));
										}
									}
								}
							}

							it++; //迭代器后移
						}

					}
					else
					{
						it++; //迭代器后移
					}
				}
			}

		}


	}).detach();

	m_nSocketFd = server_sockfd;
	m_strFilePath = path;

	return SMErrorCode_Success;
}

int CIPCServer::GetErrorCode(int nError)
{
	int nErrorCoe = SMErrorCode_Success;
	switch (nError)
	{
	case EACCES:
		nErrorCoe = SMErrorCode_PermissionDeny;
		break;
	case ENOMEM:
		nErrorCoe = SMErrorCode_OutOfMemory;
		break;
	case EPROTONOSUPPORT:
		nErrorCoe = SMErrorCode_UnSupported;
		break;
	case EOK:
		nErrorCoe = SMErrorCode_Success;
		break;
	default:
		nErrorCoe = SMErrorCode_Fail;
		break;
	}

	printf("Error occured.id [%d]:[%s]\n", nError, strerror(nError));
	return nErrorCoe;
}

CIPCBase* GetIPCServerSingelObject()
{
	return &Singleton<CIPCServer>::instance();
}
